#if !defined(TOKU_ATOMIC_H)
#define TOKU_ATOMIC_H

#define TOKU_INLINE32 inline

static TOKU_INLINE32 int32_t toku_sync_fetch_and_add_int32(int32_t *a, int32_t b) {
    return __sync_fetch_and_add(a, b);
}

#if __GNUC__ && __i386__
#define TOKU_INLINE64
// workaround for a gcc 4.1.2 bug on 32 bit platforms.
static uint64_t toku_sync_fetch_and_add_uint64(uint64_t *a, uint64_t b) __attribute__((noinline));
#else
#define TOKU_INLINE64 inline
#endif

static TOKU_INLINE64 uint64_t toku_sync_fetch_and_add_uint64(uint64_t *a, uint64_t b) {
    return __sync_fetch_and_add(a, b);
}

#endif
