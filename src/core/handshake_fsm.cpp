#include "handshake_fsm.h"

#include <algorithm>

HandshakeFsm::HandshakeFsm() : device_(nullptr), timeout_ms_(300) {}

void HandshakeFsm::SetDevice(HidDevice* device) {
    device_ = device;
}

void HandshakeFsm::SetTimeout(DWORD timeout_ms) {
    timeout_ms_ = timeout_ms;
}

void HandshakeFsm::SetWaitEchoCallback(const std::function<bool(const std::vector<unsigned char>&, DWORD)>& callback) {
    wait_echo_ = callback;
}

bool HandshakeFsm::Execute(const std::vector<unsigned char>& payload, unsigned char report_id, std::wstring* error) {
    if (!device_ || !device_->IsOpen()) {
        if (error) {
            *error = L"Device not connected.";
        }
        return false;
    }
    if (payload.size() < 64) {
        if (error) {
            *error = L"Handshake payload too short.";
        }
        return false;
    }

    std::vector<unsigned char> report(65, 0x00);
    report[0] = report_id;
    std::copy(payload.begin(), payload.begin() + 64, report.begin() + 1);

    std::wstring write_error;
    if (!device_->WriteReport(report, &write_error)) {
        if (error) {
            *error = write_error.empty() ? L"Handshake write failed." : write_error;
        }
        return false;
    }

    if (wait_echo_) {
        bool matched = wait_echo_(payload, timeout_ms_);
        if (!matched) {
            if (error) {
                *error = L"Handshake timeout or mismatch.";
            }
            return false;
        }
        return true;
    }

    std::vector<unsigned char> rx_report;
    std::wstring read_error;
    if (!device_->ReadInputReport(&rx_report, timeout_ms_, &read_error)) {
        if (error) {
            *error = read_error.empty() ? L"Handshake read failed." : read_error;
        }
        return false;
    }

    if (!CompareEcho(payload, rx_report)) {
        if (error) {
            *error = L"Handshake mismatch.";
        }
        return false;
    }

    return true;
}

bool HandshakeFsm::CompareEcho(const std::vector<unsigned char>& payload, const std::vector<unsigned char>& report) const {
    if (payload.size() < 64) {
        return false;
    }

    std::vector<unsigned char> echo(64, 0x00);
    size_t offset = 0;
    size_t available = report.size();
    if (available == 65) {
        offset = 1;
        available = 64;
    }
    size_t copy_len = std::min<size_t>(64, available);
    if (copy_len > 0 && offset < report.size()) {
        std::copy_n(report.data() + offset, copy_len, echo.begin());
    }

    return std::equal(payload.begin(), payload.begin() + 64, echo.begin());
}

