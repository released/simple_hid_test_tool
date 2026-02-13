#pragma once

#include <array>
#include <string>

struct CommandRow {
    bool enabled = false;
    std::wstring text;
};

class CommandConfigManager {
public:
    CommandConfigManager();

    bool Load();
    bool Save() const;
    bool LoadFrom(const std::wstring& path);
    bool SaveTo(const std::wstring& path) const;
    bool SaveDefaultIfMissing() const;

    const std::wstring& path() const { return ini_path_; }
    std::wstring default_path() const;

    std::array<CommandRow, 6> rows;

private:
    std::wstring ini_path_;

    std::wstring GetIniPath() const;
    std::wstring GetDefaultIniPath() const;
    static bool FileExists(const std::wstring& path);
    static void SaveRowsToPath(const std::wstring& path, const std::array<CommandRow, 6>& rows);
    static std::wstring ReadString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& def_value);
    static void WriteString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& value);
    static unsigned int ReadUInt(const std::wstring& path, const std::wstring& section, const std::wstring& key, unsigned int def_value);

    void LoadFromPath(const std::wstring& path);
    void SaveToPath(const std::wstring& path) const;
};
