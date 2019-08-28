/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* get the number of (online) CPUs */

#include "mysys_priv.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static int ncpus=0;

int my_getncpus()
{
  if (!ncpus)
  {
#ifdef _SC_NPROCESSORS_ONLN
    ncpus= sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(__WIN__)
    SYSTEM_INFO sysinfo;

    /*
    * We are not calling GetNativeSystemInfo here because (1) we
    * don't believe that they return different values for number
    * of processors and (2) if WOW64 limits processors for Win32
    * then we don't want to try to override that.
    */
    GetSystemInfo(&sysinfo);

    ncpus= sysinfo.dwNumberOfProcessors;
#else
/* unknown so play safe: assume SMP and forbid uniprocessor build */
    ncpus= 2;
#endif
  }
  return ncpus;
}
