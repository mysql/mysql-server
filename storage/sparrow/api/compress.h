/*
	Compression helpers.
*/

#ifndef _engine_compress_h_
#define _engine_compress_h_

#include "include/global.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// LZJB
//////////////////////////////////////////////////////////////////////////////////////////////////////

class LZJB {
public:

	static size_t compress(const uint8_t* s_start, uint8_t* d_start, size_t s_len, size_t d_len);

	static int decompress(const uint8_t* s_start, uint8_t* d_start, size_t s_len, size_t d_len);
};

}

#endif /* #ifndef _engine_compress_h_ */
