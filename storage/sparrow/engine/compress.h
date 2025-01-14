/*
	Compression helpers.
*/

#ifndef _engine_compress_h_
#define _engine_compress_h_

#include "types.h"
#include "serial.h"
#include "list.h"
#include "hash.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// LZJB
//////////////////////////////////////////////////////////////////////////////////////////////////////

class LZJB {
public:

	static size_t compress(uint8_t* s_start, uint8_t* d_start, size_t s_len, size_t d_len);

	static int decompress(uint8_t* s_start, uint8_t* d_start, size_t s_len, size_t d_len);
};

}

#endif /* #ifndef _engine_compress_h_ */
