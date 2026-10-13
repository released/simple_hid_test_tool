#include "tx_panel.h"

#include <cwctype>
#include <cstdlib>
#include <sstream>

#include <commctrl.h>

#pragma comment(lib, "msimg32.lib")

namespace {

const int kStartId = 4001;
const int kSingleShotId = 4002;
const int kIntervalComboId = 4003;
const int kIntervalCheckId = 4004;
const int kLimitCheckId = 4007;
const int kLimitEditId = 4008;
const int kSelectAllId = 4010;
const int kSaveAsId = 4011;
const int kReloadId = 4012;

const int kSendCheckBase = 4100;
const int kRestoreBase = 4300;
const int kSendOnceBase = 4400;
const int kByteEditBase = 5000;

const int kRowCount = 6;
const int kByteCount = 64;

bool IsSeparator(wchar_t ch) {
    return iswspace(ch) || ch == L',' || ch == L';';
}

bool ParseByteList(const std::wstring& text, std::vector<unsigned char>* out_bytes, std::wstring* error) {
    if (!out_bytes) {
        return false;
    }
    out_bytes->clear();
    std::wstring token;
    auto flush = [&](void) -> bool {
        if (token.empty()) {
            return true;
        }
        unsigned char value = 0;
        std::wstring local_error;
        if (!ParseSingleByte(token, &value, &local_error)) {
            if (error) {
                *error = local_error;
            }
            return false;
        }
        out_bytes->push_back(value);
        token.clear();
        return true;
    };

    for (wchar_t ch : text) {
        if (IsSeparator(ch)) {
            if (!flush()) {
                return false;
            }
        } else {
            token.push_back(ch);
        }
    }
    if (!flush()) {
        return false;
    }
    if (out_bytes->empty()) {
        if (error) {
            *error = L"Empty input.";
        }
        return false;
    }
    if (out_bytes->size() > kByteCount) {
        if (error) {
            *error = L"Too many bytes (max 64).";
        }
        return false;
    }
    return true;
}

}

BEGIN_MESSAGE_MAP(CHexByteEdit, CEdit)
    ON_WM_CHAR()
    ON_WM_KEYDOWN()
    ON_WM_KILLFOCUS()
    ON_WM_PASTE()
END_MESSAGE_MAP()

void CHexByteEdit::SetContext(CTxPanel* owner, int row, int col) {
    owner_ = owner;
    row_ = row;
    col_ = col;
}

void CHexByteEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) {
    if (nChar == VK_BACK) {
        CEdit::OnChar(nChar, nRepCnt, nFlags);
        return;
    }
    if ((nChar >= L'0' && nChar <= L'9') || (nChar >= L'a' && nChar <= L'f') || (nChar >= L'A' && nChar <= L'F')) {
        int start = 0;
        int end = 0;
        GetSel(start, end);
        int len = GetWindowTextLengthW();
        if (len >= 2 && start == end) {
            MessageBeep(MB_OK);
            return;
        }
        wint_t upper = towupper(static_cast<wint_t>(nChar));
        wchar_t ch = static_cast<wchar_t>(upper);
        CEdit::OnChar(static_cast<UINT>(ch), nRepCnt, nFlags);
        if (owner_) {
            owner_->PostMessage(CTxPanel::MessageId(), static_cast<WPARAM>(CTxPanel::ActionSettingsChanged), 0);
        }
        if (GetWindowTextLengthW() >= 2 && owner_) {
            owner_->FocusNextByte(row_, col_, true);
        }
        return;
    }
    if (nChar == VK_RETURN || nChar == VK_TAB) {
        if (owner_) {
            owner_->FocusNextByte(row_, col_, true);
        }
        return;
    }
    MessageBeep(MB_OK);
}

void CHexByteEdit::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
    if (nChar == VK_TAB || nChar == VK_RETURN) {
        bool forward = (GetKeyState(VK_SHIFT) & 0x8000) == 0;
        if (owner_) {
            owner_->FocusNextByte(row_, col_, forward);
        }
        return;
    }
    CEdit::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CHexByteEdit::OnKillFocus(CWnd* pNewWnd) {
    CString text;
    GetWindowTextW(text);
    text.Trim();
    if (text.IsEmpty()) {
        SetWindowTextW(L"00");
    } else if (text.GetLength() == 1) {
        CString padded;
        padded.Format(L"0%1s", text.GetString());
        SetWindowTextW(padded);
    }
    CEdit::OnKillFocus(pNewWnd);
}

void CHexByteEdit::OnPaste() {
    if (!owner_) {
        Default();
        return;
    }
    if (!OpenClipboard()) {
        return;
    }
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (!data) {
        CloseClipboard();
        return;
    }
    const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(data));
    if (!text) {
        CloseClipboard();
        return;
    }
    std::wstring content(text);
    GlobalUnlock(data);
    CloseClipboard();

    if (!owner_->PasteBytes(row_, col_, content)) {
        MessageBeep(MB_OK);
        return;
    }
    owner_->PostMessage(CTxPanel::MessageId(), static_cast<WPARAM>(CTxPanel::ActionSettingsChanged), 0);
}

BEGIN_MESSAGE_MAP(CTxPanel, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_BN_CLICKED(kStartId, &CTxPanel::OnStartClicked)
    ON_BN_CLICKED(kSingleShotId, &CTxPanel::OnSingleShotClicked)
    ON_CBN_SELCHANGE(kIntervalComboId, &CTxPanel::OnIntervalChanged)
    ON_BN_CLICKED(kIntervalCheckId, &CTxPanel::OnIntervalCheck)
    ON_BN_CLICKED(kLimitCheckId, &CTxPanel::OnLimitCheck)
    ON_EN_CHANGE(kLimitEditId, &CTxPanel::OnLimitChanged)
    ON_EN_KILLFOCUS(kLimitEditId, &CTxPanel::OnLimitKillFocus)
    ON_BN_CLICKED(kSaveAsId, &CTxPanel::OnSaveAsClicked)
    ON_BN_CLICKED(kReloadId, &CTxPanel::OnReloadClicked)
    ON_BN_CLICKED(kSelectAllId, &CTxPanel::OnSelectAllClicked)
    ON_COMMAND_RANGE(kSendCheckBase, kSendCheckBase + kRowCount - 1, &CTxPanel::OnRowCheckChanged)
    ON_COMMAND_RANGE(kRestoreBase, kRestoreBase + kRowCount - 1, &CTxPanel::OnRestoreClicked)
    ON_COMMAND_RANGE(kSendOnceBase, kSendOnceBase + kRowCount - 1, &CTxPanel::OnSendOnceClicked)
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

bool CTxPanel::Create(CWnd* parent) {
    CString class_name = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), nullptr);
    return CWnd::CreateEx(0, class_name, L"TxPanel",
                          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          CRect(0, 0, 0, 0), parent, 0);
}

int CTxPanel::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    m_hexPrefix = false;
    m_running = false;

    CClientDC dc(this);
    int font_height = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);
    m_uiFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_monoFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           FIXED_PITCH | FF_MODERN, L"Consolas");

    auto set_icon = [](CButton& button, CImageList& list, HICON icon) {
        list.Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
        list.Add(icon);
        BUTTON_IMAGELIST bi = {};
        bi.himl = list.GetSafeHandle();
        bi.uAlign = BUTTON_IMAGELIST_ALIGN_LEFT;
        bi.margin.left = 2;
        bi.margin.right = 2;
        ::SendMessageW(button.GetSafeHwnd(), BCM_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(&bi));
    };

    m_startButton.Create(L"Start TX", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         CRect(0, 0, 0, 0), this, kStartId);
    m_startButton.SetFont(&m_uiFont);
    set_icon(m_startButton, m_iconStart, LoadIconW(nullptr, IDI_APPLICATION));

    m_singleShotButton.Create(L"Single-shot", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              CRect(0, 0, 0, 0), this, kSingleShotId);
    m_singleShotButton.SetFont(&m_uiFont);
    set_icon(m_singleShotButton, m_iconSingle, LoadIconW(nullptr, IDI_QUESTION));

    m_intervalCombo.Create(WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                           CRect(0, 0, 0, 0), this, kIntervalComboId);
    m_intervalCombo.SetFont(&m_uiFont);
    const int intervals[] = { 50, 100, 250, 500, 1000, 2000, 5000 };
    for (int value : intervals) {
        CString text;
        text.Format(L"%d ms", value);
        m_intervalCombo.AddString(text);
    }
    m_intervalCombo.SetCurSel(1);
    m_intervalCombo.EnableWindow(FALSE);

    m_intervalCheck.Create(L"Set interval", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                           CRect(0, 0, 0, 0), this, kIntervalCheckId);
    m_intervalCheck.SetFont(&m_uiFont);

    m_saveAsButton.Create(L"Save As", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          CRect(0, 0, 0, 0), this, kSaveAsId);
    m_saveAsButton.SetFont(&m_uiFont);
    m_reloadButton.Create(L"Load", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          CRect(0, 0, 0, 0), this, kReloadId);
    m_reloadButton.SetFont(&m_uiFont);

    m_limitCheck.Create(L"Limit TX", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                        CRect(0, 0, 0, 0), this, kLimitCheckId);
    m_limitCheck.SetFont(&m_uiFont);
    m_limitEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
                       CRect(0, 0, 0, 0), this, kLimitEditId);
    m_limitEdit.SetFont(&m_monoFont);
    m_limitEdit.SetWindowTextW(L"10");
    m_limitSpin.Create(WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS,
                       CRect(0, 0, 0, 0), this, 0);
    m_limitSpin.SetBuddy(&m_limitEdit);
    m_limitSpin.SetRange32(5, 9999);
    m_limitSpin.SetPos32(10);
    m_limitStatus.Create(L"Sent: 0  Left: 0", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_limitStatus.SetFont(&m_uiFont);

    m_selectAllCheck.Create(L"Sel. all", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                            CRect(0, 0, 0, 0), this, kSelectAllId);
    m_selectAllCheck.SetFont(&m_uiFont);

    for (int i = 0; i < kRowCount; ++i) {
        CString label;
        label.Format(L"Send %d", i + 1);
        m_rows[i].sendCheck.Create(label, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                   CRect(0, 0, 0, 0), this, kSendCheckBase + i);
        m_rows[i].sendCheck.SetFont(&m_uiFont);

        for (int j = 0; j < kByteCount; ++j) {
            int id = kByteEditBase + i * kByteCount + j;
            m_rows[i].bytes[j].Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_UPPERCASE | WS_TABSTOP,
                                      CRect(0, 0, 0, 0), this, id);
            m_rows[i].bytes[j].SetFont(&m_monoFont);
            m_rows[i].bytes[j].SetContext(this, i, j);
        }

        m_rows[i].restoreButton.Create(L"Default", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       CRect(0, 0, 0, 0), this, kRestoreBase + i);
        m_rows[i].restoreButton.SetFont(&m_uiFont);
        set_icon(m_rows[i].restoreButton, m_iconRestore, LoadIconW(nullptr, IDI_WARNING));

        m_rows[i].sendOnceButton.Create(L"Send Once", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        CRect(0, 0, 0, 0), this, kSendOnceBase + i);
        m_rows[i].sendOnceButton.SetFont(&m_uiFont);
        set_icon(m_rows[i].sendOnceButton, m_iconSend, LoadIconW(nullptr, IDI_APPLICATION));

        RestoreDefault(i);
    }

    m_tooltip.Create(this, TTS_ALWAYSTIP);
    m_tooltip.SetMaxTipWidth(260);
    m_tooltip.AddTool(&m_startButton, L"Start / Stop TX schedule");
    m_tooltip.AddTool(&m_singleShotButton, L"Send one TX cycle");
    m_tooltip.AddTool(&m_intervalCombo, L"Interval in milliseconds");
    m_tooltip.AddTool(&m_intervalCheck, L"Use selected interval (unchecked = 100 ms)");
    m_tooltip.AddTool(&m_limitCheck, L"Limit TX cycles");
    m_tooltip.AddTool(&m_limitEdit, L"TX cycle count");
    m_tooltip.AddTool(&m_saveAsButton, L"Save TX commands to file");
    m_tooltip.AddTool(&m_reloadButton, L"Load TX commands from selected INI file");
    m_tooltip.AddTool(&m_selectAllCheck, L"Select or clear all TX rows");
    for (int i = 0; i < kRowCount; ++i) {
        CString tip;
        tip.Format(L"Enable row %d in scheduler", i + 1);
        m_tooltip.AddTool(&m_rows[i].sendCheck, tip);
        tip.Format(L"Reset row %d to 0xFF", i + 1);
        m_tooltip.AddTool(&m_rows[i].restoreButton, tip);
        tip.Format(L"Send row %d once immediately", i + 1);
        m_tooltip.AddTool(&m_rows[i].sendOnceButton, tip);
    }

    return 0;
}

void CTxPanel::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CTxPanel::LayoutControls(int cx, int cy) {
    const int margin = 10;
    const int header_height = 22;
    const int header_gap = 8;
    const int gap = 6;
    const int sched_height = 28;
    const int button_width = 98;
    const int small_button_width = 78;
    const int combo_width = 86;
    const int limit_check_width = 90;
    const int limit_edit_width = 50;
    const int limit_spin_width = 16;
    const int check_width = 70;

    (void)cy;
    int width = cx - margin * 2;
    if (width < 0) {
        width = 0;
    }

    int y = margin + header_height + header_gap;
    int x = margin;
    m_startButton.MoveWindow(x, y, button_width, sched_height);
    x += button_width + gap;
    m_singleShotButton.MoveWindow(x, y, button_width, sched_height);
    x += button_width + gap;

    m_intervalCheck.MoveWindow(x, y, 100, sched_height);
    x += 100 + gap;
    int item_height = m_intervalCombo.GetItemHeight(0);
    if (item_height <= 0) {
        item_height = sched_height - 8;
    }
    int combo_drop_height = sched_height + item_height * 7 + 6;
    m_intervalCombo.MoveWindow(x, y, combo_width, combo_drop_height);
    x += combo_width + gap;

    m_saveAsButton.MoveWindow(x, y, small_button_width, sched_height);
    x += small_button_width + gap;
    m_reloadButton.MoveWindow(x, y, small_button_width, sched_height);

    CClientDC dc(this);
    y += sched_height + gap;

    const int limit_height = sched_height - 6;
    int limit_x = margin;
    m_limitCheck.MoveWindow(limit_x, y, limit_check_width, limit_height);
    limit_x += limit_check_width + gap;
    m_limitEdit.MoveWindow(limit_x, y, limit_edit_width, limit_height);
    m_limitSpin.MoveWindow(limit_x + limit_edit_width - limit_spin_width, y, limit_spin_width, limit_height);
    limit_x += limit_edit_width + gap;
    int limit_status_width = width - (limit_x - margin);
    if (limit_status_width < 80) {
        limit_status_width = 80;
    }
    m_limitStatus.MoveWindow(limit_x, y + 2, limit_status_width, limit_height);
    y += limit_height + gap;

    dc.SelectObject(&m_monoFont);
    TEXTMETRIC tm = {};
    dc.GetTextMetrics(&tm);
    m_headerHeight = tm.tmHeight + 4;
    m_rowHeight = tm.tmHeight + 6;

    int edit_width = width - check_width - m_rowButtonWidth * 2 - gap * 3;
    if (edit_width < 120) {
        edit_width = 120;
    }
    m_dataStartX = margin + check_width + gap;
    m_cellWidthBase = edit_width / kByteCount;
    m_cellWidthExtra = edit_width % kByteCount;
    if (m_cellWidthBase < 8) {
        m_cellWidthBase = 8;
        m_cellWidthExtra = 0;
        edit_width = m_cellWidthBase * kByteCount;
    }
    int data_end = m_dataStartX + edit_width;

    m_headerRect = CRect(m_dataStartX, y, data_end, y + m_headerHeight);
    m_selectAllCheck.MoveWindow(margin, y, check_width, m_headerHeight);

    y += m_headerHeight + gap;
    m_rowsTop = y;
    m_rowGap = gap;
    for (int i = 0; i < kRowCount; ++i) {
        int row_x = margin;
        m_rows[i].sendCheck.MoveWindow(row_x, y, check_width, m_rowHeight);
        row_x = m_dataStartX;

        int x_pos = row_x;
        for (int j = 0; j < kByteCount; ++j) {
            int cell_width = m_cellWidthBase + (j < m_cellWidthExtra ? 1 : 0);
            m_rows[i].bytes[j].MoveWindow(x_pos, y, cell_width, m_rowHeight);
            x_pos += cell_width;
            if (x_pos >= data_end) {
                break;
            }
        }

        int button_x = data_end + gap;
        m_rows[i].restoreButton.MoveWindow(button_x, y, m_rowButtonWidth, m_rowHeight);
        button_x += m_rowButtonWidth + gap;
        m_rows[i].sendOnceButton.MoveWindow(button_x, y, m_rowButtonWidth, m_rowHeight);
        button_x += m_rowButtonWidth + gap;

        y += m_rowHeight + gap;
    }
}

void CTxPanel::OnPaint() {
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

    const int header_height = 22;
    CRect titleRect(rect.left + 10, rect.top + 4, rect.right - 10, rect.top + 4 + header_height);
    dc.SetBkMode(TRANSPARENT);
    dc.SelectObject(&m_uiFont);
    dc.SetTextColor(RGB(15, 23, 42));
    dc.DrawText(L"Command Send", titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (m_headerRect.Width() > 0) {
        dc.SelectObject(&m_monoFont);
        dc.SetTextColor(RGB(70, 75, 85));

        CRect header_band(m_headerRect);
        dc.FillSolidRect(&header_band, RGB(226, 232, 240));

        CPen hpen(PS_SOLID, 1, RGB(203, 213, 225));
        CPen* old_hpen = dc.SelectObject(&hpen);
        dc.MoveTo(m_headerRect.left, m_headerRect.bottom - 1);
        dc.LineTo(m_headerRect.right, m_headerRect.bottom - 1);
        dc.SelectObject(old_hpen);

        int x = m_headerRect.left;
        for (int i = 0; i < kByteCount; ++i) {
            int cell_width = m_cellWidthBase + (i < m_cellWidthExtra ? 1 : 0);
            CRect cell(x, m_headerRect.top, x + cell_width, m_headerRect.bottom);
            wchar_t buf[4] = {};
            swprintf_s(buf, L"%u", i);
            dc.DrawText(buf, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            x += cell_width;
            if (x >= m_headerRect.right) {
                break;
            }
        }
    }

    const int margin = 10;
    const int gap = 6;
    int y = m_headerRect.bottom + gap + m_rowHeight;
    CPen pen(PS_SOLID, 1, RGB(203, 213, 225));
    CPen* old_pen = dc.SelectObject(&pen);
    for (int i = 1; i < kRowCount; ++i) {
        int line_y = y - gap / 2;
        dc.MoveTo(rect.left + 10, line_y);
        dc.LineTo(rect.right - 10, line_y);
        y += m_rowHeight + gap;
    }
    dc.SelectObject(old_pen);
}

BOOL CTxPanel::PreTranslateMessage(MSG* pMsg) {
    if (m_tooltip.GetSafeHwnd()) {
        m_tooltip.RelayEvent(pMsg);
    }
    return CWnd::PreTranslateMessage(pMsg);
}

bool CTxPanel::IsRowEnabled(int row) const {
    if (row < 0 || row >= kRowCount) {
        return false;
    }
    return m_rows[row].sendCheck.GetCheck() == BST_CHECKED;
}

bool CTxPanel::GetRowPayload(int row, std::vector<unsigned char>* out_bytes, std::wstring* error) const {
    if (row < 0 || row >= kRowCount) {
        if (error) {
            *error = L"Invalid row index.";
        }
        return false;
    }
    return GetRowBytes(row, out_bytes, error);
}

std::wstring CTxPanel::GetRowText(int row) const {
    if (row < 0 || row >= kRowCount) {
        return L"";
    }
    std::vector<unsigned char> bytes;
    std::wstring error;
    if (!GetRowBytes(row, &bytes, &error)) {
        return L"";
    }
    return FormatCommandBytes(bytes, m_hexPrefix);
}

void CTxPanel::SetRowText(int row, const std::wstring& text) {
    if (row < 0 || row >= kRowCount) {
        return;
    }
    std::vector<unsigned char> bytes;
    std::wstring error;
    if (!ParseCommandBytes(text, &bytes, &error)) {
        return;
    }
    if (bytes.size() < kByteCount) {
        bytes.resize(kByteCount, 0xFF);
    } else if (bytes.size() > kByteCount) {
        bytes.resize(kByteCount);
    }
    SetRowBytes(row, bytes);
}

void CTxPanel::SetRowEnabled(int row, bool enabled) {
    if (row < 0 || row >= kRowCount) {
        return;
    }
    m_rows[row].sendCheck.SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
    UpdateSelectAllState();
}

void CTxPanel::SetSchedulerEnabled(bool enabled) {
    m_connected = enabled;
    UpdateEnableStates();
}

void CTxPanel::SetRunning(bool running) {
    m_running = running;
    m_startButton.SetWindowTextW(running ? L"Stop TX" : L"Start TX");
    UpdateEnableStates();
}

bool CTxPanel::IsHexPrefixMode() const {
    return false;
}

void CTxPanel::SetHexPrefixMode(bool enabled) {
    (void)enabled;
    m_hexPrefix = false;
}

DWORD CTxPanel::GetIntervalValue() const {
    int sel = m_intervalCombo.GetCurSel();
    if (sel == CB_ERR) {
        return 100;
    }
    CString text;
    m_intervalCombo.GetLBText(sel, text);
    return static_cast<DWORD>(_wtoi(text));
}

bool CTxPanel::IsIntervalEnabled() const {
    return m_intervalCheck.GetCheck() == BST_CHECKED;
}

void CTxPanel::SetIntervalValue(DWORD value) {
    int count = m_intervalCombo.GetCount();
    for (int i = 0; i < count; ++i) {
        CString text;
        m_intervalCombo.GetLBText(i, text);
        if (static_cast<DWORD>(_wtoi(text)) == value) {
            m_intervalCombo.SetCurSel(i);
            return;
        }
    }
    m_intervalCombo.SetCurSel(1);
}

void CTxPanel::SetIntervalEnabled(bool enabled) {
    m_intervalCheck.SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
    UpdateEnableStates();
}

bool CTxPanel::IsLimitEnabled() const {
    return m_limitCheck.GetCheck() == BST_CHECKED;
}

int CTxPanel::GetLimitCount() const {
    CString text;
    m_limitEdit.GetWindowTextW(text);
    int value = _wtoi(text);
    if (value < 5) {
        value = 5;
    }
    return value;
}

void CTxPanel::SetLimitEnabled(bool enabled) {
    m_limitCheck.SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
    if (enabled) {
        int count = GetLimitCount();
        m_limitSent = 0;
        m_limitRemaining = count;
        SetLimitStatus(m_limitSent, m_limitRemaining);
    } else {
        m_limitSent = 0;
        m_limitRemaining = 0;
        SetLimitStatus(m_limitSent, m_limitRemaining);
    }
    UpdateEnableStates();
}

void CTxPanel::SetLimitCount(int count) {
    if (count < 5) {
        count = 5;
    }
    CString text;
    text.Format(L"%d", count);
    m_limitEdit.SetWindowTextW(text);
    if (m_limitSpin.GetSafeHwnd()) {
        m_limitSpin.SetPos32(count);
    }
    if (IsLimitEnabled()) {
        m_limitSent = 0;
        m_limitRemaining = count;
        SetLimitStatus(m_limitSent, m_limitRemaining);
    }
}

void CTxPanel::SetLimitStatus(int sent, int remaining) {
    m_limitSent = sent < 0 ? 0 : sent;
    m_limitRemaining = remaining < 0 ? 0 : remaining;
    if (m_limitStatus.GetSafeHwnd()) {
        CString text;
        text.Format(L"Sent: %d  Left: %d", m_limitSent, m_limitRemaining);
        m_limitStatus.SetWindowTextW(text);
    }
    UpdateEnableStates();
}

void CTxPanel::GetGridMetrics(int* out_start_x, int* out_cell_base, int* out_cell_extra,
                              int* out_row_height, int* out_row_gap) const {
    if (out_start_x) {
        *out_start_x = m_dataStartX;
    }
    if (out_cell_base) {
        *out_cell_base = m_cellWidthBase;
    }
    if (out_cell_extra) {
        *out_cell_extra = m_cellWidthExtra;
    }
    if (out_row_height) {
        *out_row_height = m_rowHeight;
    }
    if (out_row_gap) {
        *out_row_gap = m_rowGap;
    }
}

void CTxPanel::OnStartClicked() {
    NotifyParent(m_running ? ActionStop : ActionStart, 0);
}

void CTxPanel::OnSingleShotClicked() {
    NotifyParent(ActionSingleShot, 0);
}

void CTxPanel::OnIntervalChanged() {
    NotifyParent(ActionSettingsChanged, 0);
}

void CTxPanel::OnIntervalCheck() {
    UpdateEnableStates();
    NotifyParent(ActionSettingsChanged, 0);
}

void CTxPanel::OnSaveAsClicked() {
    NotifyParent(ActionSaveAs, 0);
}

void CTxPanel::OnReloadClicked() {
    NotifyParent(ActionReload, 0);
}

void CTxPanel::OnLimitChanged() {
    CString text;
    m_limitEdit.GetWindowTextW(text);
    if (text.IsEmpty()) {
        return;
    }
    int value = _wtoi(text);
    if (value < 5) {
        value = 5;
    }
    if (IsLimitEnabled()) {
        m_limitSent = 0;
        m_limitRemaining = value;
        SetLimitStatus(m_limitSent, m_limitRemaining);
    }
    NotifyParent(ActionSettingsChanged, 0);
}

void CTxPanel::OnLimitCheck() {
    bool enabled = (m_limitCheck.GetCheck() == BST_CHECKED);
    SetLimitEnabled(enabled);
    NotifyParent(ActionSettingsChanged, 0);
}

void CTxPanel::OnLimitKillFocus() {
    int value = GetLimitCount();
    SetLimitCount(value);
}

void CTxPanel::OnSelectAllClicked() {
    if (m_syncingSelectAll) {
        return;
    }
    bool checked = (m_selectAllCheck.GetCheck() == BST_CHECKED);
    m_syncingSelectAll = true;
    for (int i = 0; i < kRowCount; ++i) {
        m_rows[i].sendCheck.SetCheck(checked ? BST_CHECKED : BST_UNCHECKED);
    }
    UpdateEnableStates();
    UpdateSelectAllState();
    m_syncingSelectAll = false;
    NotifyParent(ActionSettingsChanged, 0);
}

void CTxPanel::OnRowCheckChanged(UINT id) {
    int row = static_cast<int>(id) - kSendCheckBase;
    UpdateSelectAllState();
    NotifyParent(ActionSettingsChanged, 0);
}

void CTxPanel::OnRestoreClicked(UINT id) {
    int row = static_cast<int>(id) - kRestoreBase;
    RestoreDefault(row);
}

void CTxPanel::OnSendOnceClicked(UINT id) {
    int row = static_cast<int>(id) - kSendOnceBase;
    NotifyParent(ActionSendOnce, row);
}


void CTxPanel::RestoreDefault(int row) {
    if (row < 0 || row >= kRowCount) {
        return;
    }
    std::vector<unsigned char> bytes(kByteCount, 0xFF);
    SetRowBytes(row, bytes);
}

void CTxPanel::NotifyParent(Action action, int row) {
    CWnd* parent = GetParent();
    if (!parent) {
        return;
    }
    parent->PostMessage(MessageId(), static_cast<WPARAM>(action), static_cast<LPARAM>(row));
}

bool CTxPanel::GetRowBytes(int row, std::vector<unsigned char>* out_bytes, std::wstring* error) const {
    if (!out_bytes) {
        if (error) {
            *error = L"Output buffer missing.";
        }
        return false;
    }
    out_bytes->clear();
    out_bytes->reserve(kByteCount);
    for (int i = 0; i < kByteCount; ++i) {
        CString text;
        m_rows[row].bytes[i].GetWindowTextW(text);
        text.Trim();
        if (text.IsEmpty()) {
            text = L"00";
        }
        unsigned char value = 0;
        std::wstring parse_error;
        if (!ParseSingleByte(std::wstring(text), &value, &parse_error)) {
            if (error) {
                *error = L"Byte " + std::to_wstring(i) + L" invalid: " + parse_error;
            }
            return false;
        }
        out_bytes->push_back(value);
    }
    return true;
}

void CTxPanel::SetRowBytes(int row, const std::vector<unsigned char>& bytes) {
    if (row < 0 || row >= kRowCount) {
        return;
    }
    for (int i = 0; i < kByteCount; ++i) {
        unsigned char value = i < static_cast<int>(bytes.size()) ? bytes[static_cast<size_t>(i)] : 0xFF;
        wchar_t buf[4] = {};
        swprintf_s(buf, L"%02X", value);
        m_rows[row].bytes[i].SetWindowTextW(buf);
    }
}

void CTxPanel::SetControlsEnabled(bool enabled) {
    m_connected = enabled;
    UpdateEnableStates();
}

void CTxPanel::FocusNextByte(int row, int col, bool forward) {
    int r = row;
    int c = col;
    if (forward) {
        if (c < kByteCount - 1) {
            ++c;
        } else if (r < kRowCount - 1) {
            ++r;
            c = 0;
        } else {
            return;
        }
    } else {
        if (c > 0) {
            --c;
        } else if (r > 0) {
            --r;
            c = kByteCount - 1;
        } else {
            return;
        }
    }
    m_rows[r].bytes[c].SetFocus();
    m_rows[r].bytes[c].SetSel(0, 2);
}

void CTxPanel::UpdateSelectAllState() {
    if (!m_selectAllCheck.GetSafeHwnd() || m_syncingSelectAll) {
        return;
    }
    bool all_checked = true;
    for (int i = 0; i < kRowCount; ++i) {
        if (m_rows[i].sendCheck.GetCheck() != BST_CHECKED) {
            all_checked = false;
            break;
        }
    }
    m_syncingSelectAll = true;
    m_selectAllCheck.SetCheck(all_checked ? BST_CHECKED : BST_UNCHECKED);
    m_syncingSelectAll = false;
}

void CTxPanel::UpdateEnableStates() {
    const bool active = m_connected;
    const bool idle = !m_running;
    const bool limit_active = IsLimitEnabled();

    m_startButton.EnableWindow(active);
    m_singleShotButton.EnableWindow(active && idle && !limit_active);

    m_intervalCheck.EnableWindow(active);
    m_intervalCombo.EnableWindow(active && m_intervalCheck.GetCheck() == BST_CHECKED);

    m_limitCheck.EnableWindow(active && idle);
    m_limitEdit.EnableWindow(active && idle);
    m_limitSpin.EnableWindow(active && idle);

    m_selectAllCheck.EnableWindow(active && idle);

    for (int i = 0; i < kRowCount; ++i) {
        const bool row_send_enabled = active && idle;
        m_rows[i].sendCheck.EnableWindow(row_send_enabled);
        for (int j = 0; j < kByteCount; ++j) {
            m_rows[i].bytes[j].EnableWindow(active && idle);
        }
        m_rows[i].restoreButton.EnableWindow(active && idle);
        m_rows[i].sendOnceButton.EnableWindow(active && idle);
    }
}
bool CTxPanel::GetByteValue(int row, int col, unsigned char* out_value) const {
    if (!out_value || row < 0 || row >= kRowCount || col < 0 || col >= kByteCount) {
        return false;
    }
    CString text;
    m_rows[row].bytes[col].GetWindowTextW(text);
    text.Trim();
    if (text.IsEmpty()) {
        text = L"00";
    }
    std::wstring error;
    if (!ParseSingleByte(std::wstring(text), out_value, &error)) {
        return false;
    }
    return true;
}

bool CTxPanel::PasteBytes(int row, int col, const std::wstring& text) {
    if (row < 0 || row >= kRowCount || col < 0 || col >= kByteCount) {
        return false;
    }
    std::vector<unsigned char> bytes;
    std::wstring error;
    if (!ParseByteList(text, &bytes, &error)) {
        return false;
    }
    int idx = col;
    for (size_t i = 0; i < bytes.size() && idx < kByteCount; ++i, ++idx) {
        wchar_t buf[4] = {};
        swprintf_s(buf, L"%02X", bytes[i]);
        m_rows[row].bytes[idx].SetWindowTextW(buf);
    }
    if (idx < kByteCount) {
        m_rows[row].bytes[idx].SetFocus();
        m_rows[row].bytes[idx].SetSel(0, 2);
    }
    return true;
}

void CTxPanel::NotifyHover(int row, int col) {
    if (row == m_hoverRow && col == m_hoverCol) {
        return;
    }
    m_hoverRow = row;
    m_hoverCol = col;
    CWnd* parent = GetParent();
    if (!parent) {
        return;
    }
    parent->PostMessage(MessageId(), static_cast<WPARAM>(ActionHover), MAKELPARAM(row, col));
}

void CTxPanel::ClearHover() {
    if (m_hoverRow == -1 && m_hoverCol == -1) {
        return;
    }
    m_hoverRow = -1;
    m_hoverCol = -1;
    CWnd* parent = GetParent();
    if (!parent) {
        return;
    }
    parent->PostMessage(MessageId(), static_cast<WPARAM>(ActionHoverClear), 0);
}

void CTxPanel::OnMouseMove(UINT nFlags, CPoint point) {
    CWnd::OnMouseMove(nFlags, point);
    TRACKMOUSEEVENT tme = {};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    TrackMouseEvent(&tme);

    int data_width = m_cellWidthBase * kByteCount + m_cellWidthExtra;
    int data_end = m_dataStartX + data_width;
    if (point.x < m_dataStartX || point.x >= data_end || point.y < m_rowsTop) {
        ClearHover();
        return;
    }
    int row_stride = m_rowHeight + m_rowGap;
    int rel_y = point.y - m_rowsTop;
    int row = rel_y / row_stride;
    if (row < 0 || row >= kRowCount) {
        ClearHover();
        return;
    }
    int row_top = m_rowsTop + row * row_stride;
    if (point.y > row_top + m_rowHeight) {
        ClearHover();
        return;
    }

    int x = m_dataStartX;
    for (int col = 0; col < kByteCount; ++col) {
        int cell_width = m_cellWidthBase + (col < m_cellWidthExtra ? 1 : 0);
        if (point.x >= x && point.x < x + cell_width) {
            NotifyHover(row, col);
            return;
        }
        x += cell_width;
        if (x >= data_end) {
            break;
        }
    }
    ClearHover();
}

void CTxPanel::OnMouseLeave() {
    ClearHover();
}

int CTxPanel::GetPreferredHeight() const {
    const int margin = 10;
    const int title_height = 22;
    const int header_gap = 8;
    const int gap = 6;
    const int sched_height = 28;
    const int limit_height = sched_height - 6;

    int header_height = m_headerHeight;
    int row_height = m_rowHeight;
    if (header_height <= 0 || row_height <= 0) {
        CClientDC dc(const_cast<CTxPanel*>(this));
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
    int total = margin + title_height + header_gap;
    total += sched_height + gap;
    total += limit_height + gap;
    total += header_height + gap;
    total += kRowCount * row_height;
    total += (kRowCount - 1) * gap;
    return total + 5;
}

