#pragma once

#include <windows.h>
#include <winsock2.h>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "hid_scan.h"
#include "../tcp/tcp_protocol.h"

class HidDevice {
public:
    HidDevice();
    ~HidDevice();

    bool Open(const HidScanInfo& info, std::wstring* error);
    void Close();
    bool IsOpen() const;

    bool ReadInputReport(std::vector<unsigned char>* out_report, DWORD timeout_ms, std::wstring* error);
    bool WriteReport(const std::vector<unsigned char>& report, std::wstring* error);
    bool ExecuteCommand(unsigned char command,
                        const std::vector<unsigned char>& payload,
                        unsigned char* out_status,
                        std::vector<unsigned char>* out_payload,
                        DWORD timeout_ms,
                        std::wstring* error);
    void GetLastWriteInfo(size_t* out_length, size_t* out_actual) const;
    void CancelPendingIo();

    const HidScanInfo& info() const { return info_; }

private:
    std::vector<unsigned char> ExtractRawPayload(const std::vector<unsigned char>& report) const;
    bool ReceiveFrame(tcp_tool::Frame* out_frame, DWORD timeout_ms, std::wstring* error);
    bool ReceiveFrameLocked(tcp_tool::Frame* out_frame, DWORD timeout_ms, std::wstring* error);
    bool PopPendingFrameLocked(unsigned char command,
                               std::uint32_t sequence,
                               bool require_sequence,
                               tcp_tool::Frame* out_frame);
    void CloseSocketOnly();
    std::uint32_t NextSequence();
    static std::string WideToUtf8(const std::wstring& text);

    SOCKET socket_;
    size_t last_write_length_ = 0;
    size_t last_write_actual_ = 0;
    HidScanInfo info_;
    std::vector<unsigned char> recv_buffer_;
    std::deque<tcp_tool::Frame> pending_frames_;
    mutable CRITICAL_SECTION socket_lock_;
    CRITICAL_SECTION recv_lock_;
    std::uint32_t next_sequence_;
};

