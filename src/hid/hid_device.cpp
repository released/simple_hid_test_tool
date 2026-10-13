#include "hid_device.h"

#include <ws2tcpip.h>

#include <algorithm>
#include <vector>

#include "../tcp/tcp_protocol.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

bool EnsureWinsock(std::wstring* error) {
    static bool initialized = false;
    static bool success = false;
    WSADATA wsa_data = {};

    if (initialized) {
        if (!success && error) {
            *error = L"WSAStartup failed.";
        }
        return success;
    }

    initialized = true;
    success = (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0);
    if (!success && error) {
        *error = L"WSAStartup failed.";
    }
    return success;
}

bool SendAll(SOCKET socket_handle, const unsigned char* data, std::size_t size) {
    std::size_t sent = 0;
    int result = 0;

    while (sent < size) {
        result = send(socket_handle,
                      reinterpret_cast<const char*>(data + sent),
                      static_cast<int>(size - sent),
                      0);
        if (result <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }

    return true;
}

bool WaitForSocketReadable(SOCKET socket_handle, DWORD timeout_ms) {
    fd_set read_set;
    TIMEVAL timeout = {};

    FD_ZERO(&read_set);
    FD_SET(socket_handle, &read_set);
    timeout.tv_sec = static_cast<long>(timeout_ms / 1000u);
    timeout.tv_usec = static_cast<long>((timeout_ms % 1000u) * 1000u);
    return select(0, &read_set, nullptr, nullptr, &timeout) > 0;
}

}

HidDevice::HidDevice()
    : socket_(INVALID_SOCKET),
      last_write_length_(0),
      last_write_actual_(0),
      next_sequence_(1) {
    InitializeCriticalSection(&socket_lock_);
    InitializeCriticalSection(&recv_lock_);
}

HidDevice::~HidDevice() {
    Close();
    DeleteCriticalSection(&recv_lock_);
    DeleteCriticalSection(&socket_lock_);
}

bool HidDevice::Open(const HidScanInfo& info, std::wstring* error) {
    addrinfo hints = {};
    addrinfo* result = nullptr;
    addrinfo* iter = nullptr;
    std::string host;
    std::string port_text;
    SOCKET socket_handle = INVALID_SOCKET;

    Close();

    if (!EnsureWinsock(error)) {
        return false;
    }

    if (info.host.empty()) {
        if (error) {
            *error = L"TCP host is empty.";
        }
        return false;
    }

    host = WideToUtf8(info.host);
    port_text = std::to_string(info.port);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0) {
        if (error) {
            *error = L"getaddrinfo failed.";
        }
        return false;
    }

    for (iter = result; iter != nullptr; iter = iter->ai_next) {
        socket_handle = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
        if (socket_handle == INVALID_SOCKET) {
            continue;
        }
        if (connect(socket_handle, iter->ai_addr, static_cast<int>(iter->ai_addrlen)) == 0) {
            break;
        }
        closesocket(socket_handle);
        socket_handle = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (socket_handle == INVALID_SOCKET) {
        if (error) {
            *error = L"TCP connect failed.";
        }
        return false;
    }

    {
        BOOL no_delay = TRUE;
        setsockopt(socket_handle, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&no_delay), sizeof(no_delay));
    }

    EnterCriticalSection(&recv_lock_);
    EnterCriticalSection(&socket_lock_);
    socket_ = socket_handle;
    info_ = info;
    recv_buffer_.clear();
    pending_frames_.clear();
    next_sequence_ = 1;
    LeaveCriticalSection(&socket_lock_);
    LeaveCriticalSection(&recv_lock_);

    return true;
}

void HidDevice::CloseSocketOnly() {
    SOCKET socket_handle;

    EnterCriticalSection(&socket_lock_);
    socket_handle = socket_;
    socket_ = INVALID_SOCKET;
    last_write_length_ = 0;
    last_write_actual_ = 0;
    info_ = HidScanInfo();
    LeaveCriticalSection(&socket_lock_);

    if (socket_handle != INVALID_SOCKET) {
        shutdown(socket_handle, SD_BOTH);
        closesocket(socket_handle);
    }
}

void HidDevice::Close() {
    EnterCriticalSection(&recv_lock_);
    CloseSocketOnly();
    recv_buffer_.clear();
    pending_frames_.clear();
    LeaveCriticalSection(&recv_lock_);
}

bool HidDevice::IsOpen() const {
    bool open = false;

    EnterCriticalSection(&socket_lock_);
    open = socket_ != INVALID_SOCKET;
    LeaveCriticalSection(&socket_lock_);
    return open;
}

bool HidDevice::ReadInputReport(std::vector<unsigned char>* out_report, DWORD timeout_ms, std::wstring* error) {
    tcp_tool::Frame frame;
    if (!out_report) {
        if (error) {
            *error = L"Output buffer missing.";
        }
        return false;
    }

    EnterCriticalSection(&recv_lock_);
    if (PopPendingFrameLocked(tcp_tool::kCmdRaw64, 0, false, &frame)) {
        LeaveCriticalSection(&recv_lock_);
    } else {
        for (;;) {
            if (!ReceiveFrameLocked(&frame, timeout_ms, error)) {
                LeaveCriticalSection(&recv_lock_);
                return false;
            }
            if (frame.command == tcp_tool::kCmdRaw64) {
                LeaveCriticalSection(&recv_lock_);
                break;
            }
            pending_frames_.push_back(frame);
        }
    }

    out_report->assign(frame.payload.begin(), frame.payload.end());
    if (out_report->size() < tcp_tool::kRawPayloadSize) {
        out_report->resize(tcp_tool::kRawPayloadSize, 0x00);
    }
    return true;
}

bool HidDevice::WriteReport(const std::vector<unsigned char>& report, std::wstring* error) {
    std::vector<unsigned char> payload;
    tcp_tool::Frame frame;
    std::vector<unsigned char> packet;

    if (!IsOpen()) {
        if (error) {
            *error = L"TCP socket not open.";
        }
        return false;
    }

    payload = ExtractRawPayload(report);
    if (payload.size() != tcp_tool::kRawPayloadSize) {
        if (error) {
            *error = L"Payload length must be 64 bytes.";
        }
        return false;
    }

    frame.command = tcp_tool::kCmdRaw64;
    frame.status = 0;
    frame.sequence = NextSequence();
    frame.payload = payload;
    packet = tcp_tool::BuildFrame(frame);

    EnterCriticalSection(&socket_lock_);
    last_write_length_ = packet.size();
    last_write_actual_ = 0;
    if (!SendAll(socket_, packet.data(), packet.size())) {
        LeaveCriticalSection(&socket_lock_);
        if (error) {
            *error = L"TCP send failed.";
        }
        CloseSocketOnly();
        return false;
    }
    last_write_actual_ = packet.size();
    LeaveCriticalSection(&socket_lock_);
    return true;
}

bool HidDevice::ExecuteCommand(unsigned char command,
                               const std::vector<unsigned char>& payload,
                               unsigned char* out_status,
                               std::vector<unsigned char>* out_payload,
                               DWORD timeout_ms,
                               std::wstring* error) {
    tcp_tool::Frame request;
    tcp_tool::Frame response;
    std::vector<unsigned char> packet;

    if (!IsOpen()) {
        if (error) {
            *error = L"TCP socket not open.";
        }
        return false;
    }

    request.command = command;
    request.status = 0;
    request.sequence = NextSequence();
    request.payload = payload;
    packet = tcp_tool::BuildFrame(request);

    EnterCriticalSection(&recv_lock_);
    EnterCriticalSection(&socket_lock_);
    last_write_length_ = packet.size();
    last_write_actual_ = 0;
    if (!SendAll(socket_, packet.data(), packet.size())) {
        LeaveCriticalSection(&socket_lock_);
        LeaveCriticalSection(&recv_lock_);
        if (error) {
            *error = L"TCP send failed.";
        }
        Close();
        return false;
    }
    last_write_actual_ = packet.size();
    LeaveCriticalSection(&socket_lock_);

    if (PopPendingFrameLocked(command, request.sequence, true, &response)) {
        LeaveCriticalSection(&recv_lock_);
        if (out_status) {
            *out_status = response.status;
        }
        if (out_payload) {
            *out_payload = response.payload;
        }
        return true;
    }

    for (;;) {
        if (!ReceiveFrameLocked(&response, timeout_ms, error)) {
            LeaveCriticalSection(&recv_lock_);
            return false;
        }
        if (response.command == command && response.sequence == request.sequence) {
            break;
        }
        pending_frames_.push_back(response);
    }
    LeaveCriticalSection(&recv_lock_);

    if (out_status) {
        *out_status = response.status;
    }
    if (out_payload) {
        *out_payload = response.payload;
    }
    return true;
}

void HidDevice::GetLastWriteInfo(size_t* out_length, size_t* out_actual) const {
    EnterCriticalSection(const_cast<LPCRITICAL_SECTION>(&socket_lock_));
    if (out_length) {
        *out_length = last_write_length_;
    }
    if (out_actual) {
        *out_actual = last_write_actual_;
    }
    LeaveCriticalSection(const_cast<LPCRITICAL_SECTION>(&socket_lock_));
}

void HidDevice::CancelPendingIo() {
    CloseSocketOnly();
}

std::vector<unsigned char> HidDevice::ExtractRawPayload(const std::vector<unsigned char>& report) const {
    std::vector<unsigned char> payload;

    if (report.size() == tcp_tool::kRawPayloadSize + 1) {
        payload.assign(report.begin() + 1, report.begin() + 1 + static_cast<std::ptrdiff_t>(tcp_tool::kRawPayloadSize));
    } else if (report.size() >= tcp_tool::kRawPayloadSize) {
        payload.assign(report.begin(), report.begin() + static_cast<std::ptrdiff_t>(tcp_tool::kRawPayloadSize));
    }
    return payload;
}

bool HidDevice::ReceiveFrame(tcp_tool::Frame* out_frame, DWORD timeout_ms, std::wstring* error) {
    bool result = false;
    EnterCriticalSection(&recv_lock_);
    result = ReceiveFrameLocked(out_frame, timeout_ms, error);
    LeaveCriticalSection(&recv_lock_);
    return result;
}

bool HidDevice::ReceiveFrameLocked(tcp_tool::Frame* out_frame, DWORD timeout_ms, std::wstring* error) {
    std::vector<unsigned char> chunk;
    tcp_tool::Frame parsed;
    SOCKET socket_handle;
    int recv_result = 0;
    std::wstring parse_error;

    if (!out_frame) {
        return false;
    }

    for (;;) {
        parse_error.clear();
        if (tcp_tool::TryParseFrame(&recv_buffer_, &parsed, &parse_error)) {
            *out_frame = parsed;
            return true;
        }
        if (!parse_error.empty()) {
            if (error) {
                *error = parse_error;
            }
            return false;
        }

        EnterCriticalSection(&socket_lock_);
        socket_handle = socket_;
        LeaveCriticalSection(&socket_lock_);
        if (socket_handle == INVALID_SOCKET) {
            if (error) {
                *error = L"TCP socket closed.";
            }
            return false;
        }

        if (!WaitForSocketReadable(socket_handle, timeout_ms)) {
            if (error) {
                *error = L"TCP receive timeout.";
            }
            return false;
        }

        chunk.assign(512, 0);
        recv_result = recv(socket_handle, reinterpret_cast<char*>(chunk.data()), static_cast<int>(chunk.size()), 0);
        if (recv_result <= 0) {
            if (error) {
                *error = L"TCP receive failed.";
            }
            CloseSocketOnly();
            return false;
        }
        chunk.resize(static_cast<std::size_t>(recv_result));
        recv_buffer_.insert(recv_buffer_.end(), chunk.begin(), chunk.end());
    }
}

bool HidDevice::PopPendingFrameLocked(unsigned char command,
                                      std::uint32_t sequence,
                                      bool require_sequence,
                                      tcp_tool::Frame* out_frame) {
    std::deque<tcp_tool::Frame>::iterator it;

    if (!out_frame) {
        return false;
    }

    for (it = pending_frames_.begin(); it != pending_frames_.end(); ++it) {
        if (it->command != command) {
            continue;
        }
        if (require_sequence && it->sequence != sequence) {
            continue;
        }
        *out_frame = *it;
        pending_frames_.erase(it);
        return true;
    }
    return false;
}

std::uint32_t HidDevice::NextSequence() {
    std::uint32_t sequence = 0;
    EnterCriticalSection(&socket_lock_);
    sequence = next_sequence_++;
    LeaveCriticalSection(&socket_lock_);
    return sequence;
}

std::string HidDevice::WideToUtf8(const std::wstring& text) {
    int size = 0;
    std::string utf8;

    if (text.empty()) {
        return std::string();
    }

    size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                               nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return std::string();
    }

    utf8.resize(static_cast<std::size_t>(size));
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        &utf8[0], size, nullptr, nullptr);
    return utf8;
}
