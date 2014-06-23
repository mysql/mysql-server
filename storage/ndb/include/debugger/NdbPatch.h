/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NdbPatch_H
#define NdbPatch_H

/*
 * NdbPatch can be used for adding diagnostic logging for customer
 * specific patches.
 *
 * It defines a printf-like NDB_PATCH_INFO() that normally logs to
 * ndb data nodes and ndbapi applications local logfiles or console.
 *
 * Messages will automatically have:
 * . timestamps with seconds resolution
 * . reference to source code line emitting message
 *
 * NdbPatch must both be enabled at compile time by defining NDB_PATCH
 * (see below) and at run-time by setting environment variable
 * NDB_PATCH, see NdbPatch.cpp for more details.
 *
 * To build with NdbPatch define NDB_PATCH as a string literal.
 * This string will be prefixed to every log using NDB_PATCH_INFO,
 * avoid using % or any other character that can be mis-interpreted
 * by vsnprintf.
 *
 * Note.  NDB_PATCH must not be defined in offical MySQL Cluster
 * code.  No references to NDB_PATCH or NdbPatch should be left
 * except two places in ndb_init.cpp and the files NdbPatch.{h,cpp}.
 */
//#define NDB_PATCH "bugXYZ: "
#define NDB_PATCH "bug18496153: "

#ifndef NDB_PATCH
#define NDB_PATCH_INIT() (void)"No op"
/*
 * No definition for NDB_PATCH_INFO since use of them must not
 * be pushed to official tree.  It should only be used for specific
 * customer builds.
 */
#else

#ifdef __cplusplus
extern "C" {
#endif

void NdbPatch__init();
void NdbPatch__end();
void NdbPatch__configure(const char config[]);
void NdbPatch__info(const char fmt[], ...);
const char* NdbPatch__source_basename(const char filename[]);
extern int NdbPatch__features;

#ifdef __cplusplus
}
#endif

#define NDB_PATCH_FEATURE(n) unlikely(NdbPatch__features & (1 << (n))?1:0)
#define NDB_PATCH_INIT() NdbPatch__init()
#define NDB_PATCH_END() NdbPatch__end()
#define NDB_PATCH_CONFIGURE(s) NdbPatch__configure(s)

#define NDB_PATCH_INFO(fmt,...) \
  if (NDB_PATCH_FEATURE(0)) \
  { \
    struct NdbThread* thr = NdbThread_GetCurrentThread(1); \
    NdbPatch__info( \
      NDB_PATCH "%s: %d: %s: %s: %u/%u: " fmt, \
      NdbPatch__source_basename(__FILE__), \
      __LINE__, \
      __func__, \
      NdbThread_GetName(thr), \
      NdbThread_GetTid(thr), \
      NdbThread_GetCurrentCPU(), \
      __VA_ARGS__); \
  } \
  else (void) "Prevent take else from outher if"

#endif
#endif
