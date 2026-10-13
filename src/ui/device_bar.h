#pragma once

#include <afxwin.h>

#include <string>

class CDeviceBar : public CWnd {
public:
    bool Create(CWnd* parent);

    void SetConnected(bool connected);
    void SetConnectionState(int state);
    void SetPeerModeText(const std::wstring& text);
    void SetUptimeText(const std::wstring& text);
    void SetLastEventText(const std::wstring& text);
    std::wstring GetHost() const;
    void SetHost(const std::wstring& host);
    unsigned short GetPort() const;
    void SetPort(unsigned short port);
    bool IsAutoReconnectEnabled() const;
    void SetAutoReconnectEnabled(bool enabled);

    static UINT MessageId() { return WM_APP + 100; }
    enum Action {
        ActionConnectToggle = 1,
        ActionPing = 2,
        ActionSettingsChanged = 3,
    };
    enum ConnectionState {
        StateDisconnected = 0,
        StateConnected = 1,
        StateRxActive = 2,
        StateRxTimeout = 3,
    };

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg void OnConnectClicked();
    afx_msg void OnPingClicked();
    afx_msg void OnAutoReconnectClicked();
    afx_msg void OnEndpointChanged();

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void UpdateStatusText();
    void NotifyParent(Action action);
    std::wstring BuildVersionText() const;
    void UpdateEndpointText();

    CStatic m_hostLabel;
    CEdit m_hostEdit;
    CStatic m_portLabel;
    CEdit m_portEdit;
    CButton m_connectButton;
    CButton m_pingButton;
    CButton m_autoReconnectCheck;
    CStatic m_summaryTitleLabel;
    CStatic m_summaryTitleValue;
    CStatic m_summaryModeLabel;
    CStatic m_summaryModeValue;
    CStatic m_summaryUptimeLabel;
    CStatic m_summaryUptimeValue;
    CStatic m_summaryProtocolLabel;
    CStatic m_summaryProtocolValue;
    CStatic m_summaryPayloadLabel;
    CStatic m_summaryPayloadValue;
    CStatic m_summaryEventLabel;
    CStatic m_summaryEventValue;
    CStatic m_statusLabel;
    CStatic m_buildVersionLabel;
    CStatic m_buildStampLabel;
    bool m_connected = false;
    int m_connectionState = StateDisconnected;
    std::wstring m_peerMode = L"Unknown";
    std::wstring m_uptime = L"Unknown";
    std::wstring m_lastEvent = L"Idle";
    CFont m_uiFont;
    CBrush m_highlightBrush;
};

