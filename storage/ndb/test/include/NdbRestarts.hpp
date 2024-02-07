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

#ifndef NDBT_RESTARTS_HPP
#define NDBT_RESTARTS_HPP

#include <NdbTick.h>
#include <random.h>
#include <NdbRestarter.hpp>

/**
 * This class is used to test Ndb's ability to handle
 * node- and system-restarts.
 * For example:
 *  Node restart:         Restart one node in the cluster.
 *  System restart:       Restart all nodes in the cluster.
 *  Node crash:           Crash one node in the middle of execution and bring
                          it up again.
 *  Multiple node crash:  Crash multiple nodes with a few seconds or
                          milliseconds delay between.
 *  Initial node restart: Restart one node in the cluster without a filesystem
                          on disk.
 *
 * Each restart type is represented by a NdbRestart class and a collection
 * of these are stored in the NdbRestarts class.
 *
 * This class may be used from other programs to execute a particular restart.
 *
 */

class NDBT_Context;

class NdbRestarts {
 public:
  NdbRestarts(const char *_addr = 0) : m_restarter(_addr) {
    myRandom48Init((long)NdbTick_CurrentMillisecond());
  }

  enum NdbRestartType { NODE_RESTART, MULTIPLE_NODE_RESTART, SYSTEM_RESTART };

  struct NdbRestart {
    typedef int(restartFunc)(NDBT_Context *, NdbRestarter &, const NdbRestart *,
                             int safety);

    NdbRestart(const char *_name, NdbRestartType _type, restartFunc *_func,
               int _requiredNodes, int _requiredNodeGroups);

    const char *m_name;
    NdbRestartType m_type;
    restartFunc *m_restartFunc;
    int m_numRequiredNodes;
    int m_numRequiredNodeGroups;
  };

  int getNumRestarts();

  int executeRestart(NDBT_Context *, int _num, unsigned int _to = 120,
                     int safety = 0);
  int executeRestart(NDBT_Context *, const char *_name, unsigned int _to = 120,
                     int safety = 0);

  void listRestarts();
  void listRestarts(NdbRestartType _type);

 private:
  int executeRestart(NDBT_Context *, const NdbRestart *, unsigned int _timeout,
                     int safety);

  struct NdbErrorInsert {
    NdbErrorInsert(const char *_name, int _errorNo);

    const char *m_name;
    int m_errorNo;

   public:
    const char *getName();
  };

  int getNumErrorInserts();
  const NdbErrorInsert *getError(int _num);
  const NdbErrorInsert *getRandomError();

  static const NdbErrorInsert m_errors[];
  static const int m_NoOfErrors;

  const NdbRestart *getRestart(int _num);
  const NdbRestart *getRestart(const char *_name);

  static const NdbRestart m_restarts[];
  static const int m_NoOfRestarts;

  NdbRestarter m_restarter;
};

#endif
