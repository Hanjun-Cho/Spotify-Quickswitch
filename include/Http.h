#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Http {
    std::string get(const std::wstring& host, const std::wstring& path, const std::string& accessToken = {});
    std::string post(const std::wstring& host, const std::wstring& path, const std::string& accessToken = {}, const std::string& body = {});
    std::string put(const std::wstring& host, const std::wstring& path, const std::string& accessToken = {}, const std::string& body = {});
    std::string postRequest(const std::wstring& host, const std::wstring& path, const std::string& body);

    struct JsonNode {
        enum class Type { Null, Bool, Number, String, Array, Object };

        Type type = Type::Null;
        bool boolValue = false;
        long longValue = 0;
        std::string stringValue;
        std::vector<JsonNode> arrayValue;
        std::vector<std::pair<std::string, JsonNode>> objectValue;

        const JsonNode* get(const std::string& key) const;
        const JsonNode* get(size_t index) const;
    };

    JsonNode parseJson(const std::string& json);
}
