#include "tcp_protocol.h"

#include <algorithm>

namespace tcp_tool {

namespace {

const unsigned char kMagic[4] = { 'T', 'C', 'P', '1' };

bool HasMagic(const std::vector<unsigned char>& buffer, std::size_t offset) {
    return (offset + 4 <= buffer.size()) &&
           buffer[offset] == kMagic[0] &&
           buffer[offset + 1] == kMagic[1] &&
           buffer[offset + 2] == kMagic[2] &&
           buffer[offset + 3] == kMagic[3];
}

}

std::uint32_t Crc32(const unsigned char* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    std::size_t index = 0;
    std::size_t bit = 0;

    if (!data) {
        return 0;
    }

    for (index = 0; index < size; ++index) {
        crc ^= static_cast<std::uint32_t>(data[index]);
        for (bit = 0; bit < 8; ++bit) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1u) ^ 0xEDB88320u;
            } else {
                crc >>= 1u;
            }
        }
    }

    return ~crc;
}

std::uint32_t Crc32(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        return 0;
    }
    return Crc32(data.data(), data.size());
}

void AppendUint32Le(std::vector<unsigned char>* buffer, std::uint32_t value) {
    if (!buffer) {
        return;
    }
    buffer->push_back(static_cast<unsigned char>(value & 0xFFu));
    buffer->push_back(static_cast<unsigned char>((value >> 8u) & 0xFFu));
    buffer->push_back(static_cast<unsigned char>((value >> 16u) & 0xFFu));
    buffer->push_back(static_cast<unsigned char>((value >> 24u) & 0xFFu));
}

void AppendUint16Le(std::vector<unsigned char>* buffer, std::uint16_t value) {
    if (!buffer) {
        return;
    }
    buffer->push_back(static_cast<unsigned char>(value & 0xFFu));
    buffer->push_back(static_cast<unsigned char>((value >> 8u) & 0xFFu));
}

void AppendUint24Le(std::vector<unsigned char>* buffer, std::uint32_t value) {
    if (!buffer) {
        return;
    }
    buffer->push_back(static_cast<unsigned char>(value & 0xFFu));
    buffer->push_back(static_cast<unsigned char>((value >> 8u) & 0xFFu));
    buffer->push_back(static_cast<unsigned char>((value >> 16u) & 0xFFu));
}

std::uint32_t ReadUint32Le(const unsigned char* data) {
    if (!data) {
        return 0;
    }
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8u) |
           (static_cast<std::uint32_t>(data[2]) << 16u) |
           (static_cast<std::uint32_t>(data[3]) << 24u);
}

std::uint32_t ReadUint24Le(const unsigned char* data) {
    if (!data) {
        return 0;
    }
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8u) |
           (static_cast<std::uint32_t>(data[2]) << 16u);
}

std::uint16_t ReadUint16Le(const unsigned char* data) {
    if (!data) {
        return 0;
    }
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(data[1] << 8u);
}

std::vector<unsigned char> BuildFrame(const Frame& frame) {
    std::vector<unsigned char> buffer;
    std::uint32_t payload_crc = 0;

    buffer.reserve(kHeaderSize + frame.payload.size());
    buffer.insert(buffer.end(), kMagic, kMagic + 4);
    buffer.push_back(frame.command);
    buffer.push_back(frame.status);
    AppendUint16Le(&buffer, 0);
    AppendUint32Le(&buffer, frame.sequence);
    AppendUint32Le(&buffer, static_cast<std::uint32_t>(frame.payload.size()));

    if (!frame.payload.empty()) {
        payload_crc = Crc32(frame.payload);
    }
    AppendUint32Le(&buffer, payload_crc);
    buffer.insert(buffer.end(), frame.payload.begin(), frame.payload.end());
    return buffer;
}

bool TryParseFrame(std::vector<unsigned char>* buffer, Frame* out_frame, std::wstring* error) {
    std::size_t offset = 0;
    std::size_t payload_length = 0;
    std::uint32_t payload_crc = 0;
    std::uint32_t calc_crc = 0;

    if (!buffer || !out_frame) {
        return false;
    }

    while (offset < buffer->size() && !HasMagic(*buffer, offset)) {
        ++offset;
    }

    if (offset > 0) {
        buffer->erase(buffer->begin(), buffer->begin() + static_cast<std::ptrdiff_t>(offset));
    }

    if (buffer->size() < kHeaderSize) {
        return false;
    }

    payload_length = static_cast<std::size_t>(ReadUint32Le(&(*buffer)[12]));
    if (payload_length > (1024u * 1024u)) {
        if (error) {
            *error = L"TCP frame payload too large.";
        }
        buffer->clear();
        return false;
    }

    if (buffer->size() < (kHeaderSize + payload_length)) {
        return false;
    }

    out_frame->command = (*buffer)[4];
    out_frame->status = (*buffer)[5];
    out_frame->sequence = ReadUint32Le(&(*buffer)[8]);
    payload_crc = ReadUint32Le(&(*buffer)[16]);
    out_frame->payload.assign(buffer->begin() + static_cast<std::ptrdiff_t>(kHeaderSize),
                              buffer->begin() + static_cast<std::ptrdiff_t>(kHeaderSize + payload_length));

    calc_crc = out_frame->payload.empty() ? 0u : Crc32(out_frame->payload);
    if (calc_crc != payload_crc) {
        if (error) {
            *error = L"TCP frame CRC mismatch.";
        }
        buffer->erase(buffer->begin(), buffer->begin() + static_cast<std::ptrdiff_t>(kHeaderSize + payload_length));
        return false;
    }

    buffer->erase(buffer->begin(), buffer->begin() + static_cast<std::ptrdiff_t>(kHeaderSize + payload_length));
    return true;
}

bool TryParseGetInfoPayload(const std::vector<unsigned char>& payload, GetInfoPayload* out_info) {
    if (!out_info || payload.size() < kGetInfoPayloadSize) {
        return false;
    }
    out_info->peer_mode = payload[0];
    out_info->protocol_version = payload[1];
    out_info->reserved = ReadUint16Le(&payload[2]);
    out_info->app_start_addr = ReadUint32Le(&payload[4]);
    out_info->app_size = ReadUint32Le(&payload[8]);
    out_info->max_chunk_size = ReadUint32Le(&payload[12]);
    out_info->uptime_seconds = ReadUint32Le(&payload[16]);
    return true;
}

bool TryParseUptimePayload(const std::vector<unsigned char>& payload, UptimePayload* out_info) {
    if (!out_info || payload.size() < kUptimePayloadSize) {
        return false;
    }
    out_info->uptime_seconds = ReadUint32Le(payload.data());
    return true;
}

std::wstring StatusToText(std::uint8_t status) {
    switch (status) {
    case kStatusOk:
        return L"OK";
    case kStatusError:
        return L"Error";
    case kStatusInvalidState:
        return L"Invalid state";
    case kStatusInvalidParam:
        return L"Invalid parameter";
    case kStatusVerifyFailed:
        return L"Verify failed";
    case kStatusBusy:
        return L"Busy";
    default:
        return L"Unknown";
    }
}

}  // namespace tcp_tool
