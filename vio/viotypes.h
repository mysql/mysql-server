/* 
**  Virtual I/O library
**  Written by Andrei Errapart <andreie@no.spam.ee>
*/

/*
 * Some typedefs to external types.
 */
#ifndef vio_viotypes_h_
#define vio_viotypes_h_

#include	<stdarg.h>
#include	<stdio.h>
#include	<sys/types.h>

typedef	int		vio_bool_t;
typedef	void*		vio_ptr_t;

#ifdef	__cplusplus
VIO_NS_BEGIN

typedef vio_ptr_t	vio_ptr;
typedef char*		vio_cstring;
typedef int32_t		vio_int32;
typedef u_int32_t	vio_uint32;
typedef	vio_bool_t	vio_bool;

VIO_NS_END
#endif /* __cplusplus */

#endif /* vio_types_h_ */

