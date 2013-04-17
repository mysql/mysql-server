#ifndef _STDINT_H
#define _STDINT_H
#include <basetsd.h>
#include <sys/types.h>

//Define standard integer types.
typedef __int8                 int8_t;
typedef unsigned __int8       uint8_t;
typedef unsigned __int8      u_int8_t;
typedef __int16               int16_t;
typedef unsigned __int16     uint16_t;
typedef unsigned __int16    u_int16_t;
typedef __int32               int32_t;
typedef unsigned __int32     uint32_t;
typedef unsigned __int32    u_int32_t;
typedef __int64               int64_t;
typedef unsigned __int64     uint64_t;
typedef unsigned __int64    u_int64_t;

typedef SSIZE_T ssize_t;

//Limits
#define INT8_MIN   _I8_MIN
#define INT8_MAX   _I8_MAX
#define UINT8_MAX  _UI8_MAX
#define INT16_MIN  _I16_MIN
#define INT16_MAX  _I16_MAX
#define UINT16_MAX _UI16_MAX
#define INT32_MIN  _I32_MIN
#define INT32_MAX  _I32_MAX
#define UINT32_MAX _UI32_MAX
#define INT64_MIN  _I64_MIN
#define INT64_MAX  _I64_MAX
#define UINT64_MAX _UI64_MAX

#endif

