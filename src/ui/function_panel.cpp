#include "function_panel.h"

#include <commctrl.h>

namespace {

const int kRefreshId = 4101;
const int kApplyGpioId = 4102;
const int kApplyPwmId = 4103;
const int kPwmFreqId = 4104;
const int kPwmDutyId = 4105;
const int kPollAdcId = 4106;
const int kPollGpioId = 4107;
const int kPollPwmId = 4108;
const int kPwmFreqSliderId = 4109;
const int kPwmDutySliderId = 4110;
const int kSubTabId = 4111;
const int kLedSize = 14;

std::wstring FormatMillivoltText(std::uint32_t mv) {
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%u mV", mv);
    return std::wstring(buffer);
}

std::wstring FormatRawText(std::uint16_t value) {
    wchar_t buffer[48] = {};
    swprintf_s(buffer, L"raw %u (0x%03X)", static_cast<unsigned int>(value), static_cast<unsigned int>(value));
    return std::wstring(buffer);
}

}

BEGIN_MESSAGE_MAP(CFunctionRawPanel, CWnd)
    ON_WM_SIZE()
END_MESSAGE_MAP()

bool CFunctionRawPanel::Create(CWnd* parent, CFont* uiFont, CFont* monoFont) {
    CString class_name;

    m_uiFont = uiFont;
    m_monoFont = monoFont;
    class_name = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                     reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), nullptr);
    if (!CWnd::CreateEx(0, class_name, L"FunctionRawPanel",
                        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                        CRect(0, 0, 0, 0), parent, 0)) {
        return false;
    }

    m_txPacketGroup.Create(L"Last TX RAW64", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(0, 0, 0, 0), this, 0);
    m_rxPacketGroup.Create(L"Last RX RAW64", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(0, 0, 0, 0), this, 0);
    m_functionPacketGroup.Create(L"ADC RX (A0)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, CRect(0, 0, 0, 0), this, 0);
    m_txPacketEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(0, 0, 0, 0), this, 0);
    m_rxPacketEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(0, 0, 0, 0), this, 0);
    m_adcPacketEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(0, 0, 0, 0), this, 0);
    m_gpioPacketEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(0, 0, 0, 0), this, 0);
    m_pwmPacketEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, CRect(0, 0, 0, 0), this, 0);

    if (m_uiFont != nullptr) {
        m_txPacketGroup.SetFont(m_uiFont);
        m_rxPacketGroup.SetFont(m_uiFont);
        m_functionPacketGroup.SetFont(m_uiFont);
    }
    if (m_monoFont != nullptr) {
        m_txPacketEdit.SetFont(m_monoFont);
        m_rxPacketEdit.SetFont(m_monoFont);
        m_adcPacketEdit.SetFont(m_monoFont);
        m_gpioPacketEdit.SetFont(m_monoFont);
        m_pwmPacketEdit.SetFont(m_monoFont);
    }

    SetPacketPreview(L"", L"");
    SetFunctionRxPreview(0xA0, L"");
    SetFunctionRxPreview(0xA1, L"");
    SetFunctionRxPreview(0xA2, L"");
    SetActivePacketId(0xA0);
    return true;
}

void CFunctionRawPanel::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CFunctionRawPanel::LayoutControls(int cx, int cy) {
    const int margin = 0;
    const int group_height = 52;
    const int group_gap = 8;
    const int edit_margin_x = 10;
    const int edit_margin_top = 22;
    const int edit_height = 22;
    int group_width = cx - margin * 2;
    int edit_width = 0;
    int y = 0;

    if (group_width < 160) {
        group_width = 160;
    }
    edit_width = group_width - edit_margin_x * 2;
    if (edit_width < 120) {
        edit_width = 120;
    }

    m_txPacketGroup.MoveWindow(margin, y, group_width, group_height);
    m_txPacketEdit.MoveWindow(margin + edit_margin_x, y + edit_margin_top, edit_width, edit_height);
    y += group_height + group_gap;
    m_rxPacketGroup.MoveWindow(margin, y, group_width, group_height);
    m_rxPacketEdit.MoveWindow(margin + edit_margin_x, y + edit_margin_top, edit_width, edit_height);
    y += group_height + group_gap;
    m_functionPacketGroup.MoveWindow(margin, y, group_width, group_height);
    m_adcPacketEdit.MoveWindow(margin + edit_margin_x, y + edit_margin_top, edit_width, edit_height);
    m_gpioPacketEdit.MoveWindow(margin + edit_margin_x, y + edit_margin_top, edit_width, edit_height);
    m_pwmPacketEdit.MoveWindow(margin + edit_margin_x, y + edit_margin_top, edit_width, edit_height);
    (void)cy;
}

void CFunctionRawPanel::SetPacketPreview(const std::wstring& tx_hex, const std::wstring& rx_hex) {
    m_txPacketEdit.SetWindowTextW(tx_hex.empty() ? L"-" : tx_hex.c_str());
    m_rxPacketEdit.SetWindowTextW(rx_hex.empty() ? L"-" : rx_hex.c_str());
}

void CFunctionRawPanel::SetFunctionRxPreview(unsigned char packet_id, const std::wstring& rx_hex) {
    const wchar_t* text = rx_hex.empty() ? L"-" : rx_hex.c_str();

    switch (packet_id) {
    case 0xA0:
        m_adcPacketEdit.SetWindowTextW(text);
        break;
    case 0xA1:
        m_gpioPacketEdit.SetWindowTextW(text);
        break;
    case 0xA2:
        m_pwmPacketEdit.SetWindowTextW(text);
        break;
    default:
        break;
    }
}

void CFunctionRawPanel::SetActivePacketId(unsigned char packet_id) {
    m_activePacketId = packet_id;
    m_adcPacketEdit.ShowWindow((packet_id == 0xA0) ? SW_SHOW : SW_HIDE);
    m_gpioPacketEdit.ShowWindow((packet_id == 0xA1) ? SW_SHOW : SW_HIDE);
    m_pwmPacketEdit.ShowWindow((packet_id == 0xA2) ? SW_SHOW : SW_HIDE);
    UpdateFunctionGroupTitle();
}

void CFunctionRawPanel::SetPanelEnabled(bool enabled) {
    m_txPacketGroup.EnableWindow(enabled);
    m_rxPacketGroup.EnableWindow(enabled);
    m_functionPacketGroup.EnableWindow(enabled);
    m_txPacketEdit.EnableWindow(enabled);
    m_rxPacketEdit.EnableWindow(enabled);
    m_adcPacketEdit.EnableWindow(enabled);
    m_gpioPacketEdit.EnableWindow(enabled);
    m_pwmPacketEdit.EnableWindow(enabled);
}

void CFunctionRawPanel::UpdateFunctionGroupTitle() {
    const wchar_t* title = L"ADC RX (A0)";

    switch (m_activePacketId) {
    case 0xA1:
        title = L"GPIO RX (A1)";
        break;
    case 0xA2:
        title = L"PWM RX (A2)";
        break;
    default:
        break;
    }
    m_functionPacketGroup.SetWindowTextW(title);
}

BEGIN_MESSAGE_MAP(CFunctionPanel, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_HSCROLL()
    ON_NOTIFY(TCN_SELCHANGE, kSubTabId, &CFunctionPanel::OnSubTabChanged)
    ON_BN_CLICKED(kRefreshId, &CFunctionPanel::OnRefreshClicked)
    ON_BN_CLICKED(kApplyGpioId, &CFunctionPanel::OnApplyGpioClicked)
    ON_BN_CLICKED(kApplyPwmId, &CFunctionPanel::OnApplyPwmClicked)
    ON_BN_CLICKED(kPollAdcId, &CFunctionPanel::OnPollOptionClicked)
    ON_BN_CLICKED(kPollGpioId, &CFunctionPanel::OnPollOptionClicked)
    ON_BN_CLICKED(kPollPwmId, &CFunctionPanel::OnPollOptionClicked)
END_MESSAGE_MAP()

bool CFunctionPanel::Create(CWnd* parent) {
    CString class_name = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW),
                                            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), nullptr);
    return CWnd::CreateEx(0, class_name, L"FunctionPanel",
                          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                          CRect(0, 0, 0, 0), parent, 0);
}

int CFunctionPanel::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    CClientDC dc(this);
    TCITEM tie = {};
    int font_height = -MulDiv(9, dc.GetDeviceCaps(LOGPIXELSY), 72);

    if (CWnd::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    m_uiFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_monoFont.CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           FIXED_PITCH | FF_MODERN, L"Consolas");

    m_titleLabel.Create(L"Function Control", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_peerModeLabel.Create(L"Peer Mode: Unknown", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_packetLabel.Create(L"RAW64 Packets: ADC=0xA0 GPIO=0xA1 PWM=0xA2", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_refreshButton.Create(L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, kRefreshId);
    m_pollAdcCheck.Create(L"Poll ADC (A0)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 0, 0), this, kPollAdcId);
    m_pollGpioCheck.Create(L"Poll GPIO (A1)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 0, 0), this, kPollGpioId);
    m_pollPwmCheck.Create(L"Poll PWM (A2)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 0, 0), this, kPollPwmId);
    m_subTab.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS, CRect(0, 0, 0, 0), this, kSubTabId);
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<LPWSTR>(L"ADC");
    m_subTab.InsertItem(0, &tie);
    tie.pszText = const_cast<LPWSTR>(L"GPIO");
    m_subTab.InsertItem(1, &tie);
    tie.pszText = const_cast<LPWSTR>(L"PWM");
    m_subTab.InsertItem(2, &tie);
    m_subTab.SetCurSel(0);

    m_adc0Label.Create(L"ADC Display 1", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_adc0Value.Create(L"0 mV", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_adc0Detail.Create(L"raw 0 (0x000)", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_adc0Bar.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH, CRect(0, 0, 0, 0), this, 0);
    m_adc1Label.Create(L"ADC Display 2", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_adc1Value.Create(L"0 mV", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_adc1Detail.Create(L"raw 0 (0x000)", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_adc1Bar.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH, CRect(0, 0, 0, 0), this, 0);
    m_vrefLabel.Create(L"ADC VREF", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_vrefValue.Create(L"0 mV", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_bandgapLabel.Create(L"Band-gap Raw", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_bandgapValue.Create(L"0 (0x000)", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_gpioInLabel.Create(L"GPIO Input", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_gpioInValue.Create(L"RELEASED", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_gpioOutCheck.Create(L"GPIO Output", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 0, 0), this, 0);
    m_gpioApplyButton.Create(L"Apply GPIO", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, kApplyGpioId);
    m_pwmFreqLabel.Create(L"PWM Freq (Hz)", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_pwmFreqMinLabel.Create(L"1", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_pwmFreqMaxLabel.Create(L"200000", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_pwmFreqValueLabel.Create(L"Current: 1000 Hz", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_pwmFreqEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, kPwmFreqId);
    m_pwmFreqSlider.Create(WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, CRect(0, 0, 0, 0), this, kPwmFreqSliderId);
    m_pwmDutyLabel.Create(L"PWM Duty (%)", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_pwmDutyMinLabel.Create(L"0%", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_pwmDutyMaxLabel.Create(L"100%", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_pwmDutyValueLabel.Create(L"Current: 50%", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this);
    m_pwmDutyEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, kPwmDutyId);
    m_pwmDutySlider.Create(WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS, CRect(0, 0, 0, 0), this, kPwmDutySliderId);
    m_pwmApplyButton.Create(L"Apply PWM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, kApplyPwmId);
    if (!m_rawPanel.Create(this, &m_uiFont, &m_monoFont)) {
        return -1;
    }

    {
        CWnd* controls[] = {
            &m_titleLabel, &m_peerModeLabel, &m_packetLabel, &m_refreshButton,
            &m_pollAdcCheck, &m_pollGpioCheck, &m_pollPwmCheck, &m_subTab,
            &m_adc0Label, &m_adc0Value, &m_adc0Detail, &m_adc1Label, &m_adc1Value, &m_adc1Detail,
            &m_vrefLabel, &m_vrefValue, &m_bandgapLabel, &m_bandgapValue, &m_gpioInLabel, &m_gpioInValue,
            &m_gpioOutCheck, &m_gpioApplyButton, &m_pwmFreqLabel, &m_pwmFreqMinLabel, &m_pwmFreqMaxLabel,
            &m_pwmFreqValueLabel, &m_pwmFreqEdit, &m_pwmDutyLabel, &m_pwmDutyMinLabel, &m_pwmDutyMaxLabel,
            &m_pwmDutyValueLabel, &m_pwmDutyEdit, &m_pwmApplyButton
        };
        for (CWnd* control : controls) {
            control->SetFont(&m_uiFont);
        }
    }
    m_adc0Bar.SetRange(0, 4095);
    m_adc1Bar.SetRange(0, 4095);
    m_pwmFreqEdit.SetWindowTextW(L"1000");
    m_pwmDutyEdit.SetWindowTextW(L"50");
    m_pwmFreqSlider.SetRange(1, 200000);
    m_pwmFreqSlider.SetTicFreq(10000);
    m_pwmFreqSlider.SetPageSize(1000);
    m_pwmFreqSlider.SetLineSize(100);
    m_pwmDutySlider.SetRange(0, 100);
    m_pwmDutySlider.SetTicFreq(10);
    m_pwmDutySlider.SetPageSize(5);
    m_pwmDutySlider.SetLineSize(1);
    m_pwmFreqSlider.SetPos(1000);
    m_pwmDutySlider.SetPos(50);
    SetPollingEnabled(true, true, true);
    UpdateTelemetryText();
    UpdateSubTabVisibility();
    return 0;
}

void CFunctionPanel::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CFunctionPanel::OnPaint() {
    CPaintDC dc(this);
    if (m_activeSubTab == 1) {
        DrawLed(&dc, m_gpioInLedRect, m_telemetry.gpio_in != 0U);
        DrawLed(&dc, m_gpioOutLedRect, m_telemetry.gpio_out != 0U);
    }
}

void CFunctionPanel::LayoutControls(int cx, int cy) {
    const int margin = 10;
    const int gap = 8;
    const int row_height = 24;
    const int text_height = 22;
    const int label_width = 120;
    const int value_width = 220;
    const int adc_card_width = 460;
    const int adc_bar_width = 220;
    const int poll_width = 140;
    const int slider_width = 260;
    const int tab_height = 28;
    const int raw_panel_height = 172;
    const int min_raw_top = 220;
    int y = margin;
    int right_x = cx - margin - 110;
    int packet_width = cx - margin * 2;
    int gpio_value_x = margin + label_width;
    int pwm_value_x = margin + label_width;
    int content_top = 0;
    int content_left = margin + 12;
    int content_width = packet_width - 24;
    int subtab_width = packet_width - poll_width - gap;
    int shared_top = 0;

    if (right_x < margin) {
        right_x = margin;
    }
    if (packet_width < 120) {
        packet_width = 120;
    }
    if (content_width < 160) {
        content_width = 160;
    }
    if (subtab_width < 220) {
        subtab_width = 220;
    }

    m_titleLabel.MoveWindow(margin, y, 180, 22);
    m_refreshButton.MoveWindow(right_x, y - 2, 110, row_height);
    y += row_height + 2;
    m_peerModeLabel.MoveWindow(margin, y, 220, 22);
    y += row_height;
    m_packetLabel.MoveWindow(margin, y, packet_width, 22);
    y += row_height;
    m_subTab.MoveWindow(margin, y, subtab_width, tab_height);
    m_pollAdcCheck.MoveWindow(margin + subtab_width + gap, y + 2, poll_width, row_height);
    m_pollGpioCheck.MoveWindow(margin + subtab_width + gap, y + 2, poll_width, row_height);
    m_pollPwmCheck.MoveWindow(margin + subtab_width + gap, y + 2, poll_width, row_height);
    y += tab_height + 10;
    content_top = y;
    shared_top = cy - margin - raw_panel_height;
    if (shared_top < (content_top + min_raw_top)) {
        shared_top = content_top + min_raw_top;
    }

    y = content_top;
    m_adc0Label.MoveWindow(content_left, y, 120, text_height);
    m_adc0Value.MoveWindow(content_left + 130, y, 140, text_height);
    y += row_height;
    m_adc0Detail.MoveWindow(content_left + 130, y, adc_card_width, 20);
    y += 20;
    m_adc0Bar.MoveWindow(content_left + 130, y, adc_bar_width, 18);
    y += row_height;
    m_adc1Label.MoveWindow(content_left, y, 120, text_height);
    m_adc1Value.MoveWindow(content_left + 130, y, 140, text_height);
    y += row_height;
    m_adc1Detail.MoveWindow(content_left + 130, y, adc_card_width, 20);
    y += 20;
    m_adc1Bar.MoveWindow(content_left + 130, y, adc_bar_width, 18);
    y += row_height;
    m_vrefLabel.MoveWindow(content_left, y, label_width, text_height);
    m_vrefValue.MoveWindow(content_left + label_width, y, value_width, text_height);
    y += row_height;
    m_bandgapLabel.MoveWindow(content_left, y, label_width, text_height);
    m_bandgapValue.MoveWindow(content_left + label_width, y, value_width, text_height);

    y = content_top;
    m_gpioInLabel.MoveWindow(content_left, y, label_width, text_height);
    m_gpioInValue.MoveWindow(content_left + label_width + kLedSize + 6, y, 120, text_height);
    m_gpioInLedRect.SetRect(content_left + label_width, y + 4,
                            content_left + label_width + kLedSize, y + 4 + kLedSize);
    y += row_height + gap;
    m_gpioOutCheck.MoveWindow(content_left + kLedSize + 6, y, 170, row_height);
    m_gpioOutLedRect.SetRect(content_left, y + 4, content_left + kLedSize, y + 4 + kLedSize);
    m_gpioApplyButton.MoveWindow(content_left + 200, y - 2, 110, row_height);

    y = content_top;
    m_pwmFreqLabel.MoveWindow(content_left, y, label_width, text_height);
    m_pwmFreqValueLabel.MoveWindow(content_left + label_width, y, 180, text_height);
    y += row_height;
    m_pwmFreqMinLabel.MoveWindow(content_left + label_width, y, 50, text_height);
    m_pwmFreqSlider.MoveWindow(content_left + label_width + 42, y - 2, slider_width, row_height);
    m_pwmFreqMaxLabel.MoveWindow(content_left + label_width + 42 + slider_width + 8, y, 70, text_height);
    m_pwmFreqEdit.MoveWindow(content_left + label_width + 42 + slider_width + 86, y - 2, 90, row_height);
    y += row_height;
    m_pwmDutyLabel.MoveWindow(content_left, y, label_width, text_height);
    m_pwmDutyValueLabel.MoveWindow(content_left + label_width, y, 180, text_height);
    y += row_height;
    m_pwmDutyMinLabel.MoveWindow(content_left + label_width, y, 50, text_height);
    m_pwmDutySlider.MoveWindow(content_left + label_width + 42, y - 2, slider_width, row_height);
    m_pwmDutyMaxLabel.MoveWindow(content_left + label_width + 42 + slider_width + 8, y, 70, text_height);
    m_pwmDutyEdit.MoveWindow(content_left + label_width + 42 + slider_width + 86, y - 2, 90, row_height);
    m_pwmApplyButton.MoveWindow(content_left + label_width + 42 + slider_width + 186, y - 2, 110, row_height);
    m_rawPanel.MoveWindow(content_left, shared_top, content_width, raw_panel_height);
    (void)cy;
    (void)gpio_value_x;
    (void)pwm_value_x;
}

void CFunctionPanel::SetControlsEnabled(bool enabled) {
    m_controlsEnabled = enabled;
    m_refreshButton.EnableWindow(enabled);
    m_pollAdcCheck.EnableWindow(enabled);
    m_pollGpioCheck.EnableWindow(enabled);
    m_pollPwmCheck.EnableWindow(enabled);
    UpdateFeatureControlState();
}

void CFunctionPanel::SetPeerModeText(const std::wstring& text) {
    m_peerMode = text.empty() ? L"Unknown" : text;
    m_peerModeLabel.SetWindowTextW((L"Peer Mode: " + m_peerMode).c_str());
}

void CFunctionPanel::SetTelemetry(const TelemetryData& data) {
    m_telemetry = data;
    UpdateTelemetryText();
}

void CFunctionPanel::SetAdcTelemetry(std::uint32_t adc0_mv, std::uint32_t adc1_mv, std::uint32_t vref_mv,
                                     std::uint16_t adc0_raw, std::uint16_t adc1_raw, std::uint16_t adc_bg_raw) {
    m_telemetry.adc0_mv = adc0_mv;
    m_telemetry.adc1_mv = adc1_mv;
    m_telemetry.vref_mv = vref_mv;
    m_telemetry.adc0_raw = adc0_raw;
    m_telemetry.adc1_raw = adc1_raw;
    m_telemetry.adc_bg_raw = adc_bg_raw;
    UpdateAdcTelemetryText();
}

void CFunctionPanel::SetGpioInputTelemetry(unsigned char gpio_in) {
    m_telemetry.gpio_in = gpio_in;
    UpdateGpioInputText();
}

void CFunctionPanel::SetGpioOutputTelemetry(unsigned char gpio_out) {
    m_telemetry.gpio_out = gpio_out;
    UpdateGpioOutputText();
}

void CFunctionPanel::SetPwmTelemetry(std::uint32_t pwm_freq, std::uint32_t pwm_duty, bool update_controls) {
    m_telemetry.pwm_freq = pwm_freq;
    m_telemetry.pwm_duty = pwm_duty;
    UpdatePwmTelemetryText(update_controls);
}

void CFunctionPanel::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
    (void)nSBCode;
    (void)nPos;
    if ((pScrollBar != nullptr) &&
        ((pScrollBar->GetSafeHwnd() == m_pwmFreqSlider.GetSafeHwnd()) ||
         (pScrollBar->GetSafeHwnd() == m_pwmDutySlider.GetSafeHwnd()))) {
        UpdatePwmEditsFromSliders();
    }
    CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CFunctionPanel::OnSubTabChanged(NMHDR*, LRESULT* pResult) {
    m_activeSubTab = m_subTab.GetCurSel();
    if (m_activeSubTab < 0) {
        m_activeSubTab = 0;
    }
    UpdateSubTabVisibility();
    if (pResult) {
        *pResult = 0;
    }
}

void CFunctionPanel::SetPacketPreview(const std::wstring& tx_hex, const std::wstring& rx_hex) {
    m_rawPanel.SetPacketPreview(tx_hex, rx_hex);
}

void CFunctionPanel::SetFunctionRxPreview(unsigned char packet_id, const std::wstring& rx_hex) {
    m_rawPanel.SetFunctionRxPreview(packet_id, rx_hex);
}

void CFunctionPanel::SetPollingEnabled(bool adc_enabled, bool gpio_enabled, bool pwm_enabled) {
    m_pollAdcCheck.SetCheck(adc_enabled ? BST_CHECKED : BST_UNCHECKED);
    m_pollGpioCheck.SetCheck(gpio_enabled ? BST_CHECKED : BST_UNCHECKED);
    m_pollPwmCheck.SetCheck(pwm_enabled ? BST_CHECKED : BST_UNCHECKED);
    UpdateFeatureControlState();
}

void CFunctionPanel::GetPollingEnabled(bool* adc_enabled, bool* gpio_enabled, bool* pwm_enabled) const {
    if (adc_enabled != nullptr) {
        *adc_enabled = (m_pollAdcCheck.GetCheck() == BST_CHECKED);
    }
    if (gpio_enabled != nullptr) {
        *gpio_enabled = (m_pollGpioCheck.GetCheck() == BST_CHECKED);
    }
    if (pwm_enabled != nullptr) {
        *pwm_enabled = (m_pollPwmCheck.GetCheck() == BST_CHECKED);
    }
}

bool CFunctionPanel::GetGpioOutputState(bool* out_state) const {
    if (!out_state) {
        return false;
    }
    *out_state = (m_gpioOutCheck.GetCheck() == BST_CHECKED);
    return true;
}

bool CFunctionPanel::GetPwmSettings(std::uint32_t* out_freq, std::uint32_t* out_duty, std::wstring* error) const {
    CString text;
    unsigned long freq = 0;
    unsigned long duty = 0;

    if (!out_freq || !out_duty) {
        if (error) {
            *error = L"PWM output buffer missing.";
        }
        return false;
    }

    m_pwmFreqEdit.GetWindowTextW(text);
    freq = wcstoul(text.GetString(), nullptr, 10);
    m_pwmDutyEdit.GetWindowTextW(text);
    duty = wcstoul(text.GetString(), nullptr, 10);
    if (freq == 0 || freq > 200000UL) {
        if (error) {
            *error = L"PWM frequency must be 1..200000 Hz.";
        }
        return false;
    }
    if (duty > 100UL) {
        if (error) {
            *error = L"PWM duty must be 0..100.";
        }
        return false;
    }

    *out_freq = static_cast<std::uint32_t>(freq);
    *out_duty = static_cast<std::uint32_t>(duty);
    return true;
}

void CFunctionPanel::OnRefreshClicked() {
    NotifyParent(ActionRefresh);
}

void CFunctionPanel::OnApplyGpioClicked() {
    NotifyParent(ActionApplyGpio);
}

void CFunctionPanel::OnApplyPwmClicked() {
    NotifyParent(ActionApplyPwm);
}

void CFunctionPanel::OnPollOptionClicked() {
    UpdateFeatureControlState();
    NotifyParent(ActionPollingChanged);
}

void CFunctionPanel::UpdateFeatureControlState() {
    bool adc_enabled = false;
    bool gpio_enabled = false;
    bool pwm_enabled = false;
    bool raw_enabled = false;

    adc_enabled = m_controlsEnabled && (m_pollAdcCheck.GetCheck() == BST_CHECKED);
    gpio_enabled = m_controlsEnabled && (m_pollGpioCheck.GetCheck() == BST_CHECKED);
    pwm_enabled = m_controlsEnabled && (m_pollPwmCheck.GetCheck() == BST_CHECKED);
    raw_enabled = (m_activeSubTab == 0) ? adc_enabled :
                  ((m_activeSubTab == 1) ? gpio_enabled : pwm_enabled);

    m_adc0Label.EnableWindow(adc_enabled);
    m_adc0Value.EnableWindow(adc_enabled);
    m_adc0Detail.EnableWindow(adc_enabled);
    m_adc0Bar.EnableWindow(adc_enabled);
    m_adc1Label.EnableWindow(adc_enabled);
    m_adc1Value.EnableWindow(adc_enabled);
    m_adc1Detail.EnableWindow(adc_enabled);
    m_adc1Bar.EnableWindow(adc_enabled);
    m_vrefLabel.EnableWindow(adc_enabled);
    m_vrefValue.EnableWindow(adc_enabled);
    m_bandgapLabel.EnableWindow(adc_enabled);
    m_bandgapValue.EnableWindow(adc_enabled);

    m_gpioInLabel.EnableWindow(gpio_enabled);
    m_gpioInValue.EnableWindow(gpio_enabled);
    m_gpioOutCheck.EnableWindow(gpio_enabled);
    m_gpioApplyButton.EnableWindow(gpio_enabled);

    m_pwmFreqLabel.EnableWindow(pwm_enabled);
    m_pwmFreqMinLabel.EnableWindow(pwm_enabled);
    m_pwmFreqMaxLabel.EnableWindow(pwm_enabled);
    m_pwmFreqValueLabel.EnableWindow(pwm_enabled);
    m_pwmFreqEdit.EnableWindow(pwm_enabled);
    m_pwmFreqSlider.EnableWindow(pwm_enabled);
    m_pwmDutyLabel.EnableWindow(pwm_enabled);
    m_pwmDutyMinLabel.EnableWindow(pwm_enabled);
    m_pwmDutyMaxLabel.EnableWindow(pwm_enabled);
    m_pwmDutyValueLabel.EnableWindow(pwm_enabled);
    m_pwmDutyEdit.EnableWindow(pwm_enabled);
    m_pwmDutySlider.EnableWindow(pwm_enabled);
    m_pwmApplyButton.EnableWindow(pwm_enabled);
    m_rawPanel.SetPanelEnabled(raw_enabled);
}

void CFunctionPanel::UpdateSubTabVisibility() {
    const BOOL show_adc = (m_activeSubTab == 0) ? TRUE : FALSE;
    const BOOL show_gpio = (m_activeSubTab == 1) ? TRUE : FALSE;
    const BOOL show_pwm = (m_activeSubTab == 2) ? TRUE : FALSE;
    CRect old_gpio_in_led_rect = m_gpioInLedRect;
    CRect old_gpio_out_led_rect = m_gpioOutLedRect;

    m_pollAdcCheck.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_pollGpioCheck.ShowWindow(show_gpio ? SW_SHOW : SW_HIDE);
    m_pollPwmCheck.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);

    m_adc0Label.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_adc0Value.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_adc0Detail.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_adc0Bar.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_adc1Label.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_adc1Value.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_adc1Detail.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_adc1Bar.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_vrefLabel.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_vrefValue.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_bandgapLabel.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);
    m_bandgapValue.ShowWindow(show_adc ? SW_SHOW : SW_HIDE);

    m_gpioInLabel.ShowWindow(show_gpio ? SW_SHOW : SW_HIDE);
    m_gpioInValue.ShowWindow(show_gpio ? SW_SHOW : SW_HIDE);
    m_gpioOutCheck.ShowWindow(show_gpio ? SW_SHOW : SW_HIDE);
    m_gpioApplyButton.ShowWindow(show_gpio ? SW_SHOW : SW_HIDE);

    m_pwmFreqLabel.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmFreqMinLabel.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmFreqMaxLabel.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmFreqValueLabel.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmFreqEdit.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmFreqSlider.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmDutyLabel.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmDutyMinLabel.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmDutyMaxLabel.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmDutyValueLabel.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmDutyEdit.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmDutySlider.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_pwmApplyButton.ShowWindow(show_pwm ? SW_SHOW : SW_HIDE);
    m_rawPanel.ShowWindow(SW_SHOW);
    m_rawPanel.SetActivePacketId(show_adc ? 0xA0 : (show_gpio ? 0xA1 : 0xA2));
    UpdateFeatureControlState();
    if (!show_gpio) {
        m_gpioInLedRect.SetRectEmpty();
        m_gpioOutLedRect.SetRectEmpty();
        if (!old_gpio_in_led_rect.IsRectEmpty()) {
            InvalidateRect(&old_gpio_in_led_rect, TRUE);
        }
        if (!old_gpio_out_led_rect.IsRectEmpty()) {
            InvalidateRect(&old_gpio_out_led_rect, TRUE);
        }
    }
    {
        CRect rect;
        GetClientRect(&rect);
        LayoutControls(rect.Width(), rect.Height());
    }
    Invalidate(TRUE);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

void CFunctionPanel::NotifyParent(Action action) {
    CWnd* parent = GetParent();
    if (!parent) {
        return;
    }
    parent->SendMessage(MessageId(), static_cast<WPARAM>(action), 0);
}

void CFunctionPanel::DrawLed(CDC* dc, const CRect& rect, bool active) {
    COLORREF fill_color = active ? RGB(0, 180, 0) : RGB(150, 150, 150);
    COLORREF border_color = active ? RGB(0, 110, 0) : RGB(90, 90, 90);
    CPen pen(PS_SOLID, 1, border_color);
    CBrush brush(fill_color);
    CPen* old_pen = nullptr;
    CBrush* old_brush = nullptr;

    if ((dc == nullptr) || rect.IsRectEmpty()) {
        return;
    }

    old_pen = dc->SelectObject(&pen);
    old_brush = dc->SelectObject(&brush);
    dc->Ellipse(rect);
    dc->SelectObject(old_brush);
    dc->SelectObject(old_pen);
}

void CFunctionPanel::UpdatePwmEditsFromSliders() {
    int freq = m_pwmFreqSlider.GetPos();
    int duty = m_pwmDutySlider.GetPos();

    m_pwmFreqEdit.SetWindowTextW(std::to_wstring(freq).c_str());
    m_pwmDutyEdit.SetWindowTextW(std::to_wstring(duty).c_str());
    m_pwmFreqValueLabel.SetWindowTextW((L"Current: " + std::to_wstring(freq) + L" Hz").c_str());
    m_pwmDutyValueLabel.SetWindowTextW((L"Current: " + std::to_wstring(duty) + L"%").c_str());
}

void CFunctionPanel::UpdatePwmSlidersFromTelemetry() {
    int freq_pos = 0;
    int duty_pos = 0;

    freq_pos = static_cast<int>(m_telemetry.pwm_freq);
    if (freq_pos < 1) {
        freq_pos = 1;
    } else if (freq_pos > 200000) {
        freq_pos = 200000;
    }

    duty_pos = static_cast<int>(m_telemetry.pwm_duty);
    if (duty_pos < 0) {
        duty_pos = 0;
    } else if (duty_pos > 100) {
        duty_pos = 100;
    }

    m_pwmFreqSlider.SetPos(freq_pos);
    m_pwmDutySlider.SetPos(duty_pos);
}

void CFunctionPanel::UpdateAdcTelemetryText() {
    m_adc0Value.SetWindowTextW(FormatMillivoltText(m_telemetry.adc0_mv).c_str());
    m_adc0Detail.SetWindowTextW(FormatRawText(m_telemetry.adc0_raw).c_str());
    m_adc0Bar.SetPos(static_cast<int>(m_telemetry.adc0_raw));
    m_adc1Value.SetWindowTextW(FormatMillivoltText(m_telemetry.adc1_mv).c_str());
    m_adc1Detail.SetWindowTextW(FormatRawText(m_telemetry.adc1_raw).c_str());
    m_adc1Bar.SetPos(static_cast<int>(m_telemetry.adc1_raw));
    m_vrefValue.SetWindowTextW((std::to_wstring(m_telemetry.vref_mv) + L" mV").c_str());
    m_bandgapValue.SetWindowTextW(FormatRawText(m_telemetry.adc_bg_raw).c_str());
}

void CFunctionPanel::UpdateGpioInputText() {
    m_gpioInValue.SetWindowTextW((m_telemetry.gpio_in != 0U) ? L"PRESSED" : L"RELEASED");
    InvalidateRect(&m_gpioInLedRect, FALSE);
}

void CFunctionPanel::UpdateGpioOutputText() {
    m_gpioOutCheck.SetCheck((m_telemetry.gpio_out != 0U) ? BST_CHECKED : BST_UNCHECKED);
    InvalidateRect(&m_gpioOutLedRect, FALSE);
}

void CFunctionPanel::UpdatePwmTelemetryText(bool update_controls) {
    if (update_controls) {
        UpdatePwmSlidersFromTelemetry();
        m_pwmFreqEdit.SetWindowTextW(std::to_wstring(m_telemetry.pwm_freq).c_str());
        m_pwmDutyEdit.SetWindowTextW(std::to_wstring(m_telemetry.pwm_duty).c_str());
    }
    m_pwmFreqValueLabel.SetWindowTextW((L"Current: " + std::to_wstring(m_telemetry.pwm_freq) + L" Hz").c_str());
    m_pwmDutyValueLabel.SetWindowTextW((L"Current: " + std::to_wstring(m_telemetry.pwm_duty) + L"%").c_str());
}

void CFunctionPanel::UpdateTelemetryText() {
    UpdateAdcTelemetryText();
    UpdateGpioInputText();
    UpdateGpioOutputText();
    UpdatePwmTelemetryText(true);
}
