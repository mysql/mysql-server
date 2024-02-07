/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_global.h"

#include "portlib/NdbTick.h"
#include "portlib/ndb_socket_poller.h"
#include "util/BaseString.hpp"
#include "util/socket_io.h"

static inline int poll_socket(ndb_socket_t socket, bool read, bool write,
                              int timeout_millis, int *total_elapsed_millis) {
  const NDB_TICKS start = NdbTick_getCurrentTicks();

  timeout_millis -= *total_elapsed_millis;

  if (timeout_millis <= 0) return 0;  // Timeout occurred

  const int res = ndb_poll(socket, read, write, timeout_millis);

  // Calculate elapsed time in this function
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const int elapsed_millis = (int)(NdbTick_Elapsed(start, now).milliSec());

  // Update the total elapsed time
  *total_elapsed_millis += elapsed_millis;

  return res;
}

int read_socket(ndb_socket_t socket, int timeout_millis, char *buf,
                int buflen) {
  if (!ndb_socket_valid(socket)) return -1;

  if (buflen < 1) return 0;

  int elapsed_millis = 0;
  const int res =
      poll_socket(socket, true, false, timeout_millis, &elapsed_millis);
  if (res <= 0) return res;

  return (int)ndb_recv(socket, &buf[0], buflen, 0);
}

/*
 * On success readln_socket will read one line of data into buf.
 * At most (buflen-2) characters plus new line character ('\n') and null
 * termination ('\0').
 * Input line can be terminated using "\r\n" but will be returned as '\n'.
 * The return value will be the line length including the new line character.
 * Furthermore the elapsed time (*time) will always be set to zero.
 *
 * If nothing was read until timeout, 0 will be returned and *time will contain
 * the elapsed time.
 *
 * If some data have been read when timeout, failure (-1) will be returned and
 * the amount of read data will *not* be indicated to caller in any way.
 *
 * Note that if buflen is less than two, 0 will be returned, not -1.
 */
int readln_socket(ndb_socket_t socket, int timeout_millis, int *time, char *buf,
                  int buflen, NdbMutex *mutex) {
  if (!ndb_socket_valid(socket)) return -1;

  if (buflen <= 1) return 0;

  if (mutex) NdbMutex_Unlock(mutex);

  const int res = poll_socket(socket, true, false, timeout_millis, time);

  if (mutex) NdbMutex_Lock(mutex);

  if (res <= 0) return res;

  char *buf_free_ptr = buf;
  int buf_free_len = buflen;
  for (;;) {
    ssize_t t;
    while ((t = ndb_recv(socket, buf_free_ptr, buf_free_len, MSG_PEEK)) == -1 &&
           socket_errno == EINTR)
      ;

    if (t < 1) {
      return -1;
    }

    int i = 0;
    while (i < t && buf_free_ptr[i++] != '\n') {
      ;
    }

    /* consume peeked data up to end of line or eveything */
    for (int j = i; j > 0;) {
      ssize_t t;
      while ((t = ndb_recv(socket, buf_free_ptr, j, 0)) == -1 &&
             socket_errno == EINTR)
        ;

      if (t < 1) {
        return -1;
      }

      buf_free_ptr += t;
      buf_free_len -= t;
      j -= t;
    }

    if (buf_free_ptr[-1] == '\n') break;
    if (buf_free_len == 0) return -1;  // End of line not found

    if (poll_socket(socket, true, false, timeout_millis, time) != 1) {
      // Read some bytes but didn't find newline before all time was
      // used up => return error
      return -1;
    }
  }

  assert(buf_free_ptr[-1] == '\n');

  if (buf_free_len == 0) {
    if (buf_free_ptr[-2] != '\r') return -1;  // Too long line
    buf_free_ptr--;
    buf_free_len++;
    buf_free_ptr[-1] = '\n';
  } else if (buf_free_ptr > buf + 2) {
    if (buf_free_ptr[-2] == '\r') {
      buf_free_ptr--;
      buf_free_len++;
      buf_free_ptr[-1] = '\n';
    }
  }
  assert(buf_free_len >= 1);
  buf_free_ptr[0] = 0;

  *time = 0;
  return (int)(buf_free_ptr - buf);
}

int write_socket(ndb_socket_t socket, int timeout_millis, int *time,
                 const char buf[], int len) {
  if (poll_socket(socket, false, true, timeout_millis, time) != 1) return -1;

  const char *tmp = &buf[0];
  while (len > 0) {
    const int w = (int)ndb_send(socket, tmp, len, 0);
    if (w == -1) {
      return -1;
    }
    len -= w;
    tmp += w;

    if (len == 0) break;

    if (poll_socket(socket, false, true, timeout_millis, time) != 1) return -1;
  }

  return 0;
}

#ifdef _WIN32

class INIT_WINSOCK2 {
 public:
  INIT_WINSOCK2(void);
  ~INIT_WINSOCK2(void);

 private:
  bool m_bAcceptable;
};

INIT_WINSOCK2 g_init_winsock2;

INIT_WINSOCK2::INIT_WINSOCK2(void) : m_bAcceptable(false) {
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;

  wVersionRequested = MAKEWORD(2, 2);

  err = WSAStartup(wVersionRequested, &wsaData);
  if (err != 0) {
    /* Tell the user that we could not find a usable */
    /* WinSock DLL.                                  */
    m_bAcceptable = false;
  }

  /* Confirm that the WinSock DLL supports 2.2.*/
  /* Note that if the DLL supports versions greater    */
  /* than 2.2 in addition to 2.2, it will still return */
  /* 2.2 in wVersion since that is the version we      */
  /* requested.                                        */

  if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
    /* Tell the user that we could not find a usable */
    /* WinSock DLL.                                  */
    WSACleanup();
    m_bAcceptable = false;
  }

  /* The WinSock DLL is acceptable. Proceed. */
  m_bAcceptable = true;
}

INIT_WINSOCK2::~INIT_WINSOCK2(void) {
  if (m_bAcceptable) {
    m_bAcceptable = false;
    WSACleanup();
  }
}

#endif
