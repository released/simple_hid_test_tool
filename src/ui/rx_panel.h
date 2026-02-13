#pragma once

#include <afxwin.h>
#include <afxcmn.h>

#include <array>
#include <atomic>
#include <deque>
#include <string>
#include <vector>

#include "../hid/hid_device.h"
#include "../log/logger.h"

class CRxPanel : public CWnd {
public:
    bool Create(CWnd* parent);

    void SetDevice(HidDevice* device);
    void StartCapture();
    void StopCapture();
    bool WaitForPayload(const std::vector<unsigned char>& payload, DWORD timeout_ms);
    bool GetLastPayload(std::vector<unsigned char>* out_payload);
    bool GetLastPayloadHex(std::wstring* out_text);
    void LogTxLine(const std::wstring& line);

    bool IsLogEnabled() const;
    bool IsTimestampEnabled() const;
    void SetLogEnabled(bool enabled);
    void SetTimestampEnabled(bool enabled);
    void SetControlsEnabled(bool enabled);
    void SetClearEnabled(bool enabled);
    void SetExternalGridMetrics(int start_x, int cell_base, int cell_extra, int row_height, int row_gap);
    void ClearData();
    void SetRxState(int state);
    int GetRxState() const { return m_rxState; }
    int GetPreferredHeight() const;

    static UINT MessageId() { return WM_APP + 210; }
    enum RxEvent {
        RxCount = 1,
        RxDeviceRemoved = 2,
    };
    enum RxState {
        RxIdle = 0,
        RxReceiving = 1,
        RxTimeout = 2,
    };

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg void OnDestroy();
    afx_msg void OnSaveLogClicked();
    afx_msg void OnTimestampClicked();
    afx_msg void OnClearClicked();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    BOOL PreTranslateMessage(MSG* pMsg) override;

    DECLARE_MESSAGE_MAP()

private:
    static const int kRowCount = 10;
    static const int kByteCount = 64;

    static UINT RxThreadProc(LPVOID param);
    UINT ThreadLoop();

    void LayoutControls(int cx, int cy);
    void UpdateGridControls();
    void ProcessPendingPayloads();
    std::wstring FormatTimestamp() const;
    std::array<unsigned char, 64> ExtractPayload(const std::vector<unsigned char>& report) const;
    void AppendRow(const std::array<unsigned char, 64>& payload, const std::wstring& timestamp);
    void UpdateLastPayload(const std::vector<unsigned char>& report);
    void WriteDirectionalLogLine(const std::wstring& direction, const std::wstring& body, bool timestamped);

    CButton m_saveCheck;
    CButton m_timestampCheck;
    CButton m_clearButton;
    CRect m_headerRect;
    CRect m_gridRect;

    struct Row {
        std::array<unsigned char, 64> data{};
        std::wstring timestamp;
        bool valid = false;
    };
    std::deque<Row> m_rows;
    std::array<std::array<CEdit, kByteCount>, kRowCount> m_cells;
    HidDevice* m_device;
    CWinThread* m_thread;
    std::atomic<bool> m_stopRequested;
    RxLogger m_logger;
    int m_rxState = RxIdle;
    bool m_cleared = false;

    HANDLE m_rxEvent;
    CRITICAL_SECTION m_payloadLock;
    std::vector<unsigned char> m_lastPayload;
    CFont m_uiFont;
    CFont m_monoFont;
    CToolTipCtrl m_tooltip;
    UINT_PTR m_uiTimer = 0;
    int m_rowHeight = 18;
    int m_headerHeight = 18;
    int m_cellWidthBase = 14;
    int m_cellWidthExtra = 0;
    int m_timestampWidth = 0;
    int m_dataStartX = 0;
    int m_rowLabelWidth = 18;
    bool m_hasExternalGrid = false;
    int m_externalStartX = 0;
    int m_externalCellBase = 0;
    int m_externalCellExtra = 0;
    int m_externalRowHeight = 0;
    int m_externalRowGap = 0;
    int m_rowGap = 6;
    CRITICAL_SECTION m_pendingLock;
    std::deque<std::array<unsigned char, 64>> m_pendingPayloads;
    std::atomic<bool> m_logEnabled{ false };
    std::atomic<bool> m_timestampEnabled{ false };
};

