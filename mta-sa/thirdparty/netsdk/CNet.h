#pragma once
// netsdk/CNet.h
// Minimal but "complete" CNet stub to avoid missing include/link errors.
// This is a header-only stub that provides a realistic interface but no real network I/O.
// Modify/extend methods to add real networking (e.g. with WinSock or asio) as needed.

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <optional>

namespace netsdk {

// Status / error type
enum class NetStatus {
    OK,
    NotInitialized,
    AlreadyInitialized,
    NotConnected,
    ConnectionFailed,
    SendFailed,
    ReceiveFailed,
    Shutdown,
    UnknownError
};

// A small data buffer type alias
using Buffer = std::vector<char>;

// Callback types
using DataCallback = std::function<void(const Buffer& data)>;
using StatusCallback = std::function<void(NetStatus status, const std::string& message)>;

class CNet {
public:
    CNet() = default;
    ~CNet() {
        Shutdown();
    }

    // --- Initialization / Shutdown ---
    // Returns true if succesfully initialized (stub returns true).
    bool Initialize() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_initialized) {
            m_last_error = "Already initialized";
            return false;
        }
        m_initialized = true;
        m_last_status = NetStatus::OK;
        invokeStatusCallback(NetStatus::OK, "Initialized (stub)");
        return true;
    }

    // Shutdown - stops internal worker, clears callbacks.
    void Shutdown() {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            if (!m_initialized) {
                return;
            }
            m_running = false;
            m_connected = false;
            m_initialized = false;
        }
        // join worker if running
        if (m_worker.joinable()) m_worker.join();
        invokeStatusCallback(NetStatus::Shutdown, "Shutdown (stub)");
    }

    // --- Connection management ---
    // Stub Connect: sets connected state true only if initialized. Returns true on success.
    bool Connect(const std::string& host, uint16_t port, int timeout_ms = 5000) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_initialized) {
            m_last_error = "Not initialized";
            m_last_status = NetStatus::NotInitialized;
            return false;
        }
        if (m_connected) {
            m_last_error = "Already connected";
            m_last_status = NetStatus::OK;
            return true;
        }

        // Simulate connection delay (non-blocking minimal)
        m_connected = true;
        m_host = host;
        m_port = port;
        m_last_status = NetStatus::OK;
        m_last_error.clear();

        // start background worker that simulates incoming data (optional stub behavior)
        m_running = true;
        if (m_worker.joinable()) m_worker.join();
        m_worker = std::thread([this]() { this->workerLoop(); });

        invokeStatusCallback(NetStatus::OK, "Connected to " + host + ":" + std::to_string(port) + " (stub)");
        return true;
    }

    // Disconnect - marks disconnected and stops worker.
    void Disconnect() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_connected) return;
        m_connected = false;
        m_running = false;
        m_host.clear();
        m_port = 0;
        if (m_worker.joinable()) {
            m_worker.join();
        }
        invokeStatusCallback(NetStatus::OK, "Disconnected (stub)");
    }

    bool IsConnected() const {
        return m_connected.load();
    }

    // --- Send / Receive ---
    // Send data (stub returns true if connected)
    bool Send(const Buffer& buffer) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_initialized) {
            m_last_error = "Not initialized";
            m_last_status = NetStatus::NotInitialized;
            return false;
        }
        if (!m_connected) {
            m_last_error = "Not connected";
            m_last_status = NetStatus::NotConnected;
            return false;
        }
        // In a real implementation we'd write to socket here.
        // For stub: we store last sent buffer and optionally "echo" back.
        m_last_sent = buffer;
        m_last_status = NetStatus::OK;
        invokeStatusCallback(NetStatus::OK, "Sent " + std::to_string(buffer.size()) + " bytes (stub)");
        return true;
    }

    // Convenience overload for std::string
    bool Send(const std::string& text) {
        Buffer b(text.begin(), text.end());
        return Send(b);
    }

    // Receive: non-blocking attempt to pop one message from internal queue.
    // Returns optional<Buffer>, std::nullopt when no data.
    std::optional<Buffer> Receive() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_initialized || !m_connected) {
            return std::nullopt;
        }
        if (m_incoming.empty()) return std::nullopt;
        Buffer buf = std::move(m_incoming.front());
        m_incoming.erase(m_incoming.begin());
        return buf;
    }

    // Register callbacks
    void SetDataCallback(DataCallback cb) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_data_callback = std::move(cb);
    }

    void SetStatusCallback(StatusCallback cb) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_status_callback = std::move(cb);
    }

    // Utility getters
    NetStatus GetLastStatus() const {
        return m_last_status;
    }
    std::string GetLastError() const {
        return m_last_error;
    }

    // For testing: push fake incoming data into stub
    void PushIncoming(const Buffer& b) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_incoming.push_back(b);
        // Immediately invoke callback if exists
        if (m_data_callback) {
            // call without holding mutex to avoid deadlocks
            DataCallback cb = m_data_callback;
            lk.~lock_guard();
            cb(b);
        }
    }

private:
    // internal worker: periodically (stub) generates incoming data if "echo mode"
    void workerLoop() {
        // simple loop that, while running and connected, sleeps and optionally echoes last_sent
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            if (!m_connected) continue;

            // echo back sent buffer if present (stub behavior)
            Buffer echo;
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                if (!m_last_sent.empty()) {
                    echo = m_last_sent;
                    // clear last_sent so we don't echo repeatedly
                    m_last_sent.clear();
                }
            }
            if (!echo.empty()) {
                // push into incoming and call callback
                {
                    std::lock_guard<std::mutex> lk(m_mutex);
                    m_incoming.push_back(echo);
                }
                if (m_data_callback) {
                    m_data_callback(echo);
                }
            }
        }
    }

    void invokeStatusCallback(NetStatus s, const std::string& msg) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_status_callback) {
            // call callback without holding lock to avoid client deadlock potential
            StatusCallback cb = m_status_callback;
            lk.~lock_guard();
            cb(s, msg);
        }
    }

private:
    // state
    std::atomic<bool> m_initialized{ false };
    std::atomic<bool> m_connected{ false };
    std::atomic<bool> m_running{ false };

    // network params (stub)
    std::string m_host;
    uint16_t m_port{ 0 };

    // callbacks
    DataCallback m_data_callback{ nullptr };
    StatusCallback m_status_callback{ nullptr };

    // message queues & buffers (protected by m_mutex)
    Buffer m_last_sent;
    std::vector<Buffer> m_incoming;

    // status / error
    NetStatus m_last_status{ NetStatus::NotInitialized };
    std::string m_last_error;

    // internal worker
    std::thread m_worker;
    mutable std::mutex m_mutex;
};

} // namespace netsdk
