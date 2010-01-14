#if !defined(TOKU_ATOMIC_H)
#define TOKU_ATOMIC_H

#if TOKU_WINDOWS

static inline int32_t
toku_sync_fetch_and_add_int32(volatile int32_t *a, int32_t b) {
    return _InterlockedExchangeAdd((LONG*)a, b);
}

static inline int32_t
toku_sync_fetch_and_increment_int32(volatile int32_t *a) {
    return _InterlockedIncrement((LONG*)a);
}

static inline int32_t
toku_sync_fetch_and_decrement_int32(volatile int32_t *a) {
    return _InterlockedDecrement((LONG*)a);
}

#define TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA 0
//Vista has 64 bit atomic instruction functions.
//64 bit windows should also have it, but we're using neither right now.
#if TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA
#define TOKU_WINDOWS_HAS_FAST_ATOMIC_64 1
#else
#define TOKU_WINDOWS_HAS_FAST_ATOMIC_64 0
#endif



static inline uint64_t
toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) {
#if TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA
    //Need Vista or later for this function to exist.
    return _InterlockedExchangeAdd64((int64_t*)a, b);
#else
    //Temporarily just use 32 bit atomic instructions (treat the values as 32
    //bit only).  For now this is ok, the values are only used in show engine
    //status.
    return _InterlockedExchangeAdd((LONG*)a, b);
#endif
}

static inline uint64_t
toku_sync_fetch_and_increment_uint64(volatile uint64_t *a) {
#if TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA
    //Need Vista or later for this function to exist.
    return _InterlockedIncrement64((int64_t*)a);
#else
    //Temporarily just use 32 bit atomic instructions (treat the values as 32
    //bit only).  For now this is ok, the values are only used in show engine
    //status.
    return _InterlockedIncrement((LONG*)a);
#endif
}

#else

//Linux

static inline int32_t toku_sync_fetch_and_add_int32(volatile int32_t *a, int32_t b) {
    return __sync_fetch_and_add(a, b);
}

static inline int32_t toku_sync_fetch_and_increment_int32(volatile int32_t *a) {
    return toku_sync_fetch_and_add_int32(a, 1);
}

static inline int32_t toku_sync_fetch_and_decrement_int32(volatile int32_t *a) {
    return toku_sync_fetch_and_add_int32(a, -1);
}

#if __GNUC__ && __i386__

// workaround for a gcc 4.1.2 bug on 32 bit platforms.
uint64_t toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) __attribute__((noinline));

#else

static inline uint64_t toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) {
    return __sync_fetch_and_add(a, b);
}

#endif

static inline uint64_t toku_sync_fetch_and_increment_uint64(volatile uint64_t *a) {
    return toku_sync_fetch_and_add_uint64(a, 1);
}

#endif

#endif
