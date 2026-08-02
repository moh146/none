#pragma once
// netsdk/packet.h
// Minimal, complete, and real-looking Packet class for networking/stub use.
// Works with netsdk::CNet stub and avoids compilation errors.

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace netsdk {

// Packet class: represents a network message / byte buffer
class Packet {
public:
    Packet() = default;

    // Construct packet from existing buffer
    explicit Packet(const std::vector<char>& data) : m_buffer(data), m_read_pos(0) {}

    explicit Packet(const char* data, size_t size) : m_buffer(data, data + size), m_read_pos(0) {}

    // Append raw bytes
    void append(const char* data, size_t size) {
        m_buffer.insert(m_buffer.end(), data, data + size);
    }

    // Append basic types (int, float, double, etc.)
    template<typename T>
    void append(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable");
        const char* ptr = reinterpret_cast<const char*>(&value);
        append(ptr, sizeof(T));
    }

    // Append std::string (length + bytes)
    void append(const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        append(len); // prepend length
        append(str.data(), str.size());
    }

    // Read data of type T
    template<typename T>
    T read() {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable");
        if (m_read_pos + sizeof(T) > m_buffer.size())
            throw std::out_of_range("Packet: read past end");
        T value;
        std::memcpy(&value, m_buffer.data() + m_read_pos, sizeof(T));
        m_read_pos += sizeof(T);
        return value;
    }

    // Read std::string (assumes length-prefixed)
    std::string readString() {
        uint32_t len = read<uint32_t>();
        if (m_read_pos + len > m_buffer.size())
            throw std::out_of_range("Packet: readString past end");
        std::string s(m_buffer.data() + m_read_pos, len);
        m_read_pos += len;
        return s;
    }

    // Reset read pointer
    void reset() { m_read_pos = 0; }

    // Clear packet
    void clear() { m_buffer.clear(); m_read_pos = 0; }

    // Get underlying buffer
    const std::vector<char>& data() const { return m_buffer; }

    size_t size() const { return m_buffer.size(); }
    bool empty() const { return m_buffer.empty(); }

    // Read remaining bytes
    std::vector<char> readRemaining() {
        std::vector<char> remaining;
        if (m_read_pos < m_buffer.size()) {
            remaining.assign(m_buffer.begin() + m_read_pos, m_buffer.end());
            m_read_pos = m_buffer.size();
        }
        return remaining;
    }

private:
    std::vector<char> m_buffer;
    size_t m_read_pos{0};
};

} // namespace netsdk
