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

#include "Channel.hpp"

Channel::Channel() 
{
  reset();
}

Channel::~Channel() 
{
  /**
   * Destroy list of selected tables
   */
  for(Uint32 i=0; i < m_selectedTables.size(); i++) {
    delete m_selectedTables[i];
    m_selectedTables[i] = 0;
  }
  m_selectedTables=0;
}

void
Channel::reset() 
{
  for (Uint32 i=0; i<MAX_NO_OF_NODE_GROUPS; i++) {
    for (Uint32 j=0; j<NO_OF_POSITIONS; j++) {
      state[i][j].set(1,0);
    }
  }
  m_noOfNodeGroups = 0;
  m_requestorEnabled = true;
  m_transferEnabled = true;
  m_applyEnabled = true;
  m_deleteEnabled = true;
  m_autoStartEnabled = false;
  m_stopEpochId = intervalMax;
  setSubKey(0);
  setSubId(0);
  m_stateSub = NO_SUBSCRIPTION_EXISTS;
  m_stateRep = CONSISTENT;
  m_metaScanEpochs = emptyInterval;
  m_dataScanEpochs = emptyInterval;
}

bool 
Channel::requestTransfer(Uint32 nodeGrp, Interval * i)
{
  invariant();
  Interval tmp1, tmp2;

  // i = PS - SSReq - SS - App
  intervalLeftMinus(state[nodeGrp][PS], state[nodeGrp][SSReq], &tmp1);
  intervalLeftMinus(tmp1, state[nodeGrp][SS], &tmp2);
  intervalLeftMinus(tmp2, state[nodeGrp][App], i);
  
  i->onlyLeft(GREP_SYSTEM_TABLE_MAX_RANGE);
  i->onlyUpToValue(m_stopEpochId);
  if (i->isEmpty()) return false;

  add(SSReq, nodeGrp, *i);
  invariant();
  return true;
}

bool 
Channel::requestApply(Uint32 nodeGrp, Uint32 * epoch)
{
  invariant();
  Interval tmp1, tmp2;

  // tmp2 = SS - AppReq - App
  intervalLeftMinus(state[nodeGrp][SS], state[nodeGrp][AppReq], &tmp1);
  intervalLeftMinus(tmp1, state[nodeGrp][App], &tmp2);

  tmp2.onlyUpToValue(m_stopEpochId);
  if (tmp2.isEmpty()) return false;
  tmp2.onlyLeft(1);

  // Check that all GCI Buffers for epoch exists in SS
  for (Uint32 i=0; i<m_noOfNodeGroups; i++) {
    if (!state[nodeGrp][SS].inInterval(tmp2.first()))
      return false;
  }

  invariant();
  add(AppReq, nodeGrp, tmp2);
  invariant();
  *epoch = tmp2.first();
  return true;
}

bool 
Channel::requestDelete(Uint32 nodeGrp, Interval * i)
{
  invariant();
  Interval tmp1;

  // i = (App cut PS) - DelReq
  intervalCut(state[nodeGrp][App], state[nodeGrp][PS], &tmp1);
  intervalLeftMinus(tmp1, state[nodeGrp][DelReq], i);
  
  if (i->isEmpty()) return false;
  i->onlyLeft(GREP_SYSTEM_TABLE_MAX_RANGE);

  invariant();
  add(DelReq, nodeGrp, *i);
  invariant();
  return true;
}

void 
Channel::add(Position pos, Uint32 nodeGrp, const Interval i) 
{
  Interval r;
  intervalAdd(state[nodeGrp][pos], i, &r);
  state[nodeGrp][pos].set(r);
}

void
Channel::clear(Position p, Uint32 nodeGrp, const Interval i)
{
  Interval r;
  intervalLeftMinus(state[nodeGrp][p], i, &r);
  state[nodeGrp][p].set(r);
}

bool
Channel::isSynchable(Uint32 nodeGrp)
{
  return true;
  /*
    @todo This should be implemented...

    Interval tmp1, tmp2;
    intervalAdd(state[nodeGrp][PS], state[nodeGrp][SSReq], &tmp1);
    intervalAdd(tmp1, state[nodeGrp][SSReq], &tmp2);
    intervalAdd(tmp2, state[nodeGrp][SS], &tmp1);
    intervalAdd(tmp1, state[nodeGrp][AppReq], &tmp2);
    intervalAdd(tmp2, state[nodeGrp][App], &tmp1);
    if (intervalInclude(state[nodeGrp][PS], tmp1.right()))
    return true;
    else 
    return false;
  */
}

/**
 * Return the cut of all App:s.
 */
void
Channel::getFullyAppliedEpochs(Interval * interval)
{
  if (m_noOfNodeGroups < 1) {
    *interval = emptyInterval;
    return;
  }

  *interval = universeInterval;
  for (Uint32 i=0; i<m_noOfNodeGroups; i++) {
     if (state[i][App].isEmpty()) {
       *interval = emptyInterval;
      return;
    }
    
     if (interval->first() < state[i][App].first()) {
       interval->setFirst(state[i][App].first());
     }
     if (state[i][App].last() < interval->last()) {
       interval->setLast(state[i][App].last());
     }
  }
  interval->normalize();
  return;
}

/**
 * Return true if it is ok to remove the subscription and then stop channel
 */
bool 
Channel::isStoppable()
{
  /**
   * Check that AppReq are empty for all nodegrps
   */
  for (Uint32 i=0; i<m_noOfNodeGroups; i++) {
    if (!state[i][AppReq].isEmpty()) {
      RLOG(("Stop disallowed. AppReq is non-empty"));
      return false;
    }
  }

  /**
   * If stop immediately, then it is ok to stop now
   */
  if (m_stopEpochId == 0) {
    RLOG(("Stop allowed. AppReq empty and immediate stop requested"));
    return true;
  }

  /**
   * If stop on a certain epoch, then 
   * check that stopEpochId is equal to the last applied epoch
   */
  Interval interval;
  getFullyAppliedEpochs(&interval);
  if (m_stopEpochId > interval.last()) {
    RLOG(("Stop disallowed. AppReq empty. Stop %d, LastApplied %d",
	  m_stopEpochId, interval.last()));
    return false;
  }

  return true;
}

GrepError::Code
Channel::setStopEpochId(Uint32 n) 
{
  /**
   * If n equal to zero, use next possible epoch (max(App, AppReq))
   */
  if (n == 0) {
    for (Uint32 i=0; i<m_noOfNodeGroups; i++) {
      n = (state[i][App].last() > n) ? state[i][App].last() : n;
      n = (state[i][AppReq].last() > n) ? state[i][AppReq].last() : n;
    }
  }

  /**
   *  If n >= max(App, AppReq) then set value, else return error code
   */
  for (Uint32 i=0; i<m_noOfNodeGroups; i++) {
    if (n < state[i][App].last()) return GrepError::ILLEGAL_STOP_EPOCH_ID;
    if (n < state[i][AppReq].last()) return GrepError::ILLEGAL_STOP_EPOCH_ID;
  }

  m_stopEpochId = n;
  return GrepError::NO_ERROR;
};

bool 
Channel::shouldStop() 
{
  /**
   * If (m_stopEpochId == App) then channel should stop
   */
  for (Uint32 i=0; i<m_noOfNodeGroups; i++) {
    if(m_stopEpochId != state[i][App].last()) return false;
  }
  return true;
}

/*****************************************************************************
 * SELECTIVE TABLE INTERFACE
 *****************************************************************************/

GrepError::Code  
Channel::addTable(const char * tableName)
{
  if(strlen(tableName)>MAX_TAB_NAME_SIZE)
    return GrepError::REP_NOT_PROPER_TABLE;
  /**
   * No of separators are the number of table_name_separator found in tableName
   * since a table is defined as <db>/<schema>/tablename.
   * if noOfSeparators is not equal to 2, then it is not a valid
   * table name.
   */
  Uint32 noOfSeps = 0;
  if(strlen(tableName) < 5)
    return GrepError::REP_NOT_PROPER_TABLE;
  for(Uint32 i =0; i < strlen(tableName); i++)
    if(tableName[i]==table_name_separator)
      noOfSeps++;
  if(noOfSeps!=2)
    return GrepError::REP_NOT_PROPER_TABLE;
  table * t= new table(tableName);
  for(Uint32 i=0; i<m_selectedTables.size(); i++) {
    if(strcmp(tableName, m_selectedTables[i]->tableName)==0)
      return GrepError::REP_TABLE_ALREADY_SELECTED;
  }
  m_selectedTables.push_back(t);
  return GrepError::NO_ERROR;
}

GrepError::Code  
Channel::removeTable(const char * tableName)
{
  if(strlen(tableName)>MAX_TAB_NAME_SIZE)
    return GrepError::REP_NOT_PROPER_TABLE;
  /**
   * No of separators are the number of table_name_separator found in tableName
   * since a table is defined as <db>/<schema>/tablename.
   * If noOfSeparators is not equal to 2, 
   * then it is not a valid table name.
   */
  Uint32 noOfSeps = 0;
  if(strlen(tableName) < 5)
    return GrepError::REP_NOT_PROPER_TABLE;
  for(Uint32 i =0; i < strlen(tableName); i++)
    if(tableName[i]==table_name_separator)
      noOfSeps++;
  if(noOfSeps!=2)
    return GrepError::REP_NOT_PROPER_TABLE;
  for(Uint32 i=0; i<m_selectedTables.size(); i++) {
    if(strcmp(tableName, m_selectedTables[i]->tableName)==0) {
      delete m_selectedTables[i];
      m_selectedTables.erase(i);
      return GrepError::NO_ERROR;
    }
  }
  return GrepError::REP_TABLE_NOT_FOUND;
}

void 
Channel::printTables() 
{
  if(m_selectedTables.size() == 0)
    ndbout_c("|   ALL TABLES                                        "
	     "                    |");
  else {
    for(Uint32 i=0; i<m_selectedTables.size(); i++)
      ndbout_c("|   %-69s |", m_selectedTables[i]->tableName);
  }
}

Vector<struct table *> *  
Channel::getSelectedTables() 
{
  if(m_selectedTables.size() == 0) return 0;
  return &m_selectedTables;
}

/*****************************************************************************
 * PRINT
 *****************************************************************************/

void 
Channel::print(Position pos) 
{
  switch(pos){
  case PS:      ndbout << "PS Rep"; break;
  case SSReq:   ndbout << "Tra-Req"; break;
  case SS:      ndbout << "SS Rep"; break;
  case AppReq:  ndbout << "App-Req"; break;
  case App:     ndbout << "Applied"; break;
  case DelReq:  ndbout << "Del-Req"; break;
  default:      REPABORT("Unknown replication position");
  }
}

void 
Channel::print() 
{
  for (Uint32 i=0; i<m_noOfNodeGroups; i++) {
    print(i); 
  }
}

void 
Channel::print(Position pos, Uint32 nodeGrp) 
{
  print(pos); 
  if (state[nodeGrp][pos].first() == 1 && state[nodeGrp][pos].last() == 0) {
    ndbout << " EMPTY";
  } else {
    ndbout << " [" << state[nodeGrp][pos].first() << "-" 
	   << state[nodeGrp][pos].last() << "]";
  }
}

static const char* 
channelline =
"+-------------------------------------------------------------------------+\n"
;

void
Channel::getEpochState(Position p, 
		       Uint32 nodeGrp, 
		       Uint32 * first,
		       Uint32 * last) {
  if(state[nodeGrp][p].isEmpty()) {
    *first = 1;
    *last  = 0;
    return;
  }  
  *first = state[nodeGrp][p].first();
  *last  = state[nodeGrp][p].last();
}


void 
Channel::print(Uint32 nodeGrp) 
{
  ndbout << channelline;
  ndbout_c("|                        |       Meta scan       |"
	   "        Data scan       |");
  ndbout.print("|                        ");
  if (m_metaScanEpochs.isEmpty()) {
    ndbout.print("|                       ");
  } else {
    ndbout.print("| %10u-%-10u ",
		 m_metaScanEpochs.first(), m_metaScanEpochs.last());
  }
  if (m_dataScanEpochs.isEmpty()) {
    ndbout_c("|                        |");
  } else {
    ndbout_c("|  %10u-%-10u |",
	     m_dataScanEpochs.first(), m_dataScanEpochs.last());
  }

  /* --- */

  ndbout << channelline;
  ndbout_c("|    Source Rep Server   |    Being Transfered   |"
	   " Destination Rep Server |");
  if (state[nodeGrp][PS].isEmpty()) {
    ndbout.print("|                        ");
  } else {
    ndbout.print("|  %10u-%-10u ",
		 state[nodeGrp][PS].first(), state[nodeGrp][PS].last());
  }
  if (state[nodeGrp][SSReq].isEmpty()) {
    ndbout.print("|                       ");
  } else {
    ndbout.print("| %10u-%-10u ",
		 state[nodeGrp][SSReq].first(), state[nodeGrp][SSReq].last());
  }
  if (state[nodeGrp][SS].isEmpty()) {
    ndbout_c("|                        |");
  } else {
    ndbout_c("|  %10u-%-10u |",
	     state[nodeGrp][SS].first(), state[nodeGrp][SS].last());
  }

  /* --- */

  ndbout << channelline;
  ndbout_c("|      Being Applied     |        Applied        |"
	   "      Being Deleted     |");
  if (state[nodeGrp][AppReq].isEmpty()) {
    ndbout.print("|                        ");
  } else {
    ndbout.print("|  %10u-%-10u ", state[nodeGrp][AppReq].first(), 
		 state[nodeGrp][AppReq].last());
  }
  if (state[nodeGrp][App].isEmpty()) {
    ndbout.print("|                       ");
  } else {
    ndbout.print("| %10u-%-10u ",
		 state[nodeGrp][App].first(), state[nodeGrp][App].last());
  }
  if (state[nodeGrp][DelReq].isEmpty()) {
    ndbout_c("|                        |");
  } else {
    ndbout_c("|  %10u-%-10u |",
	     state[nodeGrp][DelReq].first(), state[nodeGrp][DelReq].last());
  }
}

/*****************************************************************************
 * Private Methods
 *****************************************************************************/

void
Channel::invariant() 
{
  for (Uint32 j=0; j<MAX_NO_OF_NODE_GROUPS; j++) 
  {
    if (!intervalDisjoint(state[j][SSReq], state[j][SS]))
      REPABORT("Invariant 1 violated");
    if (!intervalDisjoint(state[j][AppReq], state[j][App]))
      REPABORT("Invariant 2 violated");
  }
}
