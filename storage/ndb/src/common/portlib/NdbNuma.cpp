/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbNuma.h>
#include <ndb_global.h>

static int NDB_TRACE_NUMA = 0;

#if defined HAVE_DLFCN_H && defined HAVE_DLOPEN
#include <dlfcn.h>

/**
 * Load libnuma using dlopen, not have to put link dependency on it...
 * - handle fact that there are 2 versions of libnuma...
 *   use existance of symbol "numa_all_nodes_ptr" to use v2 abi
 */
struct bitmask;
extern "C"
{
  typedef int (* fun0)(void);
  typedef void (* fun1)(struct bitmask*);
  typedef void (* fun2)(int);
  typedef int (* fun3)(int node, unsigned long * bug, int buflen);
  typedef bitmask * (* fun4)();
  typedef void (* fun5)(struct bitmask*);
  typedef bitmask * (* fun6)(struct bitmask*);
};

class NdbNuma
{
public:
  NdbNuma() { handle = 0;}
  ~NdbNuma() { if (handle) dlclose(handle); }

  int open();
  int build_cputonodemap();

  void * handle;
  fun0 numa_available;

  fun0 numa_max_node;
  fun0 numa_max_possible_node;
  fun1 numa_set_interleave_mask;
  fun2 numa_set_strict;
  fun3 numa_node_to_cpus;
  fun4 numa_allocate_nodemask;
  fun5 numa_bitmask_free;
  fun6 numa_bitmask_setall;

  struct bitmask * numa_all_nodes;
  struct bitmask * numa_all_nodes_ptr;
};

static
void*
my_dlopen(const char * name)
{
  void * p = dlopen(name, RTLD_LAZY);
  if (NDB_TRACE_NUMA)
  {
    if (p == 0)
      printf("info: failed to load %s\n", name);
    else
      printf("info: loaded %s\n", name);
  }
  return p;
}

static
void*
my_dlsym(void * handle, const char * name)
{
  void * p = dlsym(handle, name);
  if (NDB_TRACE_NUMA)
  {
    if (p != 0)
    {
      printf("info: %s OK\n", name);
    }
    else
    {
      printf("info: %s NOT FOUND\n", name);
    }
  }
  return p;
}

int
NdbNuma::open()
{
  handle = my_dlopen("libnuma.so");
  if (handle == 0)
  {
    handle = my_dlopen("libnuma.so.1");
  }
  if (handle == 0)
  {
    return -1;
  }

  numa_available = (fun0)my_dlsym(handle, "numa_available");
  if (numa_available == 0)
  {
    goto fail;
  }

  if ((* numa_available)() == -1)
  {
    if (NDB_TRACE_NUMA)
    {
      printf("info: numa_available() returns -1 => no numa support\n");
    }
    goto fail;
  }

  numa_max_node = (fun0)my_dlsym(handle, "numa_max_node");
  numa_set_interleave_mask = (fun1)my_dlsym(handle, "numa_set_interleave_mask");
  numa_set_strict = (fun2)my_dlsym(handle, "numa_set_strict");
  numa_node_to_cpus = (fun3)my_dlsym(handle, "numa_node_to_cpus");
  numa_all_nodes = (struct bitmask*)my_dlsym(handle, "numa_all_nodes");
  numa_all_nodes_ptr = (struct bitmask*)my_dlsym(handle, "numa_all_nodes_ptr");
  numa_allocate_nodemask = (fun4)my_dlsym(handle, "numa_allocate_nodemask");
  numa_bitmask_free = (fun5)my_dlsym(handle, "numa_bitmask_free");
  numa_bitmask_setall = (fun6)my_dlsym(handle, "numa_bitmask_setall");


  return 0;
fail:
  dlclose(handle);
  handle = 0;
  return -1;
}

static
bool
bit_is_set(unsigned long * mask, int bit)
{
  int n = bit / (8 * sizeof(unsigned long));
  int b = bit % (8 * sizeof(unsigned long));
  return (mask[n] & (1UL << b)) != 0;
}

int
NdbNuma::build_cputonodemap()
{
  int len = 512;
  unsigned long * buf = (unsigned long*)malloc(len);
  if (buf == 0)
    return -1;

  int m = (* numa_max_node)();
  for (int i = 0; i <= m; i++)
  {
retry:
    int r = (* numa_node_to_cpus)(i, buf, len);
    if (r == -1)
    {
      if (errno != ERANGE)
        goto fail;

      len = len + 512;
      if (len > 4096)
        goto fail;

      void * p = realloc(buf, len);
      if (p == 0)
        goto fail;

      buf = (unsigned long*)p;
      goto retry;
    }
    printf("node %d cpu(s): ", i);
    for (int j = 0; j<8*len;j++)
      if (bit_is_set(buf, j))
        printf("%d ", j);
    printf("\n");
  }
  free(buf);
  return 0;
fail:
  free(buf);
  return -1;
}

extern "C"
int
NdbNuma_setInterleaved()
{
  NdbNuma numa;
  if (numa.open() == -1)
    return -1;

  if (numa.numa_set_interleave_mask == 0)
    return -1;

  if (numa.numa_all_nodes_ptr != 0)
  {
    /**
     * libnuma v2
     */
    if (numa.numa_allocate_nodemask != 0 &&
        numa.numa_bitmask_setall != 0 &&
        numa.numa_bitmask_free != 0)
    {
      struct bitmask * bm = (* numa.numa_allocate_nodemask)();
      if (bm != 0)
      {
        (* numa.numa_bitmask_setall)(bm);
        (* numa.numa_set_interleave_mask)(bm);
        (* numa.numa_bitmask_free)(bm);
      }
      else
      {
        return -1;
      }
    }
    else
    {
      return -1;
    }
  }
  else if (numa.numa_all_nodes != 0)
  {
    /**
     * libnuma v1
     */
    (* numa.numa_set_interleave_mask)(numa.numa_all_nodes);
  }
  else
  {
    return -1;
  }

  return 0;
}

#else
extern "C"
int
NdbNuma_setInterleaved()
{
  return -1;
}

extern "C"
int
NdbNuma_setInterleavedOnCpus(unsigned cpu[], unsigned len)
{
  return -1;
}
#endif

#ifdef TEST_NDBNUMA
#include <NdbTap.hpp>

TAPTEST(SetInterleaved)
{
  NDB_TRACE_NUMA = 1;
  NdbNuma_setInterleaved();
  return 1; // OK
}
#endif
