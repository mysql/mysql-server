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

DWORD    TlsAlloc( void);
BOOL     TlsFree( DWORD);
PVOID    TlsGetValue( DWORD);
BOOL     TlsSetValue( DWORD, PVOID);

#define TLS_MINIMUM_AVAILABLE   64


PULONG           tls_storage;   /* TLS local storage */
DWORD            tls_bits[2];   /* TLS in-use bits   */
pthread_mutex_t  tls_mutex;     /* TLS mutex for in-use bits */

DWORD    TlsAlloc( void)
{
   DWORD index = -1;
   DWORD mask, tibidx;
   int   i;

   if (tls_storage == NULL) {

      APIRET rc;

      // allocate memory for TLS storage
      rc = DosAllocThreadLocalMemory( 1, &tls_storage);
      if (rc) {
         fprintf( stderr, "DosAllocThreadLocalMemory error: return code = %u\n", rc);
      }

      // create a mutex
      if (pthread_mutex_init( &tls_mutex, NULL))
         fprintf( stderr, "Failed to init TLS mutex\n");
   }

   pthread_mutex_lock( &tls_mutex);

   tibidx = 0;
   if (tls_bits[0] == 0xFFFFFFFF) {
      if (tls_bits[1] == 0xFFFFFFFF) {
            fprintf( stderr, "tid#%d, no more TLS bits available\n", _threadid);
            pthread_mutex_unlock( &tls_mutex);
            return -1;
      }
      tibidx = 1;
   }
   for( i=0; i<32; i++) {
      mask = (1 << i);
      if ((tls_bits[ tibidx] & mask) == 0) {
         tls_bits[ tibidx] |= mask;
         index = (tibidx*32) + i;
         break;
      }
   }
   tls_storage[index] = 0;

   pthread_mutex_unlock( &tls_mutex);

   //fprintf( stderr, "tid#%d, TlsAlloc index %d\n", _threadid, index);

   return index;
}

BOOL     TlsFree( DWORD index)
{
   int    tlsidx;
   DWORD  mask;

   if (index >= TLS_MINIMUM_AVAILABLE)
      return NULL;

   pthread_mutex_lock( &tls_mutex);

   tlsidx = 0;
   if (index > 32) {
      tlsidx++;
   }
   mask = (1 << index);
   if (tls_bits[ tlsidx] & mask) {
      tls_bits[tlsidx] &= ~mask;
      tls_storage[index] = 0;
      pthread_mutex_unlock( &tls_mutex);
      return TRUE;
   }

   pthread_mutex_unlock( &tls_mutex);
   return FALSE;
}


PVOID    TlsGetValue( DWORD index)
{
   if (index >= TLS_MINIMUM_AVAILABLE)
      return NULL;

   // verify if memory has been allocated for this thread
   if (*tls_storage == NULL) {
      // allocate memory for indexes
      *tls_storage = (ULONG)calloc( TLS_MINIMUM_AVAILABLE, sizeof(int));
      //fprintf( stderr, "tid#%d, tls_storage %x\n", _threadid, *tls_storage);
   }

   ULONG* tls_array = (ULONG*) *tls_storage;
   return (PVOID) tls_array[ index];
}

BOOL     TlsSetValue( DWORD index, PVOID val)
{

   // verify if memory has been allocated for this thread
   if (*tls_storage == NULL) {
      // allocate memory for indexes
      *tls_storage = (ULONG)calloc( TLS_MINIMUM_AVAILABLE, sizeof(int));
      //fprintf( stderr, "tid#%d, tls_storage %x\n", _threadid, *tls_storage);
   }

   if (index >= TLS_MINIMUM_AVAILABLE)
      return FALSE;

   ULONG* tls_array = (ULONG*) *tls_storage;
   //fprintf( stderr, "tid#%d, TlsSetValue array %08x index %d -> %08x (old)\n", _threadid, tls_array, index, tls_array[ index]);
   tls_array[ index] = (ULONG) val;
   //fprintf( stderr, "tid#%d, TlsSetValue array %08x index %d -> %08x\n", _threadid, tls_array, index, val);

   return TRUE;
}
