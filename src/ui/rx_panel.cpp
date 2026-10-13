#include "rx_panel.h"

#include <algorithm>
#include <memory>
#include <sstream>

#pragma comment(lib, "msimg32.lib")

namespace {

const int kSaveCheckId = 3001;
const int kTimestampCheckId = 3002;
const int kClearId = 3003;
const int kRxByteBase = 6000;
const UINT_PTR kUiTimerId = 1;

std::wstring BytesToHex(const unsigned char* data, size_t length) {
    std::wstringstream ss;
    ss << std::uppercase << std::hex;
    for (size_t i = 0; i < length; ++i) {
        if (i > 0) {
            ss << L' ';
        }
        ss.width(2);
        ss.fill(L'0');
        ss << static_cast<int>(data[i]);
    }
    return ss.str();
}

}

BEGIN_MESSAGE_MAP(CRxPanel, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_BN_CLICKED(kSaveCheckId, &CRxPanel::OnSaveLogClicked)
    ON_BN_CLICKED(kTimestampCheckId, &CRxPanel::OnTimestampClicked)
    ON_BN_CLICKED(kClearId, &CRxPanel::OnClearClicked)
END_MESSAGE_MAP()

bool CRxPanel::Create(CWnd* parent) {
    CString class_name = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), nullptr);
    return CWnd::CreateEx(0, class_name, L"RxPanel",
                          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          CRect(0, 0, 0, 0), parent, 0);
}

int CRxPanel::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    CClientDC dc(this);
    int font_height = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);
    m_uiFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_monoFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           FIXED_PITCH | FF_MODERN, L"Consolas");

    m_saveCheck.Create(L"Save to log", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                       CRect(0, 0, 0, 0), this, kSaveCheckId);
    m_saveCheck.SetFont(&m_uiFont);
    m_timestampCheck.Create(L"Add timestamp", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                            CRect(0, 0, 0, 0), this, kTimestampCheckId);
    m_timestampCheck.SetFont(&m_uiFont);
    m_clearButton.Create(L"Clear Data", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         CRect(0, 0, 0, 0), this, kClearId);
    m_clearButton.SetFont(&m_uiFont);

    m_tooltip.Create(this, TTS_ALWAYSTIP);
    m_tooltip.SetMaxTipWidth(260);
    m_tooltip.AddTool(&m_saveCheck, L"Write TX/RX data to log file");
    m_tooltip.AddTool(&m_timestampCheck, L"Prefix RX lines with timestamp");
    m_tooltip.AddTool(&m_clearButton, L"Clear all RX rows");

    for (int row = 0; row < kRowCount; ++row) {
        for (int col = 0; col < kByteCount; ++col) {
            int id = kRxByteBase + row * kByteCount + col;
            m_cells[row][col].Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_READONLY | WS_DISABLED,
                                     CRect(0, 0, 0, 0), this, id);
            m_cells[row][col].SetFont(&m_monoFont);
            m_cells[row][col].SetWindowTextW(L"");
        }
    }

    m_device = nullptr;
    m_thread = nullptr;
    m_stopRequested = false;
    m_logEnabled = false;
    m_timestampEnabled = false;
    m_rxEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    InitializeCriticalSection(&m_payloadLock);
    InitializeCriticalSection(&m_pendingLock);
    if (!m_uiTimer) {
        m_uiTimer = SetTimer(kUiTimerId, 50, nullptr);
    }
    return 0;
}

void CRxPanel::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CRxPanel::LayoutControls(int cx, int cy) {
    const int margin = 10;
    const int header_height = 16;
    const int gap = 8;
    const int top_height = 24;

    int width = cx - margin * 2;
    if (width < 0) {
        width = 0;
    }

    int y = margin + header_height;
    int check_width = 140;
    m_saveCheck.MoveWindow(margin, y, check_width, top_height);
    m_timestampCheck.MoveWindow(margin + check_width + gap, y, check_width + 20, top_height);
    m_clearButton.MoveWindow(margin + check_width * 2 + gap * 2 + 20, y, 110, top_height);

    y += top_height + gap;

    CClientDC dc(this);
    dc.SelectObject(&m_monoFont);
    TEXTMETRIC tm = {};
    dc.GetTextMetrics(&tm);
    int min_row_height = tm.tmHeight + 6;
    m_headerHeight = tm.tmHeight + 4;
    m_timestampWidth = dc.GetTextExtent(L"00:00:00.000 ").cx + 4;
    m_rowLabelWidth = dc.GetTextExtent(L"No.10").cx + 8;
    if (m_rowLabelWidth < 24) {
        m_rowLabelWidth = 24;
    }

    const int tx_check_width = 70;
    const int tx_gap = 6;
    const int tx_button_width = 90;

    int edit_width = width - tx_check_width - tx_button_width * 2 - tx_gap * 3;
    if (edit_width < 120) {
        edit_width = 120;
    }
    if (m_hasExternalGrid) {
        m_dataStartX = m_externalStartX;
        m_cellWidthBase = m_externalCellBase;
        m_cellWidthExtra = m_externalCellExtra;
        if (m_externalRowHeight > 0) {
            m_rowHeight = m_externalRowHeight;
        } else {
            m_rowHeight = min_row_height;
        }
        if (m_externalRowGap > 0) {
            m_rowGap = m_externalRowGap;
        } else {
            m_rowGap = 6;
        }
    } else {
        m_dataStartX = margin + tx_check_width + tx_gap;
        m_cellWidthBase = edit_width / 64;
        m_cellWidthExtra = edit_width % 64;
        if (m_cellWidthBase < 8) {
            m_cellWidthBase = 8;
            m_cellWidthExtra = 0;
        }
        m_rowHeight = min_row_height;
        m_rowGap = 6;
    }

    m_headerRect = CRect(margin, y, margin + width, y + m_headerHeight);
    y += m_headerHeight + 4;
    int grid_height = cy - y - margin;
    if (grid_height < 0) {
        grid_height = 0;
    }
    int needed = m_rowHeight * 10 + m_rowGap * 9;
    if (!m_hasExternalGrid && grid_height > 0 && needed > grid_height) {
        int gap_override = m_rowGap;
        int available = grid_height - gap_override * 9;
        if (available < 10) {
            gap_override = 2;
            available = grid_height - gap_override * 9;
        }
        if (available > 0) {
            int shrink_height = available / 10;
            if (shrink_height < m_rowHeight) {
                m_rowHeight = shrink_height;
                if (m_rowHeight < 8) {
                    m_rowHeight = 8;
                }
            }
        }
        m_rowGap = gap_override;
    }
    m_gridRect = CRect(margin, y, margin + width, y + m_rowHeight * 10 + m_rowGap * 9);

    int data_end = m_dataStartX + m_cellWidthBase * 64 + m_cellWidthExtra;
    if (data_end > m_gridRect.right - 2) {
        data_end = m_gridRect.right - 2;
    }
    for (int row = 0; row < kRowCount; ++row) {
        int row_top = m_gridRect.top + row * (m_rowHeight + m_rowGap);
        int x = m_dataStartX;
        for (int col = 0; col < kByteCount; ++col) {
            int cell_width = m_cellWidthBase + (col < m_cellWidthExtra ? 1 : 0);
            if (x >= data_end) {
                break;
            }
            m_cells[row][col].MoveWindow(x, row_top, cell_width, m_rowHeight);
            x += cell_width;
        }
    }
}

void CRxPanel::OnPaint() {
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

    TRIVERTEX verts[2] = {};
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
    GRADIENT_RECT grad = { 0, 1 };
    ::GradientFill(dc.GetSafeHdc(), verts, 2, &grad, 1, GRADIENT_FILL_RECT_V);

    dc.DrawEdge(&rect, EDGE_SUNKEN, BF_RECT);

    const int title_height = 16;
    CRect titleRect(rect.left + 10, rect.top + 4, rect.right - 10, rect.top + 4 + title_height);
    dc.SetBkMode(TRANSPARENT);
    dc.SelectObject(&m_uiFont);
    dc.SetTextColor(RGB(15, 23, 42));
    const wchar_t* title = L"RX: Idle";
    if (m_rxState == RxReceiving) {
        title = L"RX: Receiving";
    } else if (m_rxState == RxTimeout) {
        title = L"RX: Timeout";
    } else if (m_cleared) {
        title = L"RX: Idle (cleared)";
    }
    dc.DrawText(title, titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (m_headerRect.Width() <= 0 || m_gridRect.Width() <= 0) {
        return;
    }

    dc.SelectObject(&m_monoFont);
    dc.SetTextColor(RGB(15, 23, 42));

    int data_start = m_dataStartX;
    int data_width = m_cellWidthBase * 64 + m_cellWidthExtra;
    int data_end = data_start + data_width;
    if (data_end > m_gridRect.right - 2) {
        data_end = m_gridRect.right - 2;
    }

    CRect header_band(data_start - 2, m_headerRect.top, data_end + 2, m_headerRect.bottom);
    dc.FillSolidRect(&header_band, RGB(226, 232, 240));
    CPen hpen(PS_SOLID, 1, RGB(203, 213, 225));
    CPen* old_hpen = dc.SelectObject(&hpen);
    dc.MoveTo(header_band.left, header_band.bottom - 1);
    dc.LineTo(header_band.right, header_band.bottom - 1);
    dc.SelectObject(old_hpen);

    int x = data_start;
    for (int i = 0; i < 64; ++i) {
        int cell_width = m_cellWidthBase + (i < m_cellWidthExtra ? 1 : 0);
        CRect cell(x, m_headerRect.top, x + cell_width, m_headerRect.bottom);
        wchar_t buf[4] = {};
        swprintf_s(buf, L"%u", i);
        dc.DrawText(buf, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        x += cell_width;
        if (x >= data_end) {
            break;
        }
    }

    dc.SetTextColor(RGB(15, 23, 42));
    for (int row = 0; row < kRowCount; ++row) {
        int row_top = m_gridRect.top + row * (m_rowHeight + m_rowGap);
        if (row_top >= m_gridRect.bottom) {
            break;
        }
        wchar_t idx_buf[8] = {};
        swprintf_s(idx_buf, L"No.%d", row + 1);
        CRect idx_rect(m_gridRect.left + 2, row_top, m_gridRect.left + 2 + m_rowLabelWidth, row_top + m_rowHeight);
        dc.DrawText(idx_buf, &idx_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

BOOL CRxPanel::PreTranslateMessage(MSG* pMsg) {
    if (m_tooltip.GetSafeHwnd()) {
        m_tooltip.RelayEvent(pMsg);
    }
    return CWnd::PreTranslateMessage(pMsg);
}

void CRxPanel::OnDestroy() {
    StopCapture();
    if (m_uiTimer) {
        KillTimer(m_uiTimer);
        m_uiTimer = 0;
    }
    if (m_rxEvent) {
        CloseHandle(m_rxEvent);
        m_rxEvent = nullptr;
    }
    DeleteCriticalSection(&m_payloadLock);
    DeleteCriticalSection(&m_pendingLock);
    CWnd::OnDestroy();
}

void CRxPanel::SetDevice(HidDevice* device) {
    m_device = device;
}

void CRxPanel::StartCapture() {
    if (m_thread) {
        return;
    }
    m_rxState = RxIdle;
    m_stopRequested = false;
    m_thread = AfxBeginThread(RxThreadProc, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    if (!m_thread) {
        return;
    }
    m_thread->m_bAutoDelete = FALSE;
    m_thread->ResumeThread();
}

void CRxPanel::StopCapture() {
    DWORD wait_result = WAIT_OBJECT_0;

    m_stopRequested = true;
    if (m_thread) {
        wait_result = WaitForSingleObject(m_thread->m_hThread, 1500);
        if (wait_result == WAIT_TIMEOUT && m_device) {
            m_device->CancelPendingIo();
            wait_result = WaitForSingleObject(m_thread->m_hThread, 1500);
        }
        if (wait_result == WAIT_OBJECT_0) {
            delete m_thread;
            m_thread = nullptr;
        }
    }
    m_rxState = RxIdle;
}

UINT CRxPanel::RxThreadProc(LPVOID param) {
    CRxPanel* panel = reinterpret_cast<CRxPanel*>(param);
    return panel ? panel->ThreadLoop() : 0;
}

UINT CRxPanel::ThreadLoop() {
    while (!m_stopRequested) {
        if (!m_device || !m_device->IsOpen()) {
            Sleep(100);
            continue;
        }

        std::wstring error;
        std::vector<unsigned char> report;
        bool ok = m_device->ReadInputReport(&report, 200, &error);
        if (!ok) {
            if (error == L"Device removed." ||
                error == L"TCP socket closed." ||
                error == L"TCP receive failed.") {
                CWnd* parent = GetParent();
                if (parent) {
                    parent->PostMessage(MessageId(), static_cast<WPARAM>(RxDeviceRemoved), 0);
                }
                m_stopRequested = true;
            }
            continue;
        }

        std::array<unsigned char, 64> payload = ExtractPayload(report);
        if (m_logEnabled.load()) {
            std::wstring hex = BytesToHex(payload.data(), payload.size());
            WriteDirectionalLogLine(L"RX", hex, m_timestampEnabled.load());
        }
        UpdateLastPayload(report);
        EnterCriticalSection(&m_pendingLock);
        m_pendingPayloads.push_back(payload);
        LeaveCriticalSection(&m_pendingLock);

        CWnd* parent = GetParent();
        if (parent) {
            parent->PostMessage(MessageId(), static_cast<WPARAM>(RxCount), 0);
        }
    }
    return 0;
}

std::array<unsigned char, 64> CRxPanel::ExtractPayload(const std::vector<unsigned char>& report) const {
    std::array<unsigned char, 64> payload{};
    size_t offset = 0;
    size_t available = report.size();
    if (available == 65) {
        offset = 1;
        available = 64;
    }
    size_t copy_len = std::min<size_t>(64, available);
    if (copy_len > 0 && offset < report.size()) {
        std::copy_n(report.data() + offset, copy_len, payload.begin());
    }
    return payload;
}

void CRxPanel::UpdateLastPayload(const std::vector<unsigned char>& report) {
    std::vector<unsigned char> payload(64, 0x00);
    size_t offset = 0;
    size_t available = report.size();
    if (available == 65) {
        offset = 1;
        available = 64;
    }
    size_t copy_len = std::min<size_t>(64, available);
    if (copy_len > 0 && offset < report.size()) {
        std::copy_n(report.data() + offset, copy_len, payload.begin());
    }

    EnterCriticalSection(&m_payloadLock);
    m_lastPayload.swap(payload);
    LeaveCriticalSection(&m_payloadLock);

    if (m_rxEvent) {
        SetEvent(m_rxEvent);
    }
}

void CRxPanel::AppendRow(const std::array<unsigned char, 64>& payload, const std::wstring& timestamp) {
    Row row;
    row.data = payload;
    row.timestamp = timestamp;
    row.valid = true;
    m_rows.push_back(row);
    while (m_rows.size() > 10) {
        m_rows.pop_front();
    }
}

std::wstring CRxPanel::FormatTimestamp() const {
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%02u:%02u:%02u.%03u", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    return buffer;
}

void CRxPanel::OnSaveLogClicked() {
    bool enabled = (m_saveCheck.GetCheck() == BST_CHECKED);
    m_logEnabled = enabled;
    m_logger.Enable(enabled);
}

void CRxPanel::OnTimestampClicked() {
    m_timestampEnabled = (m_timestampCheck.GetCheck() == BST_CHECKED);
    InvalidateRect(m_headerRect, FALSE);
    InvalidateRect(m_gridRect, FALSE);
}

void CRxPanel::OnClearClicked() {
    ClearData();
}

void CRxPanel::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kUiTimerId) {
        ProcessPendingPayloads();
        return;
    }
    CWnd::OnTimer(nIDEvent);
}

bool CRxPanel::WaitForPayload(const std::vector<unsigned char>& payload, DWORD timeout_ms) {
    if (!m_rxEvent) {
        return false;
    }
    if (payload.size() < 64) {
        return false;
    }
    DWORD remaining = timeout_ms;
    DWORD start_tick = GetTickCount();
    while (remaining > 0) {
        DWORD wait = WaitForSingleObject(m_rxEvent, remaining);
        if (wait == WAIT_TIMEOUT) {
            return false;
        }
        if (wait != WAIT_OBJECT_0) {
            return false;
        }
        std::vector<unsigned char> last;
        EnterCriticalSection(&m_payloadLock);
        last = m_lastPayload;
        LeaveCriticalSection(&m_payloadLock);
        if (last.size() >= 64 && std::equal(payload.begin(), payload.begin() + 64, last.begin())) {
            return true;
        }
        DWORD now = GetTickCount();
        DWORD elapsed = now - start_tick;
        if (elapsed >= timeout_ms) {
            return false;
        }
        remaining = timeout_ms - elapsed;
    }
    return false;
}

bool CRxPanel::GetLastPayloadHex(std::wstring* out_text) {
    if (!out_text) {
        return false;
    }
    std::vector<unsigned char> payload;
    EnterCriticalSection(&m_payloadLock);
    payload = m_lastPayload;
    LeaveCriticalSection(&m_payloadLock);

    if (payload.size() < 64) {
        out_text->clear();
        return false;
    }
    *out_text = BytesToHex(payload.data(), 64);
    return true;
}

void CRxPanel::LogTxLine(const std::wstring& line) {
    if (line.empty()) {
        return;
    }
    WriteDirectionalLogLine(L"TX", line, m_timestampEnabled.load());
}

void CRxPanel::PushExternalPayload(const std::vector<unsigned char>& payload) {
    std::array<unsigned char, 64> bytes;
    std::wstring hex;
    CWnd* parent;

    bytes = ExtractPayload(payload);
    if (m_logEnabled.load()) {
        hex = BytesToHex(bytes.data(), bytes.size());
        WriteDirectionalLogLine(L"RX", hex, m_timestampEnabled.load());
    }
    UpdateLastPayload(payload);
    EnterCriticalSection(&m_pendingLock);
    m_pendingPayloads.push_back(bytes);
    LeaveCriticalSection(&m_pendingLock);

    parent = GetParent();
    if (parent) {
        parent->PostMessage(MessageId(), static_cast<WPARAM>(RxCount), 0);
    }
}

bool CRxPanel::GetLastPayload(std::vector<unsigned char>* out_payload) {
    if (!out_payload) {
        return false;
    }
    EnterCriticalSection(&m_payloadLock);
    *out_payload = m_lastPayload;
    LeaveCriticalSection(&m_payloadLock);

    if (out_payload->size() < 64) {
        out_payload->clear();
        return false;
    }
    if (out_payload->size() > 64) {
        out_payload->resize(64);
    }
    return true;
}

bool CRxPanel::IsLogEnabled() const {
    return m_saveCheck.GetCheck() == BST_CHECKED;
}

bool CRxPanel::IsTimestampEnabled() const {
    return m_timestampCheck.GetCheck() == BST_CHECKED;
}

void CRxPanel::SetLogEnabled(bool enabled) {
    m_saveCheck.SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
    m_logEnabled = enabled;
    m_logger.Enable(enabled);
}

void CRxPanel::SetTimestampEnabled(bool enabled) {
    m_timestampCheck.SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
    m_timestampEnabled = enabled;
    InvalidateRect(m_headerRect, FALSE);
    InvalidateRect(m_gridRect, FALSE);
}

void CRxPanel::SetExternalGridMetrics(int start_x, int cell_base, int cell_extra, int row_height, int row_gap) {
    if (start_x <= 0 || cell_base <= 0) {
        return;
    }
    m_hasExternalGrid = true;
    m_externalStartX = start_x;
    m_externalCellBase = cell_base;
    m_externalCellExtra = cell_extra < 0 ? 0 : cell_extra;
    m_externalRowHeight = row_height;
    m_externalRowGap = row_gap;

    if (GetSafeHwnd()) {
        CRect rect;
        GetClientRect(&rect);
        LayoutControls(rect.Width(), rect.Height());
        Invalidate(FALSE);
    }
}

void CRxPanel::SetControlsEnabled(bool enabled) {
    m_saveCheck.EnableWindow(TRUE);
    m_timestampCheck.EnableWindow(TRUE);
    (void)enabled;
}

void CRxPanel::SetClearEnabled(bool enabled) {
    m_clearButton.EnableWindow(enabled);
}

void CRxPanel::ClearData() {
    m_rows.clear();
    m_lastPayload.clear();
    m_cleared = true;
    EnterCriticalSection(&m_pendingLock);
    m_pendingPayloads.clear();
    LeaveCriticalSection(&m_pendingLock);
    UpdateGridControls();
    SetRxState(RxIdle);
    Invalidate(FALSE);
}

void CRxPanel::ProcessPendingPayloads() {
    std::deque<std::array<unsigned char, 64>> batch;
    EnterCriticalSection(&m_pendingLock);
    if (!m_pendingPayloads.empty()) {
        batch.swap(m_pendingPayloads);
    }
    LeaveCriticalSection(&m_pendingLock);

    if (batch.empty()) {
        return;
    }

    bool timestamped = m_timestampEnabled.load();
    for (const auto& bytes : batch) {
        m_cleared = false;
        std::wstring timestamp;
        if (timestamped) {
            timestamp = FormatTimestamp();
        }
        AppendRow(bytes, timestamp);
    }

    UpdateGridControls();
    InvalidateRect(m_gridRect, FALSE);
}

void CRxPanel::WriteDirectionalLogLine(const std::wstring& direction, const std::wstring& body, bool timestamped) {
    if (!m_logger.IsEnabled() || direction.empty() || body.empty()) {
        return;
    }
    std::wstring line;
    if (timestamped) {
        line = L"[" + FormatTimestamp() + L"] [" + direction + L"] [" + body + L"]";
    } else {
        line = L"[" + direction + L"] [" + body + L"]";
    }
    std::wstring error;
    m_logger.WriteLine(line, &error);
}

void CRxPanel::UpdateGridControls() {
    if (!m_cells[0][0].GetSafeHwnd()) {
        return;
    }
    size_t row_count = m_rows.size();
    for (int row = 0; row < kRowCount; ++row) {
        bool valid = row < static_cast<int>(row_count) && m_rows[static_cast<size_t>(row)].valid;
        for (int col = 0; col < kByteCount; ++col) {
            if (!valid) {
                m_cells[row][col].SetWindowTextW(L"");
                continue;
            }
            unsigned char value = m_rows[static_cast<size_t>(row)].data[static_cast<size_t>(col)];
            wchar_t buf[4] = {};
            swprintf_s(buf, L"%02X", value);
            m_cells[row][col].SetWindowTextW(buf);
        }
    }
}

void CRxPanel::SetRxState(int state) {
    if (m_rxState == state) {
        return;
    }
    m_rxState = state;
    InvalidateRect(m_headerRect, FALSE);
}

int CRxPanel::GetPreferredHeight() const {
    const int margin = 10;
    const int title_height = 16;
    const int gap = 8;
    const int top_height = 24;

    int header_height = m_headerHeight;
    int row_height = m_rowHeight;
    int row_gap = m_rowGap;
    if (header_height <= 0 || row_height <= 0) {
        CClientDC dc(const_cast<CRxPanel*>(this));
        CFont* old = dc.SelectObject(const_cast<CFont*>(&m_monoFont));
        TEXTMETRIC tm = {};
        if (dc.GetTextMetrics(&tm)) {
            header_height = tm.tmHeight + 4;
            row_height = tm.tmHeight + 6;
        } else {
            header_height = 16;
            row_height = 24;
        }
        dc.SelectObject(old);
    }

    int total = margin + title_height;
    total += top_height + gap;
    total += header_height + 4;
    total += row_height * 10 + row_gap * 9;
    return total;
}
