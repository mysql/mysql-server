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


#include "ha_ibmdb2i.h"

/* Helper function for records_in_range.
     Input: Bitmap of used key parts.
     Output: Number of used key parts.                                        */

static inline int getKeyCntFromMap(key_part_map keypart_map)  
{
  int cnt = 0;
  while (keypart_map)
  {
    keypart_map = keypart_map >> 1;
    cnt++;
  }
  return (cnt); 
}


/**
  @brief
  Given a starting key and an ending key, estimate the number of rows that
  will exist between the two keys.

 INPUT
    inx      Index to use
    min_key    Min key. Is NULL if no min range
    max_key    Max key. Is NULL if no max range

 NOTES
    min_key.flag can have one of the following values:
      HA_READ_KEY_EXACT    Include the key in the range
      HA_READ_AFTER_KEY    Don't include key in range

    max_key.flag can have one of the following values:  
      HA_READ_BEFORE_KEY   Don't include key in range
      HA_READ_AFTER_KEY    Include all 'end_key' values in the range

  RETURN
   HA_POS_ERROR         Error or the storage engine cannot estimate the number of rows
   1                    There are no matching keys in the given range
   n > 0                There are approximately n rows in the range
*/
ha_rows ha_ibmdb2i::records_in_range(uint inx,
                                     key_range *min_key,
                                     key_range *max_key)
{
  DBUG_ENTER("ha_ibmdb2i::records_in_range");
  int rc = 0;                 // Return code
  ha_rows rows = 0;           // Row count returned to caller of this method 
  uint32 spcLen;              // Length of space passed to DB2   
  uint32 keyCnt;              // Number of fields in the key composite
  uint32 literalCnt = 0;      // Number of literals
  uint32 boundsOff;           // Offset from beginning of space to range bounds     
  uint32 litDefOff;           // Offset from beginning of space to literal definitions
  uint32 literalsOff;         // Offset from beginning of space to literal values
  uint32 cutoff = 0;          // Early exit cutoff (currently not used)
  uint64 recCnt;              // Row count from DB2       
  uint16 rtnCode;             // Return code from DB2
  Bounds* boundsPtr;          // Pointer to a pair of range bounds      
  Bound* boundPtr;            // Pointer to a single (high or low) range bound
  LitDef* litDefPtr;          // Pointer to a literal definition
  char* literalsPtr;          // Pointer to the start of all literal values
  char* literalPtr;           // Pointer to the start of this literal value
  char* tempPtr;              // Temporary pointer
  char* tempMinPtr;           // Temporary pointer into min_key
  int minKeyCnt = 0;          // Number of fields in the min_key composite 
  int maxKeyCnt = 0;          // Number of fields in the max_key composite
  size_t tempLen = 0;         // Temporary length
  uint16 DB2FieldWidth = 0;   // DB2 field width
  uint32 workFieldLen = 0;    // Length of workarea needed for CCSID conversions
  bool overrideInclusion;     // Indicator for inclusion/exclusion     
  char* endOfLiteralPtr;      // Pointer to the end of this literal
  char* endOfMinPtr;          // Pointer to end of min_key
  uint16 endByte = 0;         // End byte of char or graphic literal (padding not included) 
  bool reuseLiteral;          // Indicator that hi and lo bounds use same literal
  char* minPtr = NULL;               // Work pointer for traversing min_key  
  char* maxPtr = NULL;               // Work pointer for traversing max_key    
  /*                                                                                  
     Handle the special case of 'x < null' anywhere in the key range. There are
     no values less than null, but return 1 so that MySQL does not assume
     the empty set for the query. 
                                                                              */
  if (min_key != NULL && max_key != NULL &&
      min_key->flag == HA_READ_AFTER_KEY && max_key->flag == HA_READ_BEFORE_KEY && 
      min_key->length == max_key->length &&
     (memcmp((uchar*)min_key->key,(uchar*)max_key->key,min_key->length)==0))
  {
    DBUG_PRINT("ha_ibmdb2i::records_in_range",("Estimate 1 row for key %d; special case: < null", inx));
    DBUG_RETURN((ha_rows) 1 );                               
  }
  /*                                                                                  
     Determine the number of fields in the key composite.                             
                                                                              */

  if (min_key)
  {
    minKeyCnt = getKeyCntFromMap(min_key->keypart_map);
    minPtr = (char*)min_key->key;
  }
  if (max_key)
  {
    maxKeyCnt = getKeyCntFromMap(max_key->keypart_map);
    maxPtr = (char*)max_key->key;
  }
  keyCnt = maxKeyCnt >= minKeyCnt ? maxKeyCnt : minKeyCnt;

  /*
     Handle the special case where MySQL does not pass either a min or max
     key range. In this case, set the key count to 1 (knowing that there
     is at least one key field) to flow through and create one bounds structure.
     When both the min and max key ranges are nil, the bounds structure will
     specify positive and negative infinity and DB2 will estimate the total 
     number of rows.                                                                                                                                                            */

  if (keyCnt == 0)
    keyCnt = 1;

  /*                                                                               
     Allocate the space needed to pass range information to DB2. The            
     space must be large enough to store the following:                         
        - one pair of bounds (high and low) per field in the key composite            
        - one literal definition per literal value                              
        - the literal values                                                    
        - work area for literal CCSID conversions                                     
     Since we don't know yet how many of these structures are needed,           
     allocate enough space for the maximum that we will possibly need.        
     The workarea for the literal conversion must be big enough to hold the     
     largest of the DB2 key fields.                                             
                                                                              */
  KEY& curKey = table->key_info[inx];

  for (int i = 0; i < keyCnt; i++)  
  {
    DB2FieldWidth =
      db2Table->db2Field(curKey.key_part[i].field->field_index).getByteLengthInRecord();  
    if (DB2FieldWidth > workFieldLen)
      workFieldLen = DB2FieldWidth;      // Get length of largest DB2 field
    tempLen = tempLen + DB2FieldWidth;   // Tally the DB2 field lengths
  }
  spcLen = (sizeof(Bounds)*keyCnt) + (sizeof(LitDef)*keyCnt*2) + (tempLen*2) + workFieldLen;

  ValidatedPointer<char>  spcPtr(spcLen); // Pointer to space passed to DB2
  memset(spcPtr, 0, spcLen);             // Clear the allocated space 
  /*                                                                            
     Set addressability to the various sections of the DB2 interface space.     
                                                                              */
  boundsOff = 0;                         // Range bounds are at the start of the space
  litDefOff = sizeof(Bounds) * keyCnt;   // Literal defs follow all the range bounds       
  literalsOff = litDefOff + (sizeof(LitDef) * keyCnt * 2); // Literal values are last
  boundsPtr = (Bounds_t*)(void*)spcPtr;  // Address first bounds structure
  tempPtr = (char*)((char*)spcPtr + litDefOff);
  litDefPtr = (LitDef_t*)tempPtr;        // Address first literal definition
  tempPtr = (char*)((char*)spcPtr + literalsOff); 
  literalsPtr = (char*)tempPtr;          // Address start of literal values
  literalPtr = literalsPtr;              // Address first literal value 
  /*                                                                             
     For each key part, build the low (min) and high (max) DB2 range bounds.  
     If literals are specified in the MySQL range, build DB2 literal          
     definitions and store the literal values for access by DB2.              
                                                                              
     If no value is specified for a key part, assume infinity.  Negative      
     infinity will cause processing to start at the first index entry.        
     Positive infinity will cause processing to end at the last index entry.  
     When infinity is specified in a bound, inclusion/exclusion and position  
     are ignored, and there is no literal definition or literal value for     
     the bound.                                                               
                                                                              
     If the keypart value is null, the null indicator is set in the range     
     bound and the other fields in the bound are ignored. When the bound is   
     null, only index entries with the null value will be included in the     
     estimate. If one bound is null, both bounds must be null. When the bound 
     is not null, the data offset and length must be set, and the literal     
     value stored for access by DB2.                                          
                                                                                         */
  for (int partsInUse = 0; partsInUse < keyCnt; ++partsInUse)
  {
   Field *field= curKey.key_part[partsInUse].field;
   overrideInclusion = false;
   reuseLiteral = false;
   endOfLiteralPtr = NULL;
   /*
    Build the low bound for the key range.                                       
                                                                             */
    if ((partsInUse + 1) > minKeyCnt)                             // if no min_key info for this part
      boundsPtr->LoBound.Infinity[0] = QMY_NEG_INFINITY;          // select...where 3 between x and y    
    else
    {
      if ((curKey.key_part[partsInUse].null_bit) && (char*)minPtr[0])     
      {                                                           // min_key is null
        if (max_key == NULL ||
           ((partsInUse + 1) > maxKeyCnt))                        // select...where x='ab' and y=null and z != 'c'
          boundsPtr->LoBound.Infinity[0] = QMY_NEG_INFINITY;      // select...where x not null or
                                                                  // select...where x > null
        else                                                      // max_key is not null 
        {
          if (min_key->flag == HA_READ_KEY_EXACT) 
            boundsPtr->LoBound.IsNull[0] = QMY_YES;               // select...where x is null
          else                                                 
          {
            if ((char*)maxPtr[0])                                       
              boundsPtr->LoBound.IsNull[0] = QMY_YES;             // select...where a = null and b < 5 (max-before)
                                                                  // select...where a='a' and b is null and c !='a' (max-after)
            else 
              boundsPtr->LoBound.Infinity[0] = QMY_NEG_INFINITY;  // select...where x < y
          }
        }                                                         // end min_key is null
      }
      else                                                        // min_key is not null 
      {
        if (literalCnt) litDefPtr = litDefPtr + 1;
        literalCnt = literalCnt + 1;
        boundsPtr->LoBound.Position = literalCnt;
        /*
           Determine inclusion or exclusion.
                                                                                               */
        if (min_key->flag == HA_READ_KEY_EXACT ||                //select...where a like 'this%'
            
            /* An example for the following conditions is 'select...where a = 5 and b > null'. */
 
            (max_key &&
            (memcmp((uchar*)minPtr,(uchar*)maxPtr,
                    curKey.key_part[partsInUse].store_length)==0))) 
   
        {
          if ((min_key->flag != HA_READ_KEY_EXACT) ||
              (max_key &&
              (memcmp((uchar*)minPtr,(uchar*)maxPtr,
                    curKey.key_part[partsInUse].store_length)==0)))
            overrideInclusion = true;                     // Need inclusion for both min and max 
        }
        else
          boundsPtr->LoBound.Embodiment[0] = QMY_EXCLUSION;
        litDefPtr->FieldNbr = field->field_index + 1;
        DB2Field& db2Field = db2Table->db2Field(field->field_index);
        litDefPtr->DataType = db2Field.getType();
        /*
           Convert the literal to DB2 format.  
                                                                                   */
        rc = convertMySQLtoDB2(field,
                 db2Field,
                 literalPtr,
                 (uchar*)minPtr+((curKey.key_part[partsInUse].null_bit)? 1 : 0));
        if (rc != 0) break;
        litDefPtr->Offset = (uint32_t)(literalPtr - literalsPtr);
        litDefPtr->Length = db2Field.getByteLengthInRecord();
        tempLen = litDefPtr->Length;
        /*
           Do additional conversion of a character or graphic value.
                                                                                 */
        CHARSET_INFO* fieldCharSet = field->charset();                                    
        if ((field->type() != MYSQL_TYPE_BIT) &&           // Don't do conversion on BIT data
            (field->charset() != &my_charset_bin) &&       // Don't do conversion on BINARY data
            (litDefPtr->DataType == QMY_CHAR || litDefPtr->DataType == QMY_VARCHAR ||
             litDefPtr->DataType == QMY_GRAPHIC || litDefPtr->DataType == QMY_VARGRAPHIC))
        {
           if (litDefPtr->DataType == QMY_VARCHAR ||
               litDefPtr->DataType == QMY_VARGRAPHIC) 
             tempPtr = literalPtr + sizeof(uint16);
           else
             tempPtr = literalPtr;
           /* The following code checks to determine if MySQL is passing a
              partial key. DB2 will accept a partial field value, but only
              in the last field position of the key composite (and only if
              there is no ICU sort sequence on the index).                  */
           tempMinPtr = (char*)minPtr+((curKey.key_part[partsInUse].null_bit)? 1 : 0);
           if (field->type() == MYSQL_TYPE_VARCHAR)
           {
             /* MySQL always stores key lengths as 2 bytes, little-endian. */
               tempLen = *(uint8*)tempMinPtr + ((*(uint8*)(tempMinPtr+1)) << 8);
               tempMinPtr = (char*)((char*)tempMinPtr + 2);
           }
           else
             tempLen = field->field_length;                         

           /* Determine if we are dealing with a partial key and if so, find the end of the partial key. */
           if (litDefPtr->DataType == QMY_CHAR || litDefPtr->DataType == QMY_VARCHAR )
           { /* Char or varchar.  If UTF8, no conversion is done to DB2 graphic.) */
               endOfMinPtr = (char*)memchr(tempMinPtr,field->charset()->min_sort_char,tempLen);
               if (endOfMinPtr)
                 endOfLiteralPtr = tempPtr + ((uint32_t)(endOfMinPtr - tempMinPtr));
           }
           else
           {
              if (strncmp(fieldCharSet->csname, "utf8", sizeof("utf8")) == 0)
              {  /* The MySQL charset is UTF8 but we are converting to graphic on DB2. Divide number of UTF8 bytes
                    by 3 to get the number of characters, then multiple by 2 for double-byte graphic.*/
               endOfMinPtr = (char*)memchr(tempMinPtr,field->charset()->min_sort_char,tempLen);
               if (endOfMinPtr)
                 endOfLiteralPtr = tempPtr + (((uint32_t)((endOfMinPtr - tempMinPtr)) / 3) * 2);
              } 
              else
              { /* The DB2 data type is graphic or vargraphic, and we are not converting from UTF8 to graphic. */  
                endOfMinPtr = (char*)wmemchr((wchar_t*)tempMinPtr,field->charset()->min_sort_char,tempLen/2);
                if (endOfMinPtr)
                  endOfLiteralPtr = tempPtr + (endOfMinPtr - tempMinPtr);
              }
           }
           /* Enforce here that a partial is only allowed on the last field position 
              of the key composite                                                    */
           if (endOfLiteralPtr)
           {
             if ((partsInUse + 1) < minKeyCnt)   
             {
               rc = HA_POS_ERROR;
               break;
             }
             endByte = endOfLiteralPtr - tempPtr;
             /* We're making an assumption that if MySQL gives us a partial key, 
                 the length of the partial is the same for both the min_key and max_key.      */
           }  
        }
        literalPtr = literalPtr + litDefPtr->Length;  // Bump pointer for next literal
      }
      /* If there is a max_key value for this field, and if the max_key value is 
         the same as the min_key value, then the low bound literal can be reused              
         for the high bound literal. This eliminates the overhead of copying and
         converting the same value twice.                                        */ 
      if (max_key && ((partsInUse + 1) <= maxKeyCnt) && 
          (memcmp((uchar*)minPtr,(uchar*)maxPtr,
                    curKey.key_part[partsInUse].store_length)==0 || endOfLiteralPtr))   
        reuseLiteral = true;
      minPtr += curKey.key_part[partsInUse].store_length;
    }
   /*
      Build the high bound for the key range. 
                                                                              */
    if (max_key == NULL || ((partsInUse + 1) > maxKeyCnt))                                         
      boundsPtr->HiBound.Infinity[0] = QMY_POS_INFINITY;     
    else                                                      
    {
      if ((curKey.key_part[partsInUse].null_bit) && (char*)maxPtr[0])
      {
        if (min_key == NULL)
          boundsPtr->HiBound.Infinity[0] = QMY_POS_INFINITY;   
        else
          boundsPtr->HiBound.IsNull[0] = QMY_YES;             // select...where x is null
      }  
      else                                                    // max_key field is not null
      {
        if (boundsPtr->LoBound.IsNull[0] == QMY_YES)          // select where x < 10 or x is null
        {
          rc = HA_POS_ERROR;
          break;
        }
        if (!reuseLiteral)                                    
        {  
          if (literalCnt)             
            litDefPtr = litDefPtr + 1;
          literalCnt = literalCnt + 1;
          litDefPtr->FieldNbr = field->field_index + 1;
          DB2Field& db2Field = db2Table->db2Field(field->field_index);
          litDefPtr->DataType = db2Field.getType();
        /*
           Convert the literal to DB2 format
                                                                                               */
          rc = convertMySQLtoDB2(field,
                 db2Field,
                 literalPtr,
                 (uchar*)maxPtr+((curKey.key_part[partsInUse].null_bit)? 1 : 0));
          if (rc != 0) break;
          litDefPtr->Offset = (uint32_t)(literalPtr - literalsPtr);
          litDefPtr->Length = db2Field.getByteLengthInRecord();
          tempLen = litDefPtr->Length;
          /*
             Now convert a character or graphic value.
                                                                                 */
          if ((field->type() != MYSQL_TYPE_BIT) &&     
             (litDefPtr->DataType == QMY_CHAR || litDefPtr->DataType == QMY_VARCHAR ||
              litDefPtr->DataType == QMY_GRAPHIC || litDefPtr->DataType == QMY_VARGRAPHIC))
          {
             if (litDefPtr->DataType == QMY_VARCHAR || litDefPtr->DataType == QMY_VARGRAPHIC)
             {
                tempPtr = literalPtr + sizeof(uint16);
             }
             else
               tempPtr = literalPtr;
          }
          literalPtr = literalPtr + litDefPtr->Length;   // Bump pointer for next literal
        }
        boundsPtr->HiBound.Position = literalCnt;
        if (max_key->flag == HA_READ_BEFORE_KEY && !overrideInclusion)
          boundsPtr->HiBound.Embodiment[0] = QMY_EXCLUSION;
      }
      maxPtr += curKey.key_part[partsInUse].store_length;
    }
  /*                                                                            
     Bump to the next field in the key composite.                                     
                                                                              */

    if ((partsInUse+1) < keyCnt)
      boundsPtr = boundsPtr + 1;
  }

  /*                                                                            
     Call DB2 to estimate the number of rows in the key range.                  
                                                                              */
  if (rc == 0)
  {    
    rc = db2i_ileBridge::getBridgeForThread()->recordsInRange((indexHandles[inx] ? indexHandles[inx] : db2Table->indexFile(inx)->getMasterDefnHandle()),
                                                            spcPtr,  
                                                            keyCnt,  
                                                            literalCnt,
                                                            boundsOff,
                                                            litDefOff,
                                                            literalsOff,
                                                            cutoff,
                                                            (uint32_t)(literalPtr - (char*)spcPtr),            
                                                            endByte,
                                                            &recCnt,
                                                            &rtnCode);
  }
    /*                                                                            
     Set the row count and return.                                            
     Beware that if this method returns a zero row count, MySQL assumes the
     result set for the query is zero; never return a zero row count.                      
                                                                              */
  if ((rc == 0) && (rtnCode == QMY_SUCCESS || rtnCode == QMY_EARLY_EXIT))
  {
    rows = recCnt ? (ha_rows)recCnt : 1;                                                 
  }
 
  rows = (rows > 0 ? rows : HA_POS_ERROR);

  setIndexReadEstimate(inx, rows);
  
  DBUG_PRINT("ha_ibmdb2i::recordsInRange",("Estimate %d rows for key %d", uint32(rows), inx));  
  
  DBUG_RETURN(rows);                              
}
