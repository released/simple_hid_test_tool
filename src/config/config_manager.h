#pragma once

#include <array>
#include <string>

class ConfigManager {
public:
    ConfigManager();

    bool Load();
    bool Save() const;

    const std::wstring& path() const { return ini_path_; }

    std::wstring host = L"192.168.0.2";
    unsigned short port = 5000;
    bool auto_reconnect = false;

    bool interval_enabled = false;
    unsigned int interval_ms = 100;

    bool limit_enabled = false;
    int limit_count = 10;

    bool log_enabled = false;
    bool timestamp_enabled = false;
    std::wstring firmware_path;
    bool function_poll_adc = true;
    bool function_poll_gpio = true;
    bool function_poll_pwm = true;

private:
    std::wstring ini_path_;

    std::wstring GetIniPath() const;
    static std::wstring ReadString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& def_value);
    static void WriteString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& value);
    static void DeleteKey(const std::wstring& path, const std::wstring& section, const std::wstring& key);
    static unsigned int ReadUInt(const std::wstring& path, const std::wstring& section, const std::wstring& key, unsigned int def_value);
};

