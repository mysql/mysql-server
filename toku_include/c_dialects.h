#ifndef C_DIALECTS_H
#define C_DIALECTS_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
#define C_BEGIN extern "C" {
#define C_END }
#else
#define C_BEGIN
#define C_END
#endif


#if defined(__cilkplusplus)
#define CILK_BEGIN extern "Cilk++" {
#define CILK_END }
#else
#define CILK_BEGIN
#define CILK_END
#endif

#endif
