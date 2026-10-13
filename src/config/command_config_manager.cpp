#include "command_config_manager.h"

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

const std::array<CommandRow, 6>& GetDefaultTemplateRows() {
    static const std::array<CommandRow, 6> kRows = {{
        {true, L"5A 5A 11 22 33 44 55 66 77 88 99 00 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF"},
        {false, L"5A 5A 1A FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF"},
        {false, L"5A 5A 1B FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF"},
        {false, L"5A 5A 1C FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF"},
        {false, L"5A 5A 1D FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF"},
        {false, L"5A 5A 1E FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF"},
    }};
    return kRows;
}

}

CommandConfigManager::CommandConfigManager() : ini_path_(GetIniPath()) {}

std::wstring CommandConfigManager::GetIniPath() const {
    return GetModuleDir() + L"TCPTestTool_commands.ini";
}

std::wstring CommandConfigManager::GetDefaultIniPath() const {
    return GetModuleDir() + L"TCPTestTool_commands_default.ini";
}

bool CommandConfigManager::FileExists(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void CommandConfigManager::SaveRowsToPath(const std::wstring& path, const std::array<CommandRow, 6>& rows) {
    for (int i = 0; i < 6; ++i) {
        wchar_t key[32] = {};
        swprintf_s(key, L"Row%dEnabled", i + 1);
        WriteString(path, L"TX", key, rows[i].enabled ? L"1" : L"0");
        swprintf_s(key, L"Row%dText", i + 1);
        WriteString(path, L"TX", key, rows[i].text);
    }
}

std::wstring CommandConfigManager::default_path() const {
    return GetDefaultIniPath();
}

std::wstring CommandConfigManager::ReadString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& def_value) {
    wchar_t buffer[2048] = {};
    GetPrivateProfileStringW(section.c_str(), key.c_str(), def_value.c_str(), buffer,
                             static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])), path.c_str());
    return buffer;
}

void CommandConfigManager::WriteString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& value) {
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), path.c_str());
}

unsigned int CommandConfigManager::ReadUInt(const std::wstring& path, const std::wstring& section, const std::wstring& key, unsigned int def_value) {
    return GetPrivateProfileIntW(section.c_str(), key.c_str(), def_value, path.c_str());
}

bool CommandConfigManager::Load() {
    ini_path_ = GetIniPath();
    LoadFromPath(ini_path_);
    return true;
}

bool CommandConfigManager::Save() const {
    std::wstring path = ini_path_.empty() ? GetIniPath() : ini_path_;
    SaveToPath(path);
    return true;
}

bool CommandConfigManager::LoadFrom(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    LoadFromPath(path);
    return true;
}

bool CommandConfigManager::SaveTo(const std::wstring& path) const {
    if (path.empty()) {
        return false;
    }
    SaveToPath(path);
    return true;
}

bool CommandConfigManager::SaveDefaultIfMissing() const {
    std::wstring path = GetDefaultIniPath();
    if (FileExists(path)) {
        return true;
    }
    SaveRowsToPath(path, GetDefaultTemplateRows());
    return true;
}

void CommandConfigManager::LoadFromPath(const std::wstring& path) {
    for (int i = 0; i < 6; ++i) {
        wchar_t key[32] = {};
        swprintf_s(key, L"Row%dEnabled", i + 1);
        rows[i].enabled = ReadUInt(path, L"TX", key, 0) != 0;
        swprintf_s(key, L"Row%dText", i + 1);
        rows[i].text = ReadString(path, L"TX", key, L"");
    }
}

void CommandConfigManager::SaveToPath(const std::wstring& path) const {
    SaveRowsToPath(path, rows);
}
