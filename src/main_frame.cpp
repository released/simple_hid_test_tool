#include "main_frame.h"

#include <afxdlgs.h>

#include "resource.h"

#include <afxres.h>
#include <algorithm>
#include <cwchar>
#include <memory>
#include <sstream>
#include <string>

namespace {

const UINT kIndicators[] = {
    ID_SEPARATOR,
    ID_INDICATOR_ERR,
    ID_INDICATOR_TX,
    ID_INDICATOR_RX,
    ID_INDICATOR_HS,
    ID_INDICATOR_HOVER,
};

const int kPaneConnection = 0;
const int kPaneError = 1;
const int kPaneTx = 2;
const int kPaneRx = 3;
const int kPaneHandshake = 4;
const int kPaneHover = 5;

const UINT_PTR kRxTimerId = 1;
const UINT_PTR kReconnectTimerId = 2;
const UINT_PTR kStartupScanTimerId = 3;

std::wstring BytesToHex64(const std::vector<unsigned char>& payload) {
    std::wstringstream ss;
    ss << std::uppercase << std::hex;
    size_t count = std::min<size_t>(64, payload.size());
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) {
            ss << L' ';
        }
        ss.width(2);
        ss.fill(L'0');
        ss << static_cast<unsigned int>(payload[i]);
    }
    return ss.str();
}

std::wstring BuildTxLogBody(const std::vector<unsigned char>& payload) {
    return BytesToHex64(payload);
}

bool BuildReportBuffer(unsigned char report_id, const std::vector<unsigned char>& payload, std::vector<unsigned char>* out_report, std::wstring* error) {
    if (!out_report) {
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
    out_report->assign(65, 0x00);
    (*out_report)[0] = report_id;
    std::copy(payload.begin(), payload.begin() + 64, out_report->begin() + 1);
    return true;
}

const std::wstring& GetAllFfRowText() {
    static const std::wstring kAllFf = []() {
        std::wstring text;
        text.reserve(64 * 3);
        for (int i = 0; i < 64; ++i) {
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
    ON_WM_CLOSE()
    ON_WM_TIMER()
END_MESSAGE_MAP()

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CFrameWnd::PreCreateWindow(cs)) {
        return FALSE;
    }
    cs.style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    HICON hIcon = AfxGetApp()->LoadIcon(IDI_APP_ICON);
    cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                       reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), hIcon);
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    if (!m_deviceBar.Create(this)) {
        return -1;
    }
    if (!m_txPanel.Create(this)) {
        return -1;
    }
    if (!m_rxPanel.Create(this)) {
        return -1;
    }
    if (!m_statusBar.Create(this)) {
        return -1;
    }

    InitStatusBar();
    HICON hIcon = static_cast<HICON>(::LoadImageW(AfxGetInstanceHandle(), MAKEINTRESOURCEW(IDI_APP_ICON),
                                                 IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    HICON hIconSmall = static_cast<HICON>(::LoadImageW(AfxGetInstanceHandle(), MAKEINTRESOURCEW(IDI_APP_ICON),
                                                      IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (hIcon) {
        SetIcon(hIcon, TRUE);
    }
    if (hIconSmall) {
        SetIcon(hIconSmall, FALSE);
    }

    m_rxPanel.SetDevice(&m_device);
    m_handshake.SetDevice(&m_device);
    m_handshake.SetWaitEchoCallback([this](const std::vector<unsigned char>& payload, DWORD timeout) {
        return m_rxPanel.WaitForPayload(payload, timeout);
    });

    m_scheduler.SetDevice(&m_device);
    m_scheduler.SetHandshake(&m_handshake);
    m_scheduler.SetNotifyHwnd(m_hWnd);
    m_txCount = 0;
    m_rxCount = 0;
    m_lastRxTick = 0;

    m_config.Load();
    m_commandConfig.Load();
    ApplyConfig();
    ApplyCommandConfig();
    SaveCommandConfig();
    m_lastEchoModeEnabled = m_txPanel.IsEchoModeEnabled();

    m_deviceBar.SetConnected(false);
    m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
    m_txPanel.SetSchedulerEnabled(false);
    m_txPanel.SetRunning(false);
    m_txPanel.SetControlsEnabled(false);
    m_rxPanel.SetControlsEnabled(false);
    m_rxPanel.SetClearEnabled(true);

    if (!m_rxStatusTimer) {
        m_rxStatusTimer = SetTimer(kRxTimerId, 200, nullptr);
    }
    if (!m_startupScanTimer) {
        m_startupScanTimer = SetTimer(kStartupScanTimerId, 10, nullptr);
    }

    return 0;
}

void CMainFrame::OnSize(UINT nType, int cx, int cy) {
    CFrameWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CMainFrame::LayoutControls(int cx, int cy) {
    if (cx <= 0 || cy <= 0) {
        return;
    }

    const int margin = 8;
    const int gap = 8;
    const int top_height = 170;
    const int status_height = 22;

    int width = cx - margin * 2;
    if (width < 0) {
        width = 0;
    }

    m_statusBar.MoveWindow(0, cy - status_height, cx, status_height);
    LayoutStatusBarPanes();

    int y = margin;
    m_deviceBar.MoveWindow(margin, y, width, top_height);
    y += top_height + gap;

    int available_height = cy - status_height - margin - y;
    if (available_height < 0) {
        available_height = 0;
    }

    int tx_min = m_txPanel.GetPreferredHeight();
    int rx_min = m_rxPanel.GetPreferredHeight();
    int tx_height = tx_min;
    int rx_height = rx_min;
    if (available_height >= tx_min + rx_min + gap) {
        const int rx_extra = 8;
        int rx_target = rx_min + rx_extra;
        if (available_height >= tx_min + rx_target + gap) {
            rx_height = rx_target;
        } else {
            rx_height = rx_min;
        }
        tx_height = available_height - rx_height - gap;
    } else {
        tx_height = tx_min;
        rx_height = available_height - tx_height - gap;
        if (rx_height < rx_min) {
            rx_height = rx_min;
            tx_height = available_height - rx_height - gap;
        }
    }
    if (tx_height < 0) {
        tx_height = 0;
    }
    if (rx_height < 0) {
        rx_height = 0;
    }
    if (rx_height < 0) {
        rx_height = 0;
    }

    m_txPanel.MoveWindow(margin, y, width, tx_height);
    y += tx_height + gap;
    m_rxPanel.MoveWindow(margin, y, width, rx_height);
    int data_start = 0;
    int cell_base = 0;
    int cell_extra = 0;
    int row_height = 0;
    int row_gap = 0;
    m_txPanel.GetGridMetrics(&data_start, &cell_base, &cell_extra, &row_height, &row_gap);
    if (data_start > 0 && cell_base > 0) {
        m_rxPanel.SetExternalGridMetrics(data_start, cell_base, cell_extra, row_height, row_gap);
    }
}

LRESULT CMainFrame::OnDeviceBarAction(WPARAM wParam, LPARAM lParam) {
    int action = static_cast<int>(wParam);
    int index = static_cast<int>(lParam);

    if (action == CDeviceBar::ActionConnectToggle) {
        if (m_device.IsOpen()) {
            m_scheduler.Stop();
            m_rxPanel.StopCapture();
            m_device.CancelPendingIo();
            m_device.Close();
            m_deviceBar.SetConnected(false);
            m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
            m_lastRxTick = 0;
            m_txPanel.SetSchedulerEnabled(false);
            m_txPanel.SetRunning(false);
            m_txPanel.SetControlsEnabled(false);
            m_rxPanel.SetControlsEnabled(false);
            m_rxPanel.SetClearEnabled(true);
            UpdateConnectionStatus(L"Disconnected");
            UpdateHandshakeStatus(L"Off");
            m_txPanel.SetHandshakeState(CTxPanel::HsOff);
            if (m_deviceBar.IsAutoReconnectEnabled()) {
                StartReconnectTimer();
            }
            return 0;
        }

        ConnectSelectedDevice();
        return 0;
    }

    return 0;
}

LRESULT CMainFrame::OnTxPanelAction(WPARAM wParam, LPARAM lParam) {
    int action = static_cast<int>(wParam);
    int row = static_cast<int>(lParam);
    if (action == CTxPanel::ActionSendOnce) {
        if (!m_device.IsOpen()) {
            UpdateErrorStatus(L"Not connected.");
            return 0;
        }
        unsigned char report_id = 0;
        std::wstring error;
        if (!m_txPanel.GetReportId(&report_id, &error)) {
            UpdateErrorStatus(L"ReportID invalid: " + error);
            return 0;
        }

        std::vector<unsigned char> payload;
        if (!m_txPanel.GetRowPayload(row, &payload, &error)) {
            UpdateErrorStatus(L"Row " + std::to_wstring(row + 1) + L" invalid: " + error);
            return 0;
        }

        std::vector<unsigned char> report;
        if (!BuildReportBuffer(report_id, payload, &report, &error)) {
            UpdateErrorStatus(error);
            return 0;
        }
        if (!m_device.WriteReport(report, &error)) {
            size_t intended = 0;
            size_t actual = 0;
            m_device.GetLastWriteInfo(&intended, &actual);
            if (error.empty()) {
                error = L"Write failed.";
            }
            error += L" (len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual) + L")";
            UpdateErrorStatus(error);
            if (error == L"Device removed.") {
                m_scheduler.Stop();
                m_rxPanel.StopCapture();
                m_device.Close();
                m_deviceBar.SetConnected(false);
                m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
                m_lastRxTick = 0;
                m_txPanel.SetSchedulerEnabled(false);
                m_txPanel.SetRunning(false);
                m_txPanel.SetControlsEnabled(false);
                m_rxPanel.SetControlsEnabled(false);
                m_rxPanel.SetClearEnabled(true);
                UpdateConnectionStatus(L"Disconnected");
                UpdateHandshakeStatus(L"Off");
                if (m_deviceBar.IsAutoReconnectEnabled()) {
                    StartReconnectTimer();
                }
            }
            return 0;
        }

        m_txCount++;
        size_t intended = 0;
        size_t actual = 0;
        m_device.GetLastWriteInfo(&intended, &actual);
        m_lastWriteInfo = L"len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual);
        UpdateTxCount(m_txCount);
        m_rxPanel.LogTxLine(BuildTxLogBody(payload));
        return 0;
    }
    if (action == CTxPanel::ActionStart) {
        TxScheduler::Settings settings;
        std::wstring error;
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
        UpdateHandshakeStatus(L"Off");
        m_txPanel.SetHandshakeState(CTxPanel::HsOff);
        return 0;
    }
    if (action == CTxPanel::ActionSingleShot) {
        TxScheduler::Settings settings;
        std::wstring error;
        if (!BuildSchedulerSettings(&settings, &error)) {
            UpdateErrorStatus(error);
            return 0;
        }
        m_scheduler.UpdateSettings(settings);
        m_scheduler.SingleShot();
        return 0;
    }
    if (action == CTxPanel::ActionSettingsChanged) {
        unsigned char report_id = 0;
        std::wstring report_error;
        if (m_txPanel.GetReportId(&report_id, &report_error)) {
            m_deviceBar.SetReportId(report_id);
        }
        if (m_scheduler.IsRunning()) {
            TxScheduler::Settings settings;
            std::wstring error;
            if (BuildSchedulerSettings(&settings, &error)) {
                m_scheduler.UpdateSettings(settings);
            } else {
                UpdateErrorStatus(error);
            }
        }
        if (!m_txPanel.IsHandshakeModeEnabled()) {
            UpdateHandshakeStatus(L"Off");
            m_txPanel.SetHandshakeState(CTxPanel::HsOff);
        }
        bool echo_mode = m_txPanel.IsEchoModeEnabled();
        if (m_device.IsOpen() && echo_mode != m_lastEchoModeEnabled) {
            SendEchoModeCommand(echo_mode);
        }
        m_lastEchoModeEnabled = echo_mode;
        SaveConfig();
        SaveCommandConfig();
        return 0;
    }
    if (action == CTxPanel::ActionSaveAs) {
        CFileDialog dialog(FALSE, L"ini", L"HidTestTool_commands.ini",
                           OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                           L"INI Files (*.ini)|*.ini|All Files (*.*)|*.*||",
                           this);
        if (dialog.DoModal() == IDOK) {
            SaveCommandConfigTo(std::wstring(dialog.GetPathName()));
        }
        return 0;
    }
    if (action == CTxPanel::ActionReload) {
        CFileDialog dialog(TRUE, L"ini", L"HidTestTool_commands.ini",
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
        return 0;
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
    case TxScheduler::EventHandshake:
        UpdateHandshakeStatus(payload->text);
        if (payload->text == L"WAITING") {
            m_txPanel.SetHandshakeState(CTxPanel::HsWaiting);
        } else if (payload->text == L"OK") {
            m_txPanel.SetHandshakeState(CTxPanel::HsOk);
        } else if (payload->text == L"ERROR") {
            m_txPanel.SetHandshakeState(CTxPanel::HsError);
        } else {
            m_txPanel.SetHandshakeState(CTxPanel::HsOff);
        }
        break;
    case TxScheduler::EventDeviceRemoved:
        UpdateErrorStatus(payload->text);
        m_scheduler.Stop();
        m_rxPanel.StopCapture();
        m_device.Close();
        m_deviceBar.SetConnected(false);
        m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
        m_lastRxTick = 0;
        m_txPanel.SetSchedulerEnabled(false);
        m_txPanel.SetRunning(false);
        m_txPanel.SetControlsEnabled(false);
        m_rxPanel.SetControlsEnabled(false);
        m_rxPanel.SetClearEnabled(true);
        UpdateConnectionStatus(L"Disconnected");
        UpdateHandshakeStatus(L"Off");
        m_txPanel.SetHandshakeState(CTxPanel::HsOff);
        if (m_deviceBar.IsAutoReconnectEnabled()) {
            StartReconnectTimer();
        }
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
        std::vector<unsigned char> echo;
        if (m_rxPanel.GetLastPayload(&echo)) {
            m_txPanel.SetLastEchoBytes(echo);
        }
    } else if (event == CRxPanel::RxDeviceRemoved) {
        UpdateErrorStatus(L"Device removed.");
        m_scheduler.Stop();
        m_rxPanel.StopCapture();
        m_device.Close();
        m_deviceBar.SetConnected(false);
        m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
        m_lastRxTick = 0;
        m_txPanel.SetSchedulerEnabled(false);
        m_txPanel.SetRunning(false);
        m_txPanel.SetControlsEnabled(false);
        m_rxPanel.SetControlsEnabled(false);
        m_rxPanel.SetClearEnabled(true);
        UpdateConnectionStatus(L"Disconnected");
        UpdateHandshakeStatus(L"Off");
        m_txPanel.SetHandshakeState(CTxPanel::HsOff);
        if (m_deviceBar.IsAutoReconnectEnabled()) {
            StartReconnectTimer();
        }
    }
    return 0;
}

void CMainFrame::OnClose() {
    m_scheduler.Stop();
    m_rxPanel.StopCapture();
    m_device.Close();
    if (m_rxStatusTimer) {
        KillTimer(m_rxStatusTimer);
        m_rxStatusTimer = 0;
    }
    if (m_startupScanTimer) {
        KillTimer(m_startupScanTimer);
        m_startupScanTimer = 0;
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
    m_statusBar.SetPaneInfo(kPaneHandshake, ID_INDICATOR_HS, SBPS_NORMAL, 0);
    m_statusBar.SetPaneInfo(kPaneHover, ID_INDICATOR_HOVER, SBPS_NORMAL, 0);
    LayoutStatusBarPanes();

    UpdateConnectionStatus(L"Disconnected");
    UpdateErrorStatus(L"");
    UpdateTxCount(0);
    UpdateRxCount(0);
    UpdateHandshakeStatus(L"Off");
    m_statusBar.SetPaneText(kPaneHover, L"");
}

void CMainFrame::LayoutStatusBarPanes() {
    if (!m_statusBar.GetSafeHwnd()) {
        return;
    }
    CRect rect;
    m_statusBar.GetClientRect(&rect);
    int width = rect.Width();
    if (width <= 0) {
        return;
    }

    int conn = 200;
    int tx = 220;
    int rx = 90;
    int hs = 120;
    int hover = 220;
    const int min_err = 200;
    const int min_hover = 100;
    const int min_conn = 140;
    const int min_tx = 160;

    int err = width - (conn + tx + rx + hs + hover);
    if (err < min_err) {
        int need = min_err - err;
        int take = need < (hover - min_hover) ? need : (hover - min_hover);
        hover -= take;
        err += take;
        need -= take;
        if (need > 0) {
            take = need < (conn - min_conn) ? need : (conn - min_conn);
            conn -= take;
            err += take;
            need -= take;
        }
        if (need > 0) {
            take = need < (tx - min_tx) ? need : (tx - min_tx);
            tx -= take;
            err += take;
            need -= take;
        }
        if (need > 0 && err < 80) {
            err = 80;
        }
    }

    m_statusBar.SetPaneInfo(kPaneConnection, ID_SEPARATOR, SBPS_NORMAL, conn);
    m_statusBar.SetPaneInfo(kPaneError, ID_INDICATOR_ERR, SBPS_NORMAL, err);
    m_statusBar.SetPaneInfo(kPaneTx, ID_INDICATOR_TX, SBPS_NORMAL, tx);
    m_statusBar.SetPaneInfo(kPaneRx, ID_INDICATOR_RX, SBPS_NORMAL, rx);
    m_statusBar.SetPaneInfo(kPaneHandshake, ID_INDICATOR_HS, SBPS_NORMAL, hs);
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
}

void CMainFrame::UpdateTxCount(unsigned long long count) {
    m_txCount = count;
    std::wstring text = L"TX: " + std::to_wstring(count);
    if (!m_lastWriteInfo.empty()) {
        text += L" (" + m_lastWriteInfo + L")";
    }
    m_statusBar.SetPaneText(kPaneTx, text.c_str());
}

void CMainFrame::UpdateRxCount(unsigned long long count) {
    std::wstring text = L"RX: " + std::to_wstring(count);
    m_statusBar.SetPaneText(kPaneRx, text.c_str());
}

void CMainFrame::UpdateHandshakeStatus(const std::wstring& text) {
    m_statusBar.SetPaneText(kPaneHandshake, (L"HS: " + text).c_str());
}

void CMainFrame::UpdateRxActivity() {
    if (!m_device.IsOpen()) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
        m_rxPanel.SetRxState(CRxPanel::RxIdle);
        UpdateConnectionStatus(L"Disconnected");
        return;
    }

    DWORD now = GetTickCount();
    if (m_lastRxTick == 0) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
        m_rxPanel.SetRxState(CRxPanel::RxIdle);
        UpdateConnectionStatus(L"Connected");
        return;
    }
    DWORD elapsed = now - m_lastRxTick;
    if (elapsed <= 100) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateRxActive);
        m_rxPanel.SetRxState(CRxPanel::RxReceiving);
        UpdateConnectionStatus(L"RX Active");
    } else if (elapsed > 1000) {
        m_deviceBar.SetConnectionState(CDeviceBar::StateRxTimeout);
        m_rxPanel.SetRxState(CRxPanel::RxTimeout);
        UpdateConnectionStatus(L"RX Timeout");
    } else {
        m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
        m_rxPanel.SetRxState(CRxPanel::RxIdle);
        UpdateConnectionStatus(L"Connected");
    }
}

void CMainFrame::RestoreDeviceSelection() {
    if (m_config.device_path_hash != 0) {
        m_deviceBar.SelectByPathHash(m_config.device_path_hash);
    } else if (!m_config.device_path.empty()) {
        m_deviceBar.SelectByPath(m_config.device_path);
    } else if (m_config.device_vid != 0 || m_config.device_pid != 0) {
        m_deviceBar.SelectByVidPid(m_config.device_vid, m_config.device_pid);
    }
}

void CMainFrame::StartReconnectTimer() {
    if (m_reconnectTimer) {
        return;
    }
    m_reconnectTimer = SetTimer(kReconnectTimerId, 1000, nullptr);
}

void CMainFrame::StopReconnectTimer() {
    if (m_reconnectTimer) {
        KillTimer(m_reconnectTimer);
        m_reconnectTimer = 0;
    }
}

bool CMainFrame::ConnectSelectedDevice() {
    const auto& devices = m_deviceBar.Devices();
    int index = m_deviceBar.SelectedIndex();
    if (index < 0 || index >= static_cast<int>(devices.size())) {
        UpdateErrorStatus(L"No device selected.");
        return false;
    }

    m_rxPanel.StopCapture();
    m_device.Close();
    std::wstring error;
    if (!m_device.Open(devices[index], &error)) {
        std::wstring message = L"Error: ";
        message += error.empty() ? L"Connect failed." : error;
        UpdateErrorStatus(message);
        UpdateConnectionStatus(L"Disconnected");
        m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
        return false;
    }

    m_lastConnected = devices[index];
    m_hasLastConnected = true;
    m_lastRxTick = 0;
    UpdateConnectionStatus(L"Connected");
    UpdateErrorStatus(L"");
    UpdateHandshakeStatus(L"Off");
    m_txPanel.SetHandshakeState(CTxPanel::HsOff);
    m_deviceBar.SetConnected(true);
    m_deviceBar.SetConnectionState(CDeviceBar::StateConnected);
    m_txPanel.SetSchedulerEnabled(true);
    m_txPanel.SetRunning(false);
    m_txPanel.SetControlsEnabled(true);
    m_rxPanel.SetControlsEnabled(true);
    m_rxPanel.SetClearEnabled(false);
    m_rxPanel.StartCapture();
    if (m_txPanel.IsEchoModeEnabled()) {
        SendEchoModeCommand(true);
    }
    m_lastEchoModeEnabled = m_txPanel.IsEchoModeEnabled();
    StopReconnectTimer();
    return true;
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kRxTimerId) {
        UpdateRxActivity();
    } else if (nIDEvent == kStartupScanTimerId) {
        if (m_startupScanTimer) {
            KillTimer(m_startupScanTimer);
            m_startupScanTimer = 0;
        }
        m_deviceBar.ScanDevices();
        RestoreDeviceSelection();
    } else if (nIDEvent == kReconnectTimerId) {
        if (m_device.IsOpen()) {
            StopReconnectTimer();
            return;
        }
        if (!m_deviceBar.IsAutoReconnectEnabled()) {
            StopReconnectTimer();
            return;
        }
        m_deviceBar.ScanDevices();
        if (m_hasLastConnected) {
            if (!m_deviceBar.SelectByPathHash(m_lastConnected.path_hash)) {
                m_deviceBar.SelectByVidPid(m_lastConnected.vendor_id, m_lastConnected.product_id);
            }
        } else {
            RestoreDeviceSelection();
        }
        if (m_deviceBar.SelectedIndex() >= 0) {
            ConnectSelectedDevice();
        }
    }
    CFrameWnd::OnTimer(nIDEvent);
}

bool CMainFrame::BuildSchedulerSettings(TxScheduler::Settings* out_settings, std::wstring* error) {
    if (!out_settings) {
        return false;
    }
    TxScheduler::Settings settings;
    settings.interval_enabled = m_txPanel.IsIntervalEnabled();
    settings.interval_ms = m_txPanel.GetIntervalValue();
    settings.handshake_enabled = m_txPanel.IsHandshakeModeEnabled();
    settings.handshake_timeout_ms = m_config.handshake_timeout_ms;
    settings.handshake_retry_count = m_txPanel.GetHandshakeRetryCount();
    settings.limit_enabled = m_txPanel.IsLimitEnabled();
    settings.limit_count = m_txPanel.GetLimitCount();

    unsigned char report_id = 0;
    std::wstring report_error;
    if (!m_txPanel.GetReportId(&report_id, &report_error)) {
        if (error) {
            *error = L"ReportID invalid: " + report_error;
        }
        return false;
    }
    settings.report_id = report_id;

    for (int i = 0; i < 6; ++i) {
        settings.rows[i].enabled = m_txPanel.IsRowEnabled(i);
        bool needs_payload = settings.rows[i].enabled;
        if (i == 0 && settings.handshake_enabled) {
            needs_payload = true;
        }
        if (needs_payload) {
            std::vector<unsigned char> payload;
            std::wstring parse_error;
            if (!m_txPanel.GetRowPayload(i, &payload, &parse_error)) {
                if (error) {
                    std::wstring msg = L"Row ";
                    msg += std::to_wstring(i + 1);
                    msg += L" invalid: ";
                    msg += parse_error;
                    *error = msg;
                }
                return false;
            }
            settings.rows[i].payload = payload;
        }
    }

    *out_settings = settings;
    return true;
}

bool CMainFrame::SendEchoModeCommand(bool enable) {
    if (!m_device.IsOpen()) {
        UpdateErrorStatus(L"Not connected.");
        return false;
    }

    unsigned char report_id = 0;
    std::wstring report_error;
    if (!m_txPanel.GetReportId(&report_id, &report_error)) {
        UpdateErrorStatus(L"ReportID invalid: " + report_error);
        return false;
    }

    std::vector<unsigned char> payload(64, 0x00);
    payload[0] = 0x5A;
    payload[1] = 0x5A;
    payload[2] = 0xEE;
    payload[3] = enable ? 0x01 : 0x00;

    std::vector<unsigned char> report;
    std::wstring error;
    if (!BuildReportBuffer(report_id, payload, &report, &error)) {
        UpdateErrorStatus(error);
        return false;
    }

    std::wstring write_error;
    if (!m_device.WriteReport(report, &write_error)) {
        size_t intended = 0;
        size_t actual = 0;
        m_device.GetLastWriteInfo(&intended, &actual);
        bool removed = (write_error == L"Device removed.");
        std::wstring message = write_error.empty() ? L"Write failed." : write_error;
        message += L" (len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual) + L")";
        UpdateErrorStatus(message);
        if (removed) {
            m_scheduler.Stop();
            m_rxPanel.StopCapture();
            m_device.Close();
            m_deviceBar.SetConnected(false);
            m_deviceBar.SetConnectionState(CDeviceBar::StateDisconnected);
            m_lastRxTick = 0;
            m_txPanel.SetSchedulerEnabled(false);
            m_txPanel.SetRunning(false);
            m_txPanel.SetControlsEnabled(false);
            m_rxPanel.SetControlsEnabled(false);
            m_rxPanel.SetClearEnabled(true);
            UpdateConnectionStatus(L"Disconnected");
            UpdateHandshakeStatus(L"Off");
            m_txPanel.SetHandshakeState(CTxPanel::HsOff);
            if (m_deviceBar.IsAutoReconnectEnabled()) {
                StartReconnectTimer();
            }
        }
        return false;
    }

    m_txCount++;
    size_t intended = 0;
    size_t actual = 0;
    m_device.GetLastWriteInfo(&intended, &actual);
    m_lastWriteInfo = L"len=" + std::to_wstring(intended) + L" wrote=" + std::to_wstring(actual);
    UpdateTxCount(m_txCount);
    m_rxPanel.LogTxLine(BuildTxLogBody(payload));
    return true;
}

void CMainFrame::ApplyConfig() {
    m_txPanel.SetReportId(m_config.report_id);
    m_txPanel.SetIntervalEnabled(m_config.interval_enabled);
    m_txPanel.SetIntervalValue(m_config.interval_ms);
    bool handshake_mode = m_config.handshake_mode_enabled || m_config.handshake_enabled;
    m_txPanel.SetHandshakeEnabled(handshake_mode);
    m_txPanel.SetEchoModeEnabled(m_config.echo_mode_enabled);
    m_txPanel.SetHandshakeModeEnabled(handshake_mode);
    m_deviceBar.SetReportId(m_config.report_id);
    unsigned int timeout = m_config.handshake_timeout_ms;
    if (timeout < 200) {
        timeout = 200;
    } else if (timeout > 500) {
        timeout = 500;
    }
    m_config.handshake_timeout_ms = timeout;
    m_handshake.SetTimeout(timeout);
    m_txPanel.SetHandshakeRetryCount(m_config.handshake_retry_count);
    m_txPanel.SetLimitCount(m_config.limit_count);
    m_txPanel.SetLimitEnabled(m_config.limit_enabled);

    m_rxPanel.SetLogEnabled(m_config.log_enabled);
    m_rxPanel.SetTimestampEnabled(m_config.timestamp_enabled);
}

void CMainFrame::ApplyCommandConfig() {
    const std::wstring& ff_row = GetAllFfRowText();
    for (int i = 0; i < 6; ++i) {
        m_txPanel.SetRowEnabled(i, m_commandConfig.rows[i].enabled);
        const std::wstring& row_text = m_commandConfig.rows[i].text.empty() ? ff_row : m_commandConfig.rows[i].text;
        m_txPanel.SetRowText(i, row_text);
    }
}

void CMainFrame::SaveConfig() {
    if (m_hasLastConnected) {
        const auto& info = m_lastConnected;
        m_config.device_path = info.device_path;
        m_config.device_vid = info.vendor_id;
        m_config.device_pid = info.product_id;
        m_config.device_path_hash = info.path_hash;
    } else {
        int sel = m_deviceBar.SelectedIndex();
        if (sel >= 0 && sel < static_cast<int>(m_deviceBar.Devices().size())) {
            const auto& info = m_deviceBar.Devices()[sel];
            m_config.device_path = info.device_path;
            m_config.device_vid = info.vendor_id;
            m_config.device_pid = info.product_id;
            m_config.device_path_hash = info.path_hash;
        } else {
        m_config.device_path.clear();
        m_config.device_vid = 0;
        m_config.device_pid = 0;
        m_config.device_path_hash = 0;
        }
    }

    m_config.interval_enabled = m_txPanel.IsIntervalEnabled();
    m_config.interval_ms = m_txPanel.GetIntervalValue();

    unsigned char report_id = 0;
    std::wstring dummy;
    if (m_txPanel.GetReportId(&report_id, &dummy)) {
        m_config.report_id = report_id;
    }
    m_config.handshake_enabled = m_txPanel.IsHandshakeModeEnabled();
    m_config.handshake_retry_count = m_txPanel.GetHandshakeRetryCount();
    m_config.limit_enabled = m_txPanel.IsLimitEnabled();
    m_config.limit_count = m_txPanel.GetLimitCount();
    m_config.echo_mode_enabled = m_txPanel.IsEchoModeEnabled();
    m_config.handshake_mode_enabled = m_txPanel.IsHandshakeModeEnabled();

    m_config.log_enabled = m_rxPanel.IsLogEnabled();
    m_config.timestamp_enabled = m_rxPanel.IsTimestampEnabled();

    m_config.Save();
}

void CMainFrame::SaveCommandConfig() {
    for (int i = 0; i < 6; ++i) {
        m_commandConfig.rows[i].enabled = m_txPanel.IsRowEnabled(i);
        m_commandConfig.rows[i].text = m_txPanel.GetRowText(i);
    }
    m_commandConfig.Save();
    m_commandConfig.SaveDefaultIfMissing();
}

bool CMainFrame::SaveCommandConfigTo(const std::wstring& path) {
    for (int i = 0; i < 6; ++i) {
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

