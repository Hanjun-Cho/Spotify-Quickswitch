#include "Http.h"

#include <windows.h>
#include <winhttp.h>

#include <cctype>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace {

    class JsonParser {
    public:
        explicit JsonParser(const std::string& text) : text(text) {}

        bool parse(Http::JsonNode& out) {
            skipWhitespace();
            return parseValue(out);
        }

    private:
        const std::string& text;
        size_t pos = 0;

        void skipWhitespace() {
            while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) { ++pos; }
        }

        bool parseValue(Http::JsonNode& out) {
            skipWhitespace();
            if (pos >= text.size()) { return false; }

            switch (text[pos]) {
                case '{': return parseObject(out);
                case '[': return parseArray(out);
                case '"': return parseString(out);
                case 't': case 'f': return parseLiteral(out);
                case 'n': { out = Http::JsonNode{}; return consumeLiteral("null"); }
                default: return parseNumber(out);
            }
        }

        bool parseString(Http::JsonNode& out) {
            if (pos >= text.size() || text[pos] != '"') { return false; }
            ++pos;

            std::string value;
            while (pos < text.size()) {
                char c = text[pos++];
                if (c == '"') {
                    out.type = Http::JsonNode::Type::String;
                    out.stringValue = value;
                    return true;
                }
                if (c == '\\') {
                    if (pos >= text.size()) { return false; }
                    char esc = text[pos++];
                    switch (esc) {
                        case '"': value += '"'; break;
                        case '\\': value += '\\'; break;
                        case '/': value += '/'; break;
                        case 'b': value += '\b'; break;
                        case 'f': value += '\f'; break;
                        case 'n': value += '\n'; break;
                        case 'r': value += '\r'; break;
                        case 't': value += '\t'; break;
                        case 'u': {
                            if (pos + 4 > text.size()) { return false; }
                            value += static_cast<char>(std::stoi(text.substr(pos, 4), nullptr, 16));
                            pos += 4;
                            break;
                        }
                        default: return false;
                    }
                } else {
                    value += c;
                }
            }
            return false;
        }

        bool parseLiteral(Http::JsonNode& out) {
            skipWhitespace();
            if (text.compare(pos, 4, "true") == 0) {
                out.type = Http::JsonNode::Type::Bool;
                out.boolValue = true;
                pos += 4;
                return true;
            }
            if (text.compare(pos, 5, "false") == 0) {
                out.type = Http::JsonNode::Type::Bool;
                out.boolValue = false;
                pos += 5;
                return true;
            }
            return false;
        }

        bool consumeLiteral(const char* literal) {
            skipWhitespace();
            size_t len = 0;
            while (literal[len]) { ++len; }
            if (text.compare(pos, len, literal) == 0) {
                pos += len;
                return true;
            }
            return false;
        }

        bool parseNumber(Http::JsonNode& out) {
            skipWhitespace();
            size_t start = pos;
            while (pos < text.size()) {
                char c = text[pos];
                if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' ||
                    c == 'e' || c == 'E') {
                    ++pos;
                } else {
                    break;
                }
            }
            if (pos == start) { return false; }

            out.type = Http::JsonNode::Type::Number;
            out.longValue = static_cast<long>(std::stoll(text.substr(start, pos - start)));
            return true;
        }

        bool parseArray(Http::JsonNode& out) {
            skipWhitespace();
            if (pos >= text.size() || text[pos] != '[') { return false; }
            ++pos;

            out.type = Http::JsonNode::Type::Array;
            skipWhitespace();
            if (pos < text.size() && text[pos] == ']') { ++pos; return true; }

            while (true) {
                Http::JsonNode element;
                if (!parseValue(element)) { return false; }
                out.arrayValue.push_back(element);

                skipWhitespace();
                if (pos >= text.size()) { return false; }
                if (text[pos] == ']') { ++pos; return true; }
                if (text[pos] == ',') { ++pos; continue; }
                return false;
            }
        }

        bool parseObject(Http::JsonNode& out) {
            skipWhitespace();
            if (pos >= text.size() || text[pos] != '{') { return false; }
            ++pos;

            out.type = Http::JsonNode::Type::Object;
            skipWhitespace();
            if (pos < text.size() && text[pos] == '}') { ++pos; return true; }

            while (true) {
                skipWhitespace();
                Http::JsonNode keyNode;
                if (!parseString(keyNode)) { return false; }

                skipWhitespace();
                if (pos >= text.size() || text[pos] != ':') { return false; }
                ++pos;

                Http::JsonNode value;
                if (!parseValue(value)) { return false; }
                out.objectValue.emplace_back(keyNode.stringValue, value);

                skipWhitespace();
                if (pos >= text.size()) { return false; }
                if (text[pos] == '}') { ++pos; return true; }
                if (text[pos] == ',') { ++pos; continue; }
                return false;
            }
        }
    };
}

namespace {

    std::wstring buildHeaders(const wchar_t* contentType, const std::string& accessToken) {
        std::wstring headers;
        if (accessToken.empty() && !contentType) { return headers; }

        if (!accessToken.empty()) {
            headers += L"Authorization: Bearer ";
            headers.append(accessToken.begin(), accessToken.end());
            headers += L"\r\n";
        }
        if (contentType) {
            headers += contentType;
            headers += L"\r\n";
        }
        return headers;
    }

    std::string request(const std::wstring& host, const std::wstring& path, const wchar_t* method,
                        const std::string& body, const wchar_t* contentType, const std::string& accessToken) {
        HINTERNET session = WinHttpOpen(L"SpotifyQuickWheel/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) { return {}; }

        HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connect) {
            WinHttpCloseHandle(session);
            return {};
        }

        HINTERNET hRequest = WinHttpOpenRequest(connect, method, path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return {};
        }

        std::wstring headerBuffer = buildHeaders(contentType, accessToken);
        const wchar_t* headers = headerBuffer.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerBuffer.c_str();
        DWORD bodyLength = static_cast<DWORD>(body.size());

        bool sent = WinHttpSendRequest(hRequest, headers, (DWORD)-1,
                                       bodyLength > 0 ? const_cast<char*>(body.data()) : WINHTTP_NO_REQUEST_DATA,
                                       bodyLength, bodyLength, 0);
        bool received = sent && WinHttpReceiveResponse(hRequest, nullptr);

        std::string response;
        if (received) {
            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

            if (status == 200) {
                DWORD available = 0;
                do {
                    if (!WinHttpQueryDataAvailable(hRequest, &available)) { break; }

                    std::vector<char> buffer(available ? available : 1);
                    DWORD read = 0;

                    if (!WinHttpReadData(hRequest, buffer.data(), (DWORD)buffer.size(), &read)) { break; }

                    response.append(buffer.data(), read);
                } while (available > 0);
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);

        return response;
    }
}

namespace Http {
    std::string get(const std::wstring& host, const std::wstring& path, const std::string& accessToken) {
        return request(host, path, L"GET", {}, nullptr, accessToken);
    }

    std::string post(const std::wstring& host, const std::wstring& path, const std::string& accessToken, const std::string& body) {
        return request(host, path, L"POST", body, nullptr, accessToken);
    }

    std::string put(const std::wstring& host, const std::wstring& path, const std::string& accessToken, const std::string& body) {
        return request(host, path, L"PUT", body, nullptr, accessToken);
    }

    std::string postRequest(const std::wstring& host, const std::wstring& path, const std::string& body) {
        return request(host, path, L"POST", body, L"Content-Type: application/x-www-form-urlencoded", {});
    }

    const Http::JsonNode* Http::JsonNode::get(const std::string& key) const {
        for (const auto& member : objectValue) {
            if (member.first == key) { return &member.second; }
        }
        return nullptr;
    }

    const Http::JsonNode* Http::JsonNode::get(size_t index) const {
        return index < arrayValue.size() ? &arrayValue[index] : nullptr;
    }

    Http::JsonNode Http::parseJson(const std::string& json) {
        Http::JsonNode root;
        if (JsonParser(json).parse(root)) { return root; }
        return {};
    }
}
