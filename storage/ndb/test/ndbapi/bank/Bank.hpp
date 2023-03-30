/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef BANK_HPP
#define BANK_HPP

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>
#include <NdbTick.h>
#include <random.h>


class Bank {
public:  

  Bank(Ndb_cluster_connection&, bool init = true, const char *dbase="BANK");

  void setSkipCreate(bool skip) { m_skip_create = skip; }
  int createAndLoadBank(bool overWrite, bool disk= false, int num_accounts=10);
  int dropBank();
  
  int performTransactions(int maxSleepBetweenTrans = 20, int yield=0);
  int performMakeGLs(int yield=0);
  int performValidateAllGLs();
  int performSumAccounts(int maxSleepBetweenSums = 2000, int yield=0);
  int performIncreaseTime(int maxSleepBetweenDays = 30, int yield=0);
private:

  int init();

  enum TransactionTypes{
    WithDrawal = 2000,
    Deposit = 3000
  };
  
  static const int NOT_ENOUGH_FUNDS = 1000;
  static const int VERIFICATION_FAILED = 1001;  

  int performTransaction();
  int performTransaction(int fromAccountId,
			 int toAccountId,
			 int amount );
  int performTransactionImpl1(int fromAccountId,
			      int toAccountId,
			      int amount );

  int performValidateGLs(Uint64 age = 20);
  int performValidateGL(Uint64 GLTime);
  int performValidatePurged();

  int performMakeGL(Uint64 time);
  int performMakeGLForAccountType(NdbConnection* pTrans, 
				  Uint64 time,
				  Uint32 accountTypeId);
  int sumTransactionsForGL(const Uint64 time, 
			   const Uint32 accountType,
			   Uint32& balance,
			   Uint32& withdrawalCount,
			   Uint32& withdrawalSum,
			   Uint32& depositSum,
			   Uint32& depositCount,
			   Uint32& transactionsCount,
			   NdbConnection* pTrans);
  int getBalanceForAccountType(const Uint32 accountType,
			       Uint32& balance);
  int getBalanceForGL(const Uint64 glTime,
		      const Uint32 accountType,
		      Uint32 &balance);
    
  int checkNoTransactionsOlderThan(const Uint32 accountType,
				   const Uint64 oldest);
  int getOldestPurgedGL(const Uint32 accountType,
			Uint64 &oldest);
  int getOldestNotPurgedGL(Uint64 &oldest,
			   Uint32 &accountTypeId,
			   bool &found);
  int findLastGL(Uint64 &lastTime);
  int purgeOldGLTransactions(Uint64 currTime, Uint32 age);

  int purgeTransactions(const Uint64 glTime, 
			const Uint32 accountTypeId);
  int findTransactionsToPurge(const Uint64 glTime, 
			     const Uint32 accountType,
			     NdbConnection* pTrans);


  int getSumAccounts(Uint32 &sumAccounts, 
		     Uint32 &numAccounts);
  int getNumAccounts();
  int getNumAccountTypes();
  int getMaxAmount();


  enum SystemValueId {
    LastTransactionId = 0,
    CurrentTime = 1
  };


  int readSystemValue(SystemValueId sysValId, Uint64 & value);
  int increaseSystemValue(SystemValueId sysValId, Uint64 &value);
  int increaseSystemValue2(SystemValueId sysValId, Uint64 &value);
  int writeSystemValue(SystemValueId sysValId, Uint64 value);
  int getNextTransactionId(Uint64 &value);
  int incCurrTime(Uint64 &value);
  int getCurrTime(Uint64 &time);

  int prepareReadSystemValueOp(NdbConnection*, SystemValueId sysValId, Uint64 &time);
  int prepareGetCurrTimeOp(NdbConnection*, Uint64 &time);

  int createTables(bool disk);
  int createTable(const char* tabName, bool disk);

  int dropTables();
  int dropTable(const char* tabName);

  int clearTables();
  int clearTable(const char* tabName);

  int loadGl();
  int loadAccountType();
  int loadAccount (int numAccounts);
  int loadSystemValues();

private:

  Ndb m_ndb;
  int m_maxAccount;
  bool m_initialized;
  bool m_skip_create;
};

#endif
