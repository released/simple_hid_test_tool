#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../hid/hid_device.h"

class HandshakeFsm {
public:
    HandshakeFsm();

    void SetDevice(HidDevice* device);
    void SetTimeout(DWORD timeout_ms);
    DWORD timeout() const { return timeout_ms_; }

    void SetWaitEchoCallback(const std::function<bool(const std::vector<unsigned char>&, DWORD)>& callback);

    bool Execute(const std::vector<unsigned char>& payload, unsigned char report_id, std::wstring* error);

private:
    bool CompareEcho(const std::vector<unsigned char>& payload, const std::vector<unsigned char>& report) const;

    HidDevice* device_;
    DWORD timeout_ms_;
    std::function<bool(const std::vector<unsigned char>&, DWORD)> wait_echo_;
};

