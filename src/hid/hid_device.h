#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "hid_scan.h"

class HidDevice {
public:
    HidDevice();
    ~HidDevice();

    bool Open(const HidScanInfo& info, std::wstring* error);
    void Close();
    bool IsOpen() const;

    bool ReadInputReport(std::vector<unsigned char>* out_report, DWORD timeout_ms, std::wstring* error);
    bool WriteReport(const std::vector<unsigned char>& report, std::wstring* error);
    void GetLastWriteInfo(size_t* out_length, size_t* out_actual) const;
    void CancelPendingIo();

    const HidScanInfo& info() const { return info_; }

private:
    HANDLE read_handle_;
    HANDLE write_handle_;
    size_t last_write_length_ = 0;
    size_t last_write_actual_ = 0;
    HidScanInfo info_;
};

