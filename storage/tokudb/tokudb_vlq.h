#ifndef _TOKUDB_VLQ_H
#define _TOKUDB_VLQ_H

namespace tokudb {

    // Variable length encode an unsigned integer into a buffer with limit s.
    // Returns the number of bytes used to encode n in the buffer.
    // Returns 0 if the buffer is too small.
    template <class T> size_t vlq_encode_ui(T n, void *p, size_t s) {
        unsigned char *pp = (unsigned char *)p;
        size_t i = 0;
        while (n >= 128) {
            if (i >= s)
                return 0; // not enough space
            pp[i++] = n%128;
            n = n/128;
        }
        if (i >= s) 
            return 0; // not enough space
        pp[i++] = 128+n;
        return i;
    }

    // Variable length decode an unsigned integer from a buffer with limit s.
    // Returns the number of bytes used to decode the buffer.
    // Returns 0 if the buffer is too small.
    template <class T> size_t vlq_decode_ui(T *np, void *p, size_t s) {
        unsigned char *pp = (unsigned char *)p;
        T n = 0;
        size_t i = 0;
        while (1) {
            if (i >= s)
                return 0; // not a full decode
            T m = pp[i];
            n |= (m & 127) << (7*i);
            i++;
            if ((m & 128) != 0)
                break;
        }
        *np = n;
        return i;
    }
}

#endif
