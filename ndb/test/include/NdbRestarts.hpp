/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDBT_RESTARTS_HPP
#define NDBT_RESTARTS_HPP

#include <NdbRestarter.hpp>
#include <NdbTick.h>
#include <random.h>

/**
 * This class is used to test Ndb's ability to handle 
 * node- and system-restarts.
 * For example:
 *  Node restart:         Restart one node in the cluster.
 *  System restart:       Restart all nodes in the cluster.
 *  Node crash:           Crash one node in the middle of execution and bring it up again.
 *  Multiple node crash:  Crash multiple nodes with a few seconds or milliseconds delay between.
 *  Initial node restart: Restart one node in the cluster without a filesystem on disk.
 *  
 * Each restart type is represented by a NdbRestart class and a collection of these are stored 
 * in the NdbRestarts class.
 *
 * This class may be used from other programs to execute a particular restart.
 *
 */


class NdbRestarts {
public:
  NdbRestarts(const char* _addr = 0): 
    m_restarter(_addr)
  {
    myRandom48Init(NdbTick_CurrentMillisecond());
  }

  enum NdbRestartType{
    NODE_RESTART,
    MULTIPLE_NODE_RESTART,
    SYSTEM_RESTART
  };

  struct NdbRestart {
    typedef int (restartFunc)(NdbRestarter&, const NdbRestart*);
    
    NdbRestart(const char* _name,
	       NdbRestartType _type,
	       restartFunc* _func,
	       int _requiredNodes,
	       int _arg1 = -1);
	       
    const char * m_name;
    NdbRestartType m_type;
    restartFunc* m_restartFunc;
    int m_numRequiredNodes;
    int m_arg1;

  };

  int getNumRestarts();

  int executeRestart(int _num, unsigned int _timeout = 120);
  int executeRestart(const char* _name, unsigned int _timeout = 120);

  void listRestarts();
  void listRestarts(NdbRestartType _type);
private:
  int executeRestart(const NdbRestart*, unsigned int _timeout);

  struct NdbErrorInsert {
    NdbErrorInsert(const char* _name,
		   int _errorNo);
	       
    const char * m_name;
    int m_errorNo;

  public:
    const char* getName();
  };

  int getNumErrorInserts();
  const NdbErrorInsert* getError(int _num);
  const NdbErrorInsert* getRandomError();

  static const NdbErrorInsert   m_errors[];
  static const int          m_NoOfErrors;

  const NdbRestart* getRestart(int _num);
  const NdbRestart* getRestart(const char* _name);

  static const NdbRestart   m_restarts[];
  static const int          m_NoOfRestarts;

  NdbRestarter m_restarter;
};











#endif
