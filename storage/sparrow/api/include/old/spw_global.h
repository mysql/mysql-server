#ifndef _spw_api_spw_global_h
#define _spw_api_spw_global_h

/* Typdefs for easier portability */
// Obsolete definitions. Should not be used anymore. Use standard definitins instead.
#if 0


typedef unsigned char		uchar;	/* Short for unsigned char */

#ifndef int8_t
typedef signed char			int8_t;       /* Signed integer >= 8  bits */
#endif
#ifndef uint8_t
typedef unsigned char		uint8_t;    /* Unsigned integer >= 8  bits */
#endif
#ifndef uint8_t
typedef short				int16_t;
#endif
#ifndef int16_t
typedef unsigned short		uint16_t;
#endif

#ifndef int32_t
typedef int					int32_t;
#endif
#ifndef uint32_t
typedef unsigned int		uint32_t;
#endif

#ifndef ulong
typedef unsigned long		ulong;		  /* Short for unsigned long */
#endif

#ifndef int64_t
typedef long long			int64_t;
#endif
#ifndef uint64_t
typedef unsigned long long	uint64_t;
#endif

#ifndef uint
typedef unsigned int		uint;
#endif
#ifndef ushort
typedef unsigned short		ushort;
#endif


/* First check for ANSI C99 definition: */
#ifdef ULLONG_MAX
#undef ULLONG_MAX
#endif

#ifdef ULLONG_MAX
#define ULLONG_MAX  ULLONG_MAX
#else
#define ULLONG_MAX ((unsigned long long)(~0ULL))
#endif

#define INT_MIN64       (~0x7FFFFFFFFFFFFFFFLL)
#define INT_MAX64       0x7FFFFFFFFFFFFFFFLL
#define INT_MIN32       (~0x7FFFFFFFL)
#define INT_MAX32       0x7FFFFFFFL
#define UINT_MAX32      0xFFFFFFFFL
#define INT_MIN24       (~0x007FFFFF)
#define INT_MAX24       0x007FFFFF
#define UINT_MAX24      0x00FFFFFF
#define INT_MIN16       (~0x7FFF)
#define INT_MAX16       0x7FFF
#define UINT_MAX16      0xFFFF
#define INT_MIN8        (~0x7F)
#define INT_MAX8        0x7F
#define UINT_MAX8       0xFF

#ifndef NULL
#define NULL    0
#endif

#endif

#endif /* _spw_api_spw_global_h */
