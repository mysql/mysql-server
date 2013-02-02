#ifndef _TOKUDB_VLQ_H
#define _TOKUDB_VLQ_H

namespace tokudb {

    static size_t vlq_encode_uint32(uint32_t n, void *p, size_t s) {
        unsigned char *pp = (unsigned char *)p;
        size_t i = 0;
        while (n >= 128) {
            pp[i++] = n%128;
            n = n/128;
        }
        pp[i++] = 128+n;
        return i;
    }

    static size_t vlq_decode_uint32(uint32_t *np, void *p, size_t s) {
        unsigned char *pp = (unsigned char *)p;
        uint32_t n = 0;
        uint i = 0;
        while (i < s) {
            unsigned char m = pp[i];
            n |= (m & 127) << 7*i;
            i++;
            if ((m & 128) != 0)
                break;
        }
        *np = n;
        return i;
    }

}

#endif
