#pragma once

#include <string>

struct HidScanInfo {
    std::wstring host;
    unsigned short port = 5000;
    unsigned short input_report_length = 64;
    unsigned short output_report_length = 64;
    unsigned short feature_report_length = 0;
    unsigned int path_hash = 0;
};

std::wstring BuildEndpointHashSource(const HidScanInfo& info);

