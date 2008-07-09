// read the processor time stamp register

#if defined __i386__

#define USE_RDTSC 1

static inline unsigned long long rdtsc() {
    unsigned long hi, lo;
    __asm__ __volatile__ ("rdtsc\n"
                          "movl %%edx,%0\n"
			  "movl %%eax,%1" : "=r"(hi), "=r"(lo) : : "edx", "eax");
    return ((unsigned long long) hi << 32ULL) + (unsigned long long) lo;
}

#else
#if defined __x86_64__

#define USE_RDTSC 1

static inline unsigned long long rdtsc() {
    unsigned long long r;
    __asm__ __volatile__ ("rdtsc\n"
                          "shl $32,%%rdx\n"
                          "or %%rdx,%%rax\n"
			  "movq %%rax,%0" : "=r"(r) : : "edx", "eax", "rdx", "rax");
    return r;
}

#else

#define USE_RDTSC 0

#endif
#endif
