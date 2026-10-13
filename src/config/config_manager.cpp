#include "config_manager.h"

#include <windows.h>

namespace {

std::wstring GetModuleDir() {
    wchar_t path[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return L"";
    }
    std::wstring full(path, len);
    size_t pos = full.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L"";
    }
    return full.substr(0, pos + 1);
}

}

ConfigManager::ConfigManager() : ini_path_(GetIniPath()) {}

std::wstring ConfigManager::GetIniPath() const {
    return GetModuleDir() + L"TCPTestTool.ini";
}

std::wstring ConfigManager::ReadString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& def_value) {
    wchar_t buffer[2048] = {};
    GetPrivateProfileStringW(section.c_str(), key.c_str(), def_value.c_str(), buffer,
                             static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])), path.c_str());
    return buffer;
}

void ConfigManager::WriteString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& value) {
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), path.c_str());
}

void ConfigManager::DeleteKey(const std::wstring& path, const std::wstring& section, const std::wstring& key) {
    WritePrivateProfileStringW(section.c_str(), key.c_str(), nullptr, path.c_str());
}

unsigned int ConfigManager::ReadUInt(const std::wstring& path, const std::wstring& section, const std::wstring& key, unsigned int def_value) {
    return GetPrivateProfileIntW(section.c_str(), key.c_str(), def_value, path.c_str());
}

bool ConfigManager::Load() {
    ini_path_ = GetIniPath();

    host = ReadString(ini_path_, L"Connection", L"Host", L"192.168.0.2");
    port = static_cast<unsigned short>(ReadUInt(ini_path_, L"Connection", L"Port", 5000));
    auto_reconnect = ReadUInt(ini_path_, L"Connection", L"AutoReconnect", 0) != 0;

    interval_enabled = ReadUInt(ini_path_, L"Scheduler", L"IntervalEnabled", 0) != 0;
    interval_ms = ReadUInt(ini_path_, L"Scheduler", L"IntervalMs", 100);

    limit_enabled = ReadUInt(ini_path_, L"TX", L"LimitEnabled", 0) != 0;
    limit_count = static_cast<int>(ReadUInt(ini_path_, L"TX", L"LimitCount", 10));

    log_enabled = ReadUInt(ini_path_, L"RX", L"LogEnabled", 0) != 0;
    timestamp_enabled = ReadUInt(ini_path_, L"RX", L"TimestampEnabled", 0) != 0;
    firmware_path = ReadString(ini_path_, L"Firmware", L"LastPath", L"");
    function_poll_adc = ReadUInt(ini_path_, L"Function", L"PollAdc", 1) != 0;
    function_poll_gpio = ReadUInt(ini_path_, L"Function", L"PollGpio", 1) != 0;
    function_poll_pwm = ReadUInt(ini_path_, L"Function", L"PollPwm", 1) != 0;

    return true;
}

bool ConfigManager::Save() const {
    std::wstring path = ini_path_.empty() ? GetIniPath() : ini_path_;

    WriteString(path, L"Connection", L"Host", host);
    WriteString(path, L"Connection", L"Port", std::to_wstring(port));
    WriteString(path, L"Connection", L"AutoReconnect", auto_reconnect ? L"1" : L"0");

    WriteString(path, L"Scheduler", L"IntervalEnabled", interval_enabled ? L"1" : L"0");
    WriteString(path, L"Scheduler", L"IntervalMs", std::to_wstring(interval_ms));

    WriteString(path, L"TX", L"LimitEnabled", limit_enabled ? L"1" : L"0");
    WriteString(path, L"TX", L"LimitCount", std::to_wstring(limit_count));

    WriteString(path, L"RX", L"LogEnabled", log_enabled ? L"1" : L"0");
    WriteString(path, L"RX", L"TimestampEnabled", timestamp_enabled ? L"1" : L"0");
    WriteString(path, L"Firmware", L"LastPath", firmware_path);
    WriteString(path, L"Function", L"PollAdc", function_poll_adc ? L"1" : L"0");
    WriteString(path, L"Function", L"PollGpio", function_poll_gpio ? L"1" : L"0");
    WriteString(path, L"Function", L"PollPwm", function_poll_pwm ? L"1" : L"0");

    DeleteKey(path, L"Report", L"ReportID");
    DeleteKey(path, L"TX", L"HandshakeEnabled");
    DeleteKey(path, L"TX", L"HandshakeTimeout");
    DeleteKey(path, L"TX", L"HandshakeRetryCount");
    DeleteKey(path, L"TX", L"EchoModeEnabled");
    DeleteKey(path, L"TX", L"HandshakeModeEnabled");

    return true;
}

