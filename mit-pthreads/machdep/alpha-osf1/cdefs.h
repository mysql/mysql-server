/* ==== cdefs.h ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu	
 *
 * Description : Similar to the BSD cdefs.h file.
 *
 *  1.00 94/01/26 proven
 *      -Started coding this file.
 */

#ifndef _PTHREAD_SYS_CDEFS_H_
#define _PTHREAD_SYS_CDEFS_H_

/* Stuff for compiling */
#if defined(__GNUC__)
#if defined(__cplusplus)
#define	__INLINE		static inline
#define __BEGIN_DECLS   extern "C" {
#define __END_DECLS     };
#else
#define	__INLINE		extern inline
#define __CAN_DO_EXTERN_INLINE
#define __BEGIN_DECLS
#define __END_DECLS
#if !defined(__STDC__)
#define const           __const
#define inline          __inline
#define signed          __signed
#define volatile        __volatile
#endif
#endif
#else /* !__GNUC__ */
#define __BEGIN_DECLS
#define __END_DECLS
#define	__INLINE		static 
#define inline
#endif

#ifndef __NORETURN
#define __NORETURN
#endif /* __NORETURN not defined.  */

#ifndef _U_INT32_T_
#define _U_INT32_T_
typedef unsigned int u_int32_t;
#endif

#ifndef _U_INT16_T_
#define _U_INT16_T_
typedef unsigned short u_int16_t;
#endif

#ifndef _INT32_T_
#define _INT32_T_
typedef int int32_t;
#endif

#ifndef _INT16_T_
#define _INT16_T_
typedef short int16_t;
#endif

#endif
