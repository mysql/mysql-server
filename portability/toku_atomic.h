/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef TOKU_ATOMIC_H
#define TOKU_ATOMIC_H

#include <config.h>
#include <toku_assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

__attribute__((const, always_inline))
static inline intptr_t which_cache_line(intptr_t addr) {
    static const size_t assumed_cache_line_size = 64;
    return addr / assumed_cache_line_size;
}
template <typename T> __attribute__((const, always_inline))
static inline bool crosses_boundary(T *addr, size_t width) {
    const intptr_t int_addr = reinterpret_cast<intptr_t>(addr);
    const intptr_t last_byte = int_addr + width - 1;
    return which_cache_line(int_addr) != which_cache_line(last_byte);
}

template <typename T, typename U> __attribute__((always_inline))
static inline T toku_sync_fetch_and_add(T *addr, U diff) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_fetch_and_add(addr, diff);
}
template <typename T, typename U> __attribute__((always_inline))
static inline T toku_sync_add_and_fetch(T *addr, U diff) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_add_and_fetch(addr, diff);
}
template <typename T, typename U> __attribute__((always_inline))
static inline T toku_sync_fetch_and_sub(T *addr, U diff) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_fetch_and_sub(addr, diff);
}
template <typename T, typename U> __attribute__((always_inline))
static inline T toku_sync_sub_and_fetch(T *addr, U diff) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_sub_and_fetch(addr, diff);
}
template <typename T, typename U, typename V> __attribute__((always_inline))
static inline T toku_sync_val_compare_and_swap(T *addr, U oldval, V newval) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_val_compare_and_swap(addr, oldval, newval);
}
template <typename T, typename U, typename V> __attribute__((always_inline))
static inline bool toku_sync_bool_compare_and_swap(T *addr, U oldval, V newval) {
    paranoid_invariant(!crosses_boundary(addr, sizeof *addr));
    return __sync_bool_compare_and_swap(addr, oldval, newval);
}

// in case you include this but not toku_portability.h
#pragma GCC poison __sync_fetch_and_add
#pragma GCC poison __sync_fetch_and_sub
#pragma GCC poison __sync_fetch_and_or
#pragma GCC poison __sync_fetch_and_and
#pragma GCC poison __sync_fetch_and_xor
#pragma GCC poison __sync_fetch_and_nand
#pragma GCC poison __sync_add_and_fetch
#pragma GCC poison __sync_sub_and_fetch
#pragma GCC poison __sync_or_and_fetch
#pragma GCC poison __sync_and_and_fetch
#pragma GCC poison __sync_xor_and_fetch
#pragma GCC poison __sync_nand_and_fetch
#pragma GCC poison __sync_bool_compare_and_swap
#pragma GCC poison __sync_val_compare_and_swap
#pragma GCC poison __sync_synchronize
#pragma GCC poison __sync_lock_test_and_set
#pragma GCC poison __sync_release

#endif // TOKU_ATOMIC_H
