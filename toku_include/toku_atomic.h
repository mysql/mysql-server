#if !defined(TOKU_ATOMIC_H)
#define TOKU_ATOMIC_H

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t
toku_sync_fetch_and_add_uint32(volatile uint32_t *a, uint32_t b) {
    // icc previously required _InterlockedExchangeAdd((LONG*)a, b);
    return __sync_fetch_and_add(a, b);
}

static inline uint32_t toku_sync_fetch_and_increment_uint32(volatile uint32_t *a) {
    // ICC has an _InterlockedIncrement function that returns the new result.  We'll just use our primitive.
    return toku_sync_fetch_and_add_uint32(a, 1);
}

static inline uint32_t toku_sync_fetch_and_decrement_uint32(volatile uint32_t *a) {
    return toku_sync_fetch_and_add_uint32(a, -1);
}

static inline int32_t toku_sync_fetch_and_add_int32(volatile int32_t *a, int32_t b) {
    return __sync_fetch_and_add(a, b);
}

static inline int32_t toku_sync_fetch_and_increment_int32(volatile int32_t *a) {
    return toku_sync_fetch_and_add_int32(a, 1);
}

static inline int32_t toku_sync_fetch_and_decrement_int32(volatile int32_t *a) {
    return toku_sync_fetch_and_add_int32(a, -1);
}

static inline int32_t toku_sync_add_and_fetch_int32(volatile int32_t *a, int32_t b) {
    return __sync_add_and_fetch(a, b);
}

static inline int32_t toku_sync_increment_and_fetch_int32(volatile int32_t *a) {
    return __sync_add_and_fetch(a, 1);
}

static inline int32_t toku_sync_decrement_and_fetch_int32(volatile int32_t *a) {
    return __sync_add_and_fetch(a, -1);
}

#if __GNUC__ && __i386__

// workaround for a gcc 4.1.2 bug on 32 bit platforms.
static uint64_t toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) __attribute__((noinline), (unused)) {
    return __sync_fetch_and_add(a, b);
}

#else

static inline uint64_t toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) {
    return __sync_fetch_and_add(a, b);
}

#endif

static inline uint64_t toku_sync_fetch_and_increment_uint64(volatile uint64_t *a) {
    return toku_sync_fetch_and_add_uint64(a, 1);
}

#define TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA 0

//Vista has 64 bit atomic instruction functions.
//64 bit windows should also have it, but we're using neither right now.
#if TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA || TOKU_WINDOWS_64
#define TOKU_WINDOWS_HAS_ATOMIC_64 1
#else
#define TOKU_WINDOWS_HAS_ATOMIC_64 0
#endif


#ifdef __cplusplus
}
#endif

#endif
