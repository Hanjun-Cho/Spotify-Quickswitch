#pragma once

#include <string>

class SpotifyAuth {
    public:
        SpotifyAuth();

        bool login();
        bool ensureAuthenticated();
        bool isAuthenticated() const;

        std::string getAccessToken();

    private:
        std::string clientId = "8706797d0a57415fb518a714241a7dab";

        static constexpr const char* SCOPE =
            "user-modify-playback-state "
            "user-read-playback-state "
            "user-read-currently-playing";
        static constexpr const char* REDIRECT_URI = "http://127.0.0.1:8888/callback";

        std::string accessToken;
        std::string refreshToken;

        std::string codeVerifier;
        std::string state;

        bool authenticated = false;

        std::string generateRandomString(size_t length);
        std::string sha256Url(const std::string& input);
        std::string Base64Encode(const std::string& raw);
        std::string urlEncode(const std::string& value);
        std::string buildAuthorizationUrl();

        bool waitForCallback(std::string& code);
        bool exchangeCode(const std::string& code);
        bool refreshAccessToken();

        void openBrowser(const std::string& url);

        std::string tokenFilePath() const;
        void saveRefreshToken(const std::string& token);
        std::string loadRefreshToken();
};
