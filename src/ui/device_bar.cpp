#include "device_bar.h"

#include <commctrl.h>
#include <cstring>

#include "../build_info.h"

#pragma comment(lib, "msimg32.lib")

namespace {

const int kHostId = 2001;
const int kPortId = 2002;
const int kConnectId = 2003;
const int kPingId = 2004;
const int kAutoReconnectId = 2005;

}

BEGIN_MESSAGE_MAP(CDeviceBar, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_BN_CLICKED(kConnectId, &CDeviceBar::OnConnectClicked)
    ON_BN_CLICKED(kPingId, &CDeviceBar::OnPingClicked)
    ON_BN_CLICKED(kAutoReconnectId, &CDeviceBar::OnAutoReconnectClicked)
    ON_EN_CHANGE(kHostId, &CDeviceBar::OnEndpointChanged)
    ON_EN_CHANGE(kPortId, &CDeviceBar::OnEndpointChanged)
END_MESSAGE_MAP()

bool CDeviceBar::Create(CWnd* parent) {
    CString class_name = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), nullptr);
    return CWnd::CreateEx(0, class_name, L"ConnectionBar",
                          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          CRect(0, 0, 0, 0), parent, 0);
}

int CDeviceBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    CClientDC dc(this);
    int font_height = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);

    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    m_uiFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_highlightBrush.CreateSolidBrush(RGB(15, 118, 110));

    m_hostLabel.Create(L"IP / Host", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_hostEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, kHostId);
    m_portLabel.Create(L"Port", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_portEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, kPortId);
    m_connectButton.Create(L"Connect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           CRect(0, 0, 0, 0), this, kConnectId);
    m_pingButton.Create(L"Ping", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, kPingId);
    m_autoReconnectCheck.Create(L"Auto reconnect", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                CRect(0, 0, 0, 0), this, kAutoReconnectId);
    m_summaryTitleLabel.Create(L"TCP Endpoint", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryTitleValue.Create(L"-", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryModeLabel.Create(L"Peer Mode", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryModeValue.Create(L"Unknown", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryUptimeLabel.Create(L"Uptime", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryUptimeValue.Create(L"Unknown", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryProtocolLabel.Create(L"Protocol", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryProtocolValue.Create(L"TCPTool v1 / framed packets", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryPayloadLabel.Create(L"Payload", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryPayloadValue.Create(L"RAW64 / ADC-GPIO-PWM / IAP", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryEventLabel.Create(L"Last Event", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_summaryEventValue.Create(L"Idle", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_statusLabel.Create(L"Status: Disconnected", WS_CHILD | WS_VISIBLE | SS_RIGHT, CRect(0, 0, 0, 0), this);
    m_buildVersionLabel.Create(L"Build:", WS_CHILD | WS_VISIBLE | SS_RIGHT, CRect(0, 0, 0, 0), this);
    m_buildStampLabel.Create(L"", WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 0, 0), this);

    CWnd* controls[] = {
        &m_hostLabel, &m_hostEdit, &m_portLabel, &m_portEdit, &m_connectButton, &m_pingButton,
        &m_autoReconnectCheck, &m_summaryTitleLabel, &m_summaryTitleValue, &m_summaryModeLabel, &m_summaryModeValue,
        &m_summaryUptimeLabel, &m_summaryUptimeValue, &m_summaryProtocolLabel, &m_summaryProtocolValue,
        &m_summaryPayloadLabel, &m_summaryPayloadValue, &m_summaryEventLabel, &m_summaryEventValue,
        &m_statusLabel, &m_buildVersionLabel, &m_buildStampLabel
    };
    for (CWnd* control : controls) {
        control->SetFont(&m_uiFont);
    }

    UpdateStatusText();
    UpdateEndpointText();
    m_buildStampLabel.SetWindowTextW(BuildVersionText().c_str());
    return 0;
}

void CDeviceBar::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CDeviceBar::LayoutControls(int cx, int cy) {
    const int margin = 8;
    const int gap = 6;
    const int row_height = 26;
    const int button_width = 110;
    const int host_width = 210;
    const int port_width = 80;
    const int status_width = 200;
    const int build_title_width = 46;
    const int build_value_width = 240;
    const int summary_label_width = 84;
    const int summary_colon_width = 12;
    const int summary_value_width = 440;
    const int build_row_height = 20;
    int y = margin + 2;
    int line_gap = 28;
    int summary_label_x = margin;
    int summary_colon_x = summary_label_x + summary_label_width;
    int summary_value_x = summary_colon_x + summary_colon_width;
    int build_y = 0;

    m_hostLabel.MoveWindow(margin, y + 4, 60, 20);
    m_hostEdit.MoveWindow(margin + 66, y, host_width, row_height);
    m_portLabel.MoveWindow(margin + 66 + host_width + gap, y + 4, 34, 20);
    m_portEdit.MoveWindow(margin + 66 + host_width + gap + 38, y, port_width, row_height);
    m_connectButton.MoveWindow(margin + 66 + host_width + port_width + 64, y, button_width, row_height);
    m_pingButton.MoveWindow(margin + 66 + host_width + port_width + 64 + button_width + gap, y, 90, row_height);
    m_autoReconnectCheck.MoveWindow(margin + 66 + host_width + port_width + 64 + button_width + 90 + gap * 2,
                                    y + 3, 140, 24);
    m_statusLabel.MoveWindow(cx - status_width - margin, y + 4, status_width, 20);

    y += row_height + gap + 2;
    m_summaryTitleLabel.MoveWindow(summary_label_x, y, summary_label_width, 22);
    m_summaryTitleValue.MoveWindow(summary_value_x, y, summary_value_width, 22);
    y += line_gap;
    m_summaryModeLabel.MoveWindow(summary_label_x, y, summary_label_width, 22);
    m_summaryModeValue.MoveWindow(summary_value_x, y, summary_value_width, 22);
    y += line_gap;
    m_summaryUptimeLabel.MoveWindow(summary_label_x, y, summary_label_width, 22);
    m_summaryUptimeValue.MoveWindow(summary_value_x, y, summary_value_width, 22);
    y += line_gap;
    m_summaryProtocolLabel.MoveWindow(summary_label_x, y, summary_label_width, 22);
    m_summaryProtocolValue.MoveWindow(summary_value_x, y, summary_value_width, 22);
    y += line_gap;
    m_summaryPayloadLabel.MoveWindow(summary_label_x, y, summary_label_width, 22);
    m_summaryPayloadValue.MoveWindow(summary_value_x, y, summary_value_width, 22);
    y += line_gap;
    m_summaryEventLabel.MoveWindow(summary_label_x, y, summary_label_width, 22);
    m_summaryEventValue.MoveWindow(summary_value_x, y, summary_value_width, 22);
    build_y = cy - margin - build_row_height;
    if (build_y < y) {
        build_y = y;
    }
    m_buildVersionLabel.MoveWindow(cx - margin - build_title_width - build_value_width,
                                   build_y, build_title_width, build_row_height);
    m_buildStampLabel.MoveWindow(cx - margin - build_value_width,
                                 build_y, build_value_width, build_row_height);
    (void)cy;
}

void CDeviceBar::OnPaint() {
    CPaintDC dc(this);
    CRect rect;
    CRect status_rect;
    TRIVERTEX verts[2] = {};
    GRADIENT_RECT grad = { 0, 1 };
    COLORREF dot_color = RGB(148, 163, 184);
    const int dot_size = 8;

    GetClientRect(&rect);
    verts[0].x = rect.left;
    verts[0].y = rect.top;
    verts[0].Red = static_cast<COLOR16>(248 << 8);
    verts[0].Green = static_cast<COLOR16>(250 << 8);
    verts[0].Blue = static_cast<COLOR16>(252 << 8);
    verts[1].x = rect.right;
    verts[1].y = rect.bottom;
    verts[1].Red = static_cast<COLOR16>(226 << 8);
    verts[1].Green = static_cast<COLOR16>(232 << 8);
    verts[1].Blue = static_cast<COLOR16>(240 << 8);
    ::GradientFill(dc.GetSafeHdc(), verts, 2, &grad, 1, GRADIENT_FILL_RECT_V);
    dc.DrawEdge(&rect, EDGE_SUNKEN, BF_RECT);

    switch (m_connectionState) {
    case StateConnected:
        dot_color = RGB(34, 197, 94);
        break;
    case StateRxActive:
        dot_color = RGB(56, 189, 248);
        break;
    case StateRxTimeout:
        dot_color = RGB(249, 115, 22);
        break;
    default:
        break;
    }

    m_statusLabel.GetWindowRect(&status_rect);
    ScreenToClient(&status_rect);
    {
        CBrush brush(dot_color);
        CBrush* old_brush = dc.SelectObject(&brush);
        dc.Ellipse(status_rect.left - 12, status_rect.top + 6, status_rect.left - 12 + dot_size, status_rect.top + 6 + dot_size);
        dc.SelectObject(old_brush);
    }
}

void CDeviceBar::OnConnectClicked() {
    NotifyParent(ActionConnectToggle);
}

void CDeviceBar::OnPingClicked() {
    NotifyParent(ActionPing);
}

void CDeviceBar::OnAutoReconnectClicked() {
    NotifyParent(ActionSettingsChanged);
}

void CDeviceBar::OnEndpointChanged() {
    UpdateEndpointText();
    NotifyParent(ActionSettingsChanged);
}

void CDeviceBar::NotifyParent(Action action) {
    CWnd* parent = GetParent();
    if (!parent) {
        return;
    }
    parent->PostMessage(MessageId(), static_cast<WPARAM>(action), 0);
}

void CDeviceBar::SetConnected(bool connected) {
    m_connected = connected;
    m_connectButton.SetWindowTextW(connected ? L"Disconnect" : L"Connect");
    m_hostEdit.EnableWindow(!connected);
    m_portEdit.EnableWindow(!connected);
    m_connectButton.Invalidate();
}

void CDeviceBar::SetConnectionState(int state) {
    m_connectionState = state;
    UpdateStatusText();
    Invalidate(FALSE);
}

void CDeviceBar::SetPeerModeText(const std::wstring& text) {
    m_peerMode = text;
    m_summaryModeValue.SetWindowTextW(text.c_str());
}

void CDeviceBar::SetUptimeText(const std::wstring& text) {
    m_uptime = text.empty() ? L"Unknown" : text;
    m_summaryUptimeValue.SetWindowTextW(m_uptime.c_str());
}

void CDeviceBar::SetLastEventText(const std::wstring& text) {
    m_lastEvent = text.empty() ? L"Idle" : text;
    m_summaryEventValue.SetWindowTextW(m_lastEvent.c_str());
}

std::wstring CDeviceBar::GetHost() const {
    CString text;
    m_hostEdit.GetWindowTextW(text);
    return std::wstring(text.GetString());
}

void CDeviceBar::SetHost(const std::wstring& host) {
    m_hostEdit.SetWindowTextW(host.c_str());
    UpdateEndpointText();
}

unsigned short CDeviceBar::GetPort() const {
    CString text;
    unsigned long value = 0;

    m_portEdit.GetWindowTextW(text);
    value = wcstoul(text.GetString(), nullptr, 10);
    if (value > 65535ul) {
        value = 65535ul;
    }
    return static_cast<unsigned short>(value);
}

void CDeviceBar::SetPort(unsigned short port) {
    m_portEdit.SetWindowTextW(std::to_wstring(port).c_str());
    UpdateEndpointText();
}

bool CDeviceBar::IsAutoReconnectEnabled() const {
    return m_autoReconnectCheck.GetCheck() == BST_CHECKED;
}

void CDeviceBar::SetAutoReconnectEnabled(bool enabled) {
    m_autoReconnectCheck.SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
}

void CDeviceBar::UpdateStatusText() {
    const wchar_t* text = L"Status: Disconnected";
    switch (m_connectionState) {
    case StateConnected:
        text = L"Status: Connected";
        break;
    case StateRxActive:
        text = L"Status: RX Active";
        break;
    case StateRxTimeout:
        text = L"Status: RX Timeout";
        break;
    default:
        break;
    }
    m_statusLabel.SetWindowTextW(text);
}

std::wstring CDeviceBar::BuildVersionText() const {
    return std::wstring(L"v") + TCPTESTTOOL_BUILD_VERSION_TEXT_W +
           L" | " + TCPTESTTOOL_BUILD_TIME_W;
}

void CDeviceBar::UpdateEndpointText() {
    std::wstring endpoint = GetHost();
    if (!endpoint.empty()) {
        endpoint += L":" + std::to_wstring(GetPort());
    } else {
        endpoint = L"-";
    }
    m_summaryTitleValue.SetWindowTextW(endpoint.c_str());
}
