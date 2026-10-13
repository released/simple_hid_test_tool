#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <cstdint>
#include <string>

class CFunctionRawPanel : public CWnd {
public:
    bool Create(CWnd* parent, CFont* uiFont, CFont* monoFont);
    void SetPacketPreview(const std::wstring& tx_hex, const std::wstring& rx_hex);
    void SetFunctionRxPreview(unsigned char packet_id, const std::wstring& rx_hex);
    void SetActivePacketId(unsigned char packet_id);
    void SetPanelEnabled(bool enabled);

protected:
    afx_msg void OnSize(UINT nType, int cx, int cy);

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void UpdateFunctionGroupTitle();

    CFont* m_uiFont = nullptr;
    CFont* m_monoFont = nullptr;
    CButton m_txPacketGroup;
    CButton m_rxPacketGroup;
    CButton m_functionPacketGroup;
    CEdit m_txPacketEdit;
    CEdit m_rxPacketEdit;
    CEdit m_adcPacketEdit;
    CEdit m_gpioPacketEdit;
    CEdit m_pwmPacketEdit;
    unsigned char m_activePacketId = 0xA0;
};

class CFunctionPanel : public CWnd {
public:
    struct TelemetryData {
        std::uint32_t adc0_mv = 0;
        std::uint32_t adc1_mv = 0;
        std::uint32_t vref_mv = 3300;
        std::uint16_t adc_bg_raw = 0;
        std::uint16_t adc0_raw = 0;
        std::uint16_t adc1_raw = 0;
        std::uint32_t pwm_freq = 1000;
        std::uint32_t pwm_duty = 50;
        unsigned char gpio_in = 0;
        unsigned char gpio_out = 0;
    };

    bool Create(CWnd* parent);
    void SetControlsEnabled(bool enabled);
    void SetPeerModeText(const std::wstring& text);
    void SetTelemetry(const TelemetryData& data);
    void SetAdcTelemetry(std::uint32_t adc0_mv, std::uint32_t adc1_mv, std::uint32_t vref_mv,
                         std::uint16_t adc0_raw, std::uint16_t adc1_raw, std::uint16_t adc_bg_raw);
    void SetGpioInputTelemetry(unsigned char gpio_in);
    void SetGpioOutputTelemetry(unsigned char gpio_out);
    void SetPwmTelemetry(std::uint32_t pwm_freq, std::uint32_t pwm_duty, bool update_controls);
    void SetPacketPreview(const std::wstring& tx_hex, const std::wstring& rx_hex);
    void SetFunctionRxPreview(unsigned char packet_id, const std::wstring& rx_hex);
    void SetPollingEnabled(bool adc_enabled, bool gpio_enabled, bool pwm_enabled);
    void GetPollingEnabled(bool* adc_enabled, bool* gpio_enabled, bool* pwm_enabled) const;
    int GetActiveSubTab() const { return m_activeSubTab; }
    bool GetGpioOutputState(bool* out_state) const;
    bool GetPwmSettings(std::uint32_t* out_freq, std::uint32_t* out_duty, std::wstring* error) const;

    static UINT MessageId() { return WM_APP + 220; }
    enum Action {
        ActionRefresh = 1,
        ActionApplyGpio = 2,
        ActionApplyPwm = 3,
        ActionPollingChanged = 4,
    };

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnSubTabChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRefreshClicked();
    afx_msg void OnApplyGpioClicked();
    afx_msg void OnApplyPwmClicked();
    afx_msg void OnPollOptionClicked();

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void UpdateSubTabVisibility();
    void NotifyParent(Action action);
    void DrawLed(CDC* dc, const CRect& rect, bool active);
    void UpdateFeatureControlState();
    void UpdatePwmEditsFromSliders();
    void UpdatePwmSlidersFromTelemetry();
    void UpdateAdcTelemetryText();
    void UpdateGpioInputText();
    void UpdateGpioOutputText();
    void UpdatePwmTelemetryText(bool update_controls);
    void UpdateTelemetryText();

    CFont m_uiFont;
    CFont m_monoFont;
    CStatic m_titleLabel;
    CStatic m_peerModeLabel;
    CStatic m_packetLabel;
    CButton m_refreshButton;
    CButton m_pollAdcCheck;
    CButton m_pollGpioCheck;
    CButton m_pollPwmCheck;
    CTabCtrl m_subTab;

    CStatic m_adc0Label;
    CStatic m_adc0Value;
    CStatic m_adc0Detail;
    CProgressCtrl m_adc0Bar;
    CStatic m_adc1Label;
    CStatic m_adc1Value;
    CStatic m_adc1Detail;
    CProgressCtrl m_adc1Bar;
    CStatic m_vrefLabel;
    CStatic m_vrefValue;
    CStatic m_bandgapLabel;
    CStatic m_bandgapValue;
    CStatic m_gpioInLabel;
    CStatic m_gpioInValue;
    CButton m_gpioOutCheck;
    CButton m_gpioApplyButton;
    CRect m_gpioInLedRect;
    CRect m_gpioOutLedRect;
    CStatic m_pwmFreqLabel;
    CStatic m_pwmFreqMinLabel;
    CStatic m_pwmFreqMaxLabel;
    CStatic m_pwmFreqValueLabel;
    CEdit m_pwmFreqEdit;
    CSliderCtrl m_pwmFreqSlider;
    CStatic m_pwmDutyLabel;
    CStatic m_pwmDutyMinLabel;
    CStatic m_pwmDutyMaxLabel;
    CStatic m_pwmDutyValueLabel;
    CEdit m_pwmDutyEdit;
    CSliderCtrl m_pwmDutySlider;
    CButton m_pwmApplyButton;
    CFunctionRawPanel m_rawPanel;

    TelemetryData m_telemetry;
    std::wstring m_peerMode = L"Unknown";
    bool m_controlsEnabled = false;
    int m_activeSubTab = 0;
};
