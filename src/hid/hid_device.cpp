#include "hid_device.h"

#include <algorithm>

namespace {

HANDLE OpenReadHandle(const std::wstring& path) {
    return CreateFileW(path.c_str(), GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
}

HANDLE OpenWriteHandle(const std::wstring& path) {
    return CreateFileW(path.c_str(), GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
}

bool IsDeviceRemovedError(DWORD error) {
    return error == ERROR_DEVICE_NOT_CONNECTED || error == ERROR_INVALID_HANDLE || error == ERROR_GEN_FAILURE || error == ERROR_ACCESS_DENIED;
}

}

HidDevice::HidDevice()
    : read_handle_(INVALID_HANDLE_VALUE),
      write_handle_(INVALID_HANDLE_VALUE),
      last_write_length_(0),
      last_write_actual_(0) {}

HidDevice::~HidDevice() {
    Close();
}

bool HidDevice::Open(const HidScanInfo& info, std::wstring* error) {
    Close();

    HANDLE read_handle = OpenReadHandle(info.device_path);
    if (read_handle == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = L"Failed to open device (read).";
        }
        return false;
    }
    HANDLE write_handle = OpenWriteHandle(info.device_path);
    if (write_handle == INVALID_HANDLE_VALUE) {
        CloseHandle(read_handle);
        if (error) {
            *error = L"Failed to open device (write).";
        }
        return false;
    }

    read_handle_ = read_handle;
    write_handle_ = write_handle;
    info_ = info;
    return true;
}

void HidDevice::Close() {
    if (read_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(read_handle_);
        read_handle_ = INVALID_HANDLE_VALUE;
    }
    if (write_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(write_handle_);
        write_handle_ = INVALID_HANDLE_VALUE;
    }
    last_write_length_ = 0;
    last_write_actual_ = 0;
}

bool HidDevice::IsOpen() const {
    return read_handle_ != INVALID_HANDLE_VALUE && write_handle_ != INVALID_HANDLE_VALUE;
}

bool HidDevice::ReadInputReport(std::vector<unsigned char>* out_report, DWORD timeout_ms, std::wstring* error) {
    if (!IsOpen()) {
        if (error) {
            *error = L"Device not open.";
        }
        return false;
    }
    if (!out_report) {
        if (error) {
            *error = L"Output buffer missing.";
        }
        return false;
    }
    if (info_.input_report_length == 0) {
        if (error) {
            *error = L"Device has no input reports.";
        }
        return false;
    }

    out_report->assign(info_.input_report_length, 0);

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        if (error) {
            *error = L"Failed to create read event.";
        }
        return false;
    }

    DWORD bytes_read = 0;
    BOOL ok = ReadFile(read_handle_, out_report->data(), static_cast<DWORD>(out_report->size()), &bytes_read, &overlapped);
    if (ok) {
        CloseHandle(overlapped.hEvent);
        if (bytes_read < out_report->size()) {
            out_report->resize(bytes_read);
        }
        return true;
    }

    if (GetLastError() != ERROR_IO_PENDING) {
        DWORD err = GetLastError();
        CloseHandle(overlapped.hEvent);
        if (error) {
            if (IsDeviceRemovedError(err)) {
                *error = L"Device removed.";
            } else {
                *error = L"ReadFile failed.";
            }
        }
        return false;
    }

    DWORD wait_result = WaitForSingleObject(overlapped.hEvent, timeout_ms);
    if (wait_result == WAIT_TIMEOUT) {
        CancelIo(read_handle_);
        CloseHandle(overlapped.hEvent);
        if (error) {
            *error = L"Read timed out.";
        }
        return false;
    }

    if (!GetOverlappedResult(read_handle_, &overlapped, &bytes_read, FALSE)) {
        DWORD err = GetLastError();
        CloseHandle(overlapped.hEvent);
        if (error) {
            if (IsDeviceRemovedError(err)) {
                *error = L"Device removed.";
            } else {
                *error = L"Failed to retrieve read result.";
            }
        }
        return false;
    }

    CloseHandle(overlapped.hEvent);
    if (bytes_read < out_report->size()) {
        out_report->resize(bytes_read);
    }
    return true;
}

bool HidDevice::WriteReport(const std::vector<unsigned char>& report, std::wstring* error) {
    if (!IsOpen()) {
        if (error) {
            *error = L"Device not open.";
        }
        return false;
    }
    if (info_.output_report_length == 0) {
        if (error) {
            *error = L"Device has no output reports.";
        }
        return false;
    }
    if (report.empty()) {
        if (error) {
            *error = L"Report buffer empty.";
        }
        return false;
    }

    size_t required = info_.output_report_length;
    std::vector<unsigned char> buffer;
    if (required == 64 && report.size() == 65) {
        buffer.assign(report.begin() + 1, report.begin() + 65);
    } else if (required == 65 && report.size() == 64) {
        buffer.assign(65, 0x00);
        std::copy(report.begin(), report.begin() + 64, buffer.begin() + 1);
    } else {
        buffer = report;
    }

    if (required < buffer.size()) {
        if (error) {
            *error = L"Report too long for device.";
        }
        return false;
    }

    if (buffer.size() < required) {
        buffer.resize(required, 0x00);
    }

    last_write_length_ = buffer.size();
    last_write_actual_ = 0;

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        if (error) {
            *error = L"Failed to create write event.";
        }
        return false;
    }

    DWORD written = 0;
    BOOL ok = WriteFile(write_handle_, buffer.data(), static_cast<DWORD>(buffer.size()), &written, &overlapped);
    if (!ok) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            CloseHandle(overlapped.hEvent);
            if (error) {
                if (IsDeviceRemovedError(err)) {
                    *error = L"Device removed.";
                } else {
                    *error = L"WriteFile failed.";
                }
            }
            return false;
        }
        DWORD wait_result = WaitForSingleObject(overlapped.hEvent, 500);
        if (wait_result == WAIT_TIMEOUT) {
            CancelIo(write_handle_);
            CloseHandle(overlapped.hEvent);
            if (error) {
                *error = L"Write timed out.";
            }
            return false;
        }
        if (!GetOverlappedResult(write_handle_, &overlapped, &written, FALSE)) {
            DWORD get_err = GetLastError();
            CloseHandle(overlapped.hEvent);
            if (error) {
                if (IsDeviceRemovedError(get_err)) {
                    *error = L"Device removed.";
                } else {
                    *error = L"WriteFile failed.";
                }
            }
            return false;
        }
    }
    CloseHandle(overlapped.hEvent);
    last_write_actual_ = written;
    if (last_write_actual_ != last_write_length_) {
        if (error) {
            *error = L"Write length mismatch.";
        }
        return false;
    }

    return true;
}

void HidDevice::GetLastWriteInfo(size_t* out_length, size_t* out_actual) const {
    if (out_length) {
        *out_length = last_write_length_;
    }
    if (out_actual) {
        *out_actual = last_write_actual_;
    }
}

void HidDevice::CancelPendingIo() {
    if (read_handle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(read_handle_, nullptr);
    }
    if (write_handle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(write_handle_, nullptr);
    }
}
