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

#include <ndb_global.h>

#include "DebuggerNames.hpp"

#include <BlockNumbers.h>
#include <GlobalSignalNumbers.h>
#include <signaldata/SignalDataPrint.hpp>

static const char *            localSignalNames[MAX_GSN+1];
static SignalDataPrintFunction localPrintFunctions[MAX_GSN+1];
static const char *            localBlockNames[NO_OF_BLOCKS]; 

static
int
initSignalNames(const char * dst[], const GsnName src[], unsigned short len){
  int i;
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
		   const NameFunctionPair src[], 
		   unsigned short len){
  int i;
  for(i = 0; i<=MAX_GSN; i++)
    dst[i] = 0;
  
  for(i = 0; i<len; i++){
    unsigned short gsn = src[i].gsn;
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
  int i;
  for(i = 0; i<NO_OF_BLOCKS; i++)
    dst[i] = 0;

  for(i = 0; i<len; i++){
    const int index = src[i].number - MIN_BLOCK_NO;
    if(index < 0 && index >= NO_OF_BLOCKS || dst[index] != 0){
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
						    SignalDataPrintFunctions, 
						    NO_OF_PRINT_FUNCTIONS);

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
    snprintf(buf, sizeof(buf), "BLOCK#%d", (int)blockNo);
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
