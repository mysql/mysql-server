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

#include "NdbPatch.h"

#ifdef NDB_PATCH

#include <stdlib.h>
#include <string.h>

#include "ndb_global.h"
#include "assert.h"
#include "EventLogger.hpp"

/*
 * NdbPatch is controlled by the environment variable NDB_PATCH.
 *
 * One can add one extra destination for all logging
 * via g_eventLogger, select one of:
 *
 * NDB_PATCH=features;file=/path/to/file
 * NDB_PATCH=features;syslog
 * NDB_PATCH=features;eventlog=source-name (Windows Event Log)
 *
 * If no extra log destination is needed, just set
 * NDB_PATCH=features
 *
 * features should be an int there every bit can control a feature
 * features=1 enables patch specific logging with NDB_PATCH_INFO (see NdbPatch.h)
 * feature=-1 enables all patch specific logging and features
 *
 * Example, enable all features and add logs to syslog
 * $ export NDB_PATCH='-1;syslog'
 *
 * NDB_PATCH_INFO will always be feature 0, but other features can be added
 * but will vary between specific patches.  At most 8 features including NDB_PATCH_INFO
 * can be defined.
 *
 * If you defined extra features in a patch you should add a description in
 * NdbPatch__feature_descriptions below, and use NDB_PATCH_FEATURE(n) to test if it
 * is enabled in run time.
 *
 */

#define NDB_PATCH_MAX_FEATURES 8

extern EventLogger* g_eventLogger;

int NdbPatch__features = 0;
static const char* NdbPatch__feature_descriptions[NDB_PATCH_MAX_FEATURES] = {
  "Patch specific logging.  To add extra log destinations set 'syslog' or 'file=/path/to/process-specific.file' in NDB_PATCH environment.",
  "Adds read barrier in transporter layer, see code :)",
  "Adds write barrier for NDBAPI in transporter layer, see code :)",
  /* rest of array will be zeroed */
};
static size_t NdbPatch__source_dir_length = 0;
static const char* NdbPatch__env = NULL;

extern "C" void
NdbPatch__init()
{
  const char* env = getenv("NDB_PATCH");
  if (env == NULL)
  {
    return;
  }
  NdbPatch__env = strdup(env); // Memory leak, no free
  env = NdbPatch__env;
  size_t featlen = strcspn(env, ";");

  NDB_STATIC_ASSERT(-1 == ~0); // -1 should turn on all features
  NdbPatch__features = atoi(env);

  /*
   * Look for extra log destinations
   * TODO: Unify with mgmd LogDestination configuration parameter
   */

  if (env[featlen] == ';')
  {
    const char* name = env + featlen + 1;
    size_t namelen = strcspn(name, "=");
    const char* arg = name + namelen;
    if (arg[0] == '=') arg++;
    if (strncasecmp(name, "file", namelen) == 0)
    {
      g_eventLogger->createFileHandler(arg);
    }
    else if (strncasecmp(name, "syslog", namelen) == 0)
    {
      g_eventLogger->createSyslogHandler();
    }
    else if (strncasecmp(name, "eventlog", namelen) == 0)
    {
      g_eventLogger->createEventLogHandler(arg);
    }
    g_eventLogger->enable(Logger::LL_INFO);

    /*
     * Calculate length of source directory length, used by
     * NdbPatch__source_basename to log only the
     * source file path relative source directory.
     */
    const char* this_file = __FILE__;
    const char* s = this_file;
    const char* end_path = s;
    // Find last occurence of storage in path
    while (s != NULL)
    {
      s = strstr(s, "storage");
      if (s == NULL)
        break;
      end_path = s;
      s += 7; // strlen("storage")
    }
    NdbPatch__source_dir_length = end_path - this_file;
    NdbPatch__source_dir_length += strlen("storage?ndb?");
  }
}

extern "C" void
NdbPatch__dump_config()
{
  NdbPatch__info("NDB_PATCH=%s", NdbPatch__env);

  /*
   * Log feature usage
   */

  for(unsigned i = 0; i < NDB_PATCH_MAX_FEATURES; i++)
  {
    static const char *onoff[2]={"OFF", "ON"};
    const char *desc = NdbPatch__feature_descriptions[i];
    if(desc != NULL)
    {
      NdbPatch__info("NDB_PATCH_FEATURE#%u: %s: %s",
        i, onoff[NDB_PATCH_FEATURE(i)], desc);
    }
  }
}

extern "C" void
NdbPatch__info(const char fmt[], ...)
{
  /* This functions should only be called enabled */
  assert(NDB_PATCH_FEATURE(0));
  if (unlikely(!NDB_PATCH_FEATURE(0)))
  {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  g_eventLogger->Logger::log(Logger::LL_INFO, fmt, ap);
  va_end(ap);
}

/*
 * Note: must be called with __FILE__ as argument
 */
extern "C" const char*
NdbPatch__source_basename(const char* filename)
{
  assert(memcmp(filename, __FILE__, NdbPatch__source_dir_length) == 0);
  if (unlikely(strlen(filename) <= NdbPatch__source_dir_length))
  {
    return filename;
  }
  return filename + NdbPatch__source_dir_length;
}

#endif
