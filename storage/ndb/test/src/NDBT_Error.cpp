/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

/* NDBT_Error.cpp                         */
/* This program deals with error handling */

#include <ndb_global.h>
#include <NdbOut.hpp>
#include <NdbTest.hpp>
#include <NDBT_Error.hpp>
#include <NdbSleep.h>


ErrorData::ErrorData()
{
  errorCountArray = new Uint32[6000];
  resetErrorCounters();

  key_error = false;
  temporary_resource_error = true;
  insufficient_space_error = false;
  node_recovery_error = true;
  overload_error = true;
  timeout_error = true;
  internal_error = true;
  user_error = true;
  application_error = false;
}

ErrorData::~ErrorData()
{
  delete [] errorCountArray;
}


//-------------------------------------------------------------------
// Error Handling routines
//-------------------------------------------------------------------

int ErrorData::handleErrorCommon(const NdbError & error)
{
  int retValue = 1;
  if (error.code > 6000) {
    if (user_error == true) {
      retValue = 0;
    }//if
    return retValue;
  }//if
  errorCountArray[error.code]++;
  switch(error.classification){
  case NdbError::NoDataFound:
  case NdbError::ConstraintViolation:
    if (key_error == true) {
      retValue = 0;
    }//if
    break;
  case NdbError::TemporaryResourceError:
    if (temporary_resource_error == true) {
      retValue = 0;
    }//if
    break;
  case NdbError::InsufficientSpace:
    if (insufficient_space_error == true) {
      retValue = 0;
    }//if
    break;
  case NdbError::NodeRecoveryError:
    if (node_recovery_error == true) {
      retValue = 0;
    }//if
    break;
    
  case NdbError::UnknownResultError:
    if(error.code == 4012){
      retValue = 0;
    }
    if(error.code == 4115){
      retValue = 2;
    }
    if(error.code == 4007 && node_recovery_error == true){
      retValue = 3;
    }
    break;
  case NdbError::OverloadError:
    if (overload_error == true) {
      NdbSleep_MilliSleep(50);
      retValue = 0;
    }//if
    break;
  case NdbError::TimeoutExpired:
    if (timeout_error == true) {
      retValue = 0;
    }//if
    break;
  case NdbError::InternalError:
    if (internal_error == true) {
      retValue = 0;
    }//if
    break;
  case NdbError::ApplicationError:
    if (application_error == true) {
      retValue = 0;
    }//if
    break;
  case NdbError::UserDefinedError:
    if (user_error == true) {
      retValue = 0;
    }//if
    break;
  default:
    break;
  }//switch
  if(error.status == NdbError::TemporaryError)
    retValue = 0;
  
  return retValue;
}//handleErrorCommon()


void ErrorData::printErrorCounters(NdbOut & out) const
{
  int localLoop;
  for (localLoop = 0; localLoop < 6000; localLoop++) {
    int errCount = (int)errorCountArray[localLoop];
    if (errCount > 0) {
      out << "NDBT: ErrorCode = " << localLoop << " occurred ";
      out << errCount << " times" << endl;
    }//if
  }//for
}//printErrorCounters()


void ErrorData::printSettings(NdbOut & out)
{
  out << "Key Errors are ";
  if (key_error == false) {
    out << "disallowed" << endl;
  } else {
    out << "allowed" << endl;
  }//if
  out << "Temporary Resource Errors are ";
  if (temporary_resource_error == false) {
    out << "disallowed" << endl;
  } else {
    out << "allowed" << endl;
  }//if 
  if (internal_error == true) {
    out << "Insufficient Space Errors are ";
  }
  if (insufficient_space_error == false) {
    out << "disallowed" << endl;
  } else {
    out << "allowed" << endl;
  }//if
  out << "Node Recovery Errors are ";
  if (node_recovery_error == false) {
    out << "disallowed" << endl;
  } else {
    out << "allowed" << endl;
  }//if
  out << "Overload Errors are ";
  if (overload_error == false) {
    out << "disallowed" << endl;
  } else {
    out << "allowed" << endl;
  }//if
  out << "Timeout Errors are ";
  if (timeout_error == false) {
    out << "disallowed" << endl;
  } else {
    out << "allowed" << endl;
  }//if
  out << "Internal NDB Errors are ";
  if (internal_error == false) {
    out << "disallowed" << endl;
  } else {
    out << "allowed" << endl;
  }//if
  out << "User logic reported Errors are ";
  if (user_error == false) {
    out << "disallowed" << endl;
  } else {
    out << "allowed" << endl;
  }//if
  out << "Application Errors are ";
  if (application_error == false) {
    out << "disallowed" << endl;
  } else {
    out << "allowed" << endl;
  }//if
}//printSettings


void ErrorData::printCmdLineArgs(NdbOut & out)
{
  out << "   -key_err          Allow key errors" << endl;
  out << "   -no_key_err       Disallow key errors (default)" << endl;
  out << "   -temp_res_err     Allow temporary resource errors (default)";
  out << endl;
  out << "   -no_temp_res_err  Disallow temporary resource errors" << endl;
  out << "   -ins_space_err    Allow insufficient space errors" << endl;
  out << "   -no_ins_space_err Disallow insufficient space errors (default)";
  out << endl;
  out << "   -noderec_err      Allow Node Recovery errors (default)" << endl;
  out << "   -no_noderec_err   Disallow Node Recovery errors" << endl;
  out << "   -overload_err     Allow Overload errors (default)" << endl;
  out << "   -no_overload_err  Disallow Overload errors" << endl;
  out << "   -timeout_err      Allow Time-out errors (default)" << endl;
  out << "   -no_timeout_err   Disallow Time-out errors" << endl;
  out << "   -internal_err     Allow Internal NDB errors" << endl;
  out << "   -no_internal_err  Disallow Internal NDB errors (default)";
  out << "   -user_err         Allow user logic reported errors (default)";
  out << endl;
  out << "   -no_user_err      Disallow user logic reported errors";
  out << endl;

}//printCmdLineArgs()


bool ErrorData::parseCmdLineArg(char** argv, int & i)
{
  bool ret_Value = true;
  if (strcmp(argv[i], "-key_err") == 0){
    key_error = true;
  } else if (strcmp(argv[i], "-no_key_err") == 0){
    key_error = false;
  } else if (strcmp(argv[i], "-temp_res_err") == 0){
    temporary_resource_error = true;
  } else if (strcmp(argv[i], "-no_temp_res_err") == 0){
    temporary_resource_error = false;
  } else if (strcmp(argv[i], "-ins_space_err") == 0){
    insufficient_space_error = true;
  } else if (strcmp(argv[i], "-no_ins_space_err") == 0){
    insufficient_space_error = false;
  } else if (strcmp(argv[i], "-noderec_err") == 0){
    node_recovery_error = true;
  } else if (strcmp(argv[i], "-no_noderec_err") == 0){
    node_recovery_error = false;
  } else if (strcmp(argv[i], "-overload_err") == 0){
    overload_error = true;
  } else if (strcmp(argv[i], "-no_overload_err") == 0){
    overload_error = false;
  } else if (strcmp(argv[i], "-timeout_err") == 0){
    timeout_error = true;
  } else if (strcmp(argv[i], "-no_timeout_err") == 0){
    timeout_error = false;
  } else if (strcmp(argv[i], "-internal_err") == 0){
    internal_error = true;
  } else if (strcmp(argv[i], "-no_internal_err") == 0){
    internal_error = false;
  } else if (strcmp(argv[i], "-user_err") == 0){
    user_error = true;
  } else if (strcmp(argv[i], "-no_user_err") == 0){
    user_error = false;
  } else {
    ret_Value = false;
  }//if
  return ret_Value;
}//bool parseCmdline

void ErrorData::resetErrorCounters()
{
  for (int i = 0; i < 6000; i++){
    errorCountArray[i] = 0 ;
  }
}



