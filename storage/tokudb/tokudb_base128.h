#ifndef _TOKUDB_BASE128_H
#define _TOKUDB_BASE128_H

namespace tokudb {

    static size_t base128_encode_uint32(uint32_t n, void *p, size_t s) {
        unsigned char *pp = (unsigned char *)p;
        uint i = 0;
        while (i < s) {
            uint32_t m = n & 127;
            n >>= 7;
            if (n != 0)
                m |= 128;
            pp[i++] = m;
            if (n == 0)
                break;
        }
        return i;
    }

    static size_t base128_decode_uint32(uint32_t *np, void *p, size_t s) {
        unsigned char *pp = (unsigned char *)p;
        uint32_t n = 0;
        uint i = 0;
        while (i < s) {
            uint m = pp[i];
            n |= (m & 127) << 7*i;
            i++;
            if ((m & 128) == 0)
                break;
        }
        *np = n;
        return i;
    }

}

#endif
