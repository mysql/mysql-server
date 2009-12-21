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
#include "db2i_safeString.h"

// This function is called when building the CREATE TABLE information for
// foreign key constraints. It converts a constraint, table, schema, or
// field name from EBCDIC to ASCII. If the DB2 name is quoted, it removes   
// those quotes. It then adds the appropriate quotes for a MySQL identifier.

static void convNameForCreateInfo(THD *thd, SafeString& info, char* fromName, int len)
{
  int quote;
  char cquote;                // Quote character
  char convName[MAX_DB2_FILENAME_LENGTH];    // Converted name

  memset(convName, 0, sizeof(convName));
  convFromEbcdic(fromName, convName, len);
  quote = get_quote_char_for_identifier(thd, convName, len);
  cquote = (char) quote;
  if (quote != EOF)
    info.strcat(cquote);
  if (convName[0] == '"')     // If DB2 name was quoted, remove quotes
  {
    if (strstr(convName, "\"\""))
      stripExtraQuotes(convName+1, len-1);
    info.strncat((char*)(convName+1), len-2);        
  }
  else                        // DB2 name was not quoted
    info.strncat(convName, len);
  if (quote != EOF)
    info.strcat(cquote);
}

/**
  Evaluate the parse tree to build foreign key constraint clauses
  
  @parm lex  The parse tree
  @parm appendHere  The DB2 string to receive the constraint clauses
  @parm path  The path to the table under consideration
  @parm fields  Pointer to the table's list of field pointers
  @parm[in, out] fileSortSequenceType  The sort sequence type associated with the table
  @parm[in, out] fileSortSequence  The sort sequence associated with the table
  @parm[in, out] fileSortSequenceLibrary  The sort sequence library associated with the table
  
  @return  0 if successful; HA_ERR_CANNOT_ADD_FOREIGN otherwise
*/
int ha_ibmdb2i::buildDB2ConstraintString(LEX* lex, 
                                         String& appendHere, 
                                         const char* path,
                                         Field** fields,
                                         char* fileSortSequenceType, 
                                         char* fileSortSequence, 
                                         char* fileSortSequenceLibrary)
{
  List_iterator<Key> keyIter(lex->alter_info.key_list);
  char colName[MAX_DB2_COLNAME_LENGTH+1];
  
  Key* curKey;
  
  while (curKey = keyIter++)
  {
    if (curKey->type == Key::FOREIGN_KEY)
    {  
      appendHere.append(STRING_WITH_LEN(", "));
      
      Foreign_key* fk = (Foreign_key*)curKey;
      
      char db2LibName[MAX_DB2_SCHEMANAME_LENGTH+1];
      if (fk->name)
      {
        char db2FKName[MAX_DB2_FILENAME_LENGTH+1];
        appendHere.append(STRING_WITH_LEN("CONSTRAINT "));
        if (fk->ref_table->db.str)
        {
          convertMySQLNameToDB2Name(fk->ref_table->db.str, db2LibName, sizeof(db2LibName));
        }
        else
        {
          db2i_table::getDB2LibNameFromPath(path, db2LibName);
        }
        if (lower_case_table_names == 1)
          my_casedn_str(files_charset_info, db2LibName);
        appendHere.append(db2LibName);
        
        appendHere.append('.');
        
        convertMySQLNameToDB2Name(fk->name, db2FKName, sizeof(db2FKName));
        appendHere.append(db2FKName);
      }
      
      appendHere.append(STRING_WITH_LEN(" FOREIGN KEY ("));
      
      bool firstTime = true;
      
      List_iterator<Key_part_spec> column(fk->columns);
      Key_part_spec* curColumn;
      
      while (curColumn = column++) 
      {
        if (!firstTime)
        {
          appendHere.append(',');
        }
        firstTime = false;
        
        convertMySQLNameToDB2Name(curColumn->field_name, colName, sizeof(colName));
        appendHere.append(colName);

        // DB2 requires that the sort sequence on the child table match the parent table's
        // sort sequence. We ensure that happens by updating the sort sequence according
        // to the constrained fields.                
        Field** field = fields;
        do
        {
          if (strcmp((*field)->field_name, curColumn->field_name) == 0)
          {
            int rc = updateAssociatedSortSequence((*field)->charset(),
                                                  fileSortSequenceType,
                                                  fileSortSequence,
                                                  fileSortSequenceLibrary);

            if (unlikely(rc)) return rc;
          }
        } while (*(++field));
      }
     
      firstTime = true;
        
      appendHere.append(STRING_WITH_LEN(") REFERENCES "));
      
      if (fk->ref_table->db.str)
      {
        convertMySQLNameToDB2Name(fk->ref_table->db.str, db2LibName, sizeof(db2LibName));
      }
      else
      {
        db2i_table::getDB2LibNameFromPath(path, db2LibName);
      }
      if (lower_case_table_names == 1)
        my_casedn_str(files_charset_info, db2LibName);
      appendHere.append(db2LibName);      
      appendHere.append('.');
      
      char db2FileName[MAX_DB2_FILENAME_LENGTH+1];
      convertMySQLNameToDB2Name(fk->ref_table->table.str, db2FileName, sizeof(db2FileName));
      if (lower_case_table_names)
        my_casedn_str(files_charset_info, db2FileName);
      appendHere.append(db2FileName);
      
      
      if (!fk->ref_columns.is_empty())
      {
        List_iterator<Key_part_spec> ref(fk->ref_columns);
        Key_part_spec* curRef;
        appendHere.append(STRING_WITH_LEN(" ("));


        while (curRef = ref++) 
        {
          if (!firstTime)
          {
            appendHere.append(',');
          }
          firstTime = false;

          convertMySQLNameToDB2Name(curRef->field_name, colName, sizeof(colName));
          appendHere.append(colName);
        }

        appendHere.append(STRING_WITH_LEN(") "));
      }
      
      if (fk->delete_opt != Foreign_key::FK_OPTION_UNDEF)
      {
        appendHere.append(STRING_WITH_LEN("ON DELETE "));
        switch (fk->delete_opt)
        {
          case Foreign_key::FK_OPTION_RESTRICT:
            appendHere.append(STRING_WITH_LEN("RESTRICT ")); break;
          case Foreign_key::FK_OPTION_CASCADE:
            appendHere.append(STRING_WITH_LEN("CASCADE ")); break;
          case Foreign_key::FK_OPTION_SET_NULL:
            appendHere.append(STRING_WITH_LEN("SET NULL ")); break;
          case Foreign_key::FK_OPTION_NO_ACTION:
            appendHere.append(STRING_WITH_LEN("NO ACTION ")); break;
          case Foreign_key::FK_OPTION_DEFAULT:
            appendHere.append(STRING_WITH_LEN("SET DEFAULT ")); break;
          default:
            return HA_ERR_CANNOT_ADD_FOREIGN; break;
        }
      }
      
      if (fk->update_opt != Foreign_key::FK_OPTION_UNDEF)
      {
        appendHere.append(STRING_WITH_LEN("ON UPDATE "));
        switch (fk->update_opt)
        {
          case Foreign_key::FK_OPTION_RESTRICT:
            appendHere.append(STRING_WITH_LEN("RESTRICT ")); break;
          case Foreign_key::FK_OPTION_NO_ACTION:
            appendHere.append(STRING_WITH_LEN("NO ACTION ")); break;
          default:
            return HA_ERR_CANNOT_ADD_FOREIGN; break;
        }
      }
     
    }
    
  }
  
  return 0;
}


/***********************************************************************
Get the foreign key information in the form of a character string so
that it can be inserted into a CREATE TABLE statement. This is used by
the SHOW CREATE TABLE statement. The string will later be freed by the
free_foreign_key_create_info() method.
************************************************************************/

char* ha_ibmdb2i::get_foreign_key_create_info(void)
{
  DBUG_ENTER("ha_ibmdb2i::get_foreign_key_create_info");
  int rc = 0;
  char* infoBuffer = NULL;     // Pointer to string returned to MySQL
  uint32 constraintSpaceLength;// Length of space passed to DB2   
  ValidatedPointer<char> constraintSpace; // Space pointer passed to DB2
  uint32 neededLen;            // Length returned from DB2                    
  uint32 cstCnt;               // Number of foreign key constraints from DB2
  uint32 fld;                  //
  constraint_hdr* cstHdr;      // Pointer to constraint header structure 
  FK_constraint* FKCstDef;     // Pointer to constraint definition structure
  cst_name* fieldName;         // Pointer to field name structure   
  char* tempPtr;               // Temp pointer for traversing constraint space
  char convName[128];
 
  /* Allocate space to retrieve the DB2 constraint information.          */

  if (!(share = get_share(table_share->path.str, table)))
    DBUG_RETURN(NULL);

  constraintSpaceLength = 5000;             // Try allocating 5000 bytes and see if enough.

  initBridge();

  constraintSpace.alloc(constraintSpaceLength);
  rc =  bridge()->expectErrors(QMY_ERR_NEED_MORE_SPACE)
                ->constraints(db2Table->dataFile()->getMasterDefnHandle(), 
                              constraintSpace,
                              constraintSpaceLength,
                              &neededLen,
                              &cstCnt);
 
  if (unlikely(rc == QMY_ERR_NEED_MORE_SPACE))
  {
    constraintSpaceLength = neededLen;     // Get length of space that's needed
    constraintSpace.realloc(constraintSpaceLength);
    rc =  bridge()->expectErrors(QMY_ERR_NEED_MORE_SPACE)
                  ->constraints(db2Table->dataFile()->getMasterDefnHandle(), 
                                constraintSpace,
                                constraintSpaceLength,
                                &neededLen,
                                &cstCnt);
  }

 /* If constraint information was returned by DB2, build a text string  */
 /* to return to MySQL.                                                 */

  if ((rc == 0) && (cstCnt > 0))
  {
    THD* thd = ha_thd();
    infoBuffer = (char*) my_malloc(MAX_FOREIGN_LEN + 1, MYF(MY_WME));    
    if (infoBuffer == NULL)
    {
      free_share(share);
      DBUG_RETURN(NULL);
    }
    
    SafeString info(infoBuffer, MAX_FOREIGN_LEN + 1);
 
    /* Loop through the DB2 constraints and build a text string for each foreign  */
    /* key constraint that is found.                                              */

    tempPtr = constraintSpace;
    cstHdr = (constraint_hdr_t*)(void*)constraintSpace;    // Address first constraint definition 
    for (int i = 0; i < cstCnt && !info.overflowed(); ++i)                                     
    {
      if (cstHdr->CstType[0] == QMY_CST_FK)   // If this is a foreign key constraint
      {
        tempPtr = (char*)(tempPtr + cstHdr->CstDefOff);
        FKCstDef = (FK_constraint_t*)tempPtr;

       /* Process the constraint name.                                           */

        info.strncat(STRING_WITH_LEN(",\n  CONSTRAINT "));
        convNameForCreateInfo(thd, info,
             FKCstDef->CstName.Name, FKCstDef->CstName.Len);
 
       /* Process the names of the foreign keys.                                 */

        info.strncat(STRING_WITH_LEN(" FOREIGN KEY ("));
        tempPtr = (char*)(tempPtr + FKCstDef->KeyColOff);
        fieldName= (cst_name_t*)tempPtr;
        for (fld = 0; fld < FKCstDef->KeyCnt; ++fld)
        {
          convNameForCreateInfo(thd, info, fieldName->Name, fieldName->Len);
          if ((fld + 1) < FKCstDef->KeyCnt)
          {
            info.strncat(STRING_WITH_LEN(", "));
            fieldName = fieldName + 1;   
           }
        }

      /* Process the schema-name and name of the referenced table.              */

        info.strncat(STRING_WITH_LEN(") REFERENCES "));
        convNameForCreateInfo(thd, info,
            FKCstDef->RefSchema.Name, FKCstDef->RefSchema.Len);
        info.strcat('.');
        convNameForCreateInfo(thd, info,
            FKCstDef->RefTable.Name, FKCstDef->RefTable.Len);
        info.strncat(STRING_WITH_LEN(" ("));

     /* Process the names of the referenced keys.                              */

        tempPtr = (char*)FKCstDef; 
        tempPtr = (char*)(tempPtr + FKCstDef->RefColOff);
        fieldName= (cst_name_t*)tempPtr;
        for (fld = 0; fld < FKCstDef->RefCnt; ++fld)
        {
          convNameForCreateInfo(thd, info, fieldName->Name, fieldName->Len);
          if ((fld + 1) < FKCstDef->RefCnt)
            {
            info.strncat(STRING_WITH_LEN(", "));
            fieldName = fieldName + 1;
          }  
        }

    /* Process the ON UPDATE and ON DELETE rules.                             */

        info.strncat(STRING_WITH_LEN(") ON UPDATE "));
        switch(FKCstDef->UpdMethod)
        {
          case QMY_NOACTION: info.strncat(STRING_WITH_LEN("NO ACTION")); break;
           case QMY_RESTRICT: info.strncat(STRING_WITH_LEN("RESTRICT")); break;
          default: break;
        }
        info.strncat(STRING_WITH_LEN(" ON DELETE "));  
        switch(FKCstDef->DltMethod)
        {
            case QMY_CASCADE: info.strncat(STRING_WITH_LEN("CASCADE")); break;
            case QMY_SETDFT: info.strncat(STRING_WITH_LEN("SET DEFAULT")); break;
            case QMY_SETNULL: info.strncat(STRING_WITH_LEN("SET NULL")); break; 
            case QMY_NOACTION: info.strncat(STRING_WITH_LEN("NO ACTION")); break;
            case QMY_RESTRICT: info.strncat(STRING_WITH_LEN("RESTRICT")); break;
            default: break;
        }
      }

    /* Address the next constraint, if any.                                   */

      if ((i+1) < cstCnt) 
      { 
        tempPtr = (char*)cstHdr + cstHdr->CstLen;
        cstHdr = (constraint_hdr_t*)(tempPtr);
      }
    }
  }

  /* Cleanup and return                                                     */
  free_share(share);

  DBUG_RETURN(infoBuffer);
}

/***********************************************************************
Free the foreign key create info (for a table) that was acquired by the
get_foreign_key_create_info() method.   
***********************************************************************/

void ha_ibmdb2i::free_foreign_key_create_info(char* info)
{
  DBUG_ENTER("ha_ibmdb2i::free_foreign_key_create_info");

  if (info)
  {
    my_free(info, MYF(0));
  }
  DBUG_VOID_RETURN;
}

/***********************************************************************
This method returns to MySQL a list, with one entry in the list describing
each foreign key constraint. 
***********************************************************************/

int ha_ibmdb2i::get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list)
{
  DBUG_ENTER("ha_ibmdb2i::get_foreign_key_list");
  int rc = 0;
  uint32 constraintSpaceLength;             // Length of space passed to DB2 
  ValidatedPointer<char> constraintSpace; // Space pointer passed to DB2
  uint16 rtnCode;              // Return code from DB2
  uint32 neededLen;            // Bytes needed to contain DB2 constraint info
  uint32 cstCnt;               // Number of constraints returned by DB2
  uint32 fld;      
  constraint_hdr* cstHdr;      // Pointer to a cst header structure
  FK_constraint* FKCstDef;     // Pointer to definition of foreign key constraint
  cst_name* fieldName;         // Pointer to field name structure
  const char *method;
  ulong methodLen;
  char* tempPtr;               // Temp pointer for traversing constraint space
  char convName[128];

  if (!(share = get_share(table_share->path.str, table)))
     DBUG_RETURN(0);               

  // Allocate space to retrieve the DB2 constraint information. 
  constraintSpaceLength = 5000;              // Try allocating 5000 bytes and see if enough.

  constraintSpace.alloc(constraintSpaceLength);
  rc =  bridge()->expectErrors(QMY_ERR_NEED_MORE_SPACE)
                ->constraints(db2Table->dataFile()->getMasterDefnHandle(), 
                              constraintSpace,
                              constraintSpaceLength,
                              &neededLen,
                              &cstCnt);
 
  if (unlikely(rc == QMY_ERR_NEED_MORE_SPACE))
  {
    constraintSpaceLength = neededLen;     // Get length of space that's needed
    constraintSpace.realloc(constraintSpaceLength);
    rc =  bridge()->expectErrors(QMY_ERR_NEED_MORE_SPACE)
                  ->constraints(db2Table->dataFile()->getMasterDefnHandle(), 
                                constraintSpace,
                                constraintSpaceLength,
                                &neededLen,
                                &cstCnt);
  }

  /* If constraint information was returned by DB2, build a text string  */
  /* to return to MySQL.                                                 */
  if ((rc == 0) && (cstCnt > 0))
  {
    tempPtr = constraintSpace;
    cstHdr = (constraint_hdr_t*)(void*)constraintSpace;  // Address first constraint definition 
    for (int i = 0; i < cstCnt; ++i)
    {
      if (cstHdr->CstType[0] == QMY_CST_FK)   // If this is a foreign key constraint
      {
        FOREIGN_KEY_INFO f_key_info;
        LEX_STRING *name= 0;
        tempPtr = (char*)(tempPtr + cstHdr->CstDefOff);
        FKCstDef = (FK_constraint_t*)tempPtr;

      /* Process the constraint name.                                           */

        convFromEbcdic(FKCstDef->CstName.Name, convName,FKCstDef->CstName.Len);
        if (convName[0] == '"')        // If quoted, exclude quotes. 
          f_key_info.forein_id = thd_make_lex_string(thd, 0,
                      convName + 1, (uint) (FKCstDef->CstName.Len - 2), 1);
        else                           // Not quoted                         
          f_key_info.forein_id = thd_make_lex_string(thd, 0,
                      convName, (uint) FKCstDef->CstName.Len, 1);
 
      /* Process the names of the foreign keys.                                 */


        tempPtr = (char*)(tempPtr + FKCstDef->KeyColOff);  
        fieldName = (cst_name_t*)tempPtr;
        for (fld = 0; fld < FKCstDef->KeyCnt; ++fld)
        {
         convFromEbcdic(fieldName->Name, convName, fieldName->Len);
         if (convName[0] == '"')        // If quoted, exclude quotes.
           name = thd_make_lex_string(thd, name,
                 convName + 1, (uint) (fieldName->Len - 2), 1);
         else
           name = thd_make_lex_string(thd, name, convName, (uint) fieldName->Len, 1);
          f_key_info.foreign_fields.push_back(name);
          if ((fld + 1) < FKCstDef->KeyCnt)
            fieldName = fieldName + 1;  
        }

     /* Process the schema and name of the referenced table.                   */

        convFromEbcdic(FKCstDef->RefSchema.Name, convName, FKCstDef->RefSchema.Len);
        if (convName[0] == '"')        // If quoted, exclude quotes.
          f_key_info.referenced_db = thd_make_lex_string(thd, 0,
                  convName + 1, (uint) (FKCstDef->RefSchema.Len -2), 1);
        else
          f_key_info.referenced_db = thd_make_lex_string(thd, 0,
                  convName, (uint) FKCstDef->RefSchema.Len, 1);
        convFromEbcdic(FKCstDef->RefTable.Name, convName, FKCstDef->RefTable.Len);
        if (convName[0] == '"')        // If quoted, exclude quotes.
          f_key_info.referenced_table = thd_make_lex_string(thd, 0,
                  convName +1, (uint) (FKCstDef->RefTable.Len -2), 1);
        else
          f_key_info.referenced_table = thd_make_lex_string(thd, 0,
                  convName, (uint) FKCstDef->RefTable.Len, 1);

     /* Process the names of the referenced keys.                              */

        tempPtr = (char*)FKCstDef; 
        tempPtr = (char*)(tempPtr + FKCstDef->RefColOff);
        fieldName= (cst_name_t*)tempPtr;
        for (fld = 0; fld < FKCstDef->RefCnt; ++fld)
        {
          convFromEbcdic(fieldName->Name, convName, fieldName->Len);
          if (convName[0] == '"')        // If quoted, exclude quotes.
            name = thd_make_lex_string(thd, name,
                  convName + 1, (uint) (fieldName->Len -2), 1);
          else
            name = thd_make_lex_string(thd, name, convName, (uint) fieldName->Len, 1);
          f_key_info.referenced_fields.push_back(name);
          if ((fld + 1) < FKCstDef->RefCnt)
            fieldName = fieldName + 1;                                 
        }

    /* Process the ON UPDATE and ON DELETE rules.                             */

        switch(FKCstDef->UpdMethod)
        {
          case QMY_NOACTION:
            { 
              method = "NO ACTION";
              methodLen=9; 
             }
            break;
          case QMY_RESTRICT:
            {
              method = "RESTRICT";
              methodLen = 8;  
            }
            break;
          default: break;
        }
        f_key_info.update_method = thd_make_lex_string(
                    thd, f_key_info.update_method, method, methodLen, 1);
        switch(FKCstDef->DltMethod)
        {
          case QMY_CASCADE: 
            {
              method = "CASCADE";
              methodLen = 7;  
            }
            break;
          case QMY_SETDFT: 
            {
              method = "SET DEFAULT";
              methodLen = 11; 
            }
            break;
          case QMY_SETNULL: 
            {
              method = "SET NULL";
              methodLen = 8;  
            }
            break; 
          case QMY_NOACTION: 
            {
              method = "NO ACTION";
              methodLen = 9;  
            }
            break;
          case QMY_RESTRICT: 
            {
              method = "RESTRICT";
              methodLen = 8;  
            }
            break;
          default: break;
        }
        f_key_info.delete_method = thd_make_lex_string(
                  thd, f_key_info.delete_method, method, methodLen, 1);
        f_key_info.referenced_key_name= thd_make_lex_string(thd, 0, (char *)"", 1, 1);
        FOREIGN_KEY_INFO *pf_key_info = (FOREIGN_KEY_INFO *)
                  thd_memdup(thd, &f_key_info, sizeof(FOREIGN_KEY_INFO));
        f_key_list->push_back(pf_key_info);
      }

   /* Address the next constraint, if any.                                   */

       if ((i+1) < cstCnt) 
      { 
        tempPtr = (char*)cstHdr + cstHdr->CstLen;
        cstHdr = (constraint_hdr_t*)(tempPtr);
      }
    }
  }
 
  /* Cleanup and return.                                           */

  free_share(share);
  DBUG_RETURN(0);
}

/***********************************************************************
Checks if the table is referenced by a foreign key.                            
Returns: 0 if not referenced (or error occurs),
       > 0 if is referenced 
***********************************************************************/

uint ha_ibmdb2i::referenced_by_foreign_key(void)
{
  DBUG_ENTER("ha_ibmdb2i::referenced_by_foreign_key");

  int rc = 0;
  FILE_HANDLE queryFile = 0;
  uint32 resultRowLen;   
  uint32 count = 0; 

  const char* libName = db2Table->getDB2LibName(db2i_table::ASCII_SQL);
  const char* fileName = db2Table->getDB2TableName(db2i_table::ASCII_SQL);
  
  String query(128);
  query.append(STRING_WITH_LEN(" SELECT COUNT(*) FROM SYSIBM.SQLFOREIGNKEYS WHERE PKTABLE_SCHEM = '"));
  query.append(libName+1, strlen(libName)-2);                 // parent library name 
  query.append(STRING_WITH_LEN("' AND PKTABLE_NAME = '"));     
  query.append(fileName+1, strlen(fileName)-2);               // parent file name
  query.append(STRING_WITH_LEN("'"));   

  SqlStatementStream sqlStream(query);
  
  rc = bridge()->prepOpen(sqlStream.getPtrToData(),
                        &queryFile,
                        &resultRowLen);
  if (rc == 0)
  {
    IOReadBuffer rowBuffer(1, resultRowLen);
    rc = bridge()->read(queryFile, rowBuffer.ptr(), QMY_READ_ONLY, QMY_NONE, QMY_FIRST);
    if (!rc) count = *((uint32*)rowBuffer.getRowN(0));
    bridge()->deallocateFile(queryFile);
  }
  DBUG_RETURN(count);
}
