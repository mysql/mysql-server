/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#ifndef NDBMEMCACHE_DATATYPEHANDLER_H
#define NDBMEMCACHE_DATATYPEHANDLER_H

#include <sys/types.h>

#include <NdbApi.hpp>

#include "ndbmemcache_global.h"

/* Assign x := *buf, assuming correct alignment */
#define LOAD_ALIGNED_DATA(Type, x, buf) \
Type x = *((Type *) buf);

/* Assign *buf := x, assuming correct alignment */
#define STORE_ALIGNED_DATA(Type, x, buf) \
*((Type *) buf) = (Type) x;

/* Assign x := *buf */
#define ALIGN_AND_LOAD(Type, x, buf) \
Type x; \
memcpy(&x, buf, sizeof(x));

/* Assign *buf := x */
#define ALIGN_AND_STORE(Type, x, buf) \
Type tmp_value = (Type) x; \
memcpy(buf, &tmp_value, sizeof(tmp_value));

/* FOR INTEGER TYPES, x86 allows unaligned access, but most other machines do not.
   (FOR FLOATING POINT TYPES: access must be aligned on all architectures).
   Wherever in the code there is a LOAD_ALIGNED_DATA macro, we assume the record  
   has been laid out with necessary padding for alignment.  But if you ever get 
   an alignment error (e.g. Bus Error on Sparc), you can replace LOAD_ALIGNED_DATA
   with LOAD_FOR ARCHITECTURE.
*/
#if defined(__i386) || defined(__x86_64)
#define LOAD_FOR_ARCHITECTURE LOAD_ALIGNED_DATA
#define STORE_FOR_ARCHITECTURE STORE_ALIGNED_DATA
#else
#define LOAD_FOR_ARCHITECTURE ALIGN_AND_LOAD
#define STORE_FOR_ARCHITECTURE ALIGN_AND_STORE
#endif


/* DataTypeHandler is an interface.  
   Each instance of DataTypeHandler is able to read values of a certain 
   Column type (or set of types) from database records, and to write them
   to records.
   The functions have C++ linkage.
 */

/* Return status codes from some of the functions:
 */

enum {  /* These can be returned by readFromNdb() or writeToNdb() */
  DTH_NOT_SUPPORTED = -1,
  DTH_VALUE_TOO_LONG = -2,
  DTH_NUMERIC_OVERFLOW = -3
};


/* NumericHandler interface. 
   All functions return 1 on success and DTH_xxx values on error 
*/
typedef struct {
  int (*read_int32)(Int32 & result, const void * const buf, 
                    const NdbDictionary::Column *);
  int (*write_int32)(Int32 value, void * const buf,
                     const NdbDictionary::Column *);
} NumericHandler;


/* DataTypeHandler interface:
 */
typedef struct {
  // String Readers.  Returns length read. 
  int (*readFromNdb)(const NdbDictionary::Column *col, 
                     char * &str, const void * const buf); 
  size_t (*getStringifiedLength)(const NdbDictionary::Column *col, 
                                 const void * const buf);
 
  // String Writer.  Returns length written or < 0 for error.
  int (*writeToNdb)(const NdbDictionary::Column *col, size_t len, 
                    const char *str, void * const buf);

  // NumericHandler.  
  NumericHandler * native_handler;
  
  // Will readFromNdb() return a pointer to a string inside of buf?
  // 1 = CHAR; 2 = VARCHAR; 3 = LONGVARCHAR
  int contains_string;
} DataTypeHandler;


/* Function to retrieve the appropriate DataTypeHandler for an NDB Column
 */
DataTypeHandler * getDataTypeHandlerForColumn(const NdbDictionary::Column *);


#endif
