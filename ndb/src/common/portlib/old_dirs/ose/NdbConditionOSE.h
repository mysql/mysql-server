/* Copyright (C) 2003 MySQL AB

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

#ifndef NDB_CONDITIONOSE_H
#define NDB_CONDITIONOSE_H


#ifdef	__cplusplus
extern "C" {
#endif


#define NDBCOND_SIGBASE  4000
   
#define NDBCOND_WAIT           (NDBCOND_SIGBASE + 1)  /* !-SIGNO(struct NdbCondWait)-! */  
#define NDBCOND_WAITTIMEOUT           (NDBCOND_SIGBASE + 2)  /* !-SIGNO(struct NdbCondWaitTimeOut)-! */  
#define NDBCOND_SIGNAL    (NDBCOND_SIGBASE + 3)  /* !-SIGNO(struct NdbCondSignal)-! */  
#define NDBCOND_BROADCAST    (NDBCOND_SIGBASE + 4)  /* !-SIGNO(struct NdbCondBroadcast)-! */  


const char *
sigNo2String(SIGSELECT sigNo){
  switch(sigNo){
  case NDBCOND_WAIT:
    return "NDBCOND_WAIT";
    break;
  case NDBCOND_WAITTIMEOUT:
    return "NDBCOND_WAITTIMEOUT";
    break;
  case NDBCOND_SIGNAL:
    return "NDBCOND_SIGNAL";
    break;
  case NDBCOND_BROADCAST:
    return "NDBCOND_BROADCAST";
    break;
  }
  return "UNKNOWN";
}

struct NdbCondWait
{
  SIGSELECT sigNo;
  int status;
};

/**
 * Signal received
 */
#define NDBCOND_SIGNALED 1
  
/**
 * Timeout occured
 */
#define NDBCOND_TIMEOUT 2

struct NdbCondWaitTimeout
{
  SIGSELECT sigNo;
  int timeout;
  int status;

};

struct NdbCondSignal
{
  SIGSELECT sigNo;
};

struct NdbCondBroadcast
{
  SIGSELECT sigNo;
};


union SIGNAL 
{
  SIGSELECT sigNo;
  struct NdbCondWait          condWait;
  struct NdbCondWaitTimeout   condWaitTimeout;
  struct NdbCondSignal        condSignal;
  struct NdbCondBroadcast     condBroadcast;
};



#ifdef	__cplusplus
}
#endif

#endif
