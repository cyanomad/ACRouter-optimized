/**
 * @file RemoteLogBuffer.h
 * @brief In-memory ring buffer that mirrors ESP_LOG output for remote retrieval
 *
 * Installs a custom vprintf hook (via esp_log_set_vprintf) that copies every
 * ESP_LOG* line into a fixed-size ring buffer, in addition to forwarding it
 * to the original destination (UART). This lets the console log be fetched
 * over HTTP - useful when the device is only reachable over WiFi and a
 * serial/USB cable isn't available (e.g. to read the output of `debug-adc`).
 */

#ifndef REMOTE_LOG_BUFFER_H
#define REMOTE_LOG_BUFFER_H

#include <Arduino.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

/**
 * @brief Captures ESP_LOG output into an in-memory ring buffer
 *
 * Singleton. Call begin() once at startup, as early as possible, so the
 * buffer captures as much of the boot log as fits.
 */
class RemoteLogBuffer {
public:
    static RemoteLogBuffer& getInstance();

    RemoteLogBuffer(const RemoteLogBuffer&) = delete;
    RemoteLogBuffer& operator=(const RemoteLogBuffer&) = delete;

    /**
     * @brief Install the log capture hook
     *
     * Safe to call multiple times; only installs once.
     */
    void begin();

    /**
     * @brief Get a snapshot of everything currently in the ring buffer,
     *        oldest line first.
     */
    String getBuffer();

    /**
     * @brief Clear the ring buffer
     */
    void clear();

private:
    RemoteLogBuffer() = default;

    static int vprintfHook(const char* fmt, va_list args);
    void append(const char* data, size_t len);

    // Keeps roughly the last ~4KB of log output - enough for several
    // periods of `debug-adc` output plus normal command responses.
    static constexpr size_t BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = {0};
    size_t m_write_pos = 0;
    bool m_wrapped = false;
    bool m_installed = false;

    vprintf_like_t m_original_vprintf = nullptr;
    portMUX_TYPE m_mux = portMUX_INITIALIZER_UNLOCKED;
};

#endif // REMOTE_LOG_BUFFER_H
