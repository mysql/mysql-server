/*
Licensed Materials - Property of IBM
DB2 Storage Engine Enablement
Copyright IBM Corporation 2007,2008
All rights reserved

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met: 
 (a) Redistributions of source code must retain this list of conditions, the
     copyright notice in section {d} below, and the disclaimer following this
     list of conditions. 
 (b) Redistributions in binary form must reproduce this list of conditions, the
     copyright notice in section (d) below, and the disclaimer following this
     list of conditions, in the documentation and/or other materials provided
     with the distribution. 
 (c) The name of IBM may not be used to endorse or promote products derived from
     this software without specific prior written permission. 
 (d) The text of the required copyright notice is: 
       Licensed Materials - Property of IBM
       DB2 Storage Engine Enablement 
       Copyright IBM Corporation 2007,2008 
       All rights reserved

THIS SOFTWARE IS PROVIDED BY IBM CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL IBM CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/


#include "db2i_collationSupport.h"
#include "db2i_errors.h"


/*
  The following arrays define a mapping between MySQL collation names and
  corresponding IBM i sort sequences. The mapping is a 1-to-1 correlation 
  between corresponding array slots but is incomplete without case-sensitivity
  markers dynamically added to the mySqlSortSequence names.
*/
#define MAX_COLLATION 87
static const char* mySQLCollation[MAX_COLLATION] = 
{
    {"ascii_general"},
    {"ascii"},
    {"big5_chinese"},
    {"big5"},
    {"cp1250_croatian"},
    {"cp1250_general"},
    {"cp1250_polish"},
    {"cp1250"},
    {"cp1251_bulgarian"},
    {"cp1251_general"},
    {"cp1251"},
    {"cp1256_general"},
    {"cp1256"},
    {"cp850_general"},
    {"cp850"},
    {"cp852_general"},
    {"cp852"},
    {"cp932_japanese"},
    {"cp932"},
    {"euckr_korean"},
    {"euckr"},
    {"gb2312_chinese"},
    {"gb2312"},
    {"gbk_chinese"},
    {"gbk"},
    {"greek_general"},
    {"greek"},
    {"hebrew_general"},
    {"hebrew"},
    {"latin1_danish"},
    {"latin1_general"},
    {"latin1_german1"},
    {"latin1_spanish"},
    {"latin1_swedish"},
    {"latin1"},
    {"latin2_croatian"},
    {"latin2_general"},
    {"latin2_hungarian"},
    {"latin2"},
    {"latin5_turkish"},
    {"latin5"},
    {"macce_general"},
    {"macce"},
    {"sjis_japanese"},
    {"sjis"},
    {"tis620_thai"},
    {"tis620"},
    {"ucs2_czech"},
    {"ucs2_danish"},
    {"ucs2_esperanto"},
    {"ucs2_estonian"},
    {"ucs2_general"},
    {"ucs2_hungarian"},
    {"ucs2_icelandic"},
    {"ucs2_latvian"},
    {"ucs2_lithuanian"},
    {"ucs2_persian"},
    {"ucs2_polish"},
    {"ucs2_romanian"},
    {"ucs2_slovak"},
    {"ucs2_slovenian"},
    {"ucs2_spanish"},
    {"ucs2_swedish"},
    {"ucs2_turkish"},
    {"ucs2_unicode"},
    {"ucs2"},
    {"ujis_japanese"},
    {"ujis"},
    {"utf8_czech"},
    {"utf8_danish"},
    {"utf8_esperanto"},
    {"utf8_estonian"},
    {"utf8_general"},
    {"utf8_hungarian"},
    {"utf8_icelandic"},
    {"utf8_latvian"},
    {"utf8_lithuanian"},
    {"utf8_persian"},
    {"utf8_polish"},
    {"utf8_romanian"},
    {"utf8_slovak"},
    {"utf8_slovenian"},
    {"utf8_spanish"},
    {"utf8_swedish"},
    {"utf8_turkish"},
    {"utf8_unicode"},
    {"utf8"}
};


static const char* mySqlSortSequence[MAX_COLLATION] = 
{
    {"QALA101F4"},  
    {"QBLA101F4"},
    {"QACHT04B0"},  
    {"QBCHT04B0"},
    {"QALA20481"},  
    {"QCLA20481"},
    {"QDLA20481"},
    {"QELA20481"},
    {"QACYR0401"}, 
    {"QBCYR0401"},
    {"QCCYR0401"},  
    {"QAARA01A4"},
    {"QBARA01A4"},  
    {"QCLA101F4"},
    {"QDLA101F4"},  
    {"QALA20366"},
    {"QBLA20366"},  
    {"QAJPN04B0"},
    {"QBJPN04B0"},  
    {"QAKOR04B0"},
    {"QBKOR04B0"},  
    {"QACHS04B0"},  
    {"QBCHS04B0"},  
    {"QCCHS04B0"},  
    {"QDCHS04B0"},  
    {"QAELL036B"},  
    {"QBELL036B"},  
    {"QAHEB01A8"},
    {"QBHEB01A8"},  
    {"QALA1047C"},
    {"QBLA1047C"},  
    {"QCLA1047C"},
    {"QDLA1047C"},
    {"QELA1047C"},
    {"QFLA1047C"},
    {"QCLA20366"},  
    {"QELA20366"},
    {"QFLA20366"},
    {"QGLA20366"},
    {"QATRK0402"},  
    {"QBTRK0402"},
    {"QHLA20366"},  
    {"QILA20366"},
    {"QCJPN04B0"},  
    {"QDJPN04B0"},
    {"QATHA0346"},  
    {"QBTHA0346"},  
    {"ACS_CZ"},        
    {"ADA_DK"},
    {"AEO"},
    {"AET"},
    {"QAUCS04B0"},  
    {"AHU"},
    {"AIS"},
    {"ALV"},
    {"ALT"},
    {"AFA"},
    {"APL"},
    {"ARO"},
    {"ASK"},
    {"ASL"},
    {"AES"},
    {"ASW"},
    {"ATR"},
    {"AEN"},
    {"*HEX"},
    {"QEJPN04B0"},  
    {"QFJPN04B0"},
    {"ACS_CZ"},        
    {"ADA_DK"},
    {"AEO"},
    {"AET"},
    {"QAUCS04B0"},
    {"AHU"},
    {"AIS"},
    {"ALV"},
    {"ALT"},
    {"AFA"},
    {"APL"},
    {"ARO"},
    {"ASK"},
    {"ASL"},
    {"AES"},
    {"ASW"},
    {"ATR"},
    {"AEN"},
    {"*HEX"}
};


/**
  Get the IBM i sort sequence that corresponds to the given MySQL collation.
  
  @param fieldCharSet  The collated character set
  @param[out] rtnSortSequence  The corresponding sort sequence
    
  @return  0 if successful. Failure otherwise
*/
static int32 getAssociatedSortSequence(const CHARSET_INFO *fieldCharSet, const char** rtnSortSequence)
{
  DBUG_ENTER("ha_ibmdb2i::getAssociatedSortSequence");

  if (strcmp(fieldCharSet->csname,"binary") != 0)
  {
    int collationSearchLen = strlen(fieldCharSet->name);
    if (fieldCharSet->state & MY_CS_BINSORT)
      collationSearchLen -= 4;
    else
      collationSearchLen -= 3;
    
    uint16 loopCnt = 0;
    for (loopCnt; loopCnt < MAX_COLLATION; ++loopCnt)
    {
      if ((strlen(mySQLCollation[loopCnt]) == collationSearchLen) && 
          (strncmp((char*)mySQLCollation[loopCnt], fieldCharSet->name, collationSearchLen) == 0))
        break;
    }
    if (loopCnt == MAX_COLLATION)  // Did not find associated sort sequence
    {
      getErrTxt(DB2I_ERR_SRTSEQ);
      DBUG_RETURN(DB2I_ERR_SRTSEQ);
    }
    *rtnSortSequence = mySqlSortSequence[loopCnt];
  }

  DBUG_RETURN(0);
}


/**
  Update sort sequence information for a key.
  
  This function accumulates information about a key as it is called for each
  field composing the key. The caller should invoke the function for each field
  and (with the exception of the charset parm) preserve the values for the 
  parms across invocations, until a particular key has been evaluated. Once
  the last field in the key has been evaluated, the fileSortSequence and 
  fileSortSequenceLibrary parms will contain the correct information for 
  creating the corresponding DB2 key.
  
  @param charset  The character set under consideration
  @param[in, out] fileSortSequenceType  The type of the current key's sort seq
  @param[in, out] fileSortSequence  The IBM i identifier for the DB2 sort sequence
                                    that corresponds 
    
  @return  0 if successful. Failure otherwise
*/
int32 updateAssociatedSortSequence(const CHARSET_INFO* charset, 
                                   char* fileSortSequenceType, 
                                   char* fileSortSequence, 
                                   char* fileSortSequenceLibrary)
{
  DBUG_ENTER("ha_ibmdb2i::updateAssociatedSortSequence");
  DBUG_ASSERT(charset);
  if (strcmp(charset->csname,"binary") != 0)
  {
    char newSortSequence[11] = "";
    char newSortSequenceType = ' ';
    const char* foundSortSequence;
    int rc = getAssociatedSortSequence(charset, &foundSortSequence);
    if (rc) DBUG_RETURN (rc);
    switch(foundSortSequence[0])
    {
      case '*': // Binary
        strcat(newSortSequence,foundSortSequence);
        newSortSequenceType = 'B';
        break;
      case 'Q': // Non-ICU sort sequence
        strcat(newSortSequence,foundSortSequence);
        if ((charset->state & MY_CS_BINSORT) != 0)
        {
          strcat(newSortSequence,"U"); 
        }
        else if ((charset->state & MY_CS_CSSORT) != 0)
        {
          strcat(newSortSequence,"U"); 
        }
        else
        {
          strcat(newSortSequence,"S"); 
        }
        newSortSequenceType = 'N';
        break;
      default: // ICU sort sequence
	{
          if ((charset->state & MY_CS_CSSORT) == 0)
          {
            if (osVersion.v >= 6)
              strcat(newSortSequence,"I34"); // ICU 3.4
            else 
              strcat(newSortSequence,"I26"); // ICU 2.6.1
          }
          strcat(newSortSequence,foundSortSequence);
          newSortSequenceType = 'I';
        }
        break;
    }
    if (*fileSortSequenceType == ' ') // If no sort sequence has been set yet
    {
      // Set associated sort sequence
      strcpy(fileSortSequence,newSortSequence);
      strcpy(fileSortSequenceLibrary,"QSYS");
      *fileSortSequenceType = newSortSequenceType;
    }
    else if (strcmp(fileSortSequence,newSortSequence) != 0)
    {
      // Only one sort sequence/collation is supported for each DB2 index.
      getErrTxt(DB2I_ERR_MIXED_COLLATIONS);
      DBUG_RETURN(DB2I_ERR_MIXED_COLLATIONS);
    }
  }

  DBUG_RETURN(0);
}
