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
static int my_coninpfh= 0;     /* console input */

#define pthread_auto_mutex_decl(name)                           \
  HANDLE __h##name= NULL;                                       \
  char   __p##name[sizeof(#name)+16];

#define pthread_auto_mutex_lock(name, proc, time)               \
  sprintf(__p##name, "%s-%08X", #name, (proc));                 \
  __h##name= CreateMutex(NULL, FALSE, __p##name);               \
  WaitForSingleObject(__h##name, (time));

#define pthread_auto_mutex_free(name)                           \
  if (__h##name)                                                \
  {                                                             \
    ReleaseMutex(__h##name);                                    \
    CloseHandle(__h##name);                                     \
  }


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
  
  pthread_auto_mutex_decl(my_conio_mutex);
 
  /* lock the console */
  pthread_auto_mutex_lock(my_conio_mutex, GetCurrentProcessId(), INFINITE); 

  /* init console input */
  if (my_coninpfh == 0)
  {
    /* same handle will be used until process termination */
    my_coninpfh= (int)CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, 0, NULL);
  }

  if (my_coninpfh == -1) 
  {
    /* unlock the console */
    pthread_auto_mutex_free(my_conio_mutex);  
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
  pthread_auto_mutex_free(my_conio_mutex);  

  return result;
}

#endif /* __WIN__ */
