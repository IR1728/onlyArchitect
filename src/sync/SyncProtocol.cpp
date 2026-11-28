#include "ea/sync/SyncProtocol.hpp"
#include <bit>
#include <cstring>

namespace ea::sync {

namespace {

constexpr uint32_t MaxFrameBytes = 128u * 1024u * 1024u; // 128 MiB safety cap
constexpr size_t HeaderPrefixBytes = 4;                  // totalBodyLen
constexpr size_t FixedBodyBytes = 1 + 8 + 4;             // type + revision + metaLen

void appendU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

void appendI32(std::vector<uint8_t>& out, int32_t v) {
    appendU32(out, static_cast<uint32_t>(v));
}

void appendF32(std::vector<uint8_t>& out, float v) {
    appendU32(out, std::bit_cast<uint32_t>(v));
}

void appendU64(std::vector<uint8_t>& out, uint64_t v) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((v >> shift) & 0xFF));
    }
}

uint32_t readU32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

int32_t readI32(const uint8_t* p) {
    return static_cast<int32_t>(readU32(p));
}

float readF32(const uint8_t* p) {
    return std::bit_cast<float>(readU32(p));
}

uint64_t readU64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v = (v << 8) | static_cast<uint64_t>(p[i]);
    }
    return v;
}

} // namespace

std::vector<uint8_t> encodeFrame(const Frame& frame) {
    const uint32_t metaLen = static_cast<uint32_t>(frame.meta.size());
    const uint32_t bodyLen = static_cast<uint32_t>(FixedBodyBytes + metaLen + frame.payload.size());

    std::vector<uint8_t> out;
    out.reserve(HeaderPrefixBytes + bodyLen);
    appendU32(out, bodyLen);
    out.push_back(static_cast<uint8_t>(frame.type));
    appendU64(out, frame.revision);
    appendU32(out, metaLen);
    out.insert(out.end(), frame.meta.begin(), frame.meta.end());
    out.insert(out.end(), frame.payload.begin(), frame.payload.end());
    return out;
}

bool tryDecodeFrame(std::vector<uint8_t>& buffer, Frame& outFrame) {
    if (buffer.size() < HeaderPrefixBytes) return false;

    const uint32_t bodyLen = readU32(buffer.data());
    if (bodyLen < FixedBodyBytes || bodyLen > MaxFrameBytes) {
        buffer.clear();
        return false;
    }

    const size_t totalLen = HeaderPrefixBytes + static_cast<size_t>(bodyLen);
    if (buffer.size() < totalLen) return false;

    const uint8_t* body = buffer.data() + HeaderPrefixBytes;
    outFrame.type = static_cast<MessageType>(body[0]);
    outFrame.revision = readU64(body + 1);
    const uint32_t metaLen = readU32(body + 9);

    if (FixedBodyBytes + static_cast<size_t>(metaLen) > static_cast<size_t>(bodyLen)) {
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(totalLen));
        return false;
    }

    const uint8_t* metaPtr = body + FixedBodyBytes;
    outFrame.meta.assign(reinterpret_cast<const char*>(metaPtr), metaLen);

    const size_t payloadLen = static_cast<size_t>(bodyLen) - FixedBodyBytes - static_cast<size_t>(metaLen);
    const uint8_t* payloadPtr = metaPtr + metaLen;
    outFrame.payload.assign(payloadPtr, payloadPtr + payloadLen);

    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(totalLen));
    return true;
}

std::vector<uint8_t> encodeCursorPayload(const CursorPayload& cursor) {
    std::vector<uint8_t> out;
    out.reserve(12);
    appendI32(out, cursor.diagramId);
    appendF32(out, cursor.x);
    appendF32(out, cursor.y);
    return out;
}

std::optional<CursorPayload> decodeCursorPayload(const std::vector<uint8_t>& payload) {
    if (payload.size() != 12) return std::nullopt;
    CursorPayload c;
    c.diagramId = readI32(payload.data());
    c.x = readF32(payload.data() + 4);
    c.y = readF32(payload.data() + 8);
    return c;
}

std::vector<uint8_t> encodeElementMovePayload(const ElementMovePayload& move) {
    std::vector<uint8_t> out;
    out.reserve(24);
    appendI32(out, move.diagramId);
    appendI32(out, move.elementId);
    appendI32(out, move.left);
    appendI32(out, move.top);
    appendI32(out, move.right);
    appendI32(out, move.bottom);
    return out;
}

std::optional<ElementMovePayload> decodeElementMovePayload(const std::vector<uint8_t>& payload) {
    if (payload.size() != 24) return std::nullopt;
    ElementMovePayload m;
    m.diagramId = readI32(payload.data());
    m.elementId = readI32(payload.data() + 4);
    m.left = readI32(payload.data() + 8);
    m.top = readI32(payload.data() + 12);
    m.right = readI32(payload.data() + 16);
    m.bottom = readI32(payload.data() + 20);
    return m;
}

} // namespace ea::sync
