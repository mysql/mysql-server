/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>
#include <BaseString.hpp>

#include "DebuggerNames.hpp"

#include <BlockNumbers.h>
#include <BlockNames.hpp>
#include <GlobalSignalNumbers.h>
#include <signaldata/SignalDataPrint.hpp>

static const char *            localSignalNames[MAX_GSN+1];
static SignalDataPrintFunction localPrintFunctions[MAX_GSN+1];
static const char *            localBlockNames[NO_OF_BLOCKS]; 

static
int
initSignalNames(const char * dst[], const GsnName src[], unsigned short len){
  unsigned i;
  for(i = 0; i<=MAX_GSN; i++)
    dst[i] = 0;
  
  for(i = 0; i<len; i++){
    unsigned short gsn = src[i].gsn;
    const char * name  = src[i].name;
    
    if(dst[gsn] != 0 && name != 0){
      if(strcmp(dst[gsn], name) != 0){
	fprintf(stderr, 
		"Multiple definition of signal name for gsn: %d (%s, %s)\n", 
		gsn, dst[gsn], name);
	exit(0);
      }
    }
    dst[gsn] = name;
  }
  return 0;
}

static
int
initSignalPrinters(SignalDataPrintFunction dst[], 
		   const NameFunctionPair src[]){
  unsigned i;
  for(i = 0; i<=MAX_GSN; i++)
    dst[i] = 0;
  
  unsigned short gsn;
  for(i = 0; (gsn = src[i].gsn) > 0; i++){
    SignalDataPrintFunction fun = src[i].function;
    
    if(dst[gsn] != 0 && fun != 0){
      if(dst[gsn] != fun){
	fprintf(stderr, 
		"Multiple definition of signal print function for gsn: %d\n", 
		gsn);
	exit(0);
      }
    }
    dst[gsn] = fun;
  }
  return 0;
}

static
int
initBlockNames(const char * dst[],
	       const BlockName src[],
	       unsigned len){
  unsigned i;
  for(i = 0; i<NO_OF_BLOCKS; i++)
    dst[i] = 0;

  for(i = 0; i<len; i++){
    const int index = src[i].number - MIN_BLOCK_NO;
    if(index < 0 ||             // Too small
       index >= NO_OF_BLOCKS || // Too large
       dst[index] != 0)         // Already occupied
    {
      fprintf(stderr, 
	      "Invalid block name definition: %d %s\n",
	      src[i].number, src[i].name);
      exit(0);
    }
    dst[index] = src[i].name;
  }
  return 0;
}

/**
 * Run static initializer
 */
static const int 
xxx_DUMMY_SIGNAL_NAMES_xxx = initSignalNames(localSignalNames, 
					     SignalNames, 
					     NO_OF_SIGNAL_NAMES);
static const int 
xxx_DUMMY_PRINT_FUNCTIONS_xxx  = initSignalPrinters(localPrintFunctions, 
						    SignalDataPrintFunctions);

static const int
xxx_DUMMY_BLOCK_NAMES_xxx = initBlockNames(localBlockNames,
					   BlockNames,
					   NO_OF_BLOCK_NAMES);

const char * 
getSignalName(unsigned short gsn, const char * defVal){
  if(gsn > 0 && gsn <= MAX_GSN)
    return (localSignalNames[gsn] ? localSignalNames[gsn] : defVal);
  return defVal;
}

unsigned short
getGsn(const char * signalName){
  return 0;
}

const char * 
getBlockName(unsigned short blockNo, const char * ret){
  if(blockNo >= MIN_BLOCK_NO && blockNo <= MAX_BLOCK_NO)
    return localBlockNames[blockNo-MIN_BLOCK_NO];
  if (ret == 0) {
    static char buf[20];
    BaseString::snprintf(buf, sizeof(buf), "BLOCK#%d", (int)blockNo);
    return buf;
  }
  return ret;
}  

unsigned short
getBlockNo(const char * blockName){
  for(int i = 0; i<NO_OF_BLOCKS; i++)
    if(localBlockNames[i] != 0 && strcmp(localBlockNames[i], blockName) == 0)
      return i + MIN_BLOCK_NO;
  return 0;
}

SignalDataPrintFunction
findPrintFunction(unsigned short gsn){
  if(gsn > 0 && gsn <= MAX_GSN)
    return localPrintFunctions[gsn];
  return 0;
}
