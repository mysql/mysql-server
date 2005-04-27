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

#include <GrepError.hpp>

/**
 * Error descriptions.
 */

const GrepError::ErrorDescription  GrepError::errorDescriptions[] = {
  { GrepError::GE_NO_ERROR,
    "No error" },
  { GrepError::SUBSCRIPTION_ID_NOMEM, 
    "Not enough resources to allocate the subscription" },
  { GrepError::SUBSCRIPTION_ID_NOT_FOUND,
    "The requested subscription (id, key) does not exist"},
  { GrepError::SUBSCRIPTION_ID_NOT_UNIQUE,
    "A subscription with (id, key) does already exist"},
  { GrepError::SUBSCRIPTION_ID_SUMA_FAILED_CREATE,
    "Suma failed to create a new subscription id"},
  { GrepError::NULL_VALUE,
    "NULL"},
  { GrepError::SEQUENCE_ERROR,
    "Error when creating or using sequence."},
  { GrepError::NOSPACE_IN_POOL,
    "No space left in pool when trying to seize data"},
  { GrepError::SUBSCRIPTION_ID_ALREADY_EXIST,
    "A subscription for this replication channel does already exist"},
  { GrepError::SUBSCRIPTION_NOT_STARTED,
    "No subscription is started"},
  { GrepError::SUBSCRIBER_NOT_FOUND,
    "The subscriber does not exist in SUMA."},
  { GrepError::WRONG_NO_OF_SECTIONS,
    "Something is wrong with the supplied arguments"},
  { GrepError::ILLEGAL_ACTION_WHEN_STOPPING,
    "Action can not be performed while channel is in stopping state"},
  { GrepError::SELECTED_TABLE_NOT_FOUND, 
    "The selected table was not found. "},
  { GrepError::REP_APPLY_LOGRECORD_FAILED,
    "Failed applying a log record (permanent error)"},
  { GrepError::REP_APPLY_METARECORD_FAILED,
    "Failed applying a meta record (permanent error)"},
  { GrepError::REP_DELETE_NEGATIVE_EPOCH,
    "Trying to delete a GCI Buffer using a negative epoch."},
  { GrepError::REP_DELETE_NONEXISTING_EPOCH,
    "Trying to delete a non-existing GCI Buffer."},
  { GrepError::REP_NO_CONNECTED_NODES,
    "There are no connected nodes in the node group."},
  { GrepError::REP_DISCONNECT,
    "Global Replication Server disconnected."},  
  { GrepError::COULD_NOT_ALLOCATE_MEM_FOR_SIGNAL,
    "Could not allocate memory for signal."},
  { GrepError::REP_NOT_PROPER_TABLE,
    "Specified table is not a valid table. "
    "Either the format is not <db>/<schema>/<tablename> or "
    "the table name is too long "},
  { GrepError::REP_TABLE_ALREADY_SELECTED,
    "The specified table is already selected for replication" },
  { GrepError::REP_TABLE_NOT_FOUND,
    "The specified table was not found" },
  { GrepError::START_OF_COMPONENT_IN_WRONG_STATE,
    "Component or protocol can not be started in the current state."},
  { GrepError::START_ALREADY_IN_PROGRESS,
    "Start of replication protocol is already in progress."},
  { GrepError::ILLEGAL_STOP_EPOCH_ID,
    "It is not possible to stop on the requested epoch id."},
  { GrepError::ILLEGAL_USE_OF_COMMAND,
    "The command cannot be executed in this state."},
  { GrepError::CHANNEL_NOT_STOPPABLE,
    "It is not possible to stop the in this state."},

  /**
   * Applier stuff
   */
  { GrepError::REP_APPLY_NONCOMPLETE_GCIBUFFER,
    "Applier: Ordered to apply an incomplete GCI Buffer."},
  { GrepError::REP_APPLY_NULL_GCIBUFFER,
    "Applier: Tried to apply a NULL GCI Buffer."},
  { GrepError::REP_APPLIER_START_TRANSACTION,
    "Applier: Could not start a transaction."},
  { GrepError::REP_APPLIER_NO_TABLE, 
    "Applier: Table does not exist"},
  { GrepError::REP_APPLIER_NO_OPERATION, 
    "Applier: Cannot get NdbOperation record."},
  { GrepError::REP_APPLIER_EXECUTE_TRANSACTION, 
    "Applier: Execute transaction failed."},
  { GrepError::REP_APPLIER_CREATE_TABLE,
    "Applier: Create table failed."},
  { GrepError::REP_APPLIER_PREPARE_TABLE,
    "Applier: Prepare table for create failed."},

  { GrepError::NOT_YET_IMPLEMENTED,
    "Command or event not yet implemented."}
};





const Uint32 
GrepError::noOfErrorDescs = sizeof(GrepError::errorDescriptions) /
                            sizeof(GrepError::ErrorDescription);


/**
 * gets the corresponding error message to an err code
 */  
const char * 
GrepError::getErrorDesc(GrepError::GE_Code err) {
  
  for(Uint32 i = 0; i<noOfErrorDescs; i++){
    if(err == errorDescriptions[i].errCode){
      return errorDescriptions[i].name;
    }
  }
  return 0;
}



