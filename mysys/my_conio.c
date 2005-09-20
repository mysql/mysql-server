/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include "mysys_priv.h"

#ifdef __WIN__

static HANDLE my_coninpfh= 0;     /* console input */

/*
  functions my_pthread_auto_mutex_lock & my_pthread_auto_mutex_free
  are experimental at this moment, they are intended to bring
  ability of protecting code sections without necessity to explicitly
  initialize synchronization object in one of threads

  if found useful they are to be exported in mysys
*/

/*
  int my_pthread_auto_mutex_lock(HANDLE* ph, const char* name, 
                                 int id, int time)

  NOTES
    creates a mutex with given name and tries to lock it time msec.
    mutex name is appended with id to allow system wide or process wide
    locks. Handle to created mutex returned in ph argument.

  RETURN
    0	              thread owns mutex
    <>0	            error

*/
static
int my_pthread_auto_mutex_lock(HANDLE* ph, const char* name, int id, int time)
{
  int res;
  char tname[FN_REFLEN];
  
  sprintf(tname, "%s-%08X", name, id);
  
  *ph= CreateMutex(NULL, FALSE, tname);
  if (*ph == NULL)
    return GetLastError();

  res= WaitForSingleObject(*ph, time);
  
  if (res == WAIT_TIMEOUT)
    return ERROR_SEM_TIMEOUT;

  if (res == WAIT_FAILED)
    return GetLastError();

  return 0;
}

/*
  int my_pthread_auto_mutex_free(HANDLE* ph)


  NOTES
    releases a mutex.

  RETURN
    0	              thread released mutex
    <>0	            error

*/
static
int my_pthread_auto_mutex_free(HANDLE* ph)
{
  if (*ph)
  {
    ReleaseMutex(*ph);
    CloseHandle(*ph);
    *ph= NULL;
  }

  return 0;
}


#define pthread_auto_mutex_decl(name)                           \
  HANDLE __h##name= NULL;

#define pthread_auto_mutex_lock(name, proc, time)               \
  my_pthread_auto_mutex_lock(&__h##name, #name, (proc), (time))

#define pthread_auto_mutex_free(name)                           \
  my_pthread_auto_mutex_free(&__h##name)


/*
  char* my_cgets(char *string, unsigned long clen, unsigned long* plen)

  NOTES
    Replaces _cgets from libc to support input of more than 255 chars.
    Reads from the console via ReadConsole into buffer which 
    should be at least clen characters.
    Actual length of string returned in plen.

  WARNING
    my_cgets() does NOT check the pushback character buffer (i.e., _chbuf).
    Thus, my_cgets() will not return any character that is pushed back by 
    the _ungetch() call.

  RETURN
    string pointer	ok
    NULL	          Error

*/
char* my_cgets(char *buffer, unsigned long clen, unsigned long* plen)
{
  ULONG state;
  char *result;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  
  pthread_auto_mutex_decl(my_conio_cs);
 
  /* lock the console for the current process*/
  if (pthread_auto_mutex_lock(my_conio_cs, GetCurrentProcessId(), INFINITE))
  {
    /* can not lock console */
    pthread_auto_mutex_free(my_conio_cs);  
    return NULL;
  }

  /* init console input */
  if (my_coninpfh == 0)
  {
    /* same handle will be used until process termination */
    my_coninpfh= CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, 0, NULL);
  }

  if (my_coninpfh == INVALID_HANDLE_VALUE) 
  {
    /* unlock the console */
    pthread_auto_mutex_free(my_conio_cs);  
    return(NULL);
  }

  GetConsoleMode((HANDLE)my_coninpfh, &state);
  SetConsoleMode((HANDLE)my_coninpfh, ENABLE_LINE_INPUT | 
                 ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT);

  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

  /* 
    there is no known way to determine allowed buffer size for input
    though it is known it should not be more than 64K               
    so we cut 64K and try first size of screen buffer               
    if it is still to large we cut half of it and try again         
    later we may want to cycle from min(clen, 65535) to allowed size
    with small decrement to determine exact allowed buffer           
  */
  clen= min(clen, 65535);
  do
  {
    clen= min(clen, (unsigned long)csbi.dwSize.X*csbi.dwSize.Y);
    if (!ReadConsole((HANDLE)my_coninpfh, (LPVOID)buffer, clen - 1, plen, NULL))
    {
      result= NULL;
      clen>>= 1;
    }
    else
    {
      result= buffer;
      break;
    }
  }
  while (GetLastError() == ERROR_NOT_ENOUGH_MEMORY);


  if (result != NULL)
  {
    if (buffer[*plen - 2] == '\r')
    {
      *plen= *plen - 2;
    }
    else 
    {
      if (buffer[*plen - 1] == '\r')
      {
        char tmp[3];
        int  tmplen= sizeof(tmp);

        *plen= *plen - 1;
        /* read /n left in the buffer */
        ReadConsole((HANDLE)my_coninpfh, (LPVOID)tmp, tmplen, &tmplen, NULL);
      }
    }
    buffer[*plen]= '\0';
  }

  SetConsoleMode((HANDLE)my_coninpfh, state);
  /* unlock the console */
  pthread_auto_mutex_free(my_conio_cs);  

  return result;
}

#endif /* __WIN__ */
