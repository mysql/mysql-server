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

/* Win32 directory search emulation */

#if defined(OS2)

//#define _DEBUG

long  _findfirst( char* path, struct _finddata_t* dos_file)
{
   HDIR           hdir = HDIR_CREATE;
   APIRET         rc;
   FILEFINDBUF3   buf3;
   ULONG          entries = 1;

#ifdef _DEBUG
   printf( "_findfirst path %s\n", path);
#endif

   memset( &buf3, 0, sizeof( buf3));
   rc = DosFindFirst(
               path,  /*  Address of the ASCIIZ path name of the file or subdirectory to be found. */
               &hdir,        /*  Address of the handle associated with this DosFindFirst request. */
               FILE_NORMAL | FILE_DIRECTORY,  /*  Attribute value that determines the file objects to be searched for. */
               &buf3,     /*  Result buffer. */
               sizeof( buf3),        /*  The length, in bytes, of pfindbuf. */
               &entries,  /*  Pointer to the number of entries: */
               FIL_STANDARD);  /*  The level of file information required. */

#ifdef _DEBUG
   printf( "_findfirst rc=%d hdir=%d entries=%d->%s\n", rc, hdir, entries, buf3.achName);
#endif

   if (rc /* && entries == 0 */)
      return -1;

   if (dos_file) {
      memset( dos_file, 0, sizeof( struct _finddata_t));
      strcpy( dos_file->name, buf3.achName);
      dos_file->size = buf3.cbFile;
      dos_file->attrib = buf3.attrFile;
   }
   return (ULONG) hdir;
}


long  _findnext( long hdir, struct _finddata_t* dos_file)
{
   APIRET         rc;
   FILEFINDBUF3   buf3;
   ULONG          entries = 1;

   memset( &buf3, 0, sizeof( buf3));
   rc = DosFindNext(
               hdir,
               &buf3,     /*  Result buffer. */
               sizeof( buf3),        /*  The length, in bytes, of pfindbuf. */
               &entries);  /*  Pointer to the number of entries: */

#ifdef _DEBUG
   printf( "_findnext rc=%d hdir=%d entries=%d->%s\n", rc, hdir, entries, buf3.achName);
#endif

   if (rc /* && entries == 0 */)
      return -1;

   if (dos_file) {
      memset( dos_file, 0, sizeof( struct _finddata_t));
      strcpy( dos_file->name, buf3.achName);
      dos_file->size = buf3.cbFile;
      dos_file->attrib = buf3.attrFile;
   }
   return 0;
}

void  _findclose( long hdir)
{
   APIRET         rc;

   rc = DosFindClose( hdir);
#ifdef _DEBUG
   printf( "_findclose rc=%d hdir=%d\n", rc, hdir);
#endif
}

DIR* opendir( char* path)
{
   DIR* dir = (DIR*) calloc( 1, sizeof( DIR));
   char buffer[260];
   APIRET         rc;
   ULONG          entries = 1;

   strcpy( buffer, path);
   strcat( buffer, "*.*");

#ifdef _DEBUG
   printf( "_findfirst path %s\n", buffer);
#endif

   dir->hdir = HDIR_CREATE;
   memset( &dir->buf3, 0, sizeof( dir->buf3));
   rc = DosFindFirst(
               buffer,  /*  Address of the ASCIIZ path name of the file or subdirectory to be found. */
               &dir->hdir,        /*  Address of the handle associated with this DosFindFirst request. */
               FILE_NORMAL | FILE_DIRECTORY,  /*  Attribute value that determines the file objects to be searched for. */
               &dir->buf3,     /*  Result buffer. */
               sizeof( dir->buf3),        /*  The length, in bytes, of pfindbuf. */
               &entries,  /*  Pointer to the number of entries: */
               FIL_STANDARD);  /*  The level of file information required. */

#ifdef _DEBUG
   printf( "opendir rc=%d hdir=%d entries=%d->%s\n", rc, dir->hdir, entries, dir->buf3.achName);
#endif

   if (rc /* && entries == 0 */)
      return NULL;

   return dir;
}

struct dirent* readdir( DIR* dir)
{
   APIRET         rc;
   //FILEFINDBUF3   buf3;
   ULONG          entries = 1;

   if (!dir->buf3.achName[0]) // file not found on previous query
      return NULL;

   // copy last file name
   strcpy( dir->ent.d_name, dir->buf3.achName);

   // query next file
   memset( &dir->buf3, 0, sizeof( dir->buf3));
   rc = DosFindNext(
               dir->hdir,
               &dir->buf3,     /*  Result buffer. */
               sizeof( dir->buf3),        /*  The length, in bytes, of pfindbuf. */
               &entries);  /*  Pointer to the number of entries: */

#ifdef _DEBUG
   printf( "_findnext rc=%d hdir=%d entries=%d->%s\n", rc, dir->hdir, entries, dir->buf3.achName);
#endif

   if (rc /* && entries == 0 */)
      strcpy( dir->buf3.achName, ""); // reset name for next query

   return &dir->ent;
}

int closedir (DIR *dir)
{
   APIRET         rc;

   rc = DosFindClose( dir->hdir);
#ifdef _DEBUG
   printf( "_findclose rc=%d hdir=%d\n", rc, dir->hdir);
#endif
   free(dir);
   return 0;
}


#endif // OS2
