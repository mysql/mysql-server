/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysys_priv.h"
#include "mysys_err.h"

#ifndef SHARED_LIBRARY

const char *globerrs[GLOBERRS]=
{
  "Can't create/write to file '%s' (Errcode: %d)",
  "Error reading file '%s' (Errcode: %d)",
  "Error writing file '%s' (Errcode: %d)",
  "Error on close of '%s' (Errcode: %d)",
  "Out of memory (Needed %u bytes)",
  "Error on delete of '%s' (Errcode: %d)",
  "Error on rename of '%s' to '%s' (Errcode: %d)",
  "",
  "Unexpected eof found when reading file '%s' (Errcode: %d)",
  "Can't lock file (Errcode: %d)",
  "Can't unlock file (Errcode: %d)",
  "Can't read dir of '%s' (Errcode: %d)",
  "Can't get stat of '%s' (Errcode: %d)",
  "Can't change size of file (Errcode: %d)",
  "Can't open stream from handle (Errcode: %d)",
  "Can't get working dirctory (Errcode: %d)",
  "Can't change dir to '%s' (Errcode: %d)",
  "Warning: '%s' had %d links",
  "Warning: %d files and %d streams is left open\n",
  "Disk is full writing '%s' (Errcode: %d). Waiting for someone to free space... (Expect up to %d secs delay for server to continue after freeing disk space)",
  "Can't create directory '%s' (Errcode: %d)",
  "Character set '%s' is not a compiled character set and is not specified in the '%s' file",
  "Out of resources when opening file '%s' (Errcode: %d)",
  "Can't read value for symlink '%s' (Error %d)",
  "Can't create symlink '%s' pointing at '%s' (Error %d)",
  "Error on realpath() on '%s' (Error %d)",
  "Can't sync file '%s' to disk (Errcode: %d)",
  "Collation '%s' is not a compiled collation and is not specified in the '%s' file",
  "File '%s' not found (Errcode: %d)",
  "File '%s' (fileno: %d) was not closed",
  "Can't change ownership of the file '%s' (Errcode: %d)",
  "Can't change permissions of the file '%s' (Errcode: %d)",
  "Can't seek in file '%s' (Errcode: %d)"
};

void init_glob_errs(void)
{
  /* This is now done statically. */
}

#else

void init_glob_errs()
{
  EE(EE_CANTCREATEFILE) = "Can't create/write to file '%s' (Errcode: %d)";
  EE(EE_READ)		= "Error reading file '%s' (Errcode: %d)";
  EE(EE_WRITE)		= "Error writing file '%s' (Errcode: %d)";
  EE(EE_BADCLOSE)	= "Error on close of '%'s (Errcode: %d)";
  EE(EE_OUTOFMEMORY)	= "Out of memory (Needed %u bytes)";
  EE(EE_DELETE)		= "Error on delete of '%s' (Errcode: %d)";
  EE(EE_LINK)		= "Error on rename of '%s' to '%s' (Errcode: %d)";
  EE(EE_EOFERR)		= "Unexpected eof found when reading file '%s' (Errcode: %d)";
  EE(EE_CANTLOCK)	= "Can't lock file (Errcode: %d)";
  EE(EE_CANTUNLOCK)	= "Can't unlock file (Errcode: %d)";
  EE(EE_DIR)		= "Can't read dir of '%s' (Errcode: %d)";
  EE(EE_STAT)		= "Can't get stat of '%s' (Errcode: %d)";
  EE(EE_CANT_CHSIZE)	= "Can't change size of file (Errcode: %d)";
  EE(EE_CANT_OPEN_STREAM)= "Can't open stream from handle (Errcode: %d)";
  EE(EE_GETWD)		= "Can't get working directory (Errcode: %d)";
  EE(EE_SETWD)		= "Can't change dir to '%s' (Errcode: %d)";
  EE(EE_LINK_WARNING)	= "Warning: '%s' had %d links";
  EE(EE_OPEN_WARNING)	= "Warning: %d files and %d streams is left open\n";
  EE(EE_DISK_FULL)	= "Disk is full writing '%s'. Waiting for someone to free space...";
  EE(EE_CANT_MKDIR)	="Can't create directory '%s' (Errcode: %d)";
  EE(EE_UNKNOWN_CHARSET)= "Character set '%s' is not a compiled character set and is not specified in the %s file";
  EE(EE_OUT_OF_FILERESOURCES)="Out of resources when opening file '%s' (Errcode: %d)";
  EE(EE_CANT_READLINK)=	"Can't read value for symlink '%s' (Error %d)";
  EE(EE_CANT_SYMLINK)=	"Can't create symlink '%s' pointing at '%s' (Error %d)";
  EE(EE_REALPATH)=	"Error on realpath() on '%s' (Error %d)";
  EE(EE_SYNC)=		"Can't sync file '%s' to disk (Errcode: %d)";
  EE(EE_UNKNOWN_COLLATION)= "Collation '%s' is not a compiled collation and is not specified in the %s file";
  EE(EE_FILENOTFOUND)	= "File '%s' not found (Errcode: %d)";
  EE(EE_FILE_NOT_CLOSED) = "File '%s' (fileno: %d) was not closed";
  EE(EE_CHANGE_OWNERSHIP)   = "Can't change ownership of the file '%s' (Errcode: %d)";
  EE(EE_CHANGE_PERMISSIONS) = "Can't change permissions of the file '%s' (Errcode: %d)";
  EE(EE_CANT_SEEK)      = "Can't seek in file '%s' (Errcode: %d)";
}
#endif

void wait_for_free_space(const char *filename, int errors)
{
  if (errors == 0)
    my_error(EE_DISK_FULL,MYF(ME_BELL | ME_NOREFRESH),
             filename,my_errno,MY_WAIT_FOR_USER_TO_FIX_PANIC);
  if (!(errors % MY_WAIT_GIVE_USER_A_MESSAGE))
    my_printf_error(EE_DISK_FULL,
                    "Retry in %d secs. Message reprinted in %d secs",
                    MYF(ME_BELL | ME_NOREFRESH),
                    MY_WAIT_FOR_USER_TO_FIX_PANIC,
                    MY_WAIT_GIVE_USER_A_MESSAGE * MY_WAIT_FOR_USER_TO_FIX_PANIC );
  (void) sleep(MY_WAIT_FOR_USER_TO_FIX_PANIC);
}

const char **get_global_errmsgs()
{
  return globerrs;
}
