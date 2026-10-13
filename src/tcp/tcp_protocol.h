#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace tcp_tool {

static const std::size_t kRawPayloadSize = 64;
static const std::size_t kHeaderSize = 20;
static const std::uint32_t kProtocolVersion = 1;

enum Command : std::uint8_t {
    kCmdPing = 0x01,
    kCmdGetInfo = 0x02,
    kCmdGetUptime = 0x03,
    kCmdRaw64 = 0x10,
    kCmdResetToBoot = 0x20,
    kCmdIapStart = 0x30,
    kCmdIapData = 0x31,
    kCmdIapFinish = 0x32,
    kCmdResetMcu = 0x33,
};

enum Status : std::uint8_t {
    kStatusOk = 0,
    kStatusError = 1,
    kStatusInvalidState = 2,
    kStatusInvalidParam = 3,
    kStatusVerifyFailed = 4,
    kStatusBusy = 5,
};

enum PeerMode : std::uint8_t {
    kPeerUnknown = 0,
    kPeerApp = 1,
    kPeerBoot = 2,
};

enum RawPacket : std::uint8_t {
    kRawPktAdc = 0xA0,
    kRawPktGpio = 0xA1,
    kRawPktPwm = 0xA2,
};

static const std::uint32_t kAdcBandgapTypicalMv = 1200;
static const std::uint32_t kAdcFullScaleCounts = 4095;
static const unsigned char kRawIndicatorRead = 0x5A;
static const unsigned char kRawIndicatorWrite = 0xA5;

static const std::size_t kGetInfoPayloadSize = 20;
static const std::size_t kUptimePayloadSize = 4;

struct GetInfoPayload {
    std::uint8_t peer_mode = 0;
    std::uint8_t protocol_version = 0;
    std::uint16_t reserved = 0;
    std::uint32_t app_start_addr = 0;
    std::uint32_t app_size = 0;
    std::uint32_t max_chunk_size = 0;
    std::uint32_t uptime_seconds = 0;
};

struct UptimePayload {
    std::uint32_t uptime_seconds = 0;
};

struct Frame {
    std::uint8_t command = 0;
    std::uint8_t status = 0;
    std::uint32_t sequence = 0;
    std::vector<unsigned char> payload;
};

std::uint32_t Crc32(const unsigned char* data, std::size_t size);
std::uint32_t Crc32(const std::vector<unsigned char>& data);

void AppendUint32Le(std::vector<unsigned char>* buffer, std::uint32_t value);
void AppendUint16Le(std::vector<unsigned char>* buffer, std::uint16_t value);
void AppendUint24Le(std::vector<unsigned char>* buffer, std::uint32_t value);
std::uint32_t ReadUint32Le(const unsigned char* data);
std::uint32_t ReadUint24Le(const unsigned char* data);
std::uint16_t ReadUint16Le(const unsigned char* data);

std::vector<unsigned char> BuildFrame(const Frame& frame);
bool TryParseFrame(std::vector<unsigned char>* buffer, Frame* out_frame, std::wstring* error);
bool TryParseGetInfoPayload(const std::vector<unsigned char>& payload, GetInfoPayload* out_info);
bool TryParseUptimePayload(const std::vector<unsigned char>& payload, UptimePayload* out_info);
std::wstring StatusToText(std::uint8_t status);

}  // namespace tcp_tool
