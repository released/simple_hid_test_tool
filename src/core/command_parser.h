#pragma once

#include <string>
#include <vector>

bool ParseCommandBytes(const std::wstring& text, std::vector<unsigned char>* out_bytes, std::wstring* error);
bool ParseSingleByte(const std::wstring& text, unsigned char* out_value, std::wstring* error);
std::wstring FormatCommandBytes(const std::vector<unsigned char>& bytes, bool with_prefix);
std::wstring BuildDefaultPayloadText(bool with_prefix);

