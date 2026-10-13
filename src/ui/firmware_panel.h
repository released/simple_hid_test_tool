#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <string>

class CFirmwarePanel : public CWnd {
public:
    bool Create(CWnd* parent);

    std::wstring GetFilePath() const;
    void SetFilePath(const std::wstring& path);
    void SetControlsEnabled(bool enabled);
    void SetBurnRunning(bool running);
    void SetProgress(int percent, const std::wstring& text);
    void SetPeerModeText(const std::wstring& text);

    static UINT MessageId() { return WM_APP + 230; }
    enum Action {
        ActionBrowse = 1,
        ActionBurn = 2,
        ActionEnterBoot = 3,
        ActionResetMcu = 4,
    };

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg void OnBrowseClicked();
    afx_msg void OnBurnClicked();
    afx_msg void OnEnterBootClicked();
    afx_msg void OnResetClicked();

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls(int cx, int cy);
    void NotifyParent(Action action);

    CStatic m_title;
    CStatic m_modeLabel;
    CStatic m_modeValue;
    CEdit m_fileEdit;
    CButton m_browseButton;
    CButton m_burnButton;
    CButton m_enterBootButton;
    CButton m_resetButton;
    CProgressCtrl m_progress;
    CStatic m_progressText;
    CFont m_uiFont;
    bool m_running = false;
};
