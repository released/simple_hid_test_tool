#pragma once

#include <afxcmn.h>
#include <afxext.h>
#include <afxwin.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "config/command_config_manager.h"
#include "config/config_manager.h"
#include "core/scheduler.h"
#include "hid/hid_device.h"
#include "tcp/tcp_protocol.h"
#include "ui/device_bar.h"
#include "ui/firmware_panel.h"
#include "ui/function_panel.h"
#include "ui/rx_panel.h"
#include "ui/tx_panel.h"

class CMainFrame : public CFrameWnd {
public:
    CMainFrame() = default;

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnDeviceBarAction(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTxPanelAction(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSchedulerEvent(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnRxEvent(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFunctionAction(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFirmwareAction(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnControlPollResult(WPARAM wParam, LPARAM lParam);
    afx_msg void OnClose();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnTabChanged(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    struct FirmwareFailureInfo {
        std::wstring summary;
        std::wstring transport_error;
        unsigned char status = tcp_tool::kStatusError;
        std::size_t offset = 0;
        std::size_t chunk_size = 0;
        std::size_t image_size = 0;
        bool has_status = false;
        bool has_offset = false;
        bool has_chunk_size = false;
        bool has_image_size = false;
    };

    struct ControlPollResult {
        enum Type {
            TypeUptimeSuccess = 0,
            TypeUptimeFailure = 1,
            TypeFunctionSuccess = 2,
            TypeFunctionFailure = 3,
        };

        Type type = TypeUptimeSuccess;
        unsigned char packet_id = 0;
        std::uint32_t uptime_seconds = 0;
        bool fatal_disconnect = false;
        std::wstring error;
        std::vector<unsigned char> request_payload;
        std::vector<unsigned char> response_payload;
    };

    void LayoutControls(int cx, int cy);
    void LayoutStatusBarPanes();
    void InitStatusBar();
    void ShowActivePage();
    void UpdateConnectionStatus(const std::wstring& text);
    void UpdateErrorStatus(const std::wstring& text);
    void UpdateInfoStatus(const std::wstring& text);
    void UpdateTxCount(unsigned long long count);
    void UpdateRxCount(unsigned long long count);
    void UpdateRxActivity();
    void MarkControlTraffic();
    void StartStatusPollTimer();
    void StopStatusPollTimer();
    void ControlPollThreadLoop();
    void SyncControlPollSettings();
    void RequestImmediateControlPoll();
    void StartReconnectTimer();
    void StopReconnectTimer();
    bool ConnectEndpoint();
    bool BuildSchedulerSettings(TxScheduler::Settings* out_settings, std::wstring* error);
    bool RefreshPeerInfo();
    bool SendPing();
    bool PollPeerUptime(bool silent);
    bool RefreshFunctionTelemetry(bool silent);
    bool PollFunctionPackets(bool silent);
    bool PollOneFunctionPacket(bool silent);
    bool RefreshCurrentFunctionTab(bool silent);
    bool ApplyFunctionGpio();
    bool ApplyFunctionPwm();
    bool SendFunctionPacket(const std::vector<unsigned char>& request_payload,
                            std::vector<unsigned char>* response_payload,
                            std::wstring* error,
                            DWORD timeout_ms = 2000);
    bool ApplyFunctionTelemetry(const std::vector<unsigned char>& payload);
    void RecordFunctionTx(const std::vector<unsigned char>& payload);
    bool RunFirmwareUpdate();
    bool FinishFirmwareFailure(const FirmwareFailureInfo& info);
    bool RequestEnterBootloader();
    bool RequestResetMcu();
    bool ReadFileBytes(const std::wstring& path, std::vector<unsigned char>* out_data, std::wstring* error);
    void ApplyConfig();
    void ApplyCommandConfig();
    void SaveConfig();
    void SaveCommandConfig();
    bool SaveCommandConfigTo(const std::wstring& path);
    bool LoadCommandConfigFrom(const std::wstring& path);
    void DisconnectUi(bool allow_reconnect);
    void SetPeerModeText();
    void PumpUi();

    CDeviceBar m_deviceBar;
    CTabCtrl m_tab;
    CTxPanel m_txPanel;
    CRxPanel m_rxPanel;
    CFunctionPanel m_functionPanel;
    CFirmwarePanel m_firmwarePanel;
    CStatusBar m_statusBar;
    HidDevice m_device;
    TxScheduler m_scheduler;
    ConfigManager m_config;
    CommandConfigManager m_commandConfig;

    unsigned long long m_txCount = 0;
    unsigned long long m_rxCount = 0;
    DWORD m_lastRxTick = 0;
    UINT_PTR m_rxStatusTimer = 0;
    UINT_PTR m_reconnectTimer = 0;
    std::wstring m_lastWriteInfo;
    int m_activeTab = 0;
    tcp_tool::PeerMode m_peerMode = tcp_tool::kPeerUnknown;
    int m_uptimePollFailures = 0;
    std::thread m_controlPollThread;
    std::atomic<bool> m_controlPollStopRequested = false;
    std::atomic<bool> m_forceStatusPoll = false;
    std::atomic<bool> m_pollAdcEnabled = true;
    std::atomic<bool> m_pollGpioEnabled = true;
    std::atomic<bool> m_pollPwmEnabled = true;
    std::atomic<bool> m_pendingFunctionRefresh = false;
    std::atomic<bool> m_pendingGpioApply = false;
    std::atomic<bool> m_pendingPwmApply = false;
    std::atomic<unsigned int> m_pendingFunctionRefreshPacket = tcp_tool::kRawPktAdc;
    std::atomic<unsigned int> m_pendingGpioState = 0;
    std::atomic<std::uint32_t> m_pendingPwmFreq = 1000;
    std::atomic<std::uint32_t> m_pendingPwmDuty = 50;
    std::atomic<int> m_pollActiveTab = 0;
    std::atomic<int> m_pollPeerMode = static_cast<int>(tcp_tool::kPeerUnknown);
    unsigned int m_nextFunctionPollIndex = 0;
};
