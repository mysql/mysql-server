/* Copyright (C) 2010 Sergei Golubchik and Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "feedback.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <base64.h>
#include <sha1.h>

#if defined (_WIN32)
#define HAVE_SYS_UTSNAME_H

#ifndef VER_SUITE_WH_SERVER
#define VER_SUITE_WH_SERVER 0x00008000
#endif

struct utsname {
  char  sysname[16];  // Name of this implementation of the operating system. 
  char  nodename[16]; // Name of this node within the communications 
                      // network to which this node is attached, if any. 
  char  release[16];  // Current release level of this implementation. 
  char  version[256]; // Current version level of this release. 
  char  machine[16];  // Name of the hardware type on which the system is running. 
}; 

/* Get commonly used name for Windows version */
static const char *get_os_version_name(OSVERSIONINFOEX *ver)
{
  DWORD major = ver->dwMajorVersion;
  DWORD minor = ver->dwMinorVersion;

  if (major == 6 && minor == 2)
  {
    return (ver->wProductType == VER_NT_WORKSTATION)?
      "Windows 8":"Windows Server 2012";    
  }
  
  if (major == 6 && minor == 1)
  {
    return (ver->wProductType == VER_NT_WORKSTATION)?
      "Windows 7":"Windows Server 2008 R2";
  }
  if (major == 6 && minor == 0)
  {
     return (ver->wProductType == VER_NT_WORKSTATION)?
      "Windows Vista":"Windows Server 2008";
  }
  if (major == 5 && minor == 2)
  {
    if (GetSystemMetrics(SM_SERVERR2) != 0)
      return "Windows Server 2003 R2";
    if (ver->wSuiteMask & VER_SUITE_WH_SERVER)
      return "Windows Home Server";
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    if (ver->wProductType == VER_NT_WORKSTATION && 
       sysinfo.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
      return "Windows XP Professional x64 Edition";

    return "Windows Server 2003";
  }
  if (major == 5 && minor == 1)
    return "Windows XP";
  if (major == 5 && minor == 0)
    return "Windows 2000";

  return "";
}


static int uname(struct utsname *buf)
{
  OSVERSIONINFOEX ver;
  ver.dwOSVersionInfoSize = (DWORD)sizeof(ver);
  if (!GetVersionEx((OSVERSIONINFO *)&ver))
    return -1;

  buf->nodename[0]= 0;
  strcpy(buf->sysname, "Windows");
  sprintf(buf->release, "%d.%d", ver.dwMajorVersion, ver.dwMinorVersion);

  const char *version_str= get_os_version_name(&ver);
  if(version_str && version_str[0])
    sprintf(buf->version, "%s %s",version_str, ver.szCSDVersion);
  else
    sprintf(buf->version, "%s", ver.szCSDVersion);

#ifdef _WIN64
  strcpy(buf->machine, "x64");
#else
  BOOL isX64;
  if (IsWow64Process(GetCurrentProcess(), &isX64) && isX64)
    strcpy(buf->machine, "x64");
  else
    strcpy(buf->machine,"x86");
#endif
  return 0;
}

#elif defined(HAVE_SYS_UTSNAME_H)
#include <sys/utsname.h>
#endif

#ifdef HAVE_SYS_UTSNAME_H
static bool have_ubuf= false;
static struct utsname ubuf;
#endif

#ifdef TARGET_OS_LINUX
#include <glob.h>
static bool have_distribution= false;
static char distribution[256];

static const char *masks[]= {
  "/etc/*-version", "/etc/*-release",
  "/etc/*_version", "/etc/*_release"
};
#endif

bool schema_table_store_record(THD *thd, TABLE *table);

namespace feedback {

/*
  convenience macros for inserting rows into I_S table.
*/
#define INSERT2(NAME,LEN,VALUE)                       \
  do {                                                \
    table->field[0]->store(NAME, LEN, system_charset_info); \
    table->field[1]->store VALUE;                     \
    if (schema_table_store_record(thd, table))        \
      return 1;                                       \
  } while (0)

#define INSERT1(NAME,VALUE)                           \
  do {                                                \
    table->field[0]->store(NAME, sizeof(NAME)-1, system_charset_info); \
    table->field[1]->store VALUE;                     \
    if (schema_table_store_record(thd, table))        \
      return 1;                                       \
  } while (0)

static const bool UNSIGNED= true; ///< used below when inserting integers

/**
  callback for fill_plugin_version() - insert a plugin name and its version
*/
static my_bool show_plugins(THD *thd, plugin_ref plugin, void *arg)
{
  TABLE *table= (TABLE*) arg;
  char version[20];
  size_t version_len;

  version_len= my_snprintf(version, sizeof(version), "%d.%d",
                           (plugin_decl(plugin)->version) >> 8,
                           (plugin_decl(plugin)->version) & 0xff);

  INSERT2(plugin_name(plugin)->str, plugin_name(plugin)->length,
          (version, version_len, system_charset_info));

  return 0;
}

/**
  inserts all plugins and their versions into I_S.FEEDBACK
*/
int fill_plugin_version(THD *thd, TABLE_LIST *tables)
{
  return plugin_foreach_with_mask(thd, show_plugins, MYSQL_ANY_PLUGIN,
                                  ~PLUGIN_IS_FREED, tables->table);
}

#if defined(_SC_PAGE_SIZE) && !defined(_SC_PAGESIZE)
#define _SC_PAGESIZE _SC_PAGE_SIZE
#endif

/**
  return the amount of physical memory
*/
static ulonglong my_getphysmem()
{
#ifdef _WIN32
  MEMORYSTATUSEX memstatus;
  memstatus.dwLength= sizeof(memstatus);
  GlobalMemoryStatusEx(&memstatus);
  return memstatus.ullTotalPhys;
#else
  ulonglong pages= 0;

#ifdef _SC_PHYS_PAGES
  pages= sysconf(_SC_PHYS_PAGES);
#endif

#ifdef _SC_PAGESIZE
  return pages * sysconf(_SC_PAGESIZE);
#else
  return pages * my_getpagesize();
#endif
#endif
}

/* get the number of (online) CPUs */
int my_getncpus()
{
#ifdef _SC_NPROCESSORS_ONLN
  return sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(__WIN__)
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors;
#else
  return 0;
#endif
}

/**
  Find the version of the kernel and the linux distribution
*/
int prepare_linux_info()
{
#ifdef HAVE_SYS_UTSNAME_H
  have_ubuf= (uname(&ubuf) != -1);
#endif

#ifdef TARGET_OS_LINUX
  /*
    let's try to find what linux distribution it is
    we read *[-_]{release,version} file in /etc.

    Either it will be /etc/lsb-release, such as

      ==> /etc/lsb-release <==
      DISTRIB_ID=Ubuntu
      DISTRIB_RELEASE=8.04
      DISTRIB_CODENAME=hardy
      DISTRIB_DESCRIPTION="Ubuntu 8.04.4 LTS"

   Or a one-liner with the description (/etc/SuSE-release has more
   than one line, but the description is the first, so it can be
   treated as a one-liner).

   We'll read lsb-release first, and if it's not found will search
   for other files (*-version *-release *_version *_release)
*/
  int fd;
  have_distribution= false;
  if ((fd= my_open("/etc/lsb-release", O_RDONLY, MYF(0))) != -1)
  {
    /* Cool, LSB-compliant distribution! */
    size_t len= my_read(fd, (uchar*)distribution, sizeof(distribution)-1, MYF(0));
    my_close(fd, MYF(0));
    if (len != (size_t)-1)
    {
      distribution[len]= 0; // safety
      char *found= strstr(distribution, "DISTRIB_DESCRIPTION=");
      if (found)
      {
        have_distribution= true;
        char *end= strstr(found, "\n");
        if (end == NULL)
          end= distribution + len;
        found+= 20;

        if (*found == '"' && end[-1] == '"')
        {
          found++;
          end--;
        }
        *end= 0;

        char *to= strmov(distribution, "lsb: ");
        memmove(to, found, end - found + 1);
      }
    }
  }

  /* if not an LSB-compliant distribution */
  for (uint i= 0; !have_distribution && i < array_elements(masks); i++)
  {
    glob_t found;
    if (glob(masks[i], GLOB_NOSORT, NULL, &found) == 0)
    {
      int fd;
      if ((fd= my_open(found.gl_pathv[0], O_RDONLY, MYF(0))) != -1)
      {
        /*
          +5 and -8 below cut the file name part out of the
          full pathname that corresponds to the mask as above.
        */
        char *to= strmov(distribution, found.gl_pathv[0] + 5) - 8;
        *to++= ':';
        *to++= ' ';

        size_t to_len= distribution + sizeof(distribution) - 1 - to;
        size_t len= my_read(fd, (uchar*)to, to_len, MYF(0));
        my_close(fd, MYF(0));
        if (len != (size_t)-1)
        {
          to[len]= 0; // safety
          char *end= strstr(to, "\n");
          if (end)
            *end= 0;
          have_distribution= true;
        }
      }
    }
    globfree(&found);
  }
#endif
  return 0;
}

/**
  Add the linux distribution and the kernel version
*/
int fill_linux_info(THD *thd, TABLE_LIST *tables)
{
  TABLE *table= tables->table;
  CHARSET_INFO *cs= system_charset_info;

#ifdef HAVE_SYS_UTSNAME_H
  if (have_ubuf)
  {
    INSERT1("Uname_sysname", (ubuf.sysname, strlen(ubuf.sysname), cs));
    INSERT1("Uname_release", (ubuf.release, strlen(ubuf.release), cs));
    INSERT1("Uname_version", (ubuf.version, strlen(ubuf.version), cs));
    INSERT1("Uname_machine", (ubuf.machine, strlen(ubuf.machine), cs));
  }
#endif

#ifdef TARGET_OS_LINUX
  if (have_distribution)
    INSERT1("Uname_distribution", (distribution, strlen(distribution), cs));
#endif

  return 0;
}

/**
  Adds varios bits of information to the I_S.FEEDBACK
*/
int fill_misc_data(THD *thd, TABLE_LIST *tables)
{
  TABLE *table= tables->table;

#ifdef MY_ATOMIC_OK
  INSERT1("Cpu_count", (my_getncpus(), UNSIGNED));
#endif
  INSERT1("Mem_total", (my_getphysmem(), UNSIGNED));
  INSERT1("Now", (thd->query_start(), UNSIGNED));

  return 0;
}

/**
  calculates the server unique identifier
  
  UID is a base64 encoded SHA1 hash of the MAC address of one of
  the interfaces, and the tcp port that the server is listening on
*/
int calculate_server_uid(char *dest)
{
  uchar rawbuf[2 + 6];
  uchar shabuf[SHA1_HASH_SIZE];
  SHA1_CONTEXT ctx;

  int2store(rawbuf, mysqld_port);
  if (my_gethwaddr(rawbuf + 2))
  {
    sql_print_error("feedback plugin: failed to retrieve the MAC address");
    return 1;
  }

  mysql_sha1_reset(&ctx);
  mysql_sha1_input(&ctx, rawbuf, sizeof(rawbuf));
  mysql_sha1_result(&ctx, shabuf);

  assert(base64_needed_encoded_length(sizeof(shabuf)) <= SERVER_UID_SIZE);
  base64_encode(shabuf, sizeof(shabuf), dest);

  return 0;
}

} // namespace feedback
