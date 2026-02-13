#pragma once

#include <afxwin.h>
#include <afxext.h>

#include "ui/device_bar.h"
#include "ui/tx_panel.h"
#include "ui/rx_panel.h"
#include "hid/hid_device.h"
#include "core/scheduler.h"
#include "core/handshake_fsm.h"
#include "config/config_manager.h"
#include "config/command_config_manager.h"

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
    afx_msg void OnClose();
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void LayoutStatusBarPanes();
    void InitStatusBar();
    void UpdateConnectionStatus(const std::wstring& text);
    void UpdateErrorStatus(const std::wstring& text);
    void UpdateTxCount(unsigned long long count);
    void UpdateRxCount(unsigned long long count);
    void UpdateHandshakeStatus(const std::wstring& text);
    void UpdateRxActivity();
    void RestoreDeviceSelection();
    void StartReconnectTimer();
    void StopReconnectTimer();
    bool ConnectSelectedDevice();
    bool BuildSchedulerSettings(TxScheduler::Settings* out_settings, std::wstring* error);
    bool SendEchoModeCommand(bool enable);
    void ApplyConfig();
    void ApplyCommandConfig();
    void SaveConfig();
    void SaveCommandConfig();
    bool SaveCommandConfigTo(const std::wstring& path);
    bool LoadCommandConfigFrom(const std::wstring& path);

    CDeviceBar m_deviceBar;
    CTxPanel m_txPanel;
    CRxPanel m_rxPanel;
    CStatusBar m_statusBar;
    HidDevice m_device;
    TxScheduler m_scheduler;
    HandshakeFsm m_handshake;
    ConfigManager m_config;
    CommandConfigManager m_commandConfig;

    unsigned long long m_txCount;
    unsigned long long m_rxCount;

    HidScanInfo m_lastConnected;
    bool m_hasLastConnected = false;
    DWORD m_lastRxTick = 0;
    UINT_PTR m_rxStatusTimer = 0;
    UINT_PTR m_reconnectTimer = 0;
    UINT_PTR m_startupScanTimer = 0;
    std::wstring m_lastWriteInfo;
    bool m_lastEchoModeEnabled = false;
};

