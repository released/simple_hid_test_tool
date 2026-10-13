#include "firmware_panel.h"

#pragma comment(lib, "msimg32.lib")

namespace {

const int kBrowseId = 4001;
const int kBurnId = 4002;
const int kEnterBootId = 4003;
const int kResetId = 4004;

}

BEGIN_MESSAGE_MAP(CFirmwarePanel, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_BN_CLICKED(kBrowseId, &CFirmwarePanel::OnBrowseClicked)
    ON_BN_CLICKED(kBurnId, &CFirmwarePanel::OnBurnClicked)
    ON_BN_CLICKED(kEnterBootId, &CFirmwarePanel::OnEnterBootClicked)
    ON_BN_CLICKED(kResetId, &CFirmwarePanel::OnResetClicked)
END_MESSAGE_MAP()

bool CFirmwarePanel::Create(CWnd* parent) {
    CString class_name = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), nullptr);
    return CWnd::CreateEx(0, class_name, L"FirmwarePanel",
                          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          CRect(0, 0, 0, 0), parent, 0);
}

int CFirmwarePanel::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    CClientDC dc(this);
    int font_height = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);

    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    m_uiFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_title.Create(L"Firmware Update", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_modeLabel.Create(L"Peer Mode:", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_modeValue.Create(L"Unknown", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_fileEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                      CRect(0, 0, 0, 0), this, 0);
    m_browseButton.Create(L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          CRect(0, 0, 0, 0), this, kBrowseId);
    m_burnButton.Create(L"Burn Firmware", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        CRect(0, 0, 0, 0), this, kBurnId);
    m_enterBootButton.Create(L"Enter Bootloader", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             CRect(0, 0, 0, 0), this, kEnterBootId);
    m_resetButton.Create(L"Reset MCU", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         CRect(0, 0, 0, 0), this, kResetId);
    m_progress.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH, CRect(0, 0, 0, 0), this, 0);
    m_progress.SetRange32(0, 100);
    m_progress.SetPos(0);
    m_progressText.Create(L"Idle", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);

    CWnd* controls[] = {
        &m_title, &m_modeLabel, &m_modeValue, &m_fileEdit, &m_browseButton,
        &m_burnButton, &m_enterBootButton, &m_resetButton, &m_progress, &m_progressText
    };
    for (CWnd* control : controls) {
        control->SetFont(&m_uiFont);
    }

    return 0;
}

void CFirmwarePanel::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CFirmwarePanel::LayoutControls(int cx, int cy) {
    const int margin = 10;
    const int gap = 8;
    const int row_height = 26;
    const int button_width = 130;
    int width = cx - margin * 2;
    int path_width = width - button_width - gap;
    int y = margin;

    if (path_width < 120) {
        path_width = 120;
    }

    m_title.MoveWindow(margin, y, 220, 24);
    y += 34;
    m_modeLabel.MoveWindow(margin, y + 4, 80, 22);
    m_modeValue.MoveWindow(margin + 84, y + 4, 220, 22);
    y += 34;
    m_fileEdit.MoveWindow(margin, y, path_width, row_height);
    m_browseButton.MoveWindow(margin + path_width + gap, y, button_width, row_height);
    y += row_height + gap;
    m_burnButton.MoveWindow(margin, y, button_width, row_height);
    m_enterBootButton.MoveWindow(margin + button_width + gap, y, button_width + 10, row_height);
    m_resetButton.MoveWindow(margin + (button_width * 2) + gap + 18, y, button_width - 20, row_height);
    y += row_height + gap;
    m_progress.MoveWindow(margin, y, width, 22);
    y += 28;
    m_progressText.MoveWindow(margin, y, width, 24);
    (void)cy;
}

void CFirmwarePanel::OnPaint() {
    CPaintDC dc(this);
    CRect rect;
    TRIVERTEX verts[2] = {};
    GRADIENT_RECT grad = { 0, 1 };

    GetClientRect(&rect);
    verts[0].x = rect.left;
    verts[0].y = rect.top;
    verts[0].Red = static_cast<COLOR16>(250 << 8);
    verts[0].Green = static_cast<COLOR16>(250 << 8);
    verts[0].Blue = static_cast<COLOR16>(245 << 8);
    verts[1].x = rect.right;
    verts[1].y = rect.bottom;
    verts[1].Red = static_cast<COLOR16>(232 << 8);
    verts[1].Green = static_cast<COLOR16>(240 << 8);
    verts[1].Blue = static_cast<COLOR16>(226 << 8);
    ::GradientFill(dc.GetSafeHdc(), verts, 2, &grad, 1, GRADIENT_FILL_RECT_V);
    dc.DrawEdge(&rect, EDGE_SUNKEN, BF_RECT);
}

void CFirmwarePanel::OnBrowseClicked() {
    NotifyParent(ActionBrowse);
}

void CFirmwarePanel::OnBurnClicked() {
    NotifyParent(ActionBurn);
}

void CFirmwarePanel::OnEnterBootClicked() {
    NotifyParent(ActionEnterBoot);
}

void CFirmwarePanel::OnResetClicked() {
    NotifyParent(ActionResetMcu);
}

void CFirmwarePanel::NotifyParent(Action action) {
    CWnd* parent = GetParent();
    if (!parent) {
        return;
    }
    parent->PostMessage(MessageId(), static_cast<WPARAM>(action), 0);
}

std::wstring CFirmwarePanel::GetFilePath() const {
    CString text;
    m_fileEdit.GetWindowTextW(text);
    return std::wstring(text.GetString());
}

void CFirmwarePanel::SetFilePath(const std::wstring& path) {
    m_fileEdit.SetWindowTextW(path.c_str());
}

void CFirmwarePanel::SetControlsEnabled(bool enabled) {
    m_fileEdit.EnableWindow(enabled && !m_running);
    m_browseButton.EnableWindow(enabled && !m_running);
    m_burnButton.EnableWindow(enabled);
    m_enterBootButton.EnableWindow(enabled && !m_running);
    m_resetButton.EnableWindow(enabled && !m_running);
}

void CFirmwarePanel::SetBurnRunning(bool running) {
    m_running = running;
    m_burnButton.SetWindowTextW(running ? L"Stop" : L"Burn Firmware");
    SetControlsEnabled(TRUE);
}

void CFirmwarePanel::SetProgress(int percent, const std::wstring& text) {
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    m_progress.SetPos(percent);
    m_progressText.SetWindowTextW(text.c_str());
}

void CFirmwarePanel::SetPeerModeText(const std::wstring& text) {
    m_modeValue.SetWindowTextW(text.c_str());
}
