#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ea::sync {

enum class MessageType : uint8_t {
    Hello = 1,
    Welcome = 2,
    PushUpdate = 3,
    PushAck = 4,
    BroadcastUpdate = 5,
    Conflict = 6,
    PeerList = 7,
    Error = 8,
    CursorMove = 9,
    ElementMove = 10,
    ChatMessage = 11
};

struct Frame {
    MessageType type{MessageType::Error};
    uint64_t revision{0};
    std::string meta;              // UTF-8 username, comma-separated peers, or error message
    std::vector<uint8_t> payload;  // Snapshot bytes or compact real-time event payload
};

struct CursorPayload {
    int32_t diagramId{0};
    float x{0.0f};
    float y{0.0f};
};

struct ElementMovePayload {
    int32_t diagramId{0};
    int32_t elementId{0};
    int32_t left{0};
    int32_t top{0};
    int32_t right{0};
    int32_t bottom{0};
};

// Length-prefixed binary wire format (big-endian):
//   [4 bytes totalBodyLen]
//   [1 byte  MessageType]
//   [8 bytes revision]
//   [4 bytes metaLen]
//   [metaLen bytes UTF-8 meta]
//   [remaining bytes payload]
std::vector<uint8_t> encodeFrame(const Frame& frame);

// Attempts to decode one complete frame from `buffer`.
// If enough bytes are present, populates `outFrame`, erases consumed bytes from `buffer`, and returns true.
// If incomplete, leaves `buffer` untouched and returns false.
bool tryDecodeFrame(std::vector<uint8_t>& buffer, Frame& outFrame);

std::vector<uint8_t> encodeCursorPayload(const CursorPayload& cursor);
std::optional<CursorPayload> decodeCursorPayload(const std::vector<uint8_t>& payload);

std::vector<uint8_t> encodeElementMovePayload(const ElementMovePayload& move);
std::optional<ElementMovePayload> decodeElementMovePayload(const std::vector<uint8_t>& payload);

} // namespace ea::sync
