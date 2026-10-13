#include "hid_scan.h"

std::wstring BuildEndpointHashSource(const HidScanInfo& info) {
    return info.host + L":" + std::to_wstring(info.port);
}
