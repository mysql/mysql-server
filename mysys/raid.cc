/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

/* --------------------------------------------------------*
*
*  RAID support for MySQL. Raid 0 (stiping) only implemented yet.
*
*  Why RAID? Why it must be in MySQL?
*
*  This is because then you can:
*  1. Have bigger tables than your OS limit. In time of writing this
*     we are hitting to 2GB limit under linux/ext2
*  2. You can get more speed from IO bottleneck by putting
*     Raid dirs on different physical disks.
*  3. Getting more fault tolerance (not implemented yet)
*
*  Why not to use RAID:
*
*  1. You are losing some processor power to calculate things,
*     do more syscalls and interrupts.
*
*  Functionality is supplied by two classes: RaidFd and RaidName.
*  RaidFd supports funtionality over file descriptors like
*  open/create/write/seek/close. RaidName supports functionality
*  like rename/delete where we have no relations to filedescriptors.
*  RaidName can be prorably unchanged for different Raid levels. RaidFd
*  have to be virtual I think ;).
*  You can speed up some calls in MySQL code by skipping RAID code.
*  For example LOAD DATA INFILE never needs to read RAID-ed files.
*  This can be done adding proper "#undef my_read" or similar undef-s
*  in your code. Check out the raid.h!
*
*  Some explanation about _seek_vector[]
*  This is seek cache. RAID seeks too much and we cacheing this. We
*  fool it and just storing new position in file to _seek_vector.
*  When there is no seeks to do, we are putting RAID_SEEK_DONE into it.
*  Any other value requires seeking to that position.
*
*  TODO:
*
*
*  -  Implement other fancy things like RAID 1 (mirroring) and RAID 5.
*     Should not to be very complex.
*
*  -  Optimize big blob writes by resorting write buffers and writing
*     big chunks at once instead of doing many syscalls. - after thinking I
*     found this is useless. This is because same thing one can do with just
*     increasing RAID_CHUNKSIZE. Monty, what do you think? tonu.
*
*  -  If needed, then implement missing syscalls. One known to miss is stat();
*
*  -  Make and use a thread safe dynamic_array buffer. The used one
*     will not work if needs to be extended at the same time someone is
*     accessing it.
*
*
*  tonu@mysql.com & monty@mysql.com
* --------------------------------------------------------*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysys_priv.h"
#include "my_dir.h"
#include <m_string.h>
#include <assert.h>

const char *raid_type_string[]={"none","striped"};


extern "C" {
  const char *my_raid_type(int raid_type)
  {
    return raid_type_string[raid_type];
  }
}

#if defined(USE_RAID) && !defined(MYSQL_CLIENT)

#define RAID_SEEK_DONE ~(off_t) 0
#define RAID_SIZE_UNKNOWN ~(my_off_t) 0

DYNAMIC_ARRAY RaidFd::_raid_map;


/* ---------------  C compatibility  ---------------*/

extern "C" {

  void init_raid(void)
  {
  /* Allocate memory for global file to raid map */
    init_dynamic_array(&RaidFd::_raid_map, sizeof(RaidFd*), 4096, 1024);
  }
  void end_raid(void)
  {
    /* Free memory used by raid */
    delete_dynamic(&RaidFd::_raid_map);
  }

  bool is_raid(File fd)
  {
    return RaidFd::IsRaid(fd);
  }

  File my_raid_create(const char *FileName, int CreateFlags, int access_flags,
		      uint raid_type, uint raid_chunks, ulong raid_chunksize,
		      myf MyFlags)
  {
    DBUG_ENTER("my_raid_create");
    DBUG_PRINT("enter",("Filename: %s  CreateFlags: %d  access_flags: %d  MyFlags: %d",
			FileName, CreateFlags, access_flags, MyFlags));
    if (raid_type)
    {
      RaidFd *raid = new RaidFd(raid_type, raid_chunks , raid_chunksize);
      File res = raid->Create(FileName,CreateFlags,access_flags,MyFlags);
      if (res < 0 || set_dynamic(&RaidFd::_raid_map,(char*) &raid,res))
      {
	delete raid;
	DBUG_RETURN(-1);
      }
      DBUG_RETURN(res);
    }
    else
       DBUG_RETURN(my_create(FileName, CreateFlags, access_flags,  MyFlags));
  }

  File my_raid_open(const char *FileName, int Flags,
		    uint raid_type, uint raid_chunks, ulong raid_chunksize,
		    myf MyFlags)
  {
    DBUG_ENTER("my_raid_open");
    DBUG_PRINT("enter",("Filename: %s  Flags: %d  MyFlags: %d",
			FileName, Flags, MyFlags));
    if (raid_type)
    {
      RaidFd *raid = new RaidFd(raid_type, raid_chunks , raid_chunksize);
      File res = raid->Open(FileName,Flags,MyFlags);
      if (res < 0 || set_dynamic(&RaidFd::_raid_map,(char*) &raid,res))
      {
	delete raid;
	DBUG_RETURN(-1);
      }
      DBUG_RETURN(res);
    }
    else
      DBUG_RETURN(my_open(FileName, Flags, MyFlags));
  }

  my_off_t my_raid_seek(File fd, my_off_t pos,int whence,myf MyFlags)
  {
    DBUG_ENTER("my_raid_seek");
    DBUG_PRINT("enter",("Fd: %d  pos: %lu whence: %d  MyFlags: %d",
			fd, (ulong) pos, whence, MyFlags));

    assert(pos != MY_FILEPOS_ERROR);

    if (is_raid(fd))
    {
      RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,fd,RaidFd**));
      DBUG_RETURN(raid->Seek(pos,whence,MyFlags));
    }
    else
      DBUG_RETURN(my_seek(fd, pos, whence, MyFlags));
  }

  my_off_t my_raid_tell(File fd,myf MyFlags)
  {
    DBUG_ENTER("my_raid_tell");
    DBUG_PRINT("enter",("Fd: %d  MyFlags: %d",
			fd, MyFlags));
    if (is_raid(fd))
    {
      RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,fd,RaidFd**));
      DBUG_RETURN(raid->Tell(MyFlags));
    }
    else
       DBUG_RETURN(my_tell(fd, MyFlags));
  }

  uint my_raid_write(File fd,const byte *Buffer, uint Count, myf MyFlags)
  {
    DBUG_ENTER("my_raid_write");
    DBUG_PRINT("enter",("Fd: %d  Buffer: %lx  Count: %u  MyFlags: %d",
		      fd, Buffer, Count, MyFlags));
    if (is_raid(fd))
    {
      RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,fd,RaidFd**));
      DBUG_RETURN(raid->Write(Buffer,Count,MyFlags));
    } else
      DBUG_RETURN(my_write(fd,Buffer,Count,MyFlags));
  }

  uint my_raid_read(File fd, byte *Buffer, uint Count, myf MyFlags)
  {
    DBUG_ENTER("my_raid_read");
    DBUG_PRINT("enter",("Fd: %d  Buffer: %lx  Count: %u  MyFlags: %d",
		      fd, Buffer, Count, MyFlags));
    if (is_raid(fd))
    {
      RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,fd,RaidFd**));
      DBUG_RETURN(raid->Read(Buffer,Count,MyFlags));
    } else
      DBUG_RETURN(my_read(fd,Buffer,Count,MyFlags));
  }

  uint my_raid_pread(File Filedes, byte *Buffer, uint Count, my_off_t offset,
		     myf MyFlags)
  {
    DBUG_ENTER("my_raid_pread");
    DBUG_PRINT("enter",("Fd: %d  Buffer: %lx  Count: %u offset: %u  MyFlags: %d",
		      Filedes, Buffer, Count, offset, MyFlags));
     if (is_raid(Filedes))
     {
       assert(offset != MY_FILEPOS_ERROR);

       RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,Filedes,RaidFd**));
       /* Returning value isn't important because real seek is done later. */
       raid->Seek(offset,MY_SEEK_SET,MyFlags);
       DBUG_RETURN(raid->Read(Buffer,Count,MyFlags));
     }
     else
       DBUG_RETURN(my_pread(Filedes, Buffer, Count, offset, MyFlags));
  }

  uint my_raid_pwrite(int Filedes, const byte *Buffer, uint Count,
		      my_off_t offset, myf MyFlags)
  {
    DBUG_ENTER("my_raid_pwrite");
    DBUG_PRINT("enter",("Fd: %d  Buffer: %lx  Count: %u offset: %u  MyFlags: %d",
		      Filedes, Buffer, Count, offset, MyFlags));
     if (is_raid(Filedes))
     {
       assert(offset != MY_FILEPOS_ERROR);

       RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,Filedes,RaidFd**));
       /* Returning value isn't important because real seek is done later. */
       raid->Seek(offset,MY_SEEK_SET,MyFlags);
       DBUG_RETURN(raid->Write(Buffer,Count,MyFlags));
     }
     else
       DBUG_RETURN(my_pwrite(Filedes, Buffer, Count, offset, MyFlags));
  }

  int my_raid_lock(File fd, int locktype, my_off_t start, my_off_t length,
		   myf MyFlags)
  {
    DBUG_ENTER("my_raid_lock");
    DBUG_PRINT("enter",("Fd: %d  start: %u  length: %u  MyFlags: %d",
		      fd, start, length, MyFlags));
    if (my_disable_locking)
      DBUG_RETURN(0);
    if (is_raid(fd))
    {
      RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,fd,RaidFd**));
      DBUG_RETURN(raid->Lock(locktype, start, length, MyFlags));
    }
    else
      DBUG_RETURN(my_lock(fd, locktype, start, length, MyFlags));
  }

  int my_raid_close(File fd, myf MyFlags)
  {
    DBUG_ENTER("my_raid_close");
    DBUG_PRINT("enter",("Fd: %d  MyFlags: %d",
		      fd, MyFlags));
    if (is_raid(fd))
    {
      RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,fd,RaidFd**));
      RaidFd *tmp=0;
      set_dynamic(&RaidFd::_raid_map,(char*) &tmp,fd);
      int res = raid->Close(MyFlags);
      delete raid;
      DBUG_RETURN(res);
    }
    else
      DBUG_RETURN(my_close(fd, MyFlags));
  }

  int my_raid_chsize(File fd, my_off_t newlength, myf MyFlags)
  {
    DBUG_ENTER("my_raid_chsize");
    DBUG_PRINT("enter",("Fd: %d  newlength: %u  MyFlags: %d",
		      fd, newlength, MyFlags));
   if (is_raid(fd))
   {
     RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,fd,RaidFd**));
     DBUG_RETURN(raid->Chsize(fd, newlength, MyFlags));
   }
   else
     DBUG_RETURN(my_chsize(fd, newlength, MyFlags));
  }

  int my_raid_rename(const char *from, const char *to,
		     uint raid_chunks, myf MyFlags)
  {
    char from_tmp[FN_REFLEN];
    char to_tmp[FN_REFLEN];
    DBUG_ENTER("my_raid_rename");

    uint from_pos = dirname_length(from);
    uint to_pos   = dirname_length(to);
    memcpy(from_tmp, from, from_pos);
    memcpy(to_tmp, to, to_pos);
    for (uint i = 0 ; i < raid_chunks ; i++ )
    {
      sprintf(from_tmp+from_pos,"%02x/%s", i, from + from_pos);
      sprintf(to_tmp+to_pos,"%02x/%s", i, to+ to_pos);
      /* Convert if not unix */
      unpack_filename(from_tmp, from_tmp);
      unpack_filename(to_tmp,to_tmp);
      if (my_rename(from_tmp, to_tmp, MyFlags))
	DBUG_RETURN(-1);
    }
    DBUG_RETURN(0);
  }

  int my_raid_delete(const char *from, uint raid_chunks, myf MyFlags)
  {
    char from_tmp[FN_REFLEN];
    uint from_pos = dirname_length(from);
    DBUG_ENTER("my_raid_delete");

    if (!raid_chunks)
      DBUG_RETURN(my_delete(from,MyFlags));
    for (uint i = 0 ; i < raid_chunks ; i++ )
    {
      memcpy(from_tmp, from, from_pos);
      sprintf(from_tmp+from_pos,"%02x/%s", i, from + from_pos);
      /* Convert if not unix */
      unpack_filename(from_tmp, from_tmp);
      if (my_delete(from_tmp, MyFlags))
	DBUG_RETURN(-1);
    }
    DBUG_RETURN(0);
  }

  int my_raid_redel(const char *old_name, const char *new_name,
		    uint raid_chunks, myf MyFlags)
  {
    char new_name_buff[FN_REFLEN], old_name_buff[FN_REFLEN];
    char *new_end, *old_end;
    uint i,old_length,new_length;
    int error=0;
    DBUG_ENTER("my_raid_redel");

    old_end=old_name_buff+dirname_part(old_name_buff,old_name);
    old_length=dirname_length(old_name);
    new_end=new_name_buff+dirname_part(new_name_buff,new_name);
    new_length=dirname_length(new_name);
    for (i=0 ;	i < raid_chunks ; i++)
    {
      MY_STAT status;
      sprintf(new_end,"%02x",i);
      if (my_stat(new_name_buff,&status, MYF(0)))
      {
	DBUG_PRINT("info",("%02x exists, skipping directory creation",i));
      }
      else
      {
	if (my_mkdir(new_name_buff,0777,MYF(0)))
	{
	  DBUG_PRINT("error",("mkdir failed for %02x",i));
	  DBUG_RETURN(-1);
	}
      }
      strxmov(strend(new_end),"/",new_name+new_length,NullS);
      sprintf(old_end,"%02x/%s",i, old_name+old_length);
      if (my_redel(old_name_buff, new_name_buff, MyFlags))
	error=1;
    }
    DBUG_RETURN(error);
  }
}

int my_raid_fstat(int fd, MY_STAT *stat_area, myf MyFlags )
{
  DBUG_ENTER("my_raid_fstat");
  if (is_raid(fd))
  {
    RaidFd *raid= (*dynamic_element(&RaidFd::_raid_map,fd,RaidFd**));
    DBUG_RETURN(raid->Fstat(fd, stat_area, MyFlags));
  }
  else
    DBUG_RETURN(my_fstat(fd, stat_area, MyFlags));
}


/* -------------- RaidFd base class begins ----------------*/
/*
  RaidFd - raided file is identified by file descriptor
  this is useful when we open/write/read/close files
*/


bool RaidFd::
IsRaid(File fd)
{
  DBUG_ENTER("RaidFd::IsRaid");
  DBUG_RETURN((uint) fd < _raid_map.elements &&
	      *dynamic_element(&RaidFd::_raid_map,fd,RaidFd**));
}


RaidFd::
RaidFd(uint raid_type, uint raid_chunks, ulong raid_chunksize)
  :_raid_type(raid_type), _raid_chunks(raid_chunks),
   _raid_chunksize(raid_chunksize), _position(0), _size(RAID_SIZE_UNKNOWN),
   _fd_vector(0)
{
  DBUG_ENTER("RaidFd::RaidFd");
  DBUG_PRINT("enter",("RaidFd_type: %u  Disks: %u  Chunksize: %d",
		   raid_type, raid_chunks, raid_chunksize));

  /* TODO: Here we should add checks if the malloc fails */
  _seek_vector=0;				/* In case of errors */
  my_multi_malloc(MYF(MY_WME),
		  &_seek_vector,sizeof(off_t)*_raid_chunks,
		  &_fd_vector, sizeof(File) *_raid_chunks,
		  NullS);
  if (!RaidFd::_raid_map.buffer)
  {					/* Not initied */
    pthread_mutex_lock(&THR_LOCK_open);	/* Ensure that no other thread */
    if (!RaidFd::_raid_map.buffer)	/* has done init in between */
      init_raid();
    pthread_mutex_unlock(&THR_LOCK_open);
  }
  DBUG_VOID_RETURN;
}


RaidFd::
~RaidFd() {
  DBUG_ENTER("RaidFd::~RaidFd");
  /* We don't have to free _fd_vector ! */
  my_free((char*) _seek_vector, MYF(MY_ALLOW_ZERO_PTR));
  DBUG_VOID_RETURN;
}


File RaidFd::
Create(const char *FileName, int CreateFlags, int access_flags, myf MyFlags)
{
  char RaidFdFileName[FN_REFLEN];
  DBUG_ENTER("RaidFd::Create");
  DBUG_PRINT("enter",
	     ("FileName: %s  CreateFlags: %d  access_flags: %d  MyFlags: %d",
	      FileName, CreateFlags, access_flags, MyFlags));
  char DirName[FN_REFLEN];
  uint pos = dirname_part(DirName, FileName);
  MY_STAT status;
  if (!_seek_vector)
    DBUG_RETURN(-1);				/* Not enough memory */

  uint i = _raid_chunks-1;
  do
  {
    /* Create subdir */
    (void)sprintf(RaidFdFileName,"%s%02x", DirName,i);
    unpack_dirname(RaidFdFileName,RaidFdFileName);   /* Convert if not unix */
    if (my_stat(RaidFdFileName,&status, MYF(0)))
    {
      DBUG_PRINT("info",("%02x exists, skipping directory creation",i));
    }
    else
    {
      if (my_mkdir(RaidFdFileName,0777,MYF(0)))
      {
	DBUG_PRINT("error",("mkdir failed for %d",i));
	goto error;
      }
    }
    /* Create file */
    sprintf(RaidFdFileName,"%s%02x/%s",DirName, i, FileName + pos);
    unpack_filename(RaidFdFileName,RaidFdFileName); /* Convert if not unix */
    _fd = my_create(RaidFdFileName, CreateFlags ,access_flags, (myf)MyFlags);
    if (_fd < 0)
      goto error;
    _fd_vector[i]=_fd;
    _seek_vector[i]=RAID_SEEK_DONE;
  } while (i--);
  _size=0;
  DBUG_RETURN(_fd);			/* Last filenr is pointer to map */

error:
  {
    int save_errno=my_errno;
    while (++i < _raid_chunks)
    {
      my_close(_fd_vector[i],MYF(0));
      sprintf(RaidFdFileName,"%s%02x/%s",DirName, i, FileName + pos);
      unpack_filename(RaidFdFileName,RaidFdFileName);
      my_delete(RaidFdFileName,MYF(0));
    }
    my_errno=save_errno;
  }
  DBUG_RETURN(-1);
}


File RaidFd::
Open(const char *FileName, int Flags, myf MyFlags)
{
  DBUG_ENTER("RaidFd::Open");
  DBUG_PRINT("enter",("FileName: %s  Flags: %d  MyFlags: %d",
		   FileName, Flags, MyFlags));
  char DirName[FN_REFLEN];
  uint pos = dirname_part(DirName, FileName);
  if (!_seek_vector)
    DBUG_RETURN(-1);				/* Not enough memory */

  for( uint i = 0 ;  i < _raid_chunks ; i++ )
  {
    char RaidFdFileName[FN_REFLEN];
    sprintf(RaidFdFileName,"%s%02x/%s",DirName, i, FileName + pos);
    unpack_filename(RaidFdFileName,RaidFdFileName); /* Convert if not unix */
    _fd = my_open(RaidFdFileName, Flags, MyFlags);
    if (_fd < 0)
    {
      int save_errno=my_errno;
      while (i-- != 0)
	my_close(_fd_vector[i],MYF(0));
      my_errno=save_errno;
      DBUG_RETURN(_fd);
    }
    _fd_vector[i]=_fd;
    _seek_vector[i]=RAID_SEEK_DONE;
  }
  Seek(0L,MY_SEEK_END,MYF(0)); // Trick. We just need to know, how big the file is
  DBUG_PRINT("info",("MYD file logical size: %llu", _size));
  DBUG_RETURN(_fd);
}


int RaidFd::
Write(const byte *Buffer, uint Count, myf MyFlags)
{
  DBUG_ENTER("RaidFd::Write");
  DBUG_PRINT("enter",("Count: %d  MyFlags: %d",
		      Count, MyFlags));
  const byte *bufptr = Buffer;
  uint res=0, GotBytes, ReadNowCount;

  // Loop until data is written
  do {
    Calculate();
     // Do seeks when neccessary
    if (_seek_vector[_this_block] != RAID_SEEK_DONE)
    {
      if (my_seek(_fd_vector[_this_block], _seek_vector[_this_block],
		  MY_SEEK_SET,
		  MyFlags) == MY_FILEPOS_ERROR)
	DBUG_RETURN(-1);
      _seek_vector[_this_block]=RAID_SEEK_DONE;
    }
    ReadNowCount = min(Count, _remaining_bytes);
    GotBytes = my_write(_fd_vector[_this_block], bufptr, ReadNowCount,
			MyFlags);
    DBUG_PRINT("loop",("Wrote bytes: %d", GotBytes));
    if (GotBytes == MY_FILE_ERROR)
      DBUG_RETURN(-1);
    res+= GotBytes;
    if (MyFlags & (MY_NABP | MY_FNABP))
      GotBytes=ReadNowCount;
    bufptr += GotBytes;
    Count  -= GotBytes;
    _position += GotBytes;
  } while(Count);
  set_if_bigger(_size,_position);
  DBUG_RETURN(res);
}


int RaidFd::
Read(const byte *Buffer, uint Count, myf MyFlags)
{
  DBUG_ENTER("RaidFd::Read");
  DBUG_PRINT("enter",("Count: %d  MyFlags: %d",
		      Count, MyFlags));
  byte *bufptr = (byte *)Buffer;
  uint res= 0, GotBytes, ReadNowCount;

  // Loop until all data is read (Note that Count may be 0)
  while (Count)
  {
    Calculate();
    // Do seek when neccessary
    if (_seek_vector[_this_block] != RAID_SEEK_DONE)
    {
      if (my_seek(_fd_vector[_this_block], _seek_vector[_this_block],
		  MY_SEEK_SET,
		  MyFlags) == MY_FILEPOS_ERROR)
	DBUG_RETURN(-1);
      _seek_vector[_this_block]=RAID_SEEK_DONE;
    }
    // and read
    ReadNowCount = min(Count, _remaining_bytes);
    GotBytes = my_read(_fd_vector[_this_block], bufptr, ReadNowCount,
		       MyFlags & ~(MY_NABP | MY_FNABP));
    DBUG_PRINT("loop",("Got bytes: %u", GotBytes));
    if (GotBytes == MY_FILE_ERROR)
      DBUG_RETURN(-1);
    if (!GotBytes)				// End of file.
    {
      DBUG_RETURN((MyFlags & (MY_NABP | MY_FNABP)) ? -1 : (int) res);
    }
    res+= GotBytes;
    bufptr += GotBytes;
    Count  -= GotBytes;
    _position += GotBytes;
  }
  DBUG_RETURN((MyFlags & (MY_NABP | MY_FNABP)) ? 0 : res);
}


int RaidFd::
Lock(int locktype, my_off_t start, my_off_t length, myf MyFlags)
{
  DBUG_ENTER("RaidFd::Lock");
  DBUG_PRINT("enter",("locktype: %d  start: %lu  length: %lu  MyFlags: %d",
		      locktype, start, length, MyFlags));
  my_off_t bufptr = start;
  // Loop until all data is locked
  while(length)
  {
    Calculate();
    for (uint i = _this_block ; (i < _raid_chunks) && length ; i++ )
    {
       uint ReadNowCount = min(length, _remaining_bytes);
       uint GotBytes = my_lock(_fd_vector[i], locktype, bufptr, ReadNowCount,
			MyFlags);
       if ((int) GotBytes == -1)
	 DBUG_RETURN(-1);
       bufptr += ReadNowCount;
       length  -= ReadNowCount;
       Calculate();
    }
  }
  DBUG_RETURN(0);
}


int RaidFd::
Close(myf MyFlags)
{
  DBUG_ENTER("RaidFd::Close");
  DBUG_PRINT("enter",("MyFlags: %d",
		   MyFlags));
  for (uint i = 0 ; i < _raid_chunks ; ++i )
  {
    int err = my_close(_fd_vector[i], MyFlags);
    if (err != 0)
      DBUG_RETURN(err);
  }
  /* _fd_vector is erased when RaidFd is released */
  DBUG_RETURN(0);
}


my_off_t RaidFd::
Seek(my_off_t pos,int whence,myf MyFlags)
{
  DBUG_ENTER("RaidFd::Seek");
  DBUG_PRINT("enter",("Pos: %lu  Whence: %d  MyFlags: %d",
		   (ulong) pos, whence, MyFlags));
  switch (whence) {
  case MY_SEEK_CUR:
    // FIXME: This is wrong, what is going on there
    // Just I am relied on fact that MySQL 3.23.7 never uses MY_SEEK_CUR
    // for anything else except things like ltell()
    break;
  case MY_SEEK_SET:
    if ( _position != pos) // we can be already in right place
    {
      uint i;
      off_t _rounds;
      _position = pos;
      Calculate();
      _rounds = _total_block / _raid_chunks;	    // INT() assumed
      _rounds*= _raid_chunksize;
      for (i = 0; i < _raid_chunks ; i++ )
	if ( i < _this_block )
	  _seek_vector[i] = _rounds + _raid_chunksize;
	else if ( i == _this_block )
	  _seek_vector[i] = _rounds + _raid_chunksize -_remaining_bytes;
	else					// if ( i > _this_block )
	  _seek_vector[i] = _rounds;
    }
    break;
  case MY_SEEK_END:
    if (_size==RAID_SIZE_UNKNOWN) // We don't know table size yet
    {
      uint i;
      _position = 0;
      for (i = 0; i < _raid_chunks ; i++ )
      {
	my_off_t newpos = my_seek(_fd_vector[i], 0L, MY_SEEK_END, MyFlags);
	if (newpos == MY_FILEPOS_ERROR)
	  DBUG_RETURN (MY_FILEPOS_ERROR);
	_seek_vector[i]=RAID_SEEK_DONE;
	_position += newpos;
      }
      _size=_position;
    }
    else if (_position != _size) // Aren't we also already in the end?
    {
      uint i;
      off_t _rounds;
      _position = _size;
      Calculate();
      _rounds = _total_block / _raid_chunks;	    // INT() assumed
      _rounds*= _raid_chunksize;
      for (i = 0; i < _raid_chunks ; i++ )
	if ( i < _this_block )
	  _seek_vector[i] = _rounds + _raid_chunksize;
	else if ( i == _this_block )
	  _seek_vector[i] = _rounds + _raid_chunksize - _remaining_bytes;
	else					// if ( i > _this_block )
	  _seek_vector[i] = _rounds;
      _position=_size;
    }
  }
  DBUG_RETURN(_position);
}


my_off_t RaidFd::
Tell(myf MyFlags)
{
  DBUG_ENTER("RaidFd::Tell");
  DBUG_PRINT("enter",("MyFlags: %d _position %d",
		   MyFlags,_position));
  DBUG_RETURN(_position);
}

int RaidFd::
Chsize(File fd, my_off_t newlength, myf MyFlags)
{
  DBUG_ENTER("RaidFd::Chsize");
  DBUG_PRINT("enter",("Fd: %d, newlength: %d, MyFlags: %d",
		   fd, newlength,MyFlags));
  _position = newlength;
  Calculate();
  uint _rounds = _total_block / _raid_chunks;	     // INT() assumed
  for (uint i = 0; i < _raid_chunks ; i++ )
  {
    int newpos;
    if ( i < _this_block )
      newpos = my_chsize(_fd_vector[i],
			 _this_block * _raid_chunksize + (_rounds + 1) *
			 _raid_chunksize,
			 MyFlags);
    else if ( i == _this_block )
      newpos = my_chsize(_fd_vector[i],
			 _this_block * _raid_chunksize + _rounds *
			 _raid_chunksize + (newlength % _raid_chunksize),
			 MyFlags);
    else // this means: i > _this_block
      newpos = my_chsize(_fd_vector[i],
			 _this_block * _raid_chunksize + _rounds *
			 _raid_chunksize, MyFlags);
    if (newpos)
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


int RaidFd::
Fstat(int fd, MY_STAT *stat_area, myf MyFlags )
{
  DBUG_ENTER("RaidFd::Fstat");
  DBUG_PRINT("enter",("fd: %d MyFlags: %d",fd,MyFlags));
  uint i;
  int error=0;
  MY_STAT status;
  stat_area->st_size=0;
  stat_area->st_mtime=0;
  stat_area->st_atime=0;
  stat_area->st_ctime=0;

  for(i=0 ; i < _raid_chunks ; i++)
  {
    if (my_fstat(_fd_vector[i],&status,MyFlags))
      error=1;
    stat_area->st_size+=status.st_size;
    set_if_bigger(stat_area->st_mtime,status.st_mtime);
    set_if_bigger(stat_area->st_atime,status.st_atime);
    set_if_bigger(stat_area->st_ctime,status.st_ctime);
  }
  DBUG_RETURN(error);
}

#endif /* defined(USE_RAID) && !defined(MYSQL_CLIENT) */
