#include "Spotify.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <bcrypt.h>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>
#include <wincrypt.h>
#include <winhttp.h>
#include <shellapi.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")

namespace {
    std::string performTokenRequest(const std::wstring& host, const std::wstring& path, const std::string& formBody) {
        HINTERNET session = WinHttpOpen(L"SpotifyQuickWheel/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) { return {}; }

        HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connect) {
            WinHttpCloseHandle(session);
            return {};
        }

        HINTERNET request = WinHttpOpenRequest(connect, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request) {
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return {};
        }

        std::string response;
        const wchar_t* headers = L"Content-Type: application/x-www-form-urlencoded";

        if (WinHttpSendRequest(request, headers, (DWORD)-1, const_cast<char*>(formBody.data()), (DWORD)formBody.size(), (DWORD)formBody.size(), 0) 
                && WinHttpReceiveResponse(request, nullptr)) {
            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

            if (status == 200) {
                DWORD available = 0;
                do {
                    if (!WinHttpQueryDataAvailable(request, &available)) { break; }

                    std::vector<char> buffer(available ? available : 1);
                    DWORD read = 0;

                    if (!WinHttpReadData(request, buffer.data(), (DWORD)buffer.size(), &read)) { break; }

                    response.append(buffer.data(), read);
                } while (available > 0);
            }
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);

        return response;
    }

    std::string jsonStringValue(const std::string& json, const char* key) {
        std::string needle = std::string("\"") + key + "\"";
        size_t pos = json.find(needle);

        if (pos == std::string::npos) { return {}; }

        pos = json.find(':', pos);
        if (pos == std::string::npos) { return {}; }

        pos = json.find('"', pos);
        if (pos == std::string::npos) { return {}; }

        ++pos;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) { return {}; }

        return json.substr(pos, end - pos);
    }
}

SpotifyAuth::SpotifyAuth() {}

bool SpotifyAuth::login() {
    if (clientId.empty()) { return false; }
    openBrowser(buildAuthorizationUrl());

    std::string code;

    if (!waitForCallback(code)) { return false; }
    if (!exchangeCode(code)) { return false; }

    authenticated = true;
    return true;
}

bool SpotifyAuth::ensureAuthenticated() {
    if (isAuthenticated()) { return true; }

    refreshToken = loadRefreshToken();
    if (!refreshToken.empty() && refreshAccessToken()) { return true; }

    if (login()) {
        saveRefreshToken(refreshToken);
        return true;
    }

    return false;
}

bool SpotifyAuth::isAuthenticated() const {
    return authenticated;
}

std::string SpotifyAuth::generateRandomString(size_t length) {
    static constexpr char chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "-._~";

    std::random_device rd;
    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        result += chars[rd() % (sizeof(chars) - 1)];
    }

    return result;
}

std::string SpotifyAuth::sha256Url(const std::string& input) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return {};
    }

    DWORD objectSize = 0;
    DWORD resultSize = 0;
    if (BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &resultSize, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    std::vector<unsigned char> hashObject(objectSize);
    if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    std::vector<unsigned char> digest(32);
    bool ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())), static_cast<ULONG>(input.size()), 0) == 0 
                && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) == 0;

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (!ok) { return {}; }

    return std::string(digest.begin(), digest.end());
}

std::string SpotifyAuth::Base64Encode(const std::string& raw) {
    const BYTE* bytes = reinterpret_cast<const BYTE*>(raw.data());
    DWORD size = 0;

    if (!CryptBinaryToStringA(bytes, static_cast<DWORD>(raw.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &size)) { return {}; }

    std::string encoded(size, '\0');
    if (!CryptBinaryToStringA(bytes, static_cast<DWORD>(raw.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded.data(), &size)) { return {}; }
    encoded.resize(encoded.find('\0'));

    encoded.erase(std::remove(encoded.begin(), encoded.end(), '='), encoded.end());
    std::replace(encoded.begin(), encoded.end(), '+', '-');
    std::replace(encoded.begin(), encoded.end(), '/', '_');

    return encoded;
}

std::string SpotifyAuth::urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped << std::hex << std::uppercase;

    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') 
            || (c >= 'a' && c <= 'z') 
            || (c >= '0' && c <= '9') 
            || c == '-' 
            || c == '_' 
            || c == '.' 
            || c == '~') {
            escaped << c;
        }
        else {
            escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }

    return escaped.str();
}

std::string SpotifyAuth::buildAuthorizationUrl() {
    codeVerifier = generateRandomString(64);
    state = generateRandomString(32);

    std::string codeChallenge = Base64Encode(sha256Url(codeVerifier));

    std::ostringstream url;
    url << "https://accounts.spotify.com/authorize"
        << "?client_id=" << clientId
        << "&response_type=code"
        << "&redirect_uri=" << REDIRECT_URI
        << "&scope=" << SCOPE
        << "&state=" << state
        << "&code_challenge_method=S256"
        << "&code_challenge=" << codeChallenge;

    return url.str();
}

bool SpotifyAuth::waitForCallback(std::string& code) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { return false; }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    int reuse = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(8888);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    constexpr int kTimeoutSeconds = 120;
    bool success = false;

    for (int elapsed = 0; elapsed < kTimeoutSeconds && !success; ++elapsed) {
        fd_set readSet;
        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        FD_ZERO(&readSet);
        FD_SET(listenSocket, &readSet);

        if (select(0, &readSet, nullptr, nullptr, &timeout) <= 0) { continue; }

        SOCKET client = accept(listenSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET) { continue; }

        char buffer[4096] = {};
        int received = recv(client, buffer, static_cast<int>(sizeof(buffer) - 1), 0);

        std::string body = "Invalid callback request.";
        bool stop = false;

        if (received > 0) {
            std::istringstream request(buffer);
            std::string method, target, version;
            request >> method >> target >> version;

            std::string query;
            size_t queryPos = target.find('?');
            if (queryPos != std::string::npos) {
                query = target.substr(queryPos + 1);
            }

            std::string receivedState;
            std::string receivedCode;
            std::string receivedError;

            size_t start = 0;
            while (start <= query.size()) {
                size_t amp = query.find('&', start);
                if (amp == std::string::npos) {
                    amp = query.size();
                }

                std::string pair = query.substr(start, amp - start);
                start = amp + 1;

                size_t eq = pair.find('=');
                if (eq == std::string::npos) { continue; }

                std::string key = pair.substr(0, eq);
                std::string value = pair.substr(eq + 1);

                if (key == "state") {
                    receivedState = value;
                }
                else if (key == "code") {
                    receivedCode = value;
                }
                else if (key == "error") {
                    receivedError = value;
                }
            }

            if (receivedState != this->state) {
                body = "Invalid state.";
            }
            else if (!receivedError.empty()) {
                body = "Spotify authorization failed.";
                stop = true;
            }
            else if (!receivedCode.empty()) {
                code = receivedCode;
                body = "Spotify connected! You can close this window.";
                success = true;
                stop = true;
            }
            else {
                body = "Missing authorization code.";
            }
        }

        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: text/plain\r\n"
                 << "Content-Length: " << body.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << body;
        std::string responseText = response.str();
        send(client, responseText.data(), static_cast<int>(responseText.size()), 0);

        closesocket(client);

        if (stop) { break; }
    }

    closesocket(listenSocket);
    WSACleanup();
    return success;
}

void SpotifyAuth::openBrowser(const std::string& url) {
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool SpotifyAuth::exchangeCode(const std::string& code) {
    std::ostringstream body;
    body << "grant_type=authorization_code"
         << "&code=" << urlEncode(code)
         << "&redirect_uri=" << urlEncode(REDIRECT_URI)
         << "&client_id=" << urlEncode(clientId)
         << "&code_verifier=" << urlEncode(codeVerifier);

    std::string json = performTokenRequest(L"accounts.spotify.com", L"/api/token", body.str());
    if (json.empty()) { return false; }

    accessToken = jsonStringValue(json, "access_token");
    std::string newRefreshToken = jsonStringValue(json, "refresh_token");
    if (!newRefreshToken.empty()) {
        refreshToken = newRefreshToken;
    }

    return !accessToken.empty();
}

bool SpotifyAuth::refreshAccessToken() {
    if (refreshToken.empty()) { return false; }

    std::ostringstream body;
    body << "grant_type=refresh_token"
         << "&refresh_token=" << urlEncode(refreshToken)
         << "&client_id=" << urlEncode(clientId);

    std::string json = performTokenRequest(L"accounts.spotify.com", L"/api/token", body.str());
    if (json.empty()) { return false; }

    std::string newAccessToken = jsonStringValue(json, "access_token");
    if (newAccessToken.empty()) { return false; }

    accessToken = newAccessToken;

    std::string newRefreshToken = jsonStringValue(json, "refresh_token");
    if (!newRefreshToken.empty()) {
        refreshToken = newRefreshToken;
    }

    authenticated = true;
    return true;
}

std::string SpotifyAuth::tokenFilePath() const {
    const char* appData = std::getenv("APPDATA");
    std::string base = appData ? appData : ".";
    return base + "\\SpotifyQuickWheel\\refresh_token.bin";
}

void SpotifyAuth::saveRefreshToken(const std::string& token) {
    std::string path = tokenFilePath();

    size_t slash = path.find_last_of('\\');
    if (slash != std::string::npos) {
        std::string directory = path.substr(0, slash);
        CreateDirectoryA(directory.c_str(), nullptr);
    }

    std::ofstream out(path, std::ios::trunc);
    if (out.is_open()) {
        out << token;
    }
}

std::string SpotifyAuth::loadRefreshToken() {
    std::ifstream in(tokenFilePath());
    std::string token;
    if (in.is_open()) {
        std::getline(in, token);
    }
    return token;
}
