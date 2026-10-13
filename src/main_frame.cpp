#include "main_frame.h"

#include <afxdlgs.h>
#include <afxres.h>

#include <algorithm>
#include <memory>
#include <sstream>

#include "resource.h"

namespace {

const UINT kIndicators[] = {
    ID_SEPARATOR,
    ID_INDICATOR_ERR,
    ID_INDICATOR_TX,
    ID_INDICATOR_RX,
    ID_INDICATOR_HOVER,
};

const int kPaneConnection = 0;
const int kPaneError = 1;
const int kPaneTx = 2;
const int kPaneRx = 3;
const int kPaneHover = 4;

const UINT_PTR kRxTimerId = 1;
const UINT_PTR kReconnectTimerId = 2;
const UINT kTabId = 5001;
const DWORD kCommandTimeoutMs = 1000;
const DWORD kReconnectDelayMs = 500;
const DWORD kReconnectAttempts = 20;
const DWORD kStatusPollIntervalMs = 1000;
const DWORD kFunctionPollIntervalMs = 250;
const DWORD kUptimePollTimeoutMs = 200;
const DWORD kFunctionPollTimeoutMs = 200;
const UINT kControlPollMessageId = WM_APP + 260;

std::wstring BytesToHex64(const std::vector<unsigned char>& payload) {
    std::wstringstream ss;
    ss << std::uppercase << std::hex;
    std::size_t count = std::min<std::size_t>(tcp_tool::kRawPayloadSize, payload.size());
    std::size_t index = 0;
    for (index = 0; index < count; ++index) {
        if (index > 0) {
            ss << L' ';
        }
        ss.width(2);
        ss.fill(L'0');
        ss << static_cast<unsigned int>(payload[index]);
    }
    return ss.str();
}

std::wstring BuildTxLogBody(const std::vector<unsigned char>& payload) {
    return BytesToHex64(payload);
}

std::wstring FormatHex32(std::uint32_t value) {
    wchar_t buffer[16] = {};
    swprintf_s(buffer, L"%08X", value);
    return std::wstring(buffer);
}

std::wstring FormatHex8(unsigned char value) {
    wchar_t buffer[8] = {};
    swprintf_s(buffer, L"%02X", static_cast<unsigned int>(value));
    return std::wstring(buffer);
}

std::wstring FormatUptimeText(std::uint32_t seconds) {
    std::uint32_t hours = seconds / 3600U;
    std::uint32_t minutes = (seconds % 3600U) / 60U;
    std::uint32_t remain_seconds = seconds % 60U;
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%02u:%02u:%02u (%u s)", hours, minutes, remain_seconds, seconds);
    return std::wstring(buffer);
}

const std::wstring& GetAllFfRowText() {
    static const std::wstring kAllFf = []() {
        std::wstring text;
        int i = 0;
        text.reserve(tcp_tool::kRawPayloadSize * 3);
        for (i = 0; i < static_cast<int>(tcp_tool::kRawPayloadSize); ++i) {
            if (i > 0) {
                text += L" ";
            }
            text += L"FF";
        }
        return text;
    }();
    return kAllFf;
}

}

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_MESSAGE(CDeviceBar::MessageId(), &CMainFrame::OnDeviceBarAction)
    ON_MESSAGE(CTxPanel::MessageId(), &CMainFrame::OnTxPanelAction)
    ON_MESSAGE(TxScheduler::MessageId(), &CMainFrame::OnSchedulerEvent)
    ON_MESSAGE(CRxPanel::MessageId(), &CMainFrame::OnRxEvent)
    ON_MESSAGE(CFunctionPanel::MessageId(), &CMainFrame::OnFunctionAction)
    ON_MESSAGE(CFirmwarePanel::MessageId(), &CMainFrame::OnFirmwareAction)
    ON_MESSAGE(kControlPollMessageId, &CMainFrame::OnControlPollResult)
    ON_WM_CLOSE()
    ON_WM_TIMER()
    ON_NOTIFY(TCN_SELCHANGE, kTabId, &CMainFrame::OnTabChanged)
END_MESSAGE_MAP()

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
    HICON h_icon = nullptr;

    if (!CFrameWnd::PreCreateWindow(cs)) {
        return FALSE;
    }
    cs.style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    h_icon = AfxGetApp()->LoadIcon(IDI_APP_ICON);
    cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                       reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), h_icon);
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    TCITEM tie = {};
    HICON h_icon = nullptr;
    HICON h_icon_small = nullptr;

    if (CFrameWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    if (!m_deviceBar.Create(this) || !m_txPanel.Create(this) || !m_rxPanel.Create(this) ||
        !m_functionPanel.Create(this) ||
        !m_firmwarePanel.Create(this) || !m_statusBar.Create(this)) {
        return -1;
    }

    if (!m_tab.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS,
                      CRect(0, 0, 0, 0), this, kTabId)) {
        return -1;
    }

    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<LPWSTR>(L"Console");
    m_tab.InsertItem(0, &tie);
    tie.pszText = const_cast<LPWSTR>(L"Function");
    m_tab.InsertItem(1, &tie);
    tie.pszText = const_cast<LPWSTR>(L"Firmware");
    m_tab.InsertItem(2, &tie);
    m_tab.SetCurSel(0);

    InitStatusBar();

    h_icon = static_cast<HICON>(::LoadImageW(AfxGetInstanceHandle(), MAKEINTRESOURCEW(IDI_APP_ICON),
                                             IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    h_icon_small = static_cast<HICON>(::LoadImageW(AfxGetInstanceHandle(), MAKEINTRESOURCEW(IDI_APP_ICON),
                                                   IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (h_icon) {
        SetIcon(h_icon, TRUE);
    }
    if (h_icon_small) {
        SetIcon(h_icon_small, FALSE);
    }

    m_rxPanel.SetDevice(&m_device);
    m_scheduler.SetDevice(&m_device);
    m_scheduler.SetNotifyHwnd(m_hWnd);

    m_config.Load();
    m_commandConfig.Load();
    ApplyConfig();
    ApplyCommandConfig();
    SaveCommandConfig();
    SyncControlPollSettings();

    m_deviceBar.SetConnected(false);
    m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
    m_txPanel.SetSchedulerEnabled(false);
    m_txPanel.SetRunning(false);
    m_txPanel.SetControlsEnabled(false);
    m_rxPanel.SetControlsEnabled(false);
    m_rxPanel.SetClearEnabled(true);
    m_functionPanel.SetControlsEnabled(false);
    m_firmwarePanel.SetControlsEnabled(false);
    m_firmwarePanel.SetBurnRunning(false);
    m_firmwarePanel.SetProgress(0, L"Idle");
    SetPeerModeText();
    ShowActivePage();

    if (!m_rxStatusTimer) {
        m_rxStatusTimer = SetTimer(kRxTimerId, 200, nullptr);
    }
    StartStatusPollTimer();

    return 0;
}

void CMainFrame::OnSize(UINT nType, int cx, int cy) {
    CFrameWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CMainFrame::LayoutControls(int cx, int cy) {
    const int margin = 8;
    const int gap = 8;
    const int top_height = 206;
    const int tab_height = 28;
    const int status_height = 22;
    int width = cx - margin * 2;
    int y = margin;
    int available_height = 0;
    int tx_height = 0;
    int rx_height = 0;
    int page_top = 0;
    int page_height = 0;
    int data_start = 0;
    int cell_base = 0;
    int cell_extra = 0;
    int row_height = 0;
    int row_gap = 0;

    if (cx <= 0 || cy <= 0) {
        return;
    }
    if (width < 0) {
        width = 0;
    }

    m_statusBar.MoveWindow(0, cy - status_height, cx, status_height);
    LayoutStatusBarPanes();

    m_deviceBar.MoveWindow(margin, y, width, top_height);
    y += top_height + gap;
    m_tab.MoveWindow(margin, y, width, tab_height);
    y += tab_height + gap;

    page_top = y;
    page_height = cy - status_height - margin - page_top;
    if (page_height < 0) {
        page_height = 0;
    }

    if (m_activeTab == 0) {
        available_height = page_height;
        tx_height = m_txPanel.GetPreferredHeight();
        rx_height = m_rxPanel.GetPreferredHeight();
        if (available_height >= tx_height + rx_height + gap) {
            rx_height = std::min(rx_height + 8, available_height / 2);
            tx_height = available_height - rx_height - gap;
        } else {
            tx_height = std::max(0, available_height - rx_height - gap);
        }
        m_txPanel.MoveWindow(margin, page_top, width, tx_height);
        m_rxPanel.MoveWindow(margin, page_top + tx_height + gap, width, std::max(0, available_height - tx_height - gap));
        m_txPanel.GetGridMetrics(&data_start, &cell_base, &cell_extra, &row_height, &row_gap);
        if (data_start > 0 && cell_base > 0) {
            m_rxPanel.SetExternalGridMetrics(data_start, cell_base, cell_extra, row_height, row_gap);
        }
    } else if (m_activeTab == 1) {
        m_functionPanel.MoveWindow(margin, page_top, width, page_height);
    } else {
        m_firmwarePanel.MoveWindow(margin, page_top, width, page_height);
    }

    ShowActivePage();
}

void CMainFrame::ShowActivePage() {
    const BOOL show_console = (m_activeTab == 0) ? TRUE : FALSE;
    const BOOL show_function = (m_activeTab == 1) ? TRUE : FALSE;
    const BOOL show_firmware = (m_activeTab == 2) ? TRUE : FALSE;
    m_txPanel.ShowWindow(show_console ? SW_SHOW : SW_HIDE);
    m_rxPanel.ShowWindow(show_console ? SW_SHOW : SW_HIDE);
    m_functionPanel.ShowWindow(show_function ? SW_SHOW : SW_HIDE);
    m_firmwarePanel.ShowWindow(show_firmware ? SW_SHOW : SW_HIDE);
}

LRESULT CMainFrame::OnDeviceBarAction(WPARAM wParam, LPARAM) {
    int action = static_cast<int>(wParam);

    if (action == CDeviceBar::ActionConnectToggle) {
        if (m_device.IsOpen()) {
            DisconnectUi(true);
            return 0;
        }
        ConnectEndpoint();
        return 0;
    }
    if (action == CDeviceBar::ActionPing) {
        SendPing();
        return 0;
    }
    if (action == CDeviceBar::ActionSettingsChanged) {
        SaveConfig();
        return 0;
    }
    return 0;
}

LRESULT CMainFrame::OnTxPanelAction(WPARAM wParam, LPARAM lParam) {
    int action = static_cast<int>(wParam);
    int row = static_cast<int>(lParam);
    std::wstring error;

    if (action == CTxPanel::ActionSendOnce) {
        std::vector<unsigned char> payload;

        if (!m_device.IsOpen()) {
            UpdateErrorStatus(L"Not connected.");
            return 0;
        }
        if (!m_txPanel.GetRowPayload(row, &payload, &error)) {
            UpdateErrorStatus(L"Row " + std::to_wstring(row + 1) + L" invalid: " + error);
            return 0;
        }
        if (!m_device.WriteReport(payload, &error)) {
            UpdateErrorStatus(error.empty() ? L"Write failed." : error);
            DisconnectUi(true);
            return 0;
        }
        m_txCount++;
        {
            size_t intended = 0;
            size_t actual = 0;
            m_device.GetLastWriteInfo(&intended, &actual);
            m_lastWriteInfo = L"len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual);
        }
        UpdateTxCount(m_txCount);
        m_rxPanel.LogTxLine(BuildTxLogBody(payload));
        return 0;
    }
    if (action == CTxPanel::ActionStart) {
        TxScheduler::Settings settings;
        if (!BuildSchedulerSettings(&settings, &error)) {
            UpdateErrorStatus(error);
            return 0;
        }
        m_scheduler.UpdateSettings(settings);
        m_scheduler.Start();
        m_txPanel.SetRunning(true);
        return 0;
    }
    if (action == CTxPanel::ActionStop) {
        m_scheduler.Stop();
        m_txPanel.SetRunning(false);
        return 0;
    }
    if (action == CTxPanel::ActionSingleShot) {
        TxScheduler::Settings settings;
        if (!BuildSchedulerSettings(&settings, &error)) {
            UpdateErrorStatus(error);
            return 0;
        }
        m_scheduler.UpdateSettings(settings);
        m_scheduler.SingleShot();
        return 0;
    }
    if (action == CTxPanel::ActionSettingsChanged) {
        if (m_scheduler.IsRunning()) {
            TxScheduler::Settings settings;
            if (BuildSchedulerSettings(&settings, &error)) {
                m_scheduler.UpdateSettings(settings);
            } else {
                UpdateErrorStatus(error);
            }
        }
        SaveConfig();
        SaveCommandConfig();
        return 0;
    }
    if (action == CTxPanel::ActionSaveAs) {
        CFileDialog dialog(FALSE, L"ini", L"TCPTestTool_commands.ini",
                           OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                           L"INI Files (*.ini)|*.ini|All Files (*.*)|*.*||",
                           this);
        if (dialog.DoModal() == IDOK) {
            SaveCommandConfigTo(std::wstring(dialog.GetPathName()));
        }
        return 0;
    }
    if (action == CTxPanel::ActionReload) {
        CFileDialog dialog(TRUE, L"ini", L"TCPTestTool_commands.ini",
                           OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                           L"INI Files (*.ini)|*.ini|All Files (*.*)|*.*||",
                           this);
        if (dialog.DoModal() == IDOK) {
            LoadCommandConfigFrom(std::wstring(dialog.GetPathName()));
        }
        return 0;
    }
    if (action == CTxPanel::ActionHover) {
        int hover_row = LOWORD(lParam);
        int hover_col = HIWORD(lParam);
        unsigned char value = 0;
        if (m_txPanel.GetByteValue(hover_row, hover_col, &value)) {
            wchar_t buffer[64] = {};
            swprintf_s(buffer, L"TX Row %d Byte[%d] = 0x%02X", hover_row + 1, hover_col, value);
            m_statusBar.SetPaneText(kPaneHover, buffer);
        }
        return 0;
    }
    if (action == CTxPanel::ActionHoverClear) {
        m_statusBar.SetPaneText(kPaneHover, L"");
    }
    return 0;
}

LRESULT CMainFrame::OnSchedulerEvent(WPARAM, LPARAM lParam) {
    std::unique_ptr<TxScheduler::EventPayload> payload(reinterpret_cast<TxScheduler::EventPayload*>(lParam));
    if (!payload) {
        return 0;
    }
    switch (payload->type) {
    case TxScheduler::EventError:
        UpdateErrorStatus(payload->text);
        break;
    case TxScheduler::EventTxCount:
        UpdateTxCount(payload->value);
        break;
    case TxScheduler::EventDeviceRemoved:
        UpdateErrorStatus(payload->text);
        DisconnectUi(true);
        break;
    case TxScheduler::EventWriteInfo:
        if (!payload->text.empty()) {
            m_lastWriteInfo = payload->text;
            UpdateTxCount(m_txCount);
        }
        break;
    case TxScheduler::EventTxLog:
        if (!payload->text.empty()) {
            m_rxPanel.LogTxLine(payload->text);
        }
        break;
    case TxScheduler::EventLimitUpdate: {
        unsigned int remaining = static_cast<unsigned int>(payload->value & 0xFFFFFFFFULL);
        unsigned int sent = static_cast<unsigned int>((payload->value >> 32) & 0xFFFFFFFFULL);
        m_txPanel.SetLimitStatus(static_cast<int>(sent), static_cast<int>(remaining));
        if (m_txPanel.IsLimitEnabled() && remaining == 0 && m_scheduler.IsRunning()) {
            m_scheduler.Stop();
            m_txPanel.SetRunning(false);
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

LRESULT CMainFrame::OnRxEvent(WPARAM wParam, LPARAM) {
    int event = static_cast<int>(wParam);
    if (event == CRxPanel::RxCount) {
        ++m_rxCount;
        UpdateRxCount(m_rxCount);
        m_lastRxTick = GetTickCount();
        UpdateRxActivity();
    } else if (event == CRxPanel::RxDeviceRemoved) {
        UpdateErrorStatus(L"Connection closed.");
        DisconnectUi(true);
    }
    return 0;
}

LRESULT CMainFrame::OnFirmwareAction(WPARAM wParam, LPARAM) {
    int action = static_cast<int>(wParam);
    if (action == CFirmwarePanel::ActionBrowse) {
        CFileDialog dialog(TRUE, L"bin", nullptr,
                           OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                           L"Binary Files (*.bin)|*.bin|All Files (*.*)|*.*||",
                           this);
        if (dialog.DoModal() == IDOK) {
            m_firmwarePanel.SetFilePath(std::wstring(dialog.GetPathName()));
            m_config.firmware_path = std::wstring(dialog.GetPathName());
            SaveConfig();
        }
        return 0;
    }
    if (action == CFirmwarePanel::ActionBurn) {
        RunFirmwareUpdate();
        return 0;
    }
    if (action == CFirmwarePanel::ActionEnterBoot) {
        RequestEnterBootloader();
        return 0;
    }
    if (action == CFirmwarePanel::ActionResetMcu) {
        RequestResetMcu();
        return 0;
    }
    return 0;
}

LRESULT CMainFrame::OnControlPollResult(WPARAM, LPARAM lParam) {
    std::unique_ptr<ControlPollResult> result(reinterpret_cast<ControlPollResult*>(lParam));

    if (!result) {
        return 0;
    }

    if (result->type == ControlPollResult::TypeUptimeSuccess) {
        m_uptimePollFailures = 0;
        m_deviceBar.SetUptimeText(FormatUptimeText(result->uptime_seconds));
        MarkControlTraffic();
        return 0;
    }

    if (result->type == ControlPollResult::TypeUptimeFailure) {
        ++m_uptimePollFailures;
        if (result->fatal_disconnect) {
            UpdateErrorStatus(L"Peer polling failed: " + result->error);
            DisconnectUi(true);
        }
        return 0;
    }

    if (result->type == ControlPollResult::TypeFunctionFailure) {
        UpdateErrorStatus(result->error.empty() ? L"Function command failed." : result->error);
        return 0;
    }

    if (result->type == ControlPollResult::TypeFunctionSuccess) {
        RecordFunctionTx(result->request_payload);
        m_functionPanel.SetPacketPreview(BytesToHex64(result->request_payload),
                                         BytesToHex64(result->response_payload));
        if (!result->request_payload.empty()) {
            m_functionPanel.SetFunctionRxPreview(result->request_payload[0],
                                                 BytesToHex64(result->response_payload));
        }
        if (m_activeTab == 0) {
            m_rxPanel.PushExternalPayload(result->response_payload);
        }
        ApplyFunctionTelemetry(result->response_payload);
        MarkControlTraffic();
    }
    return 0;
}

LRESULT CMainFrame::OnFunctionAction(WPARAM wParam, LPARAM) {
    int action = static_cast<int>(wParam);

    if (action == CFunctionPanel::ActionRefresh) {
        unsigned int packet_id = tcp_tool::kRawPktAdc;
        int active_sub_tab = m_functionPanel.GetActiveSubTab();

        if (!m_device.IsOpen() || m_peerMode != tcp_tool::kPeerApp) {
            UpdateErrorStatus(L"Function refresh requires APP connection.");
            return 0;
        }
        if (active_sub_tab == 1) {
            packet_id = tcp_tool::kRawPktGpio;
        } else if (active_sub_tab == 2) {
            packet_id = tcp_tool::kRawPktPwm;
        }
        m_pendingFunctionRefreshPacket.store(packet_id);
        m_pendingFunctionRefresh.store(true);
        SyncControlPollSettings();
        RequestImmediateControlPoll();
        UpdateInfoStatus(L"Function refresh requested.");
        return 0;
    }
    if (action == CFunctionPanel::ActionPollingChanged) {
        SaveConfig();
        SyncControlPollSettings();
        RequestImmediateControlPoll();
        return 0;
    }
    if (action == CFunctionPanel::ActionApplyGpio) {
        bool state = false;

        if (!m_device.IsOpen() || m_peerMode != tcp_tool::kPeerApp) {
            UpdateErrorStatus(L"GPIO update requires APP connection.");
            return 0;
        }
        if (!m_functionPanel.GetGpioOutputState(&state)) {
            UpdateErrorStatus(L"GPIO state read failed.");
            return 0;
        }
        m_pendingGpioState.store(state ? 1U : 0U);
        m_pendingGpioApply.store(true);
        SyncControlPollSettings();
        RequestImmediateControlPoll();
        UpdateInfoStatus(L"GPIO update requested.");
        return 0;
    }
    if (action == CFunctionPanel::ActionApplyPwm) {
        std::wstring error;
        std::uint32_t freq = 0;
        std::uint32_t duty = 0;

        if (!m_device.IsOpen() || m_peerMode != tcp_tool::kPeerApp) {
            UpdateErrorStatus(L"PWM update requires APP connection.");
            return 0;
        }
        if (!m_functionPanel.GetPwmSettings(&freq, &duty, &error)) {
            UpdateErrorStatus(error);
            return 0;
        }
        m_pendingPwmFreq.store(freq);
        m_pendingPwmDuty.store(duty);
        m_pendingPwmApply.store(true);
        SyncControlPollSettings();
        RequestImmediateControlPoll();
        UpdateInfoStatus(L"PWM update requested.");
        return 0;
    }
    return 0;
}

void CMainFrame::OnClose() {
    m_device.CancelPendingIo();
    StopStatusPollTimer();
    m_scheduler.Stop();
    m_rxPanel.StopCapture();
    m_device.Close();
    if (m_rxStatusTimer) {
        KillTimer(m_rxStatusTimer);
        m_rxStatusTimer = 0;
    }
    StopReconnectTimer();
    SaveConfig();
    SaveCommandConfig();
    CFrameWnd::OnClose();
}

void CMainFrame::InitStatusBar() {
    m_statusBar.SetIndicators(kIndicators, static_cast<int>(sizeof(kIndicators) / sizeof(kIndicators[0])));
    m_statusBar.SetPaneInfo(kPaneConnection, ID_SEPARATOR, SBPS_NORMAL, 0);
    m_statusBar.SetPaneInfo(kPaneError, ID_INDICATOR_ERR, SBPS_NORMAL, 0);
    m_statusBar.SetPaneInfo(kPaneTx, ID_INDICATOR_TX, SBPS_NORMAL, 0);
    m_statusBar.SetPaneInfo(kPaneRx, ID_INDICATOR_RX, SBPS_NORMAL, 0);
    m_statusBar.SetPaneInfo(kPaneHover, ID_INDICATOR_HOVER, SBPS_NORMAL, 0);
    LayoutStatusBarPanes();
    UpdateConnectionStatus(L"Disconnected");
    UpdateErrorStatus(L"");
    UpdateTxCount(0);
    UpdateRxCount(0);
    m_statusBar.SetPaneText(kPaneHover, L"");
    m_deviceBar.SetUptimeText(L"Unknown");
    m_deviceBar.SetLastEventText(L"Idle");
}

void CMainFrame::LayoutStatusBarPanes() {
    CRect rect;
    int width = 0;
    int conn = 200;
    int tx = 220;
    int rx = 90;
    int hover = 220;
    int err = 0;

    if (!m_statusBar.GetSafeHwnd()) {
        return;
    }
    m_statusBar.GetClientRect(&rect);
    width = rect.Width();
    if (width <= 0) {
        return;
    }

    err = width - (conn + tx + rx + hover);
    if (err < 180) {
        err = 180;
    }

    m_statusBar.SetPaneInfo(kPaneConnection, ID_SEPARATOR, SBPS_NORMAL, conn);
    m_statusBar.SetPaneInfo(kPaneError, ID_INDICATOR_ERR, SBPS_NORMAL, err);
    m_statusBar.SetPaneInfo(kPaneTx, ID_INDICATOR_TX, SBPS_NORMAL, tx);
    m_statusBar.SetPaneInfo(kPaneRx, ID_INDICATOR_RX, SBPS_NORMAL, rx);
    m_statusBar.SetPaneInfo(kPaneHover, ID_INDICATOR_HOVER, SBPS_NORMAL, hover);
}

void CMainFrame::UpdateConnectionStatus(const std::wstring& text) {
    m_statusBar.SetPaneText(kPaneConnection, (L"Conn: " + text).c_str());
}

void CMainFrame::UpdateErrorStatus(const std::wstring& text) {
    if (text.empty()) {
        m_statusBar.SetPaneText(kPaneError, L"");
        return;
    }
    m_statusBar.SetPaneText(kPaneError, (L"Err: " + text).c_str());
    m_deviceBar.SetLastEventText(text);
}

void CMainFrame::UpdateInfoStatus(const std::wstring& text) {
    if (text.empty()) {
        m_statusBar.SetPaneText(kPaneError, L"");
        return;
    }
    m_statusBar.SetPaneText(kPaneError, (L"Info: " + text).c_str());
    m_deviceBar.SetLastEventText(text);
}

void CMainFrame::UpdateTxCount(unsigned long long count) {
    std::wstring text = L"TX: " + std::to_wstring(count);
    m_txCount = count;
    if (!m_lastWriteInfo.empty()) {
        text += L" (" + m_lastWriteInfo + L")";
    }
    m_statusBar.SetPaneText(kPaneTx, text.c_str());
}

void CMainFrame::UpdateRxCount(unsigned long long count) {
    m_statusBar.SetPaneText(kPaneRx, (L"RX: " + std::to_wstring(count)).c_str());
}

void CMainFrame::UpdateRxActivity() {
    DWORD now = GetTickCount();
    DWORD elapsed = 0;

    if (!m_device.IsOpen()) {
        m_deviceBar.SetConnected(false);
        m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
        m_rxPanel.SetRxState(CRxPanel::RxIdle);
        UpdateConnectionStatus(L"Disconnected");
        return;
    }

    if (m_lastRxTick == 0) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
        m_rxPanel.SetRxState(CRxPanel::RxIdle);
        UpdateConnectionStatus(L"Connected");
        return;
    }

    elapsed = now - m_lastRxTick;
    if (elapsed <= 100) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateRxActive);
        m_rxPanel.SetRxState(CRxPanel::RxReceiving);
        UpdateConnectionStatus(L"RX Active");
    } else if (elapsed > 1000) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
        m_rxPanel.SetRxState(CRxPanel::RxTimeout);
        UpdateConnectionStatus(L"Connected");
    } else {
        m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
        m_rxPanel.SetRxState(CRxPanel::RxIdle);
        UpdateConnectionStatus(L"Connected");
    }
}

void CMainFrame::MarkControlTraffic() {
    if (!m_device.IsOpen()) {
        return;
    }
    m_lastRxTick = GetTickCount();
    UpdateRxActivity();
}

void CMainFrame::StartStatusPollTimer() {
    if (m_controlPollThread.joinable()) {
        return;
    }
    m_controlPollStopRequested.store(false);
    m_forceStatusPoll.store(true);
    m_controlPollThread = std::thread(&CMainFrame::ControlPollThreadLoop, this);
}

void CMainFrame::StopStatusPollTimer() {
    m_controlPollStopRequested.store(true);
    if (m_controlPollThread.joinable()) {
        m_controlPollThread.join();
    }
}

void CMainFrame::ControlPollThreadLoop() {
    DWORD next_uptime_poll_tick = 0;
    DWORD next_function_poll_tick = 0;
    int uptime_failures = 0;
    unsigned int next_function_index = 0;

    while (!m_controlPollStopRequested.load()) {
        DWORD now = 0;
        int peer_mode = 0;
        bool poll_adc = false;
        bool poll_gpio = false;
        bool poll_pwm = false;

        if (!m_device.IsOpen()) {
            uptime_failures = 0;
            next_function_index = 0;
            next_uptime_poll_tick = 0;
            next_function_poll_tick = 0;
            Sleep(100);
            continue;
        }

        now = GetTickCount();
        if (m_forceStatusPoll.load()) {
            next_uptime_poll_tick = 0;
            next_function_poll_tick = 0;
            m_forceStatusPoll.store(false);
        }
        peer_mode = m_pollPeerMode.load();
        poll_adc = m_pollAdcEnabled.load();
        poll_gpio = m_pollGpioEnabled.load();
        poll_pwm = m_pollPwmEnabled.load();

        if (peer_mode == static_cast<int>(tcp_tool::kPeerApp)) {
            bool did_work = false;

            if (m_pendingGpioApply.exchange(false)) {
                unsigned char request_status = tcp_tool::kStatusError;
                std::vector<unsigned char> request(tcp_tool::kRawPayloadSize, 0x00);
                std::vector<unsigned char> response;
                std::wstring request_error;
                ControlPollResult* result = new ControlPollResult();

                request[0] = tcp_tool::kRawPktGpio;
                request[1] = tcp_tool::kRawIndicatorWrite;
                request[2] = static_cast<unsigned char>(m_pendingGpioState.load() ? 1U : 0U);
                if (m_device.ExecuteCommand(tcp_tool::kCmdRaw64, request, &request_status, &response, kFunctionPollTimeoutMs, &request_error) &&
                    request_status == tcp_tool::kStatusOk &&
                    response.size() >= tcp_tool::kRawPayloadSize) {
                    result->type = ControlPollResult::TypeFunctionSuccess;
                    result->packet_id = request[0];
                    result->request_payload = request;
                    result->response_payload = response;
                } else {
                    result->type = ControlPollResult::TypeFunctionFailure;
                    if (!request_error.empty()) {
                        result->error = request_error;
                    } else if (request_status != tcp_tool::kStatusOk) {
                        result->error = L"GPIO update rejected: " + tcp_tool::StatusToText(request_status);
                    } else {
                        result->error = L"GPIO update response is invalid.";
                    }
                }
                if (::IsWindow(m_hWnd)) {
                    PostMessage(kControlPollMessageId, 0, reinterpret_cast<LPARAM>(result));
                } else {
                    delete result;
                }
                did_work = true;
            }

            if (m_pendingPwmApply.exchange(false)) {
                unsigned char request_status = tcp_tool::kStatusError;
                std::uint32_t freq = m_pendingPwmFreq.load();
                std::uint32_t duty = m_pendingPwmDuty.load();
                std::vector<unsigned char> request(tcp_tool::kRawPayloadSize, 0x00);
                std::vector<unsigned char> response;
                std::wstring request_error;
                ControlPollResult* result = new ControlPollResult();

                request[0] = tcp_tool::kRawPktPwm;
                request[1] = tcp_tool::kRawIndicatorWrite;
                request[2] = static_cast<unsigned char>(freq & 0xFFU);
                request[3] = static_cast<unsigned char>((freq >> 8U) & 0xFFU);
                request[4] = static_cast<unsigned char>((freq >> 16U) & 0xFFU);
                request[5] = static_cast<unsigned char>(duty & 0xFFU);
                if (m_device.ExecuteCommand(tcp_tool::kCmdRaw64, request, &request_status, &response, kFunctionPollTimeoutMs, &request_error) &&
                    request_status == tcp_tool::kStatusOk &&
                    response.size() >= tcp_tool::kRawPayloadSize) {
                    result->type = ControlPollResult::TypeFunctionSuccess;
                    result->packet_id = request[0];
                    result->request_payload = request;
                    result->response_payload = response;
                } else {
                    result->type = ControlPollResult::TypeFunctionFailure;
                    if (!request_error.empty()) {
                        result->error = request_error;
                    } else if (request_status != tcp_tool::kStatusOk) {
                        result->error = L"PWM update rejected: " + tcp_tool::StatusToText(request_status);
                    } else {
                        result->error = L"PWM update response is invalid.";
                    }
                }
                if (::IsWindow(m_hWnd)) {
                    PostMessage(kControlPollMessageId, 0, reinterpret_cast<LPARAM>(result));
                } else {
                    delete result;
                }
                did_work = true;
            }

            if (m_pendingFunctionRefresh.exchange(false)) {
                unsigned char request_status = tcp_tool::kStatusError;
                unsigned int packet_id = m_pendingFunctionRefreshPacket.load();
                std::vector<unsigned char> request(tcp_tool::kRawPayloadSize, 0x00);
                std::vector<unsigned char> response;
                std::wstring request_error;
                ControlPollResult* result = new ControlPollResult();

                if (packet_id != tcp_tool::kRawPktGpio && packet_id != tcp_tool::kRawPktPwm) {
                    packet_id = tcp_tool::kRawPktAdc;
                }
                request[0] = static_cast<unsigned char>(packet_id);
                request[1] = tcp_tool::kRawIndicatorRead;
                if (m_device.ExecuteCommand(tcp_tool::kCmdRaw64, request, &request_status, &response, kFunctionPollTimeoutMs, &request_error) &&
                    request_status == tcp_tool::kStatusOk &&
                    response.size() >= tcp_tool::kRawPayloadSize) {
                    result->type = ControlPollResult::TypeFunctionSuccess;
                    result->packet_id = request[0];
                    result->request_payload = request;
                    result->response_payload = response;
                } else {
                    result->type = ControlPollResult::TypeFunctionFailure;
                    if (!request_error.empty()) {
                        result->error = request_error;
                    } else if (request_status != tcp_tool::kStatusOk) {
                        result->error = L"Function refresh rejected: " + tcp_tool::StatusToText(request_status);
                    } else {
                        result->error = L"Function refresh response is invalid.";
                    }
                }
                if (::IsWindow(m_hWnd)) {
                    PostMessage(kControlPollMessageId, 0, reinterpret_cast<LPARAM>(result));
                } else {
                    delete result;
                }
                did_work = true;
            }

            if ((next_uptime_poll_tick == 0U) || (now >= next_uptime_poll_tick)) {
                unsigned char status = tcp_tool::kStatusError;
                std::vector<unsigned char> payload;
                std::wstring error;
                tcp_tool::UptimePayload uptime;

                next_uptime_poll_tick = now + kStatusPollIntervalMs;
                if (!m_device.ExecuteCommand(tcp_tool::kCmdGetUptime, std::vector<unsigned char>(),
                                             &status, &payload, kUptimePollTimeoutMs, &error)) {
                    ++uptime_failures;
                    if (uptime_failures >= 3) {
                        ControlPollResult* result = new ControlPollResult();
                        result->type = ControlPollResult::TypeUptimeFailure;
                        result->error = error;
                        result->fatal_disconnect = true;
                        if (::IsWindow(m_hWnd)) {
                            PostMessage(kControlPollMessageId, 0, reinterpret_cast<LPARAM>(result));
                        } else {
                            delete result;
                        }
                        uptime_failures = 0;
                    }
                } else if (status == tcp_tool::kStatusOk && tcp_tool::TryParseUptimePayload(payload, &uptime)) {
                    ControlPollResult* result = new ControlPollResult();
                    result->type = ControlPollResult::TypeUptimeSuccess;
                    result->uptime_seconds = uptime.uptime_seconds;
                    if (::IsWindow(m_hWnd)) {
                        PostMessage(kControlPollMessageId, 0, reinterpret_cast<LPARAM>(result));
                    } else {
                        delete result;
                    }
                    uptime_failures = 0;
                } else {
                    ++uptime_failures;
                }
                did_work = true;
            }

            if ((poll_adc || poll_gpio || poll_pwm) &&
                ((next_function_poll_tick == 0U) || (now >= next_function_poll_tick))) {
                unsigned int attempt = 0;
                bool enabled[3];

                next_function_poll_tick = now + kFunctionPollIntervalMs;
                enabled[0] = poll_adc;
                enabled[1] = poll_gpio;
                enabled[2] = poll_pwm;

                for (attempt = 0; attempt < 3U; ++attempt) {
                    unsigned int index = (next_function_index + attempt) % 3U;
                    std::vector<unsigned char> request(tcp_tool::kRawPayloadSize, 0x00);
                    std::vector<unsigned char> response;
                    std::wstring request_error;
                    unsigned char request_status = tcp_tool::kStatusError;

                    if (!enabled[index]) {
                        continue;
                    }

                    if (index == 0U) {
                        request[0] = tcp_tool::kRawPktAdc;
                    } else if (index == 1U) {
                        request[0] = tcp_tool::kRawPktGpio;
                    } else {
                        request[0] = tcp_tool::kRawPktPwm;
                    }
                    request[1] = tcp_tool::kRawIndicatorRead;
                    next_function_index = (index + 1U) % 3U;

                    if (m_device.ExecuteCommand(tcp_tool::kCmdRaw64, request, &request_status, &response, kFunctionPollTimeoutMs, &request_error) &&
                        request_status == tcp_tool::kStatusOk &&
                        response.size() >= tcp_tool::kRawPayloadSize) {
                        ControlPollResult* result = new ControlPollResult();
                        result->type = ControlPollResult::TypeFunctionSuccess;
                        result->packet_id = request[0];
                        result->request_payload = request;
                        result->response_payload = response;
                        if (::IsWindow(m_hWnd)) {
                            PostMessage(kControlPollMessageId, 0, reinterpret_cast<LPARAM>(result));
                        } else {
                            delete result;
                        }
                    }
                    break;
                }
                did_work = true;
            }

            if (!did_work) {
                Sleep(20);
            }
        } else {
            uptime_failures = 0;
            next_uptime_poll_tick = 0;
            next_function_poll_tick = 0;
            Sleep(50);
        }
    }
}

void CMainFrame::SyncControlPollSettings() {
    bool poll_adc = false;
    bool poll_gpio = false;
    bool poll_pwm = false;

    m_functionPanel.GetPollingEnabled(&poll_adc, &poll_gpio, &poll_pwm);
    m_pollAdcEnabled.store(poll_adc);
    m_pollGpioEnabled.store(poll_gpio);
    m_pollPwmEnabled.store(poll_pwm);
    m_pollActiveTab.store(m_activeTab);
    m_pollPeerMode.store(static_cast<int>(m_peerMode));
}

void CMainFrame::RequestImmediateControlPoll() {
    m_forceStatusPoll.store(true);
}

void CMainFrame::StartReconnectTimer() {
    if (!m_reconnectTimer) {
        m_reconnectTimer = SetTimer(kReconnectTimerId, 1000, nullptr);
    }
}

void CMainFrame::StopReconnectTimer() {
    if (m_reconnectTimer) {
        KillTimer(m_reconnectTimer);
        m_reconnectTimer = 0;
    }
}

bool CMainFrame::ConnectEndpoint() {
    HidScanInfo info;
    std::wstring error;
    std::wstring endpoint_text;

    info.host = m_deviceBar.GetHost();
    info.port = m_deviceBar.GetPort();
    info.input_report_length = static_cast<unsigned short>(tcp_tool::kRawPayloadSize);
    info.output_report_length = static_cast<unsigned short>(tcp_tool::kRawPayloadSize);
    endpoint_text = info.host + L":" + std::to_wstring(info.port);

    m_rxPanel.StopCapture();
    m_device.Close();
    m_deviceBar.SetConnected(false);
    m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
    m_deviceBar.SetLastEventText(L"Connecting to " + endpoint_text + L"...");
    if (!m_device.Open(info, &error)) {
        UpdateErrorStatus(error.empty() ? L"Connect failed." : error);
        UpdateConnectionStatus(L"Disconnected");
        m_deviceBar.SetConnected(false);
        m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
        return false;
    }

    m_lastRxTick = 0;
    UpdateConnectionStatus(L"Connected");
    UpdateErrorStatus(L"");
    m_deviceBar.SetConnected(true);
    m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
    m_txPanel.SetSchedulerEnabled(true);
    m_txPanel.SetRunning(false);
    m_txPanel.SetControlsEnabled(true);
    m_rxPanel.SetControlsEnabled(true);
    m_rxPanel.SetClearEnabled(false);
    m_functionPanel.SetControlsEnabled(false);
    m_firmwarePanel.SetControlsEnabled(true);
    StopReconnectTimer();
    m_uptimePollFailures = 0;
    m_nextFunctionPollIndex = 0;
    SyncControlPollSettings();
    RequestImmediateControlPoll();
    m_deviceBar.SetLastEventText(L"TCP connected to " + endpoint_text + L". Reading peer info...");

    if (!RefreshPeerInfo()) {
        UpdateErrorStatus(L"Connected, but GET_INFO failed.");
        if (!m_device.IsOpen()) {
            DisconnectUi(true);
            return false;
        }
    } else {
        MarkControlTraffic();
    }
    m_rxPanel.StartCapture();
    return true;
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kRxTimerId) {
        UpdateRxActivity();
    } else if (nIDEvent == kReconnectTimerId) {
        if (m_device.IsOpen()) {
            StopReconnectTimer();
        } else if (m_deviceBar.IsAutoReconnectEnabled()) {
            ConnectEndpoint();
        } else {
            StopReconnectTimer();
        }
    }
    CFrameWnd::OnTimer(nIDEvent);
}

void CMainFrame::OnTabChanged(NMHDR*, LRESULT* pResult) {
    CRect rect;
    m_activeTab = m_tab.GetCurSel();
    ShowActivePage();
    if (m_activeTab == 1 && m_device.IsOpen() && m_peerMode == tcp_tool::kPeerApp) {
        SyncControlPollSettings();
        RequestImmediateControlPoll();
    } else {
        SyncControlPollSettings();
    }
    GetClientRect(&rect);
    LayoutControls(rect.Width(), rect.Height());
    if (pResult) {
        *pResult = 0;
    }
}

bool CMainFrame::BuildSchedulerSettings(TxScheduler::Settings* out_settings, std::wstring* error) {
    TxScheduler::Settings settings;
    int i = 0;

    if (!out_settings) {
        return false;
    }
    settings.interval_enabled = m_txPanel.IsIntervalEnabled();
    settings.interval_ms = m_txPanel.GetIntervalValue();
    settings.limit_enabled = m_txPanel.IsLimitEnabled();
    settings.limit_count = m_txPanel.GetLimitCount();

    for (i = 0; i < 6; ++i) {
        settings.rows[i].enabled = m_txPanel.IsRowEnabled(i);
        if (settings.rows[i].enabled) {
            std::vector<unsigned char> payload;
            std::wstring parse_error;
            if (!m_txPanel.GetRowPayload(i, &payload, &parse_error)) {
                if (error) {
                    *error = L"Row " + std::to_wstring(i + 1) + L" invalid: " + parse_error;
                }
                return false;
            }
            settings.rows[i].payload = payload;
        }
    }

    *out_settings = settings;
    return true;
}

bool CMainFrame::RefreshPeerInfo() {
    unsigned char status = tcp_tool::kStatusError;
    std::vector<unsigned char> payload;
    std::wstring error;
    std::wstring peer_text;
    std::wstring uptime_text = L"Unknown";
    std::wstring uptime_summary = L"unknown";
    tcp_tool::GetInfoPayload info;

    if (!m_device.IsOpen()) {
        return false;
    }

    if (!m_device.ExecuteCommand(tcp_tool::kCmdGetInfo, std::vector<unsigned char>(),
                                 &status, &payload, kCommandTimeoutMs, &error)) {
        UpdateErrorStatus(error);
        return false;
    }
    if (status != tcp_tool::kStatusOk || payload.size() < 2) {
        UpdateErrorStatus(L"GET_INFO rejected.");
        return false;
    }

    if (payload[0] == tcp_tool::kPeerBoot) {
        m_peerMode = tcp_tool::kPeerBoot;
    } else if (payload[0] == tcp_tool::kPeerApp) {
        m_peerMode = tcp_tool::kPeerApp;
    } else {
        m_peerMode = tcp_tool::kPeerUnknown;
    }

    SetPeerModeText();
    MarkControlTraffic();
    peer_text = (m_peerMode == tcp_tool::kPeerBoot) ? L"BOOT" :
                (m_peerMode == tcp_tool::kPeerApp) ? L"APP" : L"UNKNOWN";
    if (!tcp_tool::TryParseGetInfoPayload(payload, &info)) {
        UpdateErrorStatus(L"GET_INFO payload invalid.");
        return false;
    }
    if (m_peerMode == tcp_tool::kPeerApp) {
        uptime_text = FormatUptimeText(info.uptime_seconds);
        uptime_summary = std::to_wstring(info.uptime_seconds) + L"s";
    } else if (m_peerMode == tcp_tool::kPeerBoot) {
        uptime_text = L"N/A (BOOT)";
        uptime_summary = L"N/A";
    }
    m_deviceBar.SetUptimeText(uptime_text);
    m_deviceBar.SetLastEventText(L"GET_INFO OK. Peer=" + peer_text +
                                 L", AppStart=0x" + FormatHex32(info.app_start_addr) +
                                 L", AppSize=0x" + FormatHex32(info.app_size) +
                                 L", Chunk=" + std::to_wstring(info.max_chunk_size) +
                                 L", Uptime=" + uptime_summary);
    return true;
}

bool CMainFrame::SendPing() {
    unsigned char status = tcp_tool::kStatusError;
    std::vector<unsigned char> payload;
    std::wstring error;
    std::wstring pong;

    if (!m_device.IsOpen()) {
        UpdateErrorStatus(L"Connect first.");
        return false;
    }

    m_rxPanel.StopCapture();
    if (!m_device.ExecuteCommand(tcp_tool::kCmdPing, std::vector<unsigned char>(),
                                 &status, &payload, kCommandTimeoutMs, &error)) {
        UpdateErrorStatus(error);
        m_rxPanel.StartCapture();
        return false;
    }

    m_rxPanel.StartCapture();
    if (status != tcp_tool::kStatusOk) {
        UpdateErrorStatus(L"Ping failed: " + tcp_tool::StatusToText(status));
        return false;
    }

    pong.assign(payload.begin(), payload.end());
    if (m_peerMode == tcp_tool::kPeerApp) {
        PollPeerUptime(true);
    }
    MarkControlTraffic();
    UpdateInfoStatus(L"Ping OK: " + pong);
    return true;
}

bool CMainFrame::PollPeerUptime(bool silent) {
    unsigned char status = tcp_tool::kStatusError;
    std::vector<unsigned char> payload;
    std::wstring error;
    tcp_tool::UptimePayload uptime;

    if (!m_device.IsOpen()) {
        return false;
    }
    if (m_peerMode != tcp_tool::kPeerApp) {
        m_uptimePollFailures = 0;
        return false;
    }
    if (!m_device.ExecuteCommand(tcp_tool::kCmdGetUptime, std::vector<unsigned char>(),
                                 &status, &payload, kUptimePollTimeoutMs, &error)) {
        ++m_uptimePollFailures;
        if (!silent) {
            UpdateErrorStatus(error);
        } else if (m_uptimePollFailures >= 3) {
            UpdateErrorStatus(L"Peer polling failed: " + error);
            DisconnectUi(true);
        }
        return false;
    }
    if (status != tcp_tool::kStatusOk || !tcp_tool::TryParseUptimePayload(payload, &uptime)) {
        ++m_uptimePollFailures;
        if (!silent) {
            UpdateErrorStatus(L"GET_UPTIME rejected.");
        }
        return false;
    }
    m_uptimePollFailures = 0;
    m_deviceBar.SetUptimeText(FormatUptimeText(uptime.uptime_seconds));
    MarkControlTraffic();
    return true;
}

bool CMainFrame::SendFunctionPacket(const std::vector<unsigned char>& request_payload,
                                    std::vector<unsigned char>* response_payload,
                                    std::wstring* error,
                                    DWORD timeout_ms) {
    unsigned char status = tcp_tool::kStatusError;
    std::vector<unsigned char> response;

    if (!m_device.IsOpen()) {
        if (error) {
            *error = L"Connect first.";
        }
        return false;
    }
    if (m_peerMode != tcp_tool::kPeerApp) {
        if (error) {
            *error = L"Function packets require APP mode.";
        }
        return false;
    }
    if (!m_device.ExecuteCommand(tcp_tool::kCmdRaw64, request_payload, &status, &response, timeout_ms, error)) {
        return false;
    }
    RecordFunctionTx(request_payload);
    if (status != tcp_tool::kStatusOk) {
        if (error) {
            *error = L"RAW64 rejected: " + tcp_tool::StatusToText(status);
        }
        return false;
    }
    if (response.size() < tcp_tool::kRawPayloadSize) {
        if (error) {
            *error = L"RAW64 response length invalid.";
        }
        return false;
    }
    if (response_payload) {
        *response_payload = response;
    }
    m_functionPanel.SetPacketPreview(BytesToHex64(request_payload), BytesToHex64(response));
    if (!request_payload.empty()) {
        m_functionPanel.SetFunctionRxPreview(request_payload[0], BytesToHex64(response));
    }
    if (m_activeTab == 0) {
        m_rxPanel.PushExternalPayload(response);
    }
    MarkControlTraffic();
    return true;
}

void CMainFrame::RecordFunctionTx(const std::vector<unsigned char>& payload) {
    size_t intended = 0;
    size_t actual = 0;

    ++m_txCount;
    m_device.GetLastWriteInfo(&intended, &actual);
    m_lastWriteInfo = L"len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual);
    UpdateTxCount(m_txCount);
    m_rxPanel.LogTxLine(BytesToHex64(payload));
}

bool CMainFrame::ApplyFunctionTelemetry(const std::vector<unsigned char>& payload) {
    unsigned char packet_id = 0;
    std::uint32_t adc0_mv = 0;
    std::uint32_t adc1_mv = 0;
    std::uint32_t vref_mv = 3300;
    std::uint16_t adc_bg_raw = 0;
    std::uint16_t adc0_raw = 0;
    std::uint16_t adc1_raw = 0;
    unsigned char indicator = 0;
    unsigned char gpio_in = 0;
    unsigned char gpio_out = 0;
    std::uint32_t pwm_freq = 0;
    std::uint32_t pwm_duty = 0;
    bool update_pwm_controls = false;

    if (payload.size() < tcp_tool::kRawPayloadSize) {
        return false;
    }
    packet_id = payload[0];
    indicator = payload[1];

    switch (packet_id) {
    case tcp_tool::kRawPktAdc:
        adc_bg_raw = tcp_tool::ReadUint16Le(&payload[2]);
        adc0_raw = tcp_tool::ReadUint16Le(&payload[4]);
        adc1_raw = tcp_tool::ReadUint16Le(&payload[6]);
        if (adc_bg_raw != 0U) {
            vref_mv = (tcp_tool::kAdcBandgapTypicalMv * tcp_tool::kAdcFullScaleCounts) / adc_bg_raw;
        }
        adc0_mv = (static_cast<std::uint32_t>(adc0_raw) * vref_mv) / tcp_tool::kAdcFullScaleCounts;
        adc1_mv = (static_cast<std::uint32_t>(adc1_raw) * vref_mv) / tcp_tool::kAdcFullScaleCounts;
        m_functionPanel.SetAdcTelemetry(adc0_mv, adc1_mv, vref_mv, adc0_raw, adc1_raw, adc_bg_raw);
        break;
    case tcp_tool::kRawPktGpio:
        if (indicator == tcp_tool::kRawIndicatorRead) {
            gpio_in = payload[2];
            m_functionPanel.SetGpioInputTelemetry(gpio_in);
        } else if (indicator == tcp_tool::kRawIndicatorWrite) {
            gpio_out = payload[2];
            m_functionPanel.SetGpioOutputTelemetry(gpio_out);
        } else {
            return false;
        }
        break;
    case tcp_tool::kRawPktPwm:
        pwm_freq = tcp_tool::ReadUint24Le(&payload[2]);
        pwm_duty = static_cast<std::uint32_t>(payload[5]);
        update_pwm_controls = (indicator == tcp_tool::kRawIndicatorWrite);
        m_functionPanel.SetPwmTelemetry(pwm_freq, pwm_duty, update_pwm_controls);
        break;
    default:
        return false;
    }
    return true;
}

bool CMainFrame::RefreshFunctionTelemetry(bool silent) {
    return PollFunctionPackets(silent);
}

bool CMainFrame::RefreshCurrentFunctionTab(bool silent) {
    std::vector<unsigned char> request(tcp_tool::kRawPayloadSize, 0x00);
    std::vector<unsigned char> response;
    std::wstring error;
    int active_sub_tab = m_functionPanel.GetActiveSubTab();

    if (active_sub_tab == 0) {
        request[0] = tcp_tool::kRawPktAdc;
    } else if (active_sub_tab == 1) {
        request[0] = tcp_tool::kRawPktGpio;
    } else {
        request[0] = tcp_tool::kRawPktPwm;
    }
    request[1] = tcp_tool::kRawIndicatorRead;

    if (!SendFunctionPacket(request, &response, &error, 600)) {
        if (!silent) {
            UpdateErrorStatus(error);
        }
        return false;
    }
    if (!ApplyFunctionTelemetry(response)) {
        if (!silent) {
            UpdateErrorStatus(L"Function response parse failed.");
        }
        return false;
    }
    if (!silent) {
        UpdateInfoStatus(L"Function refresh complete.");
    }
    return true;
}

bool CMainFrame::PollFunctionPackets(bool silent) {
    std::vector<unsigned char> request;
    std::vector<unsigned char> response;
    std::wstring error;
    bool poll_adc = false;
    bool poll_gpio = false;
    bool poll_pwm = false;
    bool any_enabled = false;
    bool any_success = false;

    m_functionPanel.GetPollingEnabled(&poll_adc, &poll_gpio, &poll_pwm);
    any_enabled = poll_adc || poll_gpio || poll_pwm;
    if (!any_enabled) {
        if (!silent) {
            UpdateErrorStatus(L"Enable at least one function polling checkbox.");
        }
        return false;
    }

    if (poll_adc) {
        request.assign(tcp_tool::kRawPayloadSize, 0x00);
        request[0] = tcp_tool::kRawPktAdc;
        request[1] = tcp_tool::kRawIndicatorRead;
        if (!SendFunctionPacket(request, &response, &error, 600)) {
            if (!silent) {
                UpdateErrorStatus(error);
            }
            return false;
        }
        if (!ApplyFunctionTelemetry(response)) {
            if (!silent) {
                UpdateErrorStatus(L"ADC response parse failed.");
            }
            return false;
        }
        any_success = true;
    }

    if (poll_gpio) {
        request.assign(tcp_tool::kRawPayloadSize, 0x00);
        request[0] = tcp_tool::kRawPktGpio;
        request[1] = tcp_tool::kRawIndicatorRead;
        if (!SendFunctionPacket(request, &response, &error, 600)) {
            if (!silent) {
                UpdateErrorStatus(error);
            }
            return false;
        }
        if (!ApplyFunctionTelemetry(response)) {
            if (!silent) {
                UpdateErrorStatus(L"GPIO response parse failed.");
            }
            return false;
        }
        any_success = true;
    }

    if (poll_pwm) {
        request.assign(tcp_tool::kRawPayloadSize, 0x00);
        request[0] = tcp_tool::kRawPktPwm;
        request[1] = tcp_tool::kRawIndicatorRead;
        if (!SendFunctionPacket(request, &response, &error, 600)) {
            if (!silent) {
                UpdateErrorStatus(error);
            }
            return false;
        }
        if (!ApplyFunctionTelemetry(response)) {
            if (!silent) {
                UpdateErrorStatus(L"PWM response parse failed.");
            }
            return false;
        }
        any_success = true;
    }

    if (!silent && any_success) {
        UpdateInfoStatus(L"Function polling refresh complete.");
    }
    return any_success;
}

bool CMainFrame::PollOneFunctionPacket(bool silent) {
    std::vector<unsigned char> request;
    std::vector<unsigned char> response;
    std::wstring error;
    bool poll_adc = false;
    bool poll_gpio = false;
    bool poll_pwm = false;
    bool enabled[3];
    unsigned int attempt = 0;
    unsigned int index = 0;

    m_functionPanel.GetPollingEnabled(&poll_adc, &poll_gpio, &poll_pwm);
    enabled[0] = poll_adc;
    enabled[1] = poll_gpio;
    enabled[2] = poll_pwm;
    if (!enabled[0] && !enabled[1] && !enabled[2]) {
        return false;
    }

    for (attempt = 0; attempt < 3U; ++attempt) {
        index = (m_nextFunctionPollIndex + attempt) % 3U;
        if (!enabled[index]) {
            continue;
        }

        request.assign(tcp_tool::kRawPayloadSize, 0x00);
        if (index == 0U) {
            request[0] = tcp_tool::kRawPktAdc;
        } else if (index == 1U) {
            request[0] = tcp_tool::kRawPktGpio;
        } else {
            request[0] = tcp_tool::kRawPktPwm;
        }
        request[1] = tcp_tool::kRawIndicatorRead;

        m_nextFunctionPollIndex = (index + 1U) % 3U;
        if (!SendFunctionPacket(request, &response, &error, 600)) {
            if (!silent) {
                UpdateErrorStatus(error);
            }
            return false;
        }
        if (!ApplyFunctionTelemetry(response)) {
            if (!silent) {
                if (index == 0U) {
                    UpdateErrorStatus(L"ADC response parse failed.");
                } else if (index == 1U) {
                    UpdateErrorStatus(L"GPIO response parse failed.");
                } else {
                    UpdateErrorStatus(L"PWM response parse failed.");
                }
            }
            return false;
        }
        return true;
    }

    return false;
}

bool CMainFrame::ApplyFunctionGpio() {
    std::vector<unsigned char> request(tcp_tool::kRawPayloadSize, 0x00);
    std::vector<unsigned char> response;
    std::wstring error;
    bool state = false;

    if (!m_functionPanel.GetGpioOutputState(&state)) {
        UpdateErrorStatus(L"GPIO state read failed.");
        return false;
    }
    request[0] = tcp_tool::kRawPktGpio;
    request[1] = tcp_tool::kRawIndicatorWrite;
    request[2] = state ? 1U : 0U;
    if (!SendFunctionPacket(request, &response, &error)) {
        UpdateErrorStatus(error);
        return false;
    }
    if (!ApplyFunctionTelemetry(response)) {
        UpdateErrorStatus(L"GPIO response parse failed.");
        return false;
    }
    RequestImmediateControlPoll();
    UpdateInfoStatus(L"GPIO output updated.");
    return true;
}

bool CMainFrame::ApplyFunctionPwm() {
    std::vector<unsigned char> request(tcp_tool::kRawPayloadSize, 0x00);
    std::vector<unsigned char> response;
    std::wstring error;
    std::uint32_t freq = 0;
    std::uint32_t duty = 0;

    if (!m_functionPanel.GetPwmSettings(&freq, &duty, &error)) {
        UpdateErrorStatus(error);
        return false;
    }
    request[0] = tcp_tool::kRawPktPwm;
    request[1] = tcp_tool::kRawIndicatorWrite;
    request[2] = static_cast<unsigned char>(freq & 0xFFU);
    request[3] = static_cast<unsigned char>((freq >> 8U) & 0xFFU);
    request[4] = static_cast<unsigned char>((freq >> 16U) & 0xFFU);
    request[5] = static_cast<unsigned char>(duty & 0xFFU);
    if (!SendFunctionPacket(request, &response, &error)) {
        UpdateErrorStatus(error);
        return false;
    }
    if (!ApplyFunctionTelemetry(response)) {
        UpdateErrorStatus(L"PWM response parse failed.");
        return false;
    }
    RequestImmediateControlPoll();
    UpdateInfoStatus(L"PWM updated.");
    return true;
}

bool CMainFrame::RunFirmwareUpdate() {
    FirmwareFailureInfo failure_info;
    std::vector<unsigned char> image;
    std::vector<unsigned char> request;
    std::vector<unsigned char> response;
    std::wstring error;
    unsigned char status = tcp_tool::kStatusError;
    std::wstring path = m_firmwarePanel.GetFilePath();
    std::uint32_t image_crc = 0;
    std::uint32_t chunk_size = 1024;
    std::size_t offset = 0;
    int percent = 0;
    DWORD attempt = 0;

    if (!m_device.IsOpen()) {
        UpdateErrorStatus(L"Connect first.");
        return false;
    }

    if (m_peerMode != tcp_tool::kPeerBoot) {
        if (!RequestEnterBootloader()) {
            return false;
        }
        for (attempt = 0; attempt < kReconnectAttempts; ++attempt) {
            Sleep(kReconnectDelayMs);
            PumpUi();
            if (!m_device.IsOpen()) {
                ConnectEndpoint();
            }
            if (m_device.IsOpen() && m_peerMode == tcp_tool::kPeerBoot) {
                break;
            }
        }
        if (m_peerMode != tcp_tool::kPeerBoot) {
            failure_info.summary = L"Failed to reconnect to bootloader.";
            failure_info.image_size = image.size();
            failure_info.has_image_size = true;
            return FinishFirmwareFailure(failure_info);
        }
    }

    if (!ReadFileBytes(path, &image, &error)) {
        UpdateErrorStatus(error);
        return false;
    }

    image_crc = tcp_tool::Crc32(image);
    m_firmwarePanel.SetBurnRunning(true);
    m_firmwarePanel.SetProgress(0, L"Uploading firmware...");
    m_deviceBar.SetLastEventText(L"IAP start: image size=" + std::to_wstring(image.size()) +
                                 L" bytes, crc=0x" + FormatHex32(image_crc));
    m_rxPanel.StopCapture();

    request.clear();
    tcp_tool::AppendUint32Le(&request, static_cast<std::uint32_t>(image.size()));
    tcp_tool::AppendUint32Le(&request, image_crc);
    tcp_tool::AppendUint32Le(&request, chunk_size);
    if (!m_device.ExecuteCommand(tcp_tool::kCmdIapStart, request, &status, &response, 5000, &error)) {
        m_rxPanel.StartCapture();
        failure_info.summary = L"IAP start transport failed.";
        failure_info.transport_error = error;
        failure_info.chunk_size = chunk_size;
        failure_info.image_size = image.size();
        failure_info.has_chunk_size = true;
        failure_info.has_image_size = true;
        return FinishFirmwareFailure(failure_info);
    }
    if (status != tcp_tool::kStatusOk) {
        m_rxPanel.StartCapture();
        failure_info.summary = L"IAP start failed.";
        failure_info.status = status;
        failure_info.has_status = true;
        failure_info.chunk_size = chunk_size;
        failure_info.image_size = image.size();
        failure_info.has_chunk_size = true;
        failure_info.has_image_size = true;
        return FinishFirmwareFailure(failure_info);
    }
    if (response.size() >= 4) {
        chunk_size = tcp_tool::ReadUint32Le(response.data());
        if (chunk_size == 0) {
            chunk_size = 1024;
        }
    }
    MarkControlTraffic();

    while (offset < image.size()) {
        std::size_t current_size = std::min<std::size_t>(chunk_size, image.size() - offset);
        request.clear();
        response.clear();
        failure_info.offset = offset;
        failure_info.has_offset = true;
        tcp_tool::AppendUint32Le(&request, static_cast<std::uint32_t>(offset));
        tcp_tool::AppendUint32Le(&request, static_cast<std::uint32_t>(current_size));
        request.insert(request.end(), image.begin() + static_cast<std::ptrdiff_t>(offset),
                       image.begin() + static_cast<std::ptrdiff_t>(offset + current_size));
        if (!m_device.ExecuteCommand(tcp_tool::kCmdIapData, request, &status, &response, 5000, &error)) {
            m_rxPanel.StartCapture();
            failure_info.summary = L"IAP data transport failed.";
            failure_info.transport_error = error;
            failure_info.chunk_size = current_size;
            failure_info.image_size = image.size();
            failure_info.has_chunk_size = true;
            failure_info.has_image_size = true;
            return FinishFirmwareFailure(failure_info);
        }
        if (status != tcp_tool::kStatusOk) {
            m_rxPanel.StartCapture();
            failure_info.summary = L"IAP data failed.";
            failure_info.status = status;
            failure_info.has_status = true;
            failure_info.chunk_size = current_size;
            failure_info.image_size = image.size();
            failure_info.has_chunk_size = true;
            failure_info.has_image_size = true;
            return FinishFirmwareFailure(failure_info);
        }
        offset += current_size;
        MarkControlTraffic();
        percent = static_cast<int>((offset * 100u) / image.size());
        m_firmwarePanel.SetProgress(percent, L"Uploading... " + std::to_wstring(offset) + L" / " + std::to_wstring(image.size()) + L" bytes");
        PumpUi();
    }

    request.clear();
    tcp_tool::AppendUint32Le(&request, static_cast<std::uint32_t>(image.size()));
    tcp_tool::AppendUint32Le(&request, image_crc);
    if (!m_device.ExecuteCommand(tcp_tool::kCmdIapFinish, request, &status, &response, 5000, &error)) {
        m_rxPanel.StartCapture();
        failure_info.summary = L"IAP finish transport failed.";
        failure_info.transport_error = error;
        failure_info.offset = image.size();
        failure_info.chunk_size = chunk_size;
        failure_info.image_size = image.size();
        failure_info.has_offset = true;
        failure_info.has_chunk_size = true;
        failure_info.has_image_size = true;
        return FinishFirmwareFailure(failure_info);
    }
    m_firmwarePanel.SetBurnRunning(false);
    m_rxPanel.StartCapture();
    if (status != tcp_tool::kStatusOk) {
        failure_info.summary = L"IAP finish failed.";
        failure_info.status = status;
        failure_info.has_status = true;
        failure_info.offset = image.size();
        failure_info.chunk_size = chunk_size;
        failure_info.image_size = image.size();
        failure_info.has_offset = true;
        failure_info.has_chunk_size = true;
        failure_info.has_image_size = true;
        return FinishFirmwareFailure(failure_info);
    }

    MarkControlTraffic();
    m_firmwarePanel.SetProgress(100, L"Firmware upload complete. Click Reset MCU to reboot.");
    UpdateInfoStatus(L"Firmware upload complete.");
    return true;
}

bool CMainFrame::FinishFirmwareFailure(const FirmwareFailureInfo& info) {
    std::wstring progress_text;
    std::wstring message;
    std::wstring summary;
    std::wstring status_text;
    std::wstring offset_text;
    std::wstring chunk_text;
    std::wstring image_text;
    std::wstring transport_text;

    m_firmwarePanel.SetBurnRunning(false);
    progress_text = L"Firmware update failed. Please burn again.";
    m_firmwarePanel.SetProgress(0, progress_text);
    summary = info.summary.empty() ? L"Firmware update failed." : info.summary;
    UpdateErrorStatus(summary);
    m_deviceBar.SetLastEventText(progress_text);

    if (info.has_status) {
        status_text = tcp_tool::StatusToText(info.status) +
                      L" (0x" + FormatHex8(info.status) + L")";
    }
    if (info.has_offset) {
        offset_text = std::to_wstring(info.offset) +
                      L" bytes (0x" + FormatHex32(static_cast<std::uint32_t>(info.offset)) + L")";
    }
    if (info.has_chunk_size) {
        chunk_text = std::to_wstring(info.chunk_size) + L" bytes";
    }
    if (info.has_image_size) {
        image_text = std::to_wstring(info.image_size) + L" bytes";
    }
    transport_text = info.transport_error.empty() ? L"-" : info.transport_error;

    message = summary +
              L"\r\n\r\nFailure Offset : " + (offset_text.empty() ? L"-" : offset_text) +
              L"\r\nChunk Size     : " + (chunk_text.empty() ? L"-" : chunk_text) +
              L"\r\nImage Size     : " + (image_text.empty() ? L"-" : image_text) +
              L"\r\nFailure Status : " + (status_text.empty() ? L"-" : status_text) +
              L"\r\nTransport Error: " + transport_text +
              L"\r\n\r\nPlease burn firmware again.";
    AfxMessageBox(message.c_str(), MB_OK | MB_ICONWARNING);
    return false;
}

bool CMainFrame::RequestEnterBootloader() {
    unsigned char status = tcp_tool::kStatusError;
    std::vector<unsigned char> payload;
    std::wstring error;

    if (!m_device.IsOpen()) {
        UpdateErrorStatus(L"Connect first.");
        return false;
    }

    m_rxPanel.StopCapture();
    if (!m_device.ExecuteCommand(tcp_tool::kCmdResetToBoot, std::vector<unsigned char>(),
                                 &status, &payload, 2000, &error)) {
        UpdateErrorStatus(error);
        m_rxPanel.StartCapture();
        return false;
    }
    MarkControlTraffic();
    DisconnectUi(true);
    if (status == tcp_tool::kStatusOk) {
        UpdateInfoStatus(L"Reset to bootloader requested.");
    } else {
        UpdateErrorStatus(L"Reset to bootloader rejected.");
    }
    return (status == tcp_tool::kStatusOk);
}

bool CMainFrame::RequestResetMcu() {
    unsigned char status = tcp_tool::kStatusError;
    std::vector<unsigned char> payload;
    std::wstring error;

    if (!m_device.IsOpen()) {
        UpdateErrorStatus(L"Connect first.");
        return false;
    }

    m_rxPanel.StopCapture();
    if (!m_device.ExecuteCommand(tcp_tool::kCmdResetMcu, std::vector<unsigned char>(),
                                 &status, &payload, 2000, &error)) {
        UpdateErrorStatus(error);
        m_rxPanel.StartCapture();
        return false;
    }
    MarkControlTraffic();
    DisconnectUi(true);
    if (status == tcp_tool::kStatusOk) {
        UpdateInfoStatus(L"Reset MCU requested.");
    } else {
        UpdateErrorStatus(L"Reset MCU rejected.");
    }
    return (status == tcp_tool::kStatusOk);
}

bool CMainFrame::ReadFileBytes(const std::wstring& path, std::vector<unsigned char>* out_data, std::wstring* error) {
    CFile file;
    ULONGLONG length = 0;

    if (!out_data) {
        return false;
    }
    if (path.empty()) {
        if (error) {
            *error = L"Select a firmware .bin file first.";
        }
        return false;
    }
    if (!file.Open(path.c_str(), CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite)) {
        if (error) {
            *error = L"Failed to open firmware file.";
        }
        return false;
    }

    length = file.GetLength();
    if (length == 0 || length > 0xFFFFFFFFULL) {
        file.Close();
        if (error) {
            *error = L"Firmware file size is invalid.";
        }
        return false;
    }

    out_data->assign(static_cast<std::size_t>(length), 0);
    if (file.Read(out_data->data(), static_cast<UINT>(length)) != static_cast<UINT>(length)) {
        file.Close();
        if (error) {
            *error = L"Failed to read firmware file.";
        }
        return false;
    }

    file.Close();
    return true;
}

void CMainFrame::ApplyConfig() {
    m_deviceBar.SetHost(m_config.host);
    m_deviceBar.SetPort(m_config.port);
    m_deviceBar.SetAutoReconnectEnabled(m_config.auto_reconnect);
    m_txPanel.SetIntervalEnabled(m_config.interval_enabled);
    m_txPanel.SetIntervalValue(m_config.interval_ms);
    m_txPanel.SetLimitCount(m_config.limit_count);
    m_txPanel.SetLimitEnabled(m_config.limit_enabled);
    m_rxPanel.SetLogEnabled(m_config.log_enabled);
    m_rxPanel.SetTimestampEnabled(m_config.timestamp_enabled);
    m_firmwarePanel.SetFilePath(m_config.firmware_path);
    m_functionPanel.SetPollingEnabled(m_config.function_poll_adc,
                                      m_config.function_poll_gpio,
                                      m_config.function_poll_pwm);
}

void CMainFrame::ApplyCommandConfig() {
    const std::wstring& ff_row = GetAllFfRowText();
    int i = 0;
    for (i = 0; i < 6; ++i) {
        m_txPanel.SetRowEnabled(i, m_commandConfig.rows[i].enabled);
        m_txPanel.SetRowText(i, m_commandConfig.rows[i].text.empty() ? ff_row : m_commandConfig.rows[i].text);
    }
}

void CMainFrame::SaveConfig() {
    m_config.host = m_deviceBar.GetHost();
    m_config.port = m_deviceBar.GetPort();
    m_config.auto_reconnect = m_deviceBar.IsAutoReconnectEnabled();
    m_config.interval_enabled = m_txPanel.IsIntervalEnabled();
    m_config.interval_ms = m_txPanel.GetIntervalValue();
    m_config.limit_enabled = m_txPanel.IsLimitEnabled();
    m_config.limit_count = m_txPanel.GetLimitCount();
    m_config.log_enabled = m_rxPanel.IsLogEnabled();
    m_config.timestamp_enabled = m_rxPanel.IsTimestampEnabled();
    m_config.firmware_path = m_firmwarePanel.GetFilePath();
    m_functionPanel.GetPollingEnabled(&m_config.function_poll_adc,
                                      &m_config.function_poll_gpio,
                                      &m_config.function_poll_pwm);
    m_config.Save();
}

void CMainFrame::SaveCommandConfig() {
    int i = 0;
    for (i = 0; i < 6; ++i) {
        m_commandConfig.rows[i].enabled = m_txPanel.IsRowEnabled(i);
        m_commandConfig.rows[i].text = m_txPanel.GetRowText(i);
    }
    m_commandConfig.Save();
    m_commandConfig.SaveDefaultIfMissing();
}

bool CMainFrame::SaveCommandConfigTo(const std::wstring& path) {
    int i = 0;
    for (i = 0; i < 6; ++i) {
        m_commandConfig.rows[i].enabled = m_txPanel.IsRowEnabled(i);
        m_commandConfig.rows[i].text = m_txPanel.GetRowText(i);
    }
    return m_commandConfig.SaveTo(path);
}

bool CMainFrame::LoadCommandConfigFrom(const std::wstring& path) {
    CommandConfigManager temp;
    if (!temp.LoadFrom(path)) {
        return false;
    }
    m_commandConfig.rows = temp.rows;
    ApplyCommandConfig();
    SaveCommandConfig();
    return true;
}

void CMainFrame::DisconnectUi(bool allow_reconnect) {
    m_peerMode = tcp_tool::kPeerUnknown;
    m_pendingFunctionRefresh.store(false);
    m_pendingGpioApply.store(false);
    m_pendingPwmApply.store(false);
    SyncControlPollSettings();
    m_forceStatusPoll.store(false);
    m_device.CancelPendingIo();
    m_scheduler.Stop();
    m_rxPanel.StopCapture();
    m_device.Close();
    SetPeerModeText();
    m_deviceBar.SetUptimeText(L"Unknown");
    m_deviceBar.SetConnected(false);
    m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
    m_lastRxTick = 0;
    m_txPanel.SetSchedulerEnabled(false);
    m_txPanel.SetRunning(false);
    m_txPanel.SetControlsEnabled(false);
    m_rxPanel.SetControlsEnabled(false);
    m_rxPanel.SetClearEnabled(true);
    m_functionPanel.SetControlsEnabled(false);
    m_functionPanel.SetPeerModeText(L"Unknown");
    m_functionPanel.SetPacketPreview(L"", L"");
    m_functionPanel.SetFunctionRxPreview(tcp_tool::kRawPktAdc, L"");
    m_functionPanel.SetFunctionRxPreview(tcp_tool::kRawPktGpio, L"");
    m_functionPanel.SetFunctionRxPreview(tcp_tool::kRawPktPwm, L"");
    m_firmwarePanel.SetControlsEnabled(false);
    m_uptimePollFailures = 0;
    m_nextFunctionPollIndex = 0;
    UpdateConnectionStatus(L"Disconnected");
    m_deviceBar.SetLastEventText(L"Disconnected");
    if (allow_reconnect && m_deviceBar.IsAutoReconnectEnabled()) {
        StartReconnectTimer();
    }
}

void CMainFrame::SetPeerModeText() {
    std::wstring text = L"Unknown";
    if (m_peerMode == tcp_tool::kPeerApp) {
        text = L"APP";
    } else if (m_peerMode == tcp_tool::kPeerBoot) {
        text = L"BOOT";
    }
    m_deviceBar.SetPeerModeText(text);
    m_functionPanel.SetPeerModeText(text);
    m_functionPanel.SetControlsEnabled(m_device.IsOpen() && m_peerMode == tcp_tool::kPeerApp);
    m_firmwarePanel.SetPeerModeText(text);
    SyncControlPollSettings();
}

void CMainFrame::PumpUi() {
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
