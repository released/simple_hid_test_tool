#pragma once

#include <array>
#include <atomic>
#include <string>
#include <vector>

#include <afxwin.h>

#include "handshake_fsm.h"
struct TxCommandRow {
    bool enabled = false;
    std::vector<unsigned char> payload;
};

class TxScheduler {
public:
    TxScheduler();
    ~TxScheduler();

    void SetDevice(HidDevice* device);
    void SetHandshake(HandshakeFsm* fsm);

    void SetNotifyHwnd(HWND hwnd);

    struct Settings {
        bool interval_enabled = false;
        DWORD interval_ms = 100;
        unsigned char report_id = 0;
        bool handshake_enabled = false;
        DWORD handshake_timeout_ms = 300;
        int handshake_retry_count = 5;
        bool limit_enabled = false;
        int limit_count = 0;
        std::array<TxCommandRow, 6> rows;
    };

    void UpdateSettings(const Settings& settings);

    void Start();
    void Stop();
    void SingleShot();
    bool IsRunning() const { return running_; }

    unsigned long long tx_count() const { return tx_count_; }

    static UINT MessageId() { return WM_APP + 300; }
    enum EventType {
        EventError = 1,
        EventTxCount = 2,
        EventHandshake = 3,
        EventDeviceRemoved = 4,
        EventWriteInfo = 5,
        EventLimitUpdate = 6,
        EventTxLog = 7,
    };

    struct EventPayload {
        EventType type;
        unsigned long long value;
        std::wstring text;
    };

private:
    static UINT ThreadProc(LPVOID param);
    UINT ThreadLoop();

    void RunCycle();
    DWORD CurrentIntervalMs(const Settings& settings) const;
    bool BuildReport(unsigned char report_id, const std::vector<unsigned char>& payload, std::vector<unsigned char>* report, std::wstring* error);
    void Notify(EventType type, unsigned long long value, const std::wstring& text);

    HidDevice* device_;
    HandshakeFsm* handshake_;

    std::atomic<bool> running_;
    std::atomic<bool> stop_requested_;
    CWinThread* thread_;
    HANDLE stop_event_;
    HWND notify_hwnd_;
    std::atomic<unsigned long long> tx_count_;
    std::atomic<int> limit_remaining_;
    std::atomic<int> limit_sent_;

    CRITICAL_SECTION settings_lock_;
    Settings settings_;
};

