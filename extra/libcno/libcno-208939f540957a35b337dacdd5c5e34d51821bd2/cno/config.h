#pragma once

#ifndef CNO_BUFFER_ALLOC_MIN
// The minimum size (in bytes) in which dynamically allocated buffers grow.
// Controls the number of heap allocations & amount of wasted heap space.
#define CNO_BUFFER_ALLOC_MIN 256
#endif

#ifndef CNO_BUFFER_ALLOC_MIN_EXP
// The minimum factor by which dynamically allocated buffers grow.
#define CNO_BUFFER_ALLOC_MIN_EXP 1.5
#endif

#ifndef CNO_MAX_HEADERS
// Max. number of entries in the header table of inbound messages. Applies to
// both HTTP 1 and HTTP 2. Does not affect outbound messages. Controls stack
// space usage.
#define CNO_MAX_HEADERS 64
#endif

#ifndef CNO_MAX_CONTINUATIONS
// In HTTP 2 mode, only this many CONTINUATIONs are accepted in a row. In HTTP 1
// mode, the total length of all headers cannot exceed (this value + 1) * (max
// HTTP 2 frame size)
// + (size of the transport level read buffer). Controls peak memory
// consumption.
#define CNO_MAX_CONTINUATIONS 3
#endif

#ifndef CNO_STREAM_BUCKETS
// Number of buckets in the "stream id -> stream object" hash map. Must be prime
// to ensure an even distribution. Controls stack/heap usage, depending on where
// connection objects are allocated.
#define CNO_STREAM_BUCKETS 61
#endif

#ifndef CNO_STREAM_RESET_HISTORY
// Remember the last N streams for which RST_STREAM was sent. Frames on these
// streams will be ignored under the assumption that the other side has not seen
// the reset yet.
#define CNO_STREAM_RESET_HISTORY 10
#endif
