#include "scheduler.h"

#include <algorithm>
#include <sstream>

namespace {

std::wstring BytesToHex(const std::vector<unsigned char>& bytes) {
    std::wstringstream ss;
    ss << std::uppercase << std::hex;
    size_t count = std::min<size_t>(64, bytes.size());
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) {
            ss << L' ';
        }
        ss.width(2);
        ss.fill(L'0');
        ss << static_cast<unsigned int>(bytes[i]);
    }
    return ss.str();
}

std::wstring BuildTxLogText(const std::vector<unsigned char>& payload) {
    return BytesToHex(payload);
}

}

TxScheduler::TxScheduler()
    : device_(nullptr),
      handshake_(nullptr),
      running_(false),
      stop_requested_(false),
      thread_(nullptr),
      stop_event_(nullptr),
      notify_hwnd_(nullptr),
      tx_count_(0),
      limit_remaining_(0),
      limit_sent_(0) {
    InitializeCriticalSection(&settings_lock_);
    settings_ = Settings();
}

TxScheduler::~TxScheduler() {
    Stop();
    DeleteCriticalSection(&settings_lock_);
}

void TxScheduler::SetDevice(HidDevice* device) {
    device_ = device;
}

void TxScheduler::SetHandshake(HandshakeFsm* fsm) {
    handshake_ = fsm;
}

void TxScheduler::SetNotifyHwnd(HWND hwnd) {
    notify_hwnd_ = hwnd;
}

void TxScheduler::UpdateSettings(const Settings& settings) {
    EnterCriticalSection(&settings_lock_);
    settings_ = settings;
    LeaveCriticalSection(&settings_lock_);
}

void TxScheduler::Start() {
    if (running_) {
        return;
    }
    {
        Settings snapshot;
        EnterCriticalSection(&settings_lock_);
        snapshot = settings_;
        LeaveCriticalSection(&settings_lock_);
        if (snapshot.limit_enabled) {
            int count = snapshot.limit_count;
            if (count < 5) {
                count = 5;
            }
            limit_sent_ = 0;
            limit_remaining_ = count;
            unsigned long long packed = (static_cast<unsigned long long>(static_cast<unsigned int>(limit_sent_.load())) << 32)
                                        | static_cast<unsigned int>(limit_remaining_.load());
            Notify(EventLimitUpdate, packed, L"");
        } else {
            limit_sent_ = 0;
            limit_remaining_ = 0;
            unsigned long long packed = 0;
            Notify(EventLimitUpdate, packed, L"");
        }
    }
    stop_requested_ = false;
    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    thread_ = AfxBeginThread(ThreadProc, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    if (!thread_) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
        return;
    }
    thread_->m_bAutoDelete = FALSE;
    running_ = true;
    thread_->ResumeThread();
}

void TxScheduler::Stop() {
    stop_requested_ = true;
    if (device_) {
        device_->CancelPendingIo();
    }
    if (stop_event_) {
        SetEvent(stop_event_);
    }
    if (thread_) {
        WaitForSingleObject(thread_->m_hThread, 2000);
        delete thread_;
        thread_ = nullptr;
    }
    if (stop_event_) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
    }
    running_ = false;
}

void TxScheduler::SingleShot() {
    RunCycle();
}

UINT TxScheduler::ThreadProc(LPVOID param) {
    TxScheduler* scheduler = reinterpret_cast<TxScheduler*>(param);
    return scheduler ? scheduler->ThreadLoop() : 0;
}

UINT TxScheduler::ThreadLoop() {
    while (!stop_requested_) {
        RunCycle();
        Settings snapshot;
        EnterCriticalSection(&settings_lock_);
        snapshot = settings_;
        LeaveCriticalSection(&settings_lock_);

        DWORD interval = CurrentIntervalMs(snapshot);
        if (stop_event_) {
            WaitForSingleObject(stop_event_, interval);
        } else {
            Sleep(interval);
        }
    }
    return 0;
}

DWORD TxScheduler::CurrentIntervalMs(const Settings& settings) const {
    if (!settings.interval_enabled) {
        return 100;
    }
    return settings.interval_ms == 0 ? 100 : settings.interval_ms;
}

bool TxScheduler::BuildReport(unsigned char report_id, const std::vector<unsigned char>& payload, std::vector<unsigned char>* report, std::wstring* error) {
    if (!report) {
        if (error) {
            *error = L"Report buffer missing.";
        }
        return false;
    }
    if (payload.size() < 64) {
        if (error) {
            *error = L"Payload length must be 64 bytes.";
        }
        return false;
    }
    report->assign(65, 0x00);
    (*report)[0] = report_id;
    std::copy(payload.begin(), payload.begin() + 64, report->begin() + 1);
    return true;
}

void TxScheduler::RunCycle() {
    if (!device_ || !device_->IsOpen()) {
        Notify(EventError, 0, L"Not connected.");
        return;
    }
    Settings snapshot;
    EnterCriticalSection(&settings_lock_);
    snapshot = settings_;
    LeaveCriticalSection(&settings_lock_);

    unsigned char report_id = snapshot.report_id;
    const std::array<TxCommandRow, 6>& rows = snapshot.rows;
    bool handshake_enabled = snapshot.handshake_enabled;
    auto apply_limit = [&]() {
        if (!snapshot.limit_enabled) {
            return;
        }
        int remaining = limit_remaining_.load();
        if (remaining <= 0) {
            return;
        }
        remaining = remaining - 1;
        limit_remaining_ = remaining;
        int sent = limit_sent_.load() + 1;
        limit_sent_ = sent;
        unsigned long long packed = (static_cast<unsigned long long>(static_cast<unsigned int>(sent)) << 32)
                                    | static_cast<unsigned int>(remaining);
        Notify(EventLimitUpdate, packed, L"");
        if (remaining <= 0) {
            stop_requested_ = true;
            if (stop_event_) {
                SetEvent(stop_event_);
            }
        }
    };
    if (handshake_enabled) {
        if (!handshake_) {
            Notify(EventError, 0, L"Handshake not initialized.");
            return;
        }

        DWORD timeout_ms = snapshot.handshake_timeout_ms;
        if (timeout_ms < 200) {
            timeout_ms = 200;
        } else if (timeout_ms > 500) {
            timeout_ms = 500;
        }
        handshake_->SetTimeout(timeout_ms);

        int retries = snapshot.handshake_retry_count;
        if (retries < 5) {
            retries = 5;
        }
        bool ok = false;
        std::wstring hs_error;
        for (int attempt = 0; attempt < retries; ++attempt) {
            Notify(EventTxLog, 0, BuildTxLogText(rows[0].payload));
            Notify(EventHandshake, 0, L"WAITING");
            if (handshake_->Execute(rows[0].payload, report_id, &hs_error)) {
                ok = true;
                break;
            }
            if (hs_error == L"Device removed.") {
                Notify(EventDeviceRemoved, 0, hs_error);
                return;
            }
        }
        if (!ok) {
            Notify(EventHandshake, 0, L"ERROR");
            Notify(EventError, 0, L"communication error (hand-shaking)");
            return;
        }
        Notify(EventHandshake, 0, L"OK");
        {
            size_t intended = 0;
            size_t actual = 0;
            device_->GetLastWriteInfo(&intended, &actual);
            std::wstring info = L"len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual);
            Notify(EventWriteInfo, 0, info);
        }
        tx_count_++;
        Notify(EventTxCount, tx_count_.load(), L"");

        bool any_followup = false;
        for (int i = 1; i < 6; ++i) {
            if (!rows[i].enabled) {
                continue;
            }
            any_followup = true;
            std::vector<unsigned char> report;
            std::wstring build_error;
            if (!BuildReport(report_id, rows[i].payload, &report, &build_error)) {
                Notify(EventError, 0, build_error);
                return;
            }
            std::wstring write_error;
            if (!device_->WriteReport(report, &write_error)) {
                if (write_error == L"Device removed.") {
                    Notify(EventDeviceRemoved, 0, write_error);
                } else {
                    size_t intended = 0;
                    size_t actual = 0;
                    device_->GetLastWriteInfo(&intended, &actual);
                    std::wstring msg = write_error.empty() ? L"Write failed." : write_error;
                    msg += L" (len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual) + L")";
                    Notify(EventError, 0, msg);
                }
                return;
            }
            Notify(EventTxLog, 0, BuildTxLogText(rows[i].payload));
            {
                size_t intended = 0;
                size_t actual = 0;
                device_->GetLastWriteInfo(&intended, &actual);
                std::wstring info = L"len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual);
                Notify(EventWriteInfo, 0, info);
            }
            tx_count_++;
            Notify(EventTxCount, tx_count_.load(), L"");
        }

        if (!any_followup) {
            apply_limit();
            return;
        }

        apply_limit();
        return;
    }

    Notify(EventHandshake, 0, L"OFF");

    bool any_enabled = false;
    for (int i = 0; i < 6; ++i) {
        if (!rows[i].enabled) {
            continue;
        }
        any_enabled = true;
        std::vector<unsigned char> report;
        std::wstring build_error;
        if (!BuildReport(report_id, rows[i].payload, &report, &build_error)) {
            Notify(EventError, 0, build_error);
            return;
        }
        std::wstring write_error;
        if (!device_->WriteReport(report, &write_error)) {
            if (write_error == L"Device removed.") {
                Notify(EventDeviceRemoved, 0, write_error);
            } else {
                size_t intended = 0;
                size_t actual = 0;
                device_->GetLastWriteInfo(&intended, &actual);
                std::wstring msg = write_error.empty() ? L"Write failed." : write_error;
                msg += L" (len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual) + L")";
                Notify(EventError, 0, msg);
            }
            return;
        }
        Notify(EventTxLog, 0, BuildTxLogText(rows[i].payload));
        {
            size_t intended = 0;
            size_t actual = 0;
            device_->GetLastWriteInfo(&intended, &actual);
            std::wstring info = L"len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual);
            Notify(EventWriteInfo, 0, info);
        }
        tx_count_++;
        Notify(EventTxCount, tx_count_.load(), L"");
    }

    if (!any_enabled) {
        Notify(EventError, 0, L"No TX enabled.");
        return;
    }

    apply_limit();
}

void TxScheduler::Notify(EventType type, unsigned long long value, const std::wstring& text) {
    if (!notify_hwnd_) {
        return;
    }
    EventPayload* payload = new EventPayload();
    payload->type = type;
    payload->value = value;
    payload->text = text;
    PostMessage(notify_hwnd_, MessageId(), 0, reinterpret_cast<LPARAM>(payload));
}

