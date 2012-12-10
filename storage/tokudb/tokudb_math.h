#if !defined(_TOKUDB_MATH_H)
#define _TOKUDB_MATH_H

namespace tokudb {

// Add and subtract ints with overflow detection.
// Overflow detection adapted from "Hackers Delight", Henry S. Warren

// Return a bit mask for bits 0 .. length_bits-1
static uint64_t uint_mask(uint length_bits) __attribute__((unused));
static uint64_t uint_mask(uint length_bits) {
    return length_bits == 64 ? ~0ULL : (1ULL<<length_bits)-1;
}

// Return the highest unsigned int with a given number of bits
static uint64_t uint_high_endpoint(uint length_bits) __attribute__((unused));
static uint64_t uint_high_endpoint(uint length_bits) {
    return uint_mask(length_bits);
}

// Return the lowest unsigned int with a given number of bits
static uint64_t uint_low_endpoint(uint length_bits) __attribute__((unused));
static uint64_t uint_low_endpoint(uint length_bits) {
    return 0;
}

// Add two unsigned integers with max maximum value.
// If there is an overflow then set the sum to the max.
// Return the sum and the overflow.
static uint64_t uint_add(uint64_t x, uint64_t y, uint length_bits, bool *over) __attribute__((unused));
static uint64_t uint_add(uint64_t x, uint64_t y, uint length_bits, bool *over) {
    uint64_t mask = uint_mask(length_bits);
    assert((x & ~mask) == 0 && (y & ~mask) == 0);
    uint64_t s = (x + y) & mask;
    *over = s < x;     // check for overflow
    return s;
}

// Subtract two unsigned ints with max maximum value.
// If there is an over then set the difference to 0.
// Return the difference and the overflow.
static uint64_t uint_sub(uint64_t x, uint64_t y, uint length_bits, bool *over) __attribute__((unused));
static uint64_t uint_sub(uint64_t x, uint64_t y, uint length_bits, bool *over) {
    uint64_t mask = uint_mask(length_bits);
    assert((x & ~mask) == 0 && (y & ~mask) == 0);
    uint64_t s = (x - y) & mask;
    *over = s > x;    // check for overflow
    return s;
}

// Return the highest int with a given number of bits
static int64_t int_high_endpoint(uint length_bits) __attribute__((unused));
static int64_t int_high_endpoint(uint length_bits) {
    return (1ULL<<(length_bits-1))-1;
}

// Return the lowest int with a given number of bits
static int64_t int_low_endpoint(uint length_bits) __attribute__((unused));
static int64_t int_low_endpoint(uint length_bits) {
    int64_t mask = uint_mask(length_bits);
    return (1ULL<<(length_bits-1)) | ~mask;
}

// Sign extend to 64 bits an int with a given number of bits
static int64_t int_sign_extend(int64_t n, uint length_bits) __attribute__((unused));
static int64_t int_sign_extend(int64_t n, uint length_bits) {
    if (n & (1ULL<<(length_bits-1)))
        n |= ~uint_mask(length_bits);
    return n;
}

// Add two signed ints with max maximum value.
// If there is an overflow then set the sum to the max or the min of the int range,
// depending on the sign bit.
// Sign extend to 64 bits.
// Return the sum and the overflow.
static int64_t int_add(int64_t x, int64_t y, uint length_bits, bool *over) __attribute__((unused));
static int64_t int_add(int64_t x, int64_t y, uint length_bits, bool *over) {
    int64_t mask = uint_mask(length_bits);
    int64_t n = (x + y) & mask;
    *over = (((n ^ x) & (n ^ y)) >> (length_bits-1)) & 1;    // check for overflow
    if (n & (1LL<<(length_bits-1)))
        n |= ~mask;    // sign extend
    return n;
}

// Subtract two signed ints.
// If there is an overflow then set the sum to the max or the min of the int range,
// depending on the sign bit.
// Sign extend to 64 bits.
// Return the sum and the overflow.
static int64_t int_sub(int64_t x, int64_t y, uint length_bits, bool *over) __attribute__((unused));
static int64_t int_sub(int64_t x, int64_t y, uint length_bits, bool *over) {
    int64_t mask = uint_mask(length_bits);
    int64_t n = (x - y) & mask;
    *over = (((x ^ y) & (n ^ x)) >> (length_bits-1)) & 1;    // check for overflow
    if (n & (1LL<<(length_bits-1)))
        n |= ~mask;    // sign extend
    return n;
}

} // namespace tokudb

#endif
