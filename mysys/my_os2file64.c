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

void        _OS2errno( APIRET rc);
longlong    _lseek64( int fd, longlong offset, int seektype);
int         _lock64( int fd, int locktype, my_off_t start,
                     my_off_t length, myf MyFlags);
int         _sopen64( const char *name, int oflag, int shflag, int mask);

//
// this class is used to define a global c++ variable, that
// is initialized before main() gets called.
//
class File64bit
{
   public:
      File64bit();  /* default constructor */
} initFile64bit;

static  APIRET (* APIENTRY _DosOpenL)(PCSZ  pszFileName,
                            PHFILE phf,
                            PULONG pulAction,
                            LONGLONG cbFile,
                            ULONG ulAttribute,
                            ULONG fsOpenFlags,
                            ULONG fsOpenMode,
                            PEAOP2 peaop2);
static  APIRET (* APIENTRY _DosSetFilePtrL)(HFILE hFile,
                                   LONGLONG ib,
                                   ULONG method,
                                   PLONGLONG ibActual);
static  APIRET (* APIENTRY _DosSetFileLocksL)(HFILE hFile,
                                    PFILELOCKL pflUnlock,
                                    PFILELOCKL pflLock,
                                    ULONG timeout,
                                    ULONG flags);

#define  EIO      EINVAL
#define  ESPIPE   EBADSEEK


static unsigned char const errno_tab[] =
{
  0     , EINVAL, ENOENT, ENOENT, EMFILE,  /* 0..4 */
  EACCES, EBADF,  EIO,    ENOMEM, EIO,     /* 5..9 */
  EINVAL, ENOEXEC,EINVAL, EINVAL, EINVAL,  /* 10..14 */
  ENOENT, EBUSY,  EXDEV,  ENOENT, EROFS,   /* 15..19 */
  EIO,    EIO,    EIO,    EIO,    EIO,     /* 20..24 */
  EIO,    EIO,    EIO,    ENOSPC, EIO,     /* 25..29 */
  EIO,    EIO,    EACCES, EACCES, EIO,     /* 30..34 */
  EIO,    EIO,    EIO,    EIO,    ENOSPC,  /* 35..39 */
  EIO,    EIO,    EIO,    EIO,    EIO,     /* 40..44 */
  EIO,    EIO,    EIO,    EIO,    EIO,     /* 45..49 */
  EIO,    EIO,    EIO,    EIO,    EBUSY,   /* 50..54 */
  EIO,    EIO,    EIO,    EIO,    EIO,     /* 55..59 */
  EIO,    ENOSPC, ENOSPC, EIO,    EIO,     /* 60..64 */
  EACCES, EIO,    EIO,    EIO,    EIO,     /* 65..69 */
  EIO,    EIO,    EIO,    EROFS,  EIO,     /* 70..74 */
  EIO,    EIO,    EIO,    EIO,    EIO,     /* 75..79 */
  EEXIST, EIO,    ENOENT, EIO,    EIO,     /* 80..84 */
  EIO,    EIO,    EINVAL, EIO,    EAGAIN,  /* 85..89 */
  EIO,    EIO,    EIO,    EIO,    EIO,     /* 90..94 */
  EINTR,  EIO,    EIO,    EIO,    EACCES,  /* 95..99 */
  ENOMEM, EINVAL, EINVAL, ENOMEM, EINVAL,  /* 100..104 */
  EINVAL, ENOMEM, EIO,    EACCES, EPIPE,   /* 105..109 */
  ENOENT, E2BIG,  ENOSPC, ENOMEM, EBADF,   /* 110..114 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 115..119 */
  EINVAL, EINVAL, EINVAL, ENOENT, EINVAL,  /* 120..124 */
  ENOENT, ENOENT, ENOENT, ECHILD, ECHILD,  /* 125..129 */
  EACCES, EINVAL, ESPIPE, EINVAL, EINVAL,  /* 130..134 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 135..139 */
  EINVAL, EINVAL, EBUSY,  EINVAL, EINVAL,  /* 140..144 */
  EINVAL, EINVAL, EINVAL, EBUSY,  EINVAL,  /* 145..149 */
  EINVAL, EINVAL, ENOMEM, EINVAL, EINVAL,  /* 150..154 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 155..159 */
  EINVAL, EINVAL, EINVAL, EINVAL, EAGAIN,  /* 160..164 */
  EINVAL, EINVAL, EACCES, EINVAL, EINVAL,  /* 165..169 */
  EBUSY,  EINVAL, EINVAL, EINVAL, EINVAL,  /* 170..174 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 175..179 */
  EINVAL, EINVAL, EINVAL, EINVAL, ECHILD,  /* 180..184 */
  EINVAL, EINVAL, ENOENT, EINVAL, EINVAL,  /* 185..189 */
  ENOEXEC,ENOEXEC,ENOEXEC,ENOEXEC,ENOEXEC, /* 190..194 */
  ENOEXEC,ENOEXEC,ENOEXEC,ENOEXEC,ENOEXEC, /* 195..199 */
  ENOEXEC,ENOEXEC,ENOEXEC,ENOENT, EINVAL,  /* 200..204 */
  EINVAL, ENAMETOOLONG, EINVAL, EINVAL, EINVAL,  /* 205..209 */
  EINVAL, EINVAL, EACCES, ENOEXEC,ENOEXEC, /* 210..214 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 215..219 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 220..224 */
  EINVAL, EINVAL, EINVAL, ECHILD, EINVAL,  /* 225..229 */
  EINVAL, EBUSY,  EAGAIN, ENOTCONN, EINVAL, /* 230..234 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 235..239 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 240..244 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 245..249 */
  EACCES, EACCES, EINVAL, ENOENT, EINVAL,  /* 250..254 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 255..259 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 260..264 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 265..269 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 270..274 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 275..279 */
  EINVAL, EINVAL, EINVAL, EINVAL, EEXIST,  /* 280..284 */
  EEXIST, EINVAL, EINVAL, EINVAL, EINVAL,  /* 285..289 */
  ENOMEM, EMFILE, EINVAL, EINVAL, EINVAL,  /* 290..294 */
  EINVAL, EINVAL, EINVAL, EINVAL, EINVAL,  /* 295..299 */
  EINVAL, EBUSY,  EINVAL, ESRCH,  EINVAL,  /* 300..304 */
  ESRCH,  EINVAL, EINVAL, EINVAL, ESRCH,   /* 305..309 */
  EINVAL, ENOMEM, EINVAL, EINVAL, EINVAL,  /* 310..314 */
  EINVAL, E2BIG,  ENOENT, EIO,    EIO,     /* 315..319 */
  EINVAL, EINVAL, EINVAL, EINVAL, EAGAIN,  /* 320..324 */
  EINVAL, EINVAL, EINVAL, EIO,    ENOENT,  /* 325..329 */
  EACCES, EACCES, EACCES, ENOENT, ENOMEM   /* 330..334 */
};

/*
 * Initialize 64bit file access: dynamic load of WSeB API
*/
            File64bit :: File64bit()
{
   HMODULE hDoscalls;

   if (DosQueryModuleHandle("DOSCALLS", &hDoscalls) != NO_ERROR)
      return;

   if (DosQueryProcAddr(hDoscalls, 981, NULL, (PFN *)&_DosOpenL) != NO_ERROR)
      return;

   if (DosQueryProcAddr(hDoscalls, 988, NULL, (PFN *)&_DosSetFilePtrL) != NO_ERROR) {
      _DosOpenL = NULL;
      return;
   }

   if (DosQueryProcAddr(hDoscalls, 986, NULL, (PFN *)&_DosSetFileLocksL) != NO_ERROR) {
      _DosOpenL = NULL;
      _DosSetFilePtrL = NULL;
      return;
   }
   // notify success
#ifdef MYSQL_SERVER
   printf( "WSeB 64bit file API loaded.\n");
#endif
}

void        _OS2errno( APIRET rc)
{
  if (rc >= sizeof (errno_tab))
    errno = EINVAL;
  else
    errno = errno_tab[rc];
}

longlong    _lseek64( int fd, longlong offset, int seektype)
{
   APIRET   rc;
   longlong actual;

   if (_DosSetFilePtrL)
      rc = _DosSetFilePtrL( fd, offset, seektype, &actual);
   else {
      ULONG ulActual;
      rc = DosSetFilePtr( fd, (long) offset, seektype, &ulActual);
      actual = ulActual;
   }

   if (!rc)
      return( actual);/* NO_ERROR */

   // set errno
   _OS2errno( rc);
   // seek failed
   return(-1);
}

inline _SetFileLocksL(HFILE hFile,
                          PFILELOCKL pflUnlock,
                          PFILELOCKL pflLock,
                          ULONG timeout,
                          ULONG flags)
{
   if (_DosSetFileLocksL)
      return _DosSetFileLocksL( hFile, pflUnlock, pflLock, timeout, flags);

   FILELOCK flUnlock = { pflUnlock->lOffset, pflUnlock->lRange };
   FILELOCK flLock = { pflLock->lOffset, pflLock->lRange };
   return DosSetFileLocks( hFile, &flUnlock, &flLock, timeout, flags);
}

int         _lock64( int fd, int locktype, my_off_t start,
                     my_off_t length, myf MyFlags)
{
   FILELOCKL LockArea = {0,0}, UnlockArea = {0,0};
   ULONG     readonly = 0;
   APIRET    rc = -1;

   switch( locktype) {
   case F_UNLCK:
      UnlockArea.lOffset = start;
      UnlockArea.lRange = length ? length : LONGLONG_MAX;
      break;

   case F_RDLCK:
   case F_WRLCK:
      LockArea.lOffset = start;
      LockArea.lRange = length ? length : LONGLONG_MAX;
      readonly = (locktype == F_RDLCK ? 1 : 0);
      break;

   default:
      errno = EINVAL;
      rc = -1;
      break;
   }

   if (MyFlags & MY_DONT_WAIT) {

      rc = _SetFileLocksL( fd, &UnlockArea, &LockArea, 0, readonly);
      //printf( "fd %d, locktype %d, rc %d (dont_wait)\n", fd, locktype, rc);
      if (rc == 33) {  /* Lock Violation */

         DBUG_PRINT("info",("Was locked, trying with timeout"));
         rc = _SetFileLocksL( fd, &UnlockArea, &LockArea, 1 * 1000, readonly);
         //printf( "fd %d, locktype %d, rc %d (dont_wait with timeout)\n", fd, locktype, rc);
      }

   } else {

      while( rc = _SetFileLocksL( fd, &UnlockArea, &LockArea, 0, readonly) && (rc == 33)) {
         printf(".");
         DosSleep(1 * 1000);
      }
      //printf( "fd %d, locktype %d, rc %d (wait2)\n", fd, locktype, rc);
   }

   if (!rc)
      return( 0);/* NO_ERROR */

   // set errno
   _OS2errno( rc);
   // lock failed
   return(-1);
}

int         _sopen64( const char *name, int oflag, int shflag, int mask)
{
   int      fail_errno;
   APIRET   rc = 0;
   HFILE    hf = 0;
   ULONG    ulAction = 0;
   LONGLONG cbFile = 0;
   ULONG    ulAttribute = FILE_NORMAL;
   ULONG    fsOpenFlags = 0;
   ULONG    fsOpenMode = 0;

   /* Extract the access mode and sharing mode bits. */
   fsOpenMode = (shflag & 0xFF) | (oflag & 0x03);

   /* Translate ERROR_OPEN_FAILED to ENOENT unless O_EXCL is set (see
      below). */
   fail_errno = ENOENT;

   /* Compute `open_flag' depending on `flags'.  Note that _SO_CREAT is
      set for O_CREAT. */

   if (oflag & O_CREAT)
   {
      if (oflag & O_EXCL)
      {
         fsOpenFlags = OPEN_ACTION_FAIL_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW;
         fail_errno = EEXIST;
      }
      else if (oflag & O_TRUNC)
        fsOpenFlags = OPEN_ACTION_REPLACE_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW;
      else
        fsOpenFlags = OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW;

      if (mask & S_IWRITE)
         ulAttribute = FILE_NORMAL;
      else
         ulAttribute = FILE_READONLY;

   }
   else if (oflag & O_TRUNC)
      fsOpenFlags = OPEN_ACTION_REPLACE_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW;
   else
      fsOpenFlags = OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW;

   /* Try to open the file and handle errors. */
   if (_DosOpenL)
      rc = _DosOpenL( name, &hf, &ulAction, cbFile,
                     ulAttribute, fsOpenFlags, fsOpenMode, NULL);
   else
      rc = DosOpen( name, &hf, &ulAction, (LONG) cbFile,
                     ulAttribute, fsOpenFlags, fsOpenMode, NULL);

   if (rc == ERROR_OPEN_FAILED)
   {
      errno = fail_errno;
      return -1;
   }
   if (rc != 0)
   {
      // set errno
      _OS2errno( rc);
      return -1;
   }

   if (oflag & O_APPEND)
      _lseek64( hf, 0L, SEEK_END);

   return hf;
}

inline int   open( const char *name, int oflag)
{
   return _sopen64( name, oflag, OPEN_SHARE_DENYNONE, S_IREAD | S_IWRITE);
}

inline int   open( const char *name, int oflag, int mask)
{
   return _sopen64( name, oflag, OPEN_SHARE_DENYNONE, mask);
}

inline int   sopen( const char *name, int oflag, int shflag, int mask)
{
   return _sopen64( name, oflag, shflag, mask);
}
