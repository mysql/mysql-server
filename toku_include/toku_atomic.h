#if !defined(TOKU_ATOMIC_H)
#define TOKU_ATOMIC_H

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

#if TOKU_WINDOWS

static inline uint32_t
toku_sync_fetch_and_add_uint32(volatile uint32_t *a, uint32_t b) {
    return _InterlockedExchangeAdd((LONG*)a, b);
}

static inline uint32_t
toku_sync_fetch_and_increment_uint32(volatile uint32_t *a) {
    uint32_t r = _InterlockedIncrement((LONG*)a);
    //InterlockedIncrement returns the result, not original.
    //Return the original.
    return r - 1;
}

static inline uint32_t
toku_sync_fetch_and_decrement_uint32(volatile uint32_t *a) {
    uint32_t r = _InterlockedDecrement((LONG*)a);
    //InterlockedDecrement returns the result, not original.
    //Return the original.
    return r + 1;
}

static inline int32_t
toku_sync_fetch_and_add_int32(volatile int32_t *a, int32_t b) {
    return _InterlockedExchangeAdd((LONG*)a, b);
}

static inline int32_t
toku_sync_fetch_and_increment_int32(volatile int32_t *a) {
    int32_t r = _InterlockedIncrement((LONG*)a);
    //InterlockedIncrement returns the result, not original.
    //Return the original.
    return r - 1;
}

static inline int32_t
toku_sync_fetch_and_decrement_int32(volatile int32_t *a) {
    int32_t r = _InterlockedDecrement((LONG*)a);
    //InterlockedDecrement returns the result, not original.
    //Return the original.
    return r + 1;
}

static inline int32_t
toku_sync_increment_and_fetch_int32(volatile int32_t *a) {
    int32_t r = _InterlockedIncrement((LONG*)a);
    //InterlockedIncrement returns the result, not original.
    //Return the result.
    return r;
}

static inline int32_t
toku_sync_decrement_and_fetch_int32(volatile int32_t *a) {
    int32_t r = _InterlockedDecrement((LONG*)a);
    //InterlockedDecrement returns the result, not original.
    //Return the result.
    return r;
}


#define TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA 0

//Vista has 64 bit atomic instruction functions.
//64 bit windows should also have it, but we're using neither right now.
#if TOKU_WINDOWS_MIN_SUPPORTED_IS_VISTA || TOKU_WINDOWS_64
#define TOKU_WINDOWS_HAS_ATOMIC_64 1
#else
#define TOKU_WINDOWS_HAS_ATOMIC_64 0
#endif



static inline uint64_t
toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) {
#if TOKU_WINDOWS_HAS_ATOMIC_64
    return _InterlockedExchangeAdd64((int64_t*)a, b);
#else
    //Temporarily just use 32 bit atomic instructions (treat the values as 32
    //bit only).  For now this is ok, the values are only used in show engine
    //status.
    return toku_sync_fetch_and_add_uint32((uint32_t*)a, b);
#endif
}

static inline uint64_t
toku_sync_fetch_and_increment_uint64(volatile uint64_t *a) {
#if TOKU_WINDOWS_HAS_ATOMIC_64
    uint64_t r = _InterlockedIncrement64((int64_t*)a);
    //InterlockedIncrement64 returns the result, not original.
    //Return the original.
    return r - 1;
#else
    //Temporarily just use 32 bit atomic instructions (treat the values as 32
    //bit only).  For now this is ok, the values are only used in show engine
    //status.
    return toku_sync_fetch_and_increment_uint32((uint32_t*)a);
#endif
}

#else

//Linux

static inline uint32_t toku_sync_fetch_and_add_uint32(volatile uint32_t *a, uint32_t b) {
    return __sync_fetch_and_add(a, b);
}

static inline uint32_t toku_sync_fetch_and_increment_uint32(volatile uint32_t *a) {
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
    return toku_sync_add_and_fetch_int32(a, 1);
}

static inline int32_t toku_sync_decrement_and_fetch_int32(volatile int32_t *a) {
    return toku_sync_add_and_fetch_int32(a, -1);
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

// DO_GCC_PRAGMA(GCC __sync_fetch_and_add __sync_add_and_fetch)

#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif


#endif
