#include "command_parser.h"

#include <cwctype>
#include <sstream>

namespace {

bool IsSeparator(wchar_t ch) {
    return iswspace(ch) || ch == L',' || ch == L';';
}

bool IsHexDigit(wchar_t ch) {
    return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F');
}

int HexValue(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') {
        return ch - L'0';
    }
    if (ch >= L'a' && ch <= L'f') {
        return 10 + (ch - L'a');
    }
    if (ch >= L'A' && ch <= L'F') {
        return 10 + (ch - L'A');
    }
    return -1;
}

bool ParseToken(const std::wstring& token, unsigned char* out_value, std::wstring* error) {
    if (!out_value) {
        return false;
    }
    if (token.empty()) {
        if (error) {
            *error = L"Empty hex token.";
        }
        return false;
    }

    std::wstring trimmed = token;
    if (trimmed.rfind(L"0x", 0) == 0 || trimmed.rfind(L"0X", 0) == 0) {
        trimmed = trimmed.substr(2);
    }

    if (trimmed.empty()) {
        if (error) {
            *error = L"Hex token missing digits.";
        }
        return false;
    }
    if (trimmed.size() > 2) {
        if (error) {
            *error = L"Hex token too long.";
        }
        return false;
    }

    int value = 0;
    for (wchar_t ch : trimmed) {
        if (!IsHexDigit(ch)) {
            if (error) {
                *error = L"Invalid hex digit.";
            }
            return false;
        }
        value = (value << 4) | HexValue(ch);
    }

    if (value < 0 || value > 0xFF) {
        if (error) {
            *error = L"Hex value out of range.";
        }
        return false;
    }

    *out_value = static_cast<unsigned char>(value);
    return true;
}

}

bool ParseCommandBytes(const std::wstring& text, std::vector<unsigned char>* out_bytes, std::wstring* error) {
    if (!out_bytes) {
        if (error) {
            *error = L"Output buffer missing.";
        }
        return false;
    }
    out_bytes->clear();

    std::wstring token;
    auto flush_token = [&](void) -> bool {
        if (token.empty()) {
            return true;
        }
        unsigned char value = 0;
        std::wstring local_error;
        if (!ParseToken(token, &value, &local_error)) {
            if (error) {
                *error = local_error;
            }
            return false;
        }
        out_bytes->push_back(value);
        token.clear();
        return true;
    };

    for (wchar_t ch : text) {
        if (IsSeparator(ch)) {
            if (!flush_token()) {
                return false;
            }
        } else {
            token.push_back(ch);
        }
    }

    if (!flush_token()) {
        return false;
    }

    if (out_bytes->empty()) {
        if (error) {
            *error = L"Please enter at least 1 byte.";
        }
        return false;
    }

    if (out_bytes->size() > 64) {
        if (error) {
            *error = L"Too many bytes (max 64).";
        }
        return false;
    }

    while (out_bytes->size() < 64) {
        out_bytes->push_back(0xFF);
    }

    return true;
}

bool ParseSingleByte(const std::wstring& text, unsigned char* out_value, std::wstring* error) {
    if (!out_value) {
        if (error) {
            *error = L"Output buffer missing.";
        }
        return false;
    }

    std::wstring trimmed;
    for (wchar_t ch : text) {
        if (!iswspace(ch)) {
            trimmed.push_back(ch);
        }
    }

    if (trimmed.empty()) {
        if (error) {
            *error = L"Value is empty.";
        }
        return false;
    }

    unsigned char value = 0;
    if (!ParseToken(trimmed, &value, error)) {
        return false;
    }
    *out_value = value;
    return true;
}

std::wstring FormatCommandBytes(const std::vector<unsigned char>& bytes, bool with_prefix) {
    std::wstringstream ss;
    ss << std::uppercase << std::hex;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i > 0) {
            ss << L' ';
        }
        if (with_prefix) {
            ss << L"0x";
        }
        ss.width(2);
        ss.fill(L'0');
        ss << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

std::wstring BuildDefaultPayloadText(bool with_prefix) {
    std::vector<unsigned char> bytes(64, 0xFF);
    return FormatCommandBytes(bytes, with_prefix);
}
