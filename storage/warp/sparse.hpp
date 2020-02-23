/* Copyright (C)  2020 Justin Swanhart

	 This program is free software; you can redistribute it and/or modify
	 it under the terms of the GNU General Public License version 2.0 as
	 published by the Free  Software Foundation.

	 This program is distributed in the hope that  it will be useful, but
	 WITHOUT ANY WARRANTY; without even  the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	 General Public License version 2.0 for more details.

	 You should have received a  copy of the GNU General Public License
	 version 2.0  along with this  program; if not, write to the Free
	 Software Foundation,  Inc., 59 Temple Place, Suite 330, Boston, MA
	 02111-1307 USA  */

#ifndef WARP_SPARSE_HEADER
#define WARP_SPARSE_HEADER
#include <stdio.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MODE_SET 1
#define MODE_UNSET 0
// 64 bits
#define BLOCK_SIZE 8
#define MAX_BITS 64
#ifdef WARP_DEBUG
#define dbug(x) std::cerr << __LINE__ << ": " << x << "\n";
#else
#define dbug(x) /*x*/
#endif


class sparsebitmap
{
private:
  int debug_flag;
  int line;
  int dirty;
  int have_lock=LOCK_UN;
  inline void debug(std::string message) {
    if(debug_flag) {
      std::cerr << line << ":" << message << "\n";
    }
  } 

public:
  bool is_dirty() {
    return dirty == 1;
  }

private:

  /* Index locking */ 
  void unlock() { 
    dbug("unlock");
    if(have_lock == LOCK_UN) return;
    flock(fileno(fp), LOCK_UN); 
    have_lock = LOCK_UN; 
  }

  void lock(int mode) {
    dbug("lock");
    if(have_lock == LOCK_EX || have_lock == mode) return;
    flock(fileno(fp), mode);
    have_lock = mode;
  }

  /* bits at current filepointer location */
  unsigned long long bits; 

  /* File pointers for data and log */
  FILE *fp; 
  FILE *log; 

  /* type of rwlock held by index */
  int lock_type; 

  /* filenames of the index and the log */
  std::string fname;
  std::string lname;

  /* flag to indicate that recovery is going on */
  int recovering; 

  /* check for file existance */
  bool exists(std::string filename) {
    dbug("exists");
    struct stat buf;
    return !stat(filename.c_str(), &buf);
  }


  /* replay the changes in the log, in either redo(MODE_SET) or undo(MODE_UNSET) */
  int replay (int mode=MODE_UNSET) {
    dbug("replay");
    fseek(log,0,SEEK_SET);
    unsigned long long bitnum;
    while(!feof(log)) {
      bitnum = 0;
      int sz = fread(&bitnum,1,sizeof(bitnum), log);
      if(sz == 0) break;
      set_bit(bitnum, mode);
    }
    fsync(fileno(fp));
    
    return 0;
  }

  /* detect the commit marker ($) at the end of the file.
   * if the marker is found, then replay all the changes
   * in the log, marking the entries in the log as set.
   * otherwise, roll the changes back, unsetting the bits
   *
   * return: 
   * -1 - error
   *  0 - no recovery needed
   *  1 - recovery completed
   */ 
  int do_recovery() {
    dbug("do_recovery");
    recovering = 0;
    struct stat buf;
    log = NULL;
    //int orig_lock = lock_type;

    /* no lock has to be taken here */
    /* if log does not exist, no recovery needed*/
    int exists = !stat(lname.c_str(), &buf);
    if(!exists) {
      dirty = 0;
      return 0;
    } 

    /* lock the index */
    lock(LOCK_EX);

    /* avoid race: someone else may have recovered the log
     * if the log does not exist after obtaining the write
     * lock 
     */
    exists = !stat(lname.c_str(), &buf);
    if(!exists) {
      dirty = 0;
      return 0;
    } 

    recovering = 1;

    /* open the log if it is not open */
    if(!log) {
      log = fopen(lname.c_str(), "r+");
    }

    /* FIXME: 
     * recovery failed at this point
     * */ 
    if(!log) { 
      recovering = 0; 
      dirty=0;
      return -1; 
    }

    // start recovery
    int mode=MODE_SET;
    fseek(log,-BLOCK_SIZE,SEEK_END);
    unsigned long long marker;
    fread(&marker, BLOCK_SIZE, 1, log);
    if(marker!=0) {
      mode = MODE_UNSET;
    }
    fseek(log,0,SEEK_SET);

    // roll forward or roll back the changes
    int res = replay(mode);
    if(res) {
      /* FIXME:
       * recovery failed at this point
       * */ 
      fclose(log);
      recovering = 0;
      return res;
    }

    close(1);
    recovering = 0;
    dirty = 0;
    return 1;
  }

public:
	sparsebitmap(std::string filename,int lock_mode = LOCK_SH,int verbose = 0) {
    dbug("construct");
    fp = NULL; 
    log = NULL;
    bits = 0;
    debug_flag = verbose;
    open(filename, lock_mode);
  };

	~sparsebitmap() {
    unlock();
    if(fp){ fsync(fileno(fp)); fclose(fp); }
    if(log) { fsync(fileno(log)); fclose(log); }
  };

  /* -1 means already open */
  /* -2 means could not create*/
  /* -3 means could not open*/
  int open(std::string filename, int lock_mode = LOCK_SH) {
    dbug("open");
    close();
    bits = 0;
    int skip_recovery = 0;
    
    fname = filename;
    lname = filename + ".txlog";

    reopen:
    /* open and/or create the file */
    fp = fopen(filename.c_str(),"r+");
    if(!fp) { 
      fp = fopen(filename.c_str(),"w");
      unlink(lname.c_str());
    }
    if(!fp) return -2;    
    fclose(fp);
    fp = fopen(filename.c_str(),"r+");
    if(!fp) return -3;    


    /* if commit marker is in log, replay the log, otherwise
     * undo changes from the log
     */
    if(!skip_recovery && do_recovery() == 1) {
      skip_recovery = 1; 
      goto reopen;
    }
   
    lock(lock_mode);

    /* open the log */ 
    if(lock_mode == LOCK_EX) {
      log = fopen(lname.c_str(),"w");
    }

    return 0;
  }

  /* close the index */
  int close(int unlink_log = 0) {
    dbug("close");
    /* this will UNDO all the changes made to the index because commit() was not called*/
    if(!recovering && dirty) do_recovery();

    if(fp) fsync(fileno(fp));
    if(log) fsync(fileno(log));
    if(unlink_log) unlink(lname.c_str());
    unlock();
    if(fp) fclose(fp);
    if(log) fclose(log);

    log = NULL;
    fp = NULL;

    /* release the lock held on the index*/

    return 0;
  } 


  /* sync the changes to disk, first the log
   * and then the data file */
  int commit() {
    dbug("commit");
    int zero=0;
    int sz = fwrite(&zero, BLOCK_SIZE, 1, log);
    fsync(fileno(log));
    fsync(fileno(fp));
    close(1);
    dirty = 0;
    return !(sz == BLOCK_SIZE);
  }

  int rollback() {
    close(1);
    return 0;
  }


  /* check to see if a particular bit is set */
  inline int is_set(unsigned long long bitnum) {
    dbug("is_set");
    if(!fp) open(fname, LOCK_SH);
    lock(LOCK_SH);
    int bit_offset;
    unsigned long long at_byte = (bitnum / MAX_BITS) + ((bit_offset = (bitnum % MAX_BITS)) != 0) - 1;
    if(at_byte == 0 || at_byte != ftell(fp)) {
      fseek(fp, at_byte, SEEK_SET);
      size_t sz = fread(&bits, 8, 1, fp);
      if(sz == 0 || feof(fp)) return 0;
    }
    return (bits >> bit_offset) & 1; 
  }

  /* set a bit in the index */
  /* 1 = successful write */
  /* -1 = logging failure (no change to index) */
  /* 0 = index read/write failure  */
  inline int set_bit(unsigned long long bitnum, int mode = MODE_SET) {
    dbug("set_bit");
    if(!fp || have_lock != LOCK_EX) open(fname, LOCK_EX);
    bool force_read = false;
    if(dirty == 0) {
      force_read = true;
    }
    dirty = 1;
    // write-ahead-log (only write when not in recovery mode) 
    // sz2 will be <1 on write error
    size_t sz=0;
    if(!recovering) sz = fwrite(&bitnum, 1, 8, log);
    if(!recovering && sz != 8) return -1;

    if(lock_type != LOCK_EX) lock(LOCK_EX);

    /* which bit in the unsigned long long is to be set */
    int bit_offset;

    /* where to read at in file */
    long int at_byte = (bitnum / MAX_BITS) + ((bit_offset = (bitnum % MAX_BITS)) != 0) - 1;

    /* read the bits into memory */
    if(force_read || at_byte != ftell(fp)) {
      fseek(fp, at_byte, SEEK_SET);
      sz = fread(&bits, BLOCK_SIZE, 1, fp);
      if(ferror(fp)) return 0;
      if(sz == 0 || feof(fp)) bits = 0;
    }
    
    if(mode == MODE_SET)
      bits |= 1 << bit_offset; 
    else
      bits &= ~(1 << bit_offset);

    fseek(fp, at_byte, SEEK_SET);
    sz = fwrite(&bits, BLOCK_SIZE, 1, fp);
    /* position back so that we don't read the block in again*/
    fseek(fp, at_byte, SEEK_SET); 
    return sz == 1;
  }


};
#endif
