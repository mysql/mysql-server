/*
 * ANSI/POSIX
 */
typedef union _h_val {
    unsigned long _i[2];
    double _d;
} _h_val;

#ifdef __STDC__
extern const _h_val __huge_val;
#else
extern _h_val __huge_val;
#endif

#define HUGE_VAL __huge_val._d

