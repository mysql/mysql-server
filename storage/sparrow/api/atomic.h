/*
	Simple atomic operations.
*/

#ifndef _spw_api_atomic_h_
#define _spw_api_atomic_h_

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#if (MSVC_VER >= 1500)
// This one can be intrinsinc only with Visual Studio 2008.
#pragma intrinsic(_InterlockedAdd)
#endif
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedCompareExchange)
#elif defined(__SunOS)
#include <atomic.h>
#endif
#include "include/global.h"

namespace Sparrow {

class Atomic {
private:

	Atomic();

public:

#ifdef _WIN64
	static uint32_t add32(volatile uint32_t* target, const int32_t delta) {
		return _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(target), delta);
	}
	static uint64_t add64(volatile uint64_t* target, const int64_t delta) {
		return _InterlockedExchangeAdd64(reinterpret_cast<volatile long long*>(target), delta);
	}
	static uint32_t inc32(volatile uint32_t* target) {
		return _InterlockedIncrement(reinterpret_cast<volatile long*>(target));
	}
	static uint32_t dec32(volatile uint32_t* target) {
		return _InterlockedDecrement(reinterpret_cast<volatile long*>(target));
	}
	static bool cas32(volatile uint32_t* target, const uint32_t cmp, const uint32_t newval) {
		return _InterlockedCompareExchange(reinterpret_cast<volatile long*>(target), newval, cmp) == cmp;
	}
	static bool cas64(volatile uint64_t* target, const uint64_t cmp, const uint64_t newval) {
		return _InterlockedCompareExchange64(reinterpret_cast<volatile int64_t*>(target), newval, cmp) == cmp;
	}
#elif defined(_WIN32)
	static uint32_t add32(volatile uint32_t* target, const int32_t delta) {
		return InterlockedExchangeAdd(reinterpret_cast<volatile long*>(target), delta) + delta;
	}
	// 64-bit interlocked functions for 32-bit platforms are available only in Vista,
	// so use assembly code.
	static uint64_t interlockedCompareExchange64(volatile uint64_t* target, const uint64_t value, const uint64_t comp){
		__asm {
			    mov             esi, [target]
                mov             ebx, dword ptr [value]
                mov             ecx, dword ptr [value + 4]
                mov             eax, dword ptr [comp]
                mov             edx, dword ptr [comp + 4]
                lock cmpxchg8b  [esi]
        }
	} 
	static bool cas64(volatile uint64_t* target, const uint64_t cmp, const uint64_t newval) {
		return interlockedCompareExchange64(target, newval, cmp) == cmp;
	}
	static uint64_t add64(volatile uint64_t* target, const int64_t delta) {
		uint64_t old;
		do {
			old = *target;
		} while (!cas64(target, old, old + delta));
		return old + delta;
	}
	static uint32_t inc32(volatile uint32_t* target) {
		return InterlockedIncrement(reinterpret_cast<volatile long*>(target));
	}
	static uint32_t dec32(volatile uint32_t* target) {
		return InterlockedDecrement(reinterpret_cast<volatile long*>(target));
	}
	static bool cas32(volatile uint32_t* target, const uint32_t cmp, const uint32_t newval) {
		return InterlockedCompareExchange(reinterpret_cast<volatile long*>(target), newval, cmp) == cmp;
	}
#elif defined(__SunOS)	// Solaris.
	static uint32_t add32(volatile uint32_t* target, const int32_t delta) {
		return atomic_add_32_nv(target, delta);
	}
	static uint64_t add64(volatile uint64_t* target, const int64_t delta) {
		return atomic_add_64_nv((volatile uint64_t*)target, delta);
	}
	static uint32_t inc32(volatile uint32_t* target) {
		return atomic_inc_32_nv(target);
	}
	static uint32_t dec32(volatile uint32_t* target) {
		return atomic_dec_32_nv(target);
	}
	static bool cas32(volatile uint32_t* target, const uint32_t cmp, const uint32_t newval) {
		return atomic_cas_32(target, cmp, newval) == cmp;
	}
	static bool cas64(volatile uint64_t* target, const uint64_t cmp, const uint64_t newval) {
		return atomic_cas_64(reinterpret_cast<volatile uint64_t*>(target), cmp, newval) == cmp;
 	}
#elif defined(HAVE_GCC_ATOMIC_BUILTINS)		// gcc or Intel Compiler.
	static uint32_t add32(volatile uint32_t* target, const int32_t delta) {
		return __sync_add_and_fetch(target, delta);
	}
	static uint64_t add64(volatile uint64_t* target, const int64_t delta) {
		return __sync_add_and_fetch(target, delta);
	}
	static uint32_t inc32(volatile uint32_t* target) {
		return __sync_add_and_fetch(target, 1);
	}
	static uint32_t dec32(volatile uint32_t* target) {
		return __sync_sub_and_fetch(target, 1);
	}
	static bool cas32(volatile uint32_t* target, const uint32_t cmp, const uint32_t newval) {
		return __sync_bool_compare_and_swap(target, cmp, newval);
	}
	static bool cas64(volatile uint64_t* target, const uint64_t cmp, const uint64_t newval) {
		return __sync_bool_compare_and_swap(target, cmp, newval);
	}
#elif defined(__x86_64__) // x64 support.
	static uint32_t add32(volatile uint32_t* target, const int32_t delta) {
		uint32_t result=0;
		asm volatile ("lock; xaddl %0, %1"
			: "=r"(result), "=m"(*target)
			: "0"(delta), "m"(*target)
			: "memory", "cc");
		return result + delta;
	}

	static uint64_t add64(volatile uint64_t* target, const int64_t delta) {
		uint64_t temp = static_cast<uint64_t>(delta);
		asm volatile("lock; xaddq %0,%1"
			: "+r" (temp), "+m" (*target)
			: : "memory");
		return temp + delta;
	}
	static uint32_t inc32(volatile uint32_t* target) {
		return add32(target, 1);
	}
	static uint32_t dec32(volatile uint32_t* target) {
		return add32(target, -1);
	}
	static bool cas32(volatile uint32_t* target, const uint32_t cmp, const uint32_t newval) {
		uint32_t result;
		asm volatile ("lock; cmpxchgl %1, %2"
			: "=a" (result)
			: "r" (newval), "m" (*target), "0" (cmp)
			: "memory");
		return result == newval;
	}
	static bool cas64(volatile uint64_t* target, const uint64_t cmp, const uint64_t newval) {
		uint64_t result;
		asm volatile("lock; cmpxchgq %1,%2"
			: "=a" (result)
			: "q" (newval), "m" (*target), "0" (cmp)
			: "memory");
		return result == newval;
	}
#else
#error Missing atomic functions
#endif
	static void set64(volatile uint64_t* target, const uint64_t newVal) {
		uint64_t old;
		do {
			old = *target;
		} while (!cas64(target, old, newVal));
	}
	static uint64_t get64(volatile uint64_t* target) {
		uint64_t result;
		do {
			result = *target;
		} while (!cas64(target, result, result));
		return result;
	}
	static uint64_t inc64(volatile uint64_t* target) {
		return add64(target, 1);
	}
	static uint64_t dec64(volatile uint64_t* target) {
		return add64(target, -1);
	}
};

}

#endif /* #ifndef _spw_api_atomic_h_ */
