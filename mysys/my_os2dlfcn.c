/* Copyright (C) Yuri Dario & 2000 MySQL AB
   All the above parties has a full, independent copyright to
   the following code, including the right to use the code in
   any manner without any demands from the other parties.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
 *    dlfcn::Unix dynamic loading for OS/2
 *
 * Compatibility layer for dynamic loading.
 * Only minimal implementation
 *
*/

#define RTLD_LAZY 0
#define RTLD_NOW 0

void* dlopen( char* path, int flag);
char* dlerror( void);
void* dlsym( void* hmod, char* fn);
void  dlclose( void* hmod);

char    fail[ 256];

void* dlopen( char* path, int flag)
{
   APIRET  rc;
   HMODULE hmod;

   rc = DosLoadModule( fail, sizeof( fail), path, &hmod);
   if (rc)
      return NULL;

   return (void*) hmod;
}

char* dlerror( void)
{
   return fail;
}

void* dlsym( void* hmod, char* fn)
{
   APIRET  rc;
   PFN     addr;

   rc = DosQueryProcAddr( (HMODULE) hmod, 0l, fn, &addr);
   if (rc)
      return NULL;

   return (void*) addr;
}

void  dlclose( void* hmod)
{
   APIRET  rc;

   rc = DosFreeModule( (HMODULE) hmod);

}
