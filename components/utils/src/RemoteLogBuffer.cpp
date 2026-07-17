/**
 * @file RemoteLogBuffer.cpp
 * @brief Implementation of the ESP_LOG-capturing ring buffer
 */

#include "RemoteLogBuffer.h"
#include <cstdio>
#include <cstring>

// ============================================================
// Singleton Instance
// ============================================================

RemoteLogBuffer& RemoteLogBuffer::getInstance() {
    static RemoteLogBuffer instance;
    return instance;
}

// ============================================================
// Installation
// ============================================================

void RemoteLogBuffer::begin() {
    if (m_installed) {
        return;
    }
    m_installed = true;
    m_original_vprintf = esp_log_set_vprintf(&RemoteLogBuffer::vprintfHook);
}

// ============================================================
// vprintf Hook
// ============================================================
// Called by the ESP-IDF logging system for every ESP_LOG* line. Must copy
// the formatted text into our ring buffer, then forward to whatever vprintf
// implementation was installed before us (normally the one that writes to
// UART), so the serial monitor keeps working exactly as before.

int RemoteLogBuffer::vprintfHook(const char* fmt, va_list args) {
    RemoteLogBuffer& inst = getInstance();

    char line[256];
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(line, sizeof(line), fmt, args_copy);
    va_end(args_copy);

    if (len > 0) {
        size_t copy_len = (static_cast<size_t>(len) < sizeof(line)) ? static_cast<size_t>(len) : sizeof(line) - 1;
        inst.append(line, copy_len);
    }

    if (inst.m_original_vprintf) {
        return inst.m_original_vprintf(fmt, args);
    }
    return len;
}

// ============================================================
// Ring Buffer Operations
// ============================================================

void RemoteLogBuffer::append(const char* data, size_t len) {
    portENTER_CRITICAL(&m_mux);
    for (size_t i = 0; i < len; i++) {
        m_buffer[m_write_pos] = data[i];
        m_write_pos++;
        if (m_write_pos >= BUFFER_SIZE) {
            m_write_pos = 0;
            m_wrapped = true;
        }
    }
    portEXIT_CRITICAL(&m_mux);
}

String RemoteLogBuffer::getBuffer() {
    String result;

    portENTER_CRITICAL(&m_mux);
    if (!m_wrapped) {
        result.concat(m_buffer, m_write_pos);
    } else {
        // Oldest byte is right after the current write position;
        // read tail-then-head to get chronological order.
        size_t tail_len = BUFFER_SIZE - m_write_pos;
        result.concat(m_buffer + m_write_pos, tail_len);
        result.concat(m_buffer, m_write_pos);
    }
    portEXIT_CRITICAL(&m_mux);

    return result;
}

void RemoteLogBuffer::clear() {
    portENTER_CRITICAL(&m_mux);
    memset(m_buffer, 0, BUFFER_SIZE);
    m_write_pos = 0;
    m_wrapped = false;
    portEXIT_CRITICAL(&m_mux);
}
