#if !defined(TOKU_ATOMIC_H)
#define TOKU_ATOMIC_H

#if TOKU_WINDOWS

#define TOKU_WINDOWS_INTEL_COMPILER_HAS_ATOMIC64 0
#if TOKU_WINDOWS_INTEL_COMPILER_HAS_ATOMIC64
//Intel compiler version 11 has these functions:
#include <ia64intrin.h>
static inline int32_t
toku_sync_fetch_and_add_int32(volatile int32_t *a, int32_t b) {
    return _InterlockedExchangeAdd(a, b);

}

static inline uint64_t
toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) {
    return _InterlockedExchangeAdd64((int64_t*)a, b);
}
#else
static inline int32_t
toku_sync_fetch_and_add_int32(volatile int32_t *a, int32_t b) {
    return _InterlockedExchangeAdd((LONG*)a, b);
}

#define TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA 0

static inline uint64_t
toku_sync_fetch_and_add_uint64(volatile ULONGLONG *a, uint64_t b) {
#if TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA
    //Need Vista or later for this function to exist.
    return _InterlockedExchangeAdd64((LONGLONG*)a, b);
#else
    //Temporarily just use 32 bit atomic instructions (treat the values as 32
    //bit only).  For now this is ok, the values are only used in show engine
    //status.
    return _InterlockedExchangeAdd((LONG*)a, b);
#endif
}

#endif

#else
//Linux
#define TOKU_INLINE32 inline

static TOKU_INLINE32 int32_t toku_sync_fetch_and_add_int32(volatile int32_t *a, int32_t b) {
    return __sync_fetch_and_add(a, b);
}

#if __GNUC__ && __i386__
#define TOKU_INLINE64
// workaround for a gcc 4.1.2 bug on 32 bit platforms.
static uint64_t toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) __attribute__((noinline));
#else
#define TOKU_INLINE64 inline
#endif

static TOKU_INLINE64 uint64_t toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) {
    return __sync_fetch_and_add(a, b);
}
#endif

#endif
