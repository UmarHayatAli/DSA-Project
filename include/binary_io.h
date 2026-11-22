// ---------------------------------------------------------------
// binary_io.h
//
// Small helpers to write and read numbers and strings in binary files.
//
// Why not just write a whole struct with out.write(&record, sizeof(record))?
// Because the compiler may insert invisible "padding" bytes inside a struct,
// and the size/layout can differ between compilers. Writing every field one
// at a time, always as little-endian bytes (lowest byte first), gives the
// same file on every computer.
//
// All functions are "inline" so this header can be included in many .cpp files.
// ---------------------------------------------------------------
#pragma once

#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>
#include <string>

// ----- writing -----

inline void writeU8(std::ostream& out, uint8_t value) {
    out.put(static_cast<char>(value));
}

inline void writeU16(std::ostream& out, uint16_t value) {
    for (int i = 0; i < 2; i++) out.put(static_cast<char>((value >> (8 * i)) & 0xFF));
}

inline void writeU32(std::ostream& out, uint32_t value) {
    for (int i = 0; i < 4; i++) out.put(static_cast<char>((value >> (8 * i)) & 0xFF));
}

inline void writeU64(std::ostream& out, uint64_t value) {
    for (int i = 0; i < 8; i++) out.put(static_cast<char>((value >> (8 * i)) & 0xFF));
}

// A float is 4 bytes; we copy its bits into a uint32_t and write that.
inline void writeF32(std::ostream& out, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeU32(out, bits);
}

// A string is written as [length u16][the bytes]. Longer strings are cut at 65,535 bytes.
inline void writeString(std::ostream& out, const std::string& s) {
    uint16_t length = static_cast<uint16_t>(s.size() > 65535 ? 65535 : s.size());
    writeU16(out, length);
    out.write(s.data(), length);
}

// ----- reading (each returns false if the file ended or broke) -----

inline bool readU8(std::istream& in, uint8_t& value) {
    char c;
    if (!in.get(c)) return false;
    value = static_cast<uint8_t>(c);
    return true;
}

inline bool readU16(std::istream& in, uint16_t& value) {
    unsigned char b[2];
    if (!in.read(reinterpret_cast<char*>(b), 2)) return false;
    value = static_cast<uint16_t>(b[0] | (b[1] << 8));
    return true;
}

inline bool readU32(std::istream& in, uint32_t& value) {
    unsigned char b[4];
    if (!in.read(reinterpret_cast<char*>(b), 4)) return false;
    value = 0;
    for (int i = 3; i >= 0; i--) value = (value << 8) | b[i];
    return true;
}

inline bool readU64(std::istream& in, uint64_t& value) {
    unsigned char b[8];
    if (!in.read(reinterpret_cast<char*>(b), 8)) return false;
    value = 0;
    for (int i = 7; i >= 0; i--) value = (value << 8) | b[i];
    return true;
}

inline bool readF32(std::istream& in, float& value) {
    uint32_t bits;
    if (!readU32(in, bits)) return false;
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

inline bool readString(std::istream& in, std::string& s) {
    uint16_t length;
    if (!readU16(in, length)) return false;
    s.resize(length);
    return length == 0 || static_cast<bool>(in.read(&s[0], length));
}
