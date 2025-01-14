/*
	Compression helpers.
*/

#include "compress.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// LZJB
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define NBBY		8
#define	MATCH_BITS	6
#define	MATCH_MIN	3
#define	MATCH_MAX	((1 << MATCH_BITS) + (MATCH_MIN - 1))
#define	OFFSET_MASK	((1 << (16 - MATCH_BITS)) - 1)
#define	LEMPEL_SIZE	1024

// STATIC
size_t LZJB::compress(const uint8_t* s_start, uint8_t* d_start, size_t s_len, size_t d_len) {
	const uint8_t* src = s_start;
	uint8_t* dst = d_start;
	const uint8_t* cpy;
	uint8_t* copymap = 0;
	int copymask = 1 << (NBBY - 1);
	int mlen, offset, hash;
	uint16_t* hp;
	uint16_t lempel[LEMPEL_SIZE] = { 0 };
	while (src < s_start + s_len) {
		if ((copymask <<= 1) == (1 << NBBY)) {
			if (dst >= d_start + d_len - 1 - 2 * NBBY) {
				return s_len;
			}
			copymask = 1;
			copymap = dst;
			*dst++ = 0;
		}
		if (src > s_start + s_len - MATCH_MAX) {
			*dst++ = *src++;
			continue;
		}
		hash = (src[0] << 16) + (src[1] << 8) + src[2];
		hash += hash >> 9;
		hash += hash >> 5;
		hp = &lempel[hash & (LEMPEL_SIZE - 1)];
		offset = (uint64_t)(src - *hp) & OFFSET_MASK;
		*hp = static_cast<uint16_t>((uint64_t)src);
		cpy = src - offset;
		if (cpy >= s_start && cpy != src &&
		    src[0] == cpy[0] && src[1] == cpy[1] && src[2] == cpy[2]) {
			*copymap |= copymask;
			for (mlen = MATCH_MIN; mlen < MATCH_MAX; ++mlen) {
				if (src[mlen] != cpy[mlen]) {
					break;
				}
			}
			*dst++ = ((mlen - MATCH_MIN) << (NBBY - MATCH_BITS)) | (offset >> NBBY);
			*dst++ = static_cast<uint8_t>(offset);
			src += mlen;
		} else {
			*dst++ = *src++;
		}
	}
	return dst - d_start;
}

// STATIC
int LZJB::decompress(const uint8_t* s_start, uint8_t* d_start, size_t s_len, size_t d_len) {
	const uint8_t* src = s_start;
	uint8_t* dst = d_start;
	uint8_t* d_end = (uint8_t*)d_start + d_len;
	uint8_t* cpy;
	uint8_t copymap = 0;
	int copymask = 1 << (NBBY - 1);
	while (dst < d_end) {
		if ((copymask <<= 1) == (1 << NBBY)) {
			copymask = 1;
			copymap = *src++;
		}
		if (copymap & copymask) {
			int mlen = (src[0] >> (NBBY - MATCH_BITS)) + MATCH_MIN;
			int offset = ((src[0] << NBBY) | src[1]) & OFFSET_MASK;
			src += 2;
			if ((cpy = dst - offset) < (uint8_t *)d_start) {
				return -1;
			}
			while (--mlen >= 0 && dst < d_end) {
				*dst++ = *cpy++;
			}
		} else {
			*dst++ = *src++;
		}
	}
	return 0;
}

}
