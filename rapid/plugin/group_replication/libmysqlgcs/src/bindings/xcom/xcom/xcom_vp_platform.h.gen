/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved. */

/*
  Needed since rpcgen expands macros itself, we cannot put
  this in the xcom_vp.x file directly.
 */

#ifndef XCOM_VP_PLATFORM_H
#define XCOM_VP_PLATFORM_H

// Avoid warnings from the rpcgen
#if defined(__GNUC__) || defined(__GNUG__)
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wundef"
#endif
#endif

#ifdef __linux__
#if __linux__
#define u_longlong_t u_quad_t
#endif
#endif

#ifdef __APPLE__
#if __APPLE__
/* xdr_uint64_t and xdr_uint32_t are not defined on OSX */
#define xdr_uint64_t xdr_u_int64_t
#define xdr_uint32_t xdr_u_int32_t
#endif
#endif

#endif
