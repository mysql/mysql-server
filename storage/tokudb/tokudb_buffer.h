#if !defined(_TOKUDB_BUFFER_H)
#define _TOKUDB_BUFFER_H

#include "tokudb_vlq.h"

namespace tokudb {

// A Buffer manages a contiguous chunk of memory and supports appending new data to the end of the buffer, and
// consuming chunks from the beginning of the buffer.  The buffer will reallocate memory when appending
// new data to a full buffer. 

class buffer {
public:
    buffer(void *data, size_t s, size_t l) : m_data(data), m_size(s), m_limit(l), m_is_static(true) {
    }
    buffer() : m_data(NULL), m_size(0), m_limit(0), m_is_static(false) {
    }
    virtual ~buffer() {
        if (!m_is_static)
            free(m_data);
    }

    // Return a pointer to the end of the buffer suitable for appending a fixed number of bytes.
    void *append_ptr(size_t s) {
        maybe_realloc(s);
        void *p = (char *) m_data + m_size;
        m_size += s;
        return p;
    }

    // Append bytes to the buffer
    void append(void *p, size_t s) {
        memcpy(append_ptr(s), p, s);
    }

    // Append an unsigned int to the buffer.
    // Returns the number of bytes used to encode the number.
    // Returns 0 if the number could not be encoded.
    template<class T> size_t append_ui(T n) {
        maybe_realloc(10); // 10 bytes is big enough for up to 64 bit number
        size_t s = tokudb::vlq_encode_ui<T>(n, (char *) m_data + m_size, 10);
        m_size += s;
        return s;
    }

    // Return a pointer to the next location in the buffer where bytes are consumed from.
    void *consume_ptr(size_t s) {
        if (m_size + s > m_limit)
            return NULL;
        void *p = (char *) m_data + m_size;
        m_size += s;
        return p;
    }

    // Consume bytes from the buffer.
    void consume(void *p, size_t s) {
        memcpy(p, consume_ptr(s), s);
    }

    // Consume an unsigned int from the buffer.
    // Returns 0 if the unsigned int could not be decoded, probably because the buffer is too short.
    // Otherwise return the number of bytes consumed, and stuffs the decoded number in *p.
    template<class T> size_t consume_ui(T *p) {
        size_t s = tokudb::vlq_decode_ui<T>(p, (char *) m_data + m_size, m_limit - m_size);
        m_size += s;
        return s;
    }

    // Write p_length bytes at an offset in the buffer
    void write(void *p, size_t p_length, size_t offset) {
        assert(offset + p_length <= m_size);
        memcpy((char *)m_data + offset, p, p_length);
    }

    // Read p_length bytes at an offset in the buffer
    void read(void *p, size_t p_length, size_t offset) {
        assert(offset + p_length <= m_size);
        memcpy(p, (char *)m_data + offset, p_length);
    }

    // Replace a field in the buffer with new data.  If the new data size is different, then readjust the 
    // size of the buffer and move things around.
    void replace(size_t offset, size_t old_s, void *new_p, size_t new_s) {
        assert(offset + old_s <= m_size);
        if (new_s > old_s)
            maybe_realloc(new_s - old_s);
        char *data_offset = (char *) m_data + offset;
        if (new_s != old_s) {
            size_t n = m_size - (offset + old_s);
            assert(offset + new_s + n <= m_limit && offset + old_s + n <= m_limit);
            memmove(data_offset + new_s, data_offset + old_s, n);
            if (new_s > old_s)
                m_size += new_s - old_s;
            else
                m_size -= old_s - new_s;
            assert(m_size <= m_limit);
        }
        memcpy(data_offset, new_p, new_s);
    }

    // Return a pointer to the data in the buffer
    void *data() {
        return m_data;
    }

    // Return the size of the data in the buffer
    size_t size() {
        return m_size;
    }

    // Return the size of the underlying memory in the buffer
    size_t limit() {
        return m_limit;
    }
private:
    // Maybe reallocate the buffer when it becomes full by doubling its size.
    void maybe_realloc(size_t s) {
        if (m_size + s > m_limit) {
            size_t new_limit = m_limit * 2;
            if (new_limit < m_size + s)
                new_limit = m_size + s;
            assert(!m_is_static);
            m_data = realloc(m_data, new_limit);
            m_limit = new_limit;
        }
    }   
private:
    void *m_data;
    size_t m_size;
    size_t m_limit;
    bool m_is_static;
};

};

#endif
