#***************************************************************************
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at https://curl.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# SPDX-License-Identifier: curl
#
###########################################################################
if(NOT UNIX)
  if(WIN32)
    set(HAVE_LIBSOCKET 0)
    set(HAVE_GETHOSTNAME 1)
    set(HAVE_LIBZ 0)

    set(HAVE_ARPA_INET_H 0)
    set(HAVE_FCNTL_H 1)
    set(HAVE_IO_H 1)
    set(HAVE_NETDB_H 0)
    set(HAVE_NETINET_IN_H 0)
    set(HAVE_NET_IF_H 0)
    set(HAVE_PWD_H 0)
    set(HAVE_SETJMP_H 1)
    set(HAVE_SIGNAL_H 1)
    set(HAVE_STDLIB_H 1)
    set(HAVE_STRINGS_H 0)
    set(HAVE_STRING_H 1)
    set(HAVE_SYS_PARAM_H 0)
    set(HAVE_SYS_POLL_H 0)
    set(HAVE_SYS_SELECT_H 0)
    set(HAVE_SYS_SOCKET_H 0)
    set(HAVE_SYS_SOCKIO_H 0)
    set(HAVE_SYS_STAT_H 1)
    set(HAVE_SYS_TIME_H 0)
    set(HAVE_SYS_TYPES_H 1)
    set(HAVE_SYS_UTIME_H 1)
    set(HAVE_TERMIOS_H 0)
    set(HAVE_TERMIO_H 0)
    set(HAVE_TIME_H 1)
    set(HAVE_UTIME_H 0)

    set(HAVE_SOCKET 1)
    set(HAVE_SELECT 1)
    set(HAVE_STRDUP 1)
    set(HAVE_STRICMP 1)
    set(HAVE_STRCMPI 1)
    set(HAVE_GETTIMEOFDAY 0)
    set(HAVE_CLOSESOCKET 1)
    set(HAVE_SIGSETJMP 0)
    set(HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID 1)
    set(HAVE_GETPASS_R 0)
    set(HAVE_GETPWUID 0)
    set(HAVE_GETEUID 0)
    set(HAVE_UTIME 1)
    set(HAVE_RAND_EGD 0)
    set(HAVE_GMTIME_R 0)
    set(HAVE_GETHOSTBYNAME_R 0)
    set(HAVE_SIGNAL 1)

    set(HAVE_GETHOSTBYNAME_R_3 0)
    set(HAVE_GETHOSTBYNAME_R_3_REENTRANT 0)
    set(HAVE_GETHOSTBYNAME_R_5 0)
    set(HAVE_GETHOSTBYNAME_R_5_REENTRANT 0)
    set(HAVE_GETHOSTBYNAME_R_6 0)
    set(HAVE_GETHOSTBYNAME_R_6_REENTRANT 0)

    set(TIME_WITH_SYS_TIME 0)
    set(HAVE_O_NONBLOCK 0)
    set(HAVE_IN_ADDR_T 0)
    set(STDC_HEADERS 1)

    set(HAVE_SIGACTION 0)
    set(HAVE_MACRO_SIGSETJMP 0)
  else()
    message("This file should be included on Windows platform only")
  endif()
endif()
