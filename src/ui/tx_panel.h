#pragma once

#include <afxwin.h>
#include <afxcmn.h>

#include <array>
#include <string>
#include <vector>

#include "../core/command_parser.h"

class CTxPanel;

class CHexByteEdit : public CEdit {
public:
    void SetContext(CTxPanel* owner, int row, int col);

protected:
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnPaste();

    DECLARE_MESSAGE_MAP()

private:
    CTxPanel* owner_ = nullptr;
    int row_ = 0;
    int col_ = 0;
};

class CTxPanel : public CWnd {
public:
    bool Create(CWnd* parent);

    bool IsRowEnabled(int row) const;
    bool GetRowPayload(int row, std::vector<unsigned char>* out_bytes, std::wstring* error) const;
    std::wstring GetRowText(int row) const;
    void SetRowText(int row, const std::wstring& text);
    void SetRowEnabled(int row, bool enabled);

    void SetSchedulerEnabled(bool enabled);
    void SetRunning(bool running);
    bool GetReportId(unsigned char* out_value, std::wstring* error) const;
    void SetReportId(unsigned char value);
    bool IsHexPrefixMode() const;
    void SetHexPrefixMode(bool enabled);
    DWORD GetIntervalValue() const;
    bool IsIntervalEnabled() const;
    void SetIntervalValue(DWORD value);
    void SetIntervalEnabled(bool enabled);
    bool IsHandshakeEnabled() const;
    void SetHandshakeEnabled(bool enabled);
    int GetHandshakeRetryCount() const;
    void SetHandshakeRetryCount(int count);
    bool IsEchoModeEnabled() const;
    bool IsHandshakeModeEnabled() const;
    void SetEchoModeEnabled(bool enabled);
    void SetHandshakeModeEnabled(bool enabled);
    bool IsLimitEnabled() const;
    int GetLimitCount() const;
    void SetLimitEnabled(bool enabled);
    void SetLimitCount(int count);
    void SetLimitStatus(int sent, int remaining);
    void SetControlsEnabled(bool enabled);
    void FocusNextByte(int row, int col, bool forward);
    bool GetByteValue(int row, int col, unsigned char* out_value) const;
    bool PasteBytes(int row, int col, const std::wstring& text);
    void SetHandshakeState(int state);
    void SetLastEchoBytes(const std::vector<unsigned char>& bytes);
    void GetGridMetrics(int* out_start_x, int* out_cell_base, int* out_cell_extra,
                        int* out_row_height, int* out_row_gap) const;
    int GetPreferredHeight() const;

    static UINT MessageId() { return WM_APP + 220; }
    enum Action {
        ActionSendOnce = 1,
        ActionStart = 2,
        ActionStop = 3,
        ActionSingleShot = 4,
        ActionSettingsChanged = 5,
        ActionHover = 6,
        ActionHoverClear = 7,
        ActionSaveAs = 8,
        ActionReload = 9,
    };
    enum HandshakeState {
        HsOff = 0,
        HsWaiting = 1,
        HsOk = 2,
        HsError = 3,
    };

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg void OnStartClicked();
    afx_msg void OnSingleShotClicked();
    afx_msg void OnIntervalChanged();
    afx_msg void OnIntervalCheck();
    afx_msg void OnReportChanged();
    afx_msg void OnSelectAllClicked();
    afx_msg void OnHandshakeChanged();
    afx_msg void OnRetryChanged();
    afx_msg void OnRetryKillFocus();
    afx_msg void OnEchoModeChanged();
    afx_msg void OnHandshakeModeChanged();
    afx_msg void OnEchoHelpClicked();
    afx_msg void OnHandshakeHelpClicked();
    afx_msg void OnLimitChanged();
    afx_msg void OnLimitCheck();
    afx_msg void OnLimitKillFocus();
    afx_msg void OnSaveAsClicked();
    afx_msg void OnReloadClicked();
    afx_msg void OnRowCheckChanged(UINT id);
    afx_msg void OnRestoreClicked(UINT id);
    afx_msg void OnSendOnceClicked(UINT id);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    BOOL PreTranslateMessage(MSG* pMsg) override;

    DECLARE_MESSAGE_MAP()

private:
    struct RowControls {
        CButton sendCheck;
        std::array<CHexByteEdit, 64> bytes;
        CButton restoreButton;
        CButton sendOnceButton;
    };

    void LayoutControls(int cx, int cy);
    void RestoreDefault(int row);
    void NotifyParent(Action action, int row);
    void NotifyHover(int row, int col);
    void ClearHover();
    bool GetRowBytes(int row, std::vector<unsigned char>* out_bytes, std::wstring* error) const;
    void SetRowBytes(int row, const std::vector<unsigned char>& bytes);
    void UpdateSelectAllState();
    void UpdateEnableStates();

    static const int kRowCount = 6;

    std::array<RowControls, kRowCount> m_rows;
    CButton m_startButton;
    CButton m_singleShotButton;
    CComboBox m_intervalCombo;
    CButton m_intervalCheck;
    CStatic m_reportLabel;
    CEdit m_reportEdit;
    CStatic m_retryLabel;
    CEdit m_retryEdit;
    CSpinButtonCtrl m_retrySpin;
    CButton m_echoModeCheck;
    CButton m_handshakeModeCheck;
    CButton m_echoHelpButton;
    CButton m_handshakeHelpButton;
    CButton m_limitCheck;
    CEdit m_limitEdit;
    CSpinButtonCtrl m_limitSpin;
    CStatic m_limitStatus;
    CButton m_selectAllCheck;
    CButton m_handshakeCheck;
    CButton m_saveAsButton;
    CButton m_reloadButton;
    bool m_hexPrefix;
    bool m_running;
    bool m_connected = false;
    bool m_syncingSelectAll = false;
    CStatic m_echoLabel;
    std::array<CEdit, 64> m_echoBytes;
    int m_handshakeState = HsOff;
    int m_limitSent = 0;
    int m_limitRemaining = 0;
    int m_hoverRow = -1;
    int m_hoverCol = -1;
    CFont m_uiFont;
    CFont m_monoFont;
    CImageList m_iconRestore;
    CImageList m_iconSend;
    CImageList m_iconStart;
    CImageList m_iconSingle;
    CToolTipCtrl m_tooltip;
    CRect m_headerRect;
    int m_headerHeight = 18;
    int m_rowHeight = 24;
    int m_cellWidthBase = 10;
    int m_cellWidthExtra = 0;
    int m_dataStartX = 0;
    int m_rowButtonWidth = 90;
    int m_rowsTop = 0;
    int m_rowGap = 6;
};
