/* Copyright (C) 2000 NuSphere Corporation
   
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


#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#ifdef HAVE_GEMINI_DB

#include "my_pthread.h"
#include "dbconfig.h"
#include "dsmpub.h"
#include "recpub.h"
#include "vststat.h"

#include <m_ctype.h>
#include <myisampack.h>
#include <assert.h>
#include <hash.h>
#include <stdarg.h>
#include "geminikey.h"
#include "ha_gemini.h"

const char *ha_gemini_ext=".gmd";
const char *ha_gemini_idx_ext=".gmi";

bool gemini_skip=0;
long gemini_options = 0;
long gemini_buffer_cache;
long gemini_io_threads;
long gemini_log_cluster_size;
long gemini_locktablesize;
long gemini_lock_wait_timeout;
long gemini_spin_retries;
long gemini_connection_limit;

const char gemini_dbname[] = "gemini";
dsmContext_t *pfirstContext = NULL;

ulong gemini_recovery_options = GEMINI_RECOVERY_FULL;
/* bits in gemini_recovery_options */
const char *gemini_recovery_names[] =
{ "FULL", "NONE", "FORCE" };
TYPELIB gemini_recovery_typelib= {array_elements(gemini_recovery_names),"",
				 gemini_recovery_names};

const int start_of_name = 2;  /* Name passed as ./<db>/<table-name>
                               and we're not interested in the ./ */
static const int keyBufSize = MYMAXKEYSIZE * 2;

static int gemini_tx_begin(THD *thd);
static void print_msg(THD *thd, const char *table_name, const char *op_name,
                      const char *msg_type, const char *fmt, ...);

static int gemini_helper_threads(dsmContext_t *pContext);
pthread_handler_decl(gemini_watchdog,arg );
pthread_handler_decl(gemini_rl_writer,arg );
pthread_handler_decl(gemini_apw,arg);

/* General functions */

bool gemini_init(void)
{
    dsmStatus_t     rc = 0;
    char            pmsgsfile[MAXPATHN];

    DBUG_ENTER("gemini_init");

    /* If datadir isn't set, bail out */
    if (*mysql_real_data_home == '\0')
    {
        goto badret;
    }

    /* Gotta connect to the database regardless of the operation */
    rc = dsmContextCreate(&pfirstContext);
    if( rc != 0 )
    {
        printf("dsmContextCreate failed %ld\n",rc);
        goto badret;
    }
    rc = dsmContextSetString(pfirstContext, DSM_TAGDB_DBNAME,
                             strlen(gemini_dbname), (TEXT *)gemini_dbname);
    if( rc != 0 )
    {
        printf("Dbname tag failed %ld\n", rc);
        goto badret;
    }

    fn_format(pmsgsfile, GEM_MSGS_FILE, language, ".db", 2 | 4);
    rc = dsmContextSetString(pfirstContext, DSM_TAGDB_MSGS_FILE,
                             strlen(pmsgsfile), (TEXT *)pmsgsfile);
    if( rc != 0 )
    {
        printf("MSGS_DIR tag failed %ld\n", rc);
        goto badret;
    }

    rc = dsmContextSetLong(pfirstContext,DSM_TAGDB_ACCESS_TYPE,DSM_ACCESS_STARTUP);
    if ( rc != 0 )
    {
        printf("ACCESS TAG set failed %ld\n",rc);
        goto badret;
    }
    rc = dsmContextSetLong(pfirstContext,DSM_TAGDB_ACCESS_ENV, DSM_SQL_ENGINE);
    if( rc != 0 )
    {
        printf("ACCESS_ENV set failed %ld",rc);
        goto badret;
    }

    rc = dsmContextSetString(pfirstContext, DSM_TAGDB_DATADIR,
                             strlen(mysql_real_data_home),
                             (TEXT *)mysql_real_data_home);
    if( rc != 0 )
    {
        printf("Datadir tag failed %ld\n", rc);
        goto badret;
    }

    rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_MAX_USERS,
                           gemini_connection_limit);
    if(rc != 0)
    {
        printf("MAX_USERS tag set failed %ld",rc);
        goto badret;
    }

    rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_DEFAULT_LOCK_TIMEOUT,
                           gemini_lock_wait_timeout);
    if(rc != 0)
    {
        printf("MAX_LOCK_ENTRIES tag set failed %ld",rc);
        goto badret;
    }

    rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_MAX_LOCK_ENTRIES,
                           gemini_locktablesize);
    if(rc != 0)
    {
        printf("MAX_LOCK_ENTRIES tag set failed %ld",rc);
        goto badret;
    }

    rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_SPIN_AMOUNT,
                           gemini_spin_retries);
    if(rc != 0)
    {
        printf("SPIN_AMOUNT tag set failed %ld",rc);
        goto badret;
    }

    /* blocksize is hardcoded to 8K.  Buffer cache is in bytes
       need to convert this to 8K blocks */
    gemini_buffer_cache = gemini_buffer_cache / 8192;
    rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_DB_BUFFERS,
                           gemini_buffer_cache);
    if(rc != 0)
    {
        printf("DB_BUFFERS tag set failed %ld",rc);
        goto badret;
    }

    rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_FLUSH_AT_COMMIT,
                           ((gemini_options & GEMOPT_FLUSH_LOG) ? 1 : 0));
    if(rc != 0)
    {
        printf("FLush_Log_At_Commit tag set failed %ld",rc);
        goto badret;
    }
    rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_DIRECT_IO,
                           ((gemini_options & GEMOPT_UNBUFFERED_IO) ? 1 : 0));
    if(rc != 0)
    {
        printf("DIRECT_IO tag set failed %ld",rc);
        goto badret;
    }

    rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_CRASH_PROTECTION,
                      ((gemini_recovery_options & GEMINI_RECOVERY_FULL) ? 1 : 0));
    if(rc != 0)
    {
        printf("CRASH_PROTECTION tag set failed %ld",rc);
        goto badret;
    }

    /* cluster size will come in bytes, need to convert it to 
       16 K units. */
    gemini_log_cluster_size = (gemini_log_cluster_size + 16383) / 16384;
    rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_BI_CLUSTER_SIZE,
                           gemini_log_cluster_size);
                      
    if(rc != 0)
    {
        printf("CRASH_PROTECTION tag set failed %ld",rc);
        goto badret;
    }

    rc = dsmUserConnect(pfirstContext,(TEXT *)"Multi-user",
                        DSM_DB_OPENDB | DSM_DB_OPENFILE);
    if( rc != 0 )
    {
        printf("dsmUserConnect failed rc = %ld\n",rc);
        goto badret;
    }
    /* Set access to shared for subsequent user connects    */
    rc = dsmContextSetLong(pfirstContext,DSM_TAGDB_ACCESS_TYPE,DSM_ACCESS_SHARED);
    rc = gemini_helper_threads(pfirstContext);
    DBUG_RETURN(0);

badret:
    gemini_skip = 1;
    DBUG_RETURN(0);
}

static int gemini_helper_threads(dsmContext_t *pContext)
{
  int   rc = 0;
  pthread_t hThread;
  DBUG_ENTER("gemini_helper_threads");
  rc = pthread_create (&hThread, 0, gemini_watchdog, (void *)pContext);
  if (rc)
  {
    printf("Can't create gemini watchdog thread");
    goto done;
  }
  if(!gemini_io_threads)
    goto done;

  rc = pthread_create(&hThread, 0, gemini_rl_writer, (void *)pContext);
  if(rc)
  {
    printf("Can't create gemini recovery log writer thread");
    goto done;
  }

  for( int i = gemini_io_threads - 1;i;i--)
  {
    rc = pthread_create(&hThread, 0, gemini_apw, (void *)pContext);
    if(rc)
    {
      printf("Can't create gemini page writer thread");
      goto done;
    }
  }
done:

  DBUG_RETURN(rc);
}

pthread_handler_decl(gemini_watchdog,arg )
{
  int  rc = 0;
  dsmContext_t *pcontext = (dsmContext_t *)arg;
  dsmContext_t *pmyContext = NULL;


  rc = dsmContextCopy(pcontext,&pmyContext, DSMCONTEXTDB);
  if( rc != 0 )
  {
      printf("dsmContextCopy failed for watchdog %d\n",rc);

      return 0;
  }
  rc = dsmUserConnect(pmyContext,NULL,0);

  if( rc != 0 )
  {
      printf("dsmUserConnect failed for watchdog %d\n",rc);

      return 0;
  }
 
  my_thread_init();
  pthread_detach_this_thread();

  while(rc == 0)
  {
    rc = dsmDatabaseProcessEvents(pmyContext);
    if(!rc)
      rc = dsmWatchdog(pmyContext);
    sleep(1);
  } 
  rc = dsmUserDisconnect(pmyContext,0); 
  my_thread_end();
  return 0;
}

pthread_handler_decl(gemini_rl_writer,arg )
{
  int  rc = 0;
  dsmContext_t *pcontext = (dsmContext_t *)arg;
  dsmContext_t *pmyContext = NULL;


  rc = dsmContextCopy(pcontext,&pmyContext, DSMCONTEXTDB);
  if( rc != 0 )
  {
      printf("dsmContextCopy failed for recovery log writer %d\n",rc);

      return 0;
  }
  rc = dsmUserConnect(pmyContext,NULL,0);

  if( rc != 0 )
  {
      printf("dsmUserConnect failed for recovery log writer  %d\n",rc);

      return 0;
  }

  my_thread_init();
  pthread_detach_this_thread();

  while(rc == 0)
  {
    rc = dsmRLwriter(pmyContext);
  }
  rc = dsmUserDisconnect(pmyContext,0);
  my_thread_end();
  return 0;
}

pthread_handler_decl(gemini_apw,arg )
{
  int  rc = 0;
  dsmContext_t *pcontext = (dsmContext_t *)arg;
  dsmContext_t *pmyContext = NULL;

  my_thread_init();
  pthread_detach_this_thread();

  rc = dsmContextCopy(pcontext,&pmyContext, DSMCONTEXTDB);
  if( rc != 0 )
  {
      printf("dsmContextCopy failed for gemini page writer %d\n",rc);
      my_thread_end();
      return 0;
  }
  rc = dsmUserConnect(pmyContext,NULL,0);

  if( rc != 0 )
  {
      printf("dsmUserConnect failed for gemini page writer  %d\n",rc);
      my_thread_end();
      return 0;
  }

  while(rc == 0)
  {
    rc = dsmAPW(pmyContext);
  }
  rc = dsmUserDisconnect(pmyContext,0);
  my_thread_end();
  return 0;
}

int gemini_set_option_long(int optid, long optval)
{
  dsmStatus_t rc = 0;

  switch (optid)
  {
  case GEM_OPTID_SPIN_RETRIES:
    /* If we don't have a context yet, skip the set and just save the
    ** value in gemini_spin_retries for a later gemini_init().  This
    ** may not ever happen, but we're covered if it does.
    */
    if (pfirstContext)
    {
      rc = dsmContextSetLong(pfirstContext, DSM_TAGDB_SPIN_AMOUNT,
                             optval);
    }
    if (rc)
    {
      printf("SPIN_AMOUNT tag set failed %ld",rc);
    }
    else
    {
      gemini_spin_retries = optval;
    }
    break;
  }

  return rc;
}

static int gemini_connect(THD *thd)
{
  DBUG_ENTER("gemini_connect");

  dsmStatus_t   rc;

  rc = dsmContextCopy(pfirstContext,(dsmContext_t **)&thd->gemini.context,
                      DSMCONTEXTDB);
  if( rc != 0 )
  {
      printf("dsmContextCopy failed %ld\n",rc);

      return(rc);
  }
  rc = dsmUserConnect((dsmContext_t *)thd->gemini.context,NULL,0);

  if( rc != 0 )
  {
      printf("dsmUserConnect failed %ld\n",rc);

      return(rc);
  }
  rc = (dsmStatus_t)gemini_tx_begin(thd);

 DBUG_RETURN(rc);
}

void gemini_disconnect(THD *thd)
{
  dsmStatus_t  rc;

  if(thd->gemini.context)
  {
    rc = dsmUserDisconnect((dsmContext_t *)thd->gemini.context,0);
  }
  return;
}

bool gemini_end(void)
{
  dsmStatus_t rc;
  THD  *thd;

  DBUG_ENTER("gemini_end");
  if(pfirstContext)
  {
    rc = dsmShutdownSet(pfirstContext, DSM_SHUTDOWN_NORMAL);
    sleep(2);
    rc = dsmContextSetLong(pfirstContext,DSM_TAGDB_ACCESS_TYPE,DSM_ACCESS_STARTUP);
    rc = dsmShutdown(pfirstContext, DSMNICEBIT,DSMNICEBIT);
  }
  DBUG_RETURN(0);
}

bool gemini_flush_logs()
{
  DBUG_ENTER("gemini_flush_logs");

  DBUG_RETURN(0);
}

static int gemini_tx_begin(THD *thd)
{
  dsmStatus_t  rc;
  DBUG_ENTER("gemini_tx_begin");
  
  thd->gemini.savepoint = 1;

  rc = dsmTransaction((dsmContext_t *)thd->gemini.context,
                       &thd->gemini.savepoint,DSMTXN_START,0,NULL);
  if(!rc)
    thd->gemini.needSavepoint = 1; 

  thd->gemini.tx_isolation = thd->tx_isolation;

  DBUG_PRINT("trans",("beginning transaction"));
  DBUG_RETURN(rc);
}

int gemini_commit(THD *thd)
{
  dsmStatus_t  rc;
  LONG         txNumber = 0;

  DBUG_ENTER("gemini_commit");

  if(!thd->gemini.context)
    DBUG_RETURN(0);
 
  rc = dsmTransaction((dsmContext_t *)thd->gemini.context,
			0,DSMTXN_COMMIT,0,NULL);
  if(!rc)
    rc = gemini_tx_begin(thd);

  thd->gemini.lock_count = 0;
  
  DBUG_PRINT("trans",("ending transaction"));
  DBUG_RETURN(rc);
}

int gemini_rollback(THD *thd)
{
  dsmStatus_t rc;
  LONG        txNumber;

  DBUG_ENTER("gemini_rollback");
  DBUG_PRINT("trans",("aborting transaction"));

  if(!thd->gemini.context)
    DBUG_RETURN(0);

  thd->gemini.savepoint = 0;
  rc = dsmTransaction((dsmContext_t *)thd->gemini.context,
                        &thd->gemini.savepoint,DSMTXN_ABORT,0,NULL);
  if(!rc)
    rc = gemini_tx_begin(thd);

  thd->gemini.lock_count = 0;

  DBUG_RETURN(rc);
}

int gemini_rollback_to_savepoint(THD *thd)
{
  dsmStatus_t   rc = 0;
  DBUG_ENTER("gemini_rollback_to_savepoint");
  if(thd->gemini.savepoint > 1)
  {
    rc = dsmTransaction((dsmContext_t *)thd->gemini.context,
                        &thd->gemini.savepoint,DSMTXN_UNSAVE,0,NULL);
  }
  DBUG_RETURN(rc);
}

/* gemDataType - translates from mysql data type constant to gemini
                 key services data type contstant                     */
int gemDataType ( int mysqlType )
{
  switch (mysqlType)
  {
    case FIELD_TYPE_LONG:
    case FIELD_TYPE_TINY:
    case FIELD_TYPE_SHORT:
    case FIELD_TYPE_TIMESTAMP:
    case FIELD_TYPE_LONGLONG:
    case FIELD_TYPE_INT24:
    case FIELD_TYPE_DATE:
    case FIELD_TYPE_TIME:
    case FIELD_TYPE_DATETIME:
    case FIELD_TYPE_YEAR:
    case FIELD_TYPE_NEWDATE:
    case FIELD_TYPE_ENUM:
    case FIELD_TYPE_SET:
      return GEM_INT;
    case FIELD_TYPE_DECIMAL:
      return GEM_DECIMAL;
    case FIELD_TYPE_FLOAT:
      return GEM_FLOAT;
    case FIELD_TYPE_DOUBLE:
      return GEM_DOUBLE;
    case FIELD_TYPE_TINY_BLOB:
      return GEM_TINYBLOB;
    case FIELD_TYPE_MEDIUM_BLOB:
      return GEM_MEDIUMBLOB;
    case FIELD_TYPE_LONG_BLOB:
      return GEM_LONGBLOB;
    case FIELD_TYPE_BLOB:
      return GEM_BLOB;
    case FIELD_TYPE_VAR_STRING:
    case FIELD_TYPE_STRING:
      return GEM_CHAR;
  }
  return -1;
}

/*****************************************************************************
** Gemini tables
*****************************************************************************/

const char **ha_gemini::bas_ext() const
{ static const char *ext[]= { ha_gemini_ext, ha_gemini_idx_ext, NullS };
  return ext;
}


int ha_gemini::open(const char *name, int mode, uint test_if_locked)
{
  dsmObject_t  tableId = 0;
  THD          *thd;
  char name_buff[FN_REFLEN];
  char tabname_buff[FN_REFLEN];
  char dbname_buff[FN_REFLEN];
  unsigned     i,nameLen;
  LONG         txNumber;  
  dsmStatus_t  rc;

  DBUG_ENTER("ha_gemini::open");

  thd = current_thd;
  thr_lock_init(&alock);
  thr_lock_data_init(&alock,&lock,(void*)0);
  ref_length = sizeof(dsmRecid_t);

  if(thd->gemini.context == NULL)
  {
    /* Need to get this thread a connection into the database */
    rc = gemini_connect(thd);
    if(rc)
      return rc;
  }
  if (!(rec_buff=my_malloc(table->rec_buff_length,
                           MYF(MY_WME))))
  {
    DBUG_RETURN(1);
  }

  /* separate out the name of the table and the database (a VST must be
  ** created in the mysql database)
  */
  rc = gemini_parse_table_name(name, dbname_buff, tabname_buff);
  if (rc == 0)
  {
    if (strcmp(dbname_buff, "mysql") == 0)
    {
      tableId = gemini_is_vst(tabname_buff);
    }
  }
  sprintf(name_buff, "%s.%s", dbname_buff, tabname_buff);

  /* if it's not a VST, get the table number the regular way */
  if (!tableId)
  {
    rc = dsmObjectNameToNum((dsmContext_t *)thd->gemini.context, 
                            (dsmText_t *)name_buff,
                            &tableId);
  }
  tableNumber = tableId;
  
  if(!rc)
    rc = index_open(name_buff);
 
  fixed_length_row=!(table->db_create_options & HA_OPTION_PACK_RECORD);
  key_read = 0;
  using_ignore = 0;

  /* Get the gemini table status -- we want to know if the table
     crashed while being in the midst of a repair operation */
  rc = dsmTableStatus((dsmContext_t *)thd->gemini.context,
                       tableNumber,&tableStatus);
  if(tableStatus)
    tableStatus = HA_ERR_CRASHED;
  DBUG_RETURN (rc);
}

/* Look up and store the object numbers for the indexes on this table */
int ha_gemini::index_open(char *tableName)
{
  dsmStatus_t  rc = 0;
  int          nameLen;

  DBUG_ENTER("ha_gemini::index_open");
  if(table->keys)
  {
    THD   *thd = current_thd;
    dsmObject_t  objectNumber; 
    if (!(pindexNumbers=(dsmIndex_t *)my_malloc(table->keys*sizeof(dsmIndex_t),
                           MYF(MY_WME))))
    {
      DBUG_RETURN(1);
    }
    nameLen = strlen(tableName);
    tableName[nameLen] = '.';
    nameLen++;
    
    for( uint i = 0; i < table->keys && !rc; i++)
    {
      strcpy(&tableName[nameLen],table->key_info[i].name);
      rc = dsmObjectNameToNum((dsmContext_t *)thd->gemini.context,
                          (dsmText_t *)tableName,
                          &objectNumber); 
      pindexNumbers[i] = objectNumber;
    }
  }
  else
    pindexNumbers = 0;
     
  DBUG_RETURN(rc);
}

int ha_gemini::close(void)
{
  DBUG_ENTER("ha_gemini::close");
  thr_lock_delete(&alock);
  my_free(rec_buff,MYF(MY_ALLOW_ZERO_PTR));
  rec_buff = 0;
  my_free((char *)pindexNumbers,MYF(MY_ALLOW_ZERO_PTR));
  pindexNumbers = 0;
  DBUG_RETURN(0);
}


int ha_gemini::write_row(byte * record)
{
  int error = 0;
  dsmRecord_t dsmRecord;
  THD         *thd;

  DBUG_ENTER("write_row");

  if(tableStatus)
    DBUG_RETURN(tableStatus);

  thd = current_thd;

  statistic_increment(ha_write_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(record+table->time_stamp-1);

  if(thd->gemini.needSavepoint || using_ignore)
  {
    thd->gemini.savepoint++;
    error = dsmTransaction((dsmContext_t *)thd->gemini.context,
                           &thd->gemini.savepoint,
                           DSMTXN_SAVE, 0, 0);
    if (error)
       DBUG_RETURN(error);
    thd->gemini.needSavepoint = 0;
  }

  if (table->next_number_field && record == table->record[0])
  {
    if(thd->next_insert_id)
    {
      ULONG64  nr;
       /* A set insert-id statement so set the auto-increment value if this
          value is higher than it's current value      */
       error = dsmTableAutoIncrement((dsmContext_t *)thd->gemini.context,
                                  tableNumber, (ULONG64 *)&nr);
       if(thd->next_insert_id > nr)
       {
         error = dsmTableAutoIncrementSet((dsmContext_t *)thd->gemini.context,tableNumber,
                                        (ULONG64)thd->next_insert_id);
       }
     }
       
     update_auto_increment();
  }

  dsmRecord.table = tableNumber;
  dsmRecord.maxLength = table->reclength;

  if ((error=pack_row((byte **)&dsmRecord.pbuffer, (int *)&dsmRecord.recLength,
                      record)))
    DBUG_RETURN(error);

  error = dsmRecordCreate((dsmContext_t *)thd->gemini.context,
                          &dsmRecord,0);

  if(!error)
  {
    error = handleIndexEntries(record, dsmRecord.recid,KEY_CREATE);
    if(error == HA_ERR_FOUND_DUPP_KEY && using_ignore)
    {
       dsmStatus_t rc;
       rc = dsmTransaction((dsmContext_t *)thd->gemini.context,
                    &thd->gemini.savepoint,DSMTXN_UNSAVE,0,NULL);
       thd->gemini.needSavepoint = 1;
    }
  }

  DBUG_RETURN(error);
}

longlong ha_gemini::get_auto_increment()
{
  longlong nr;
  int      error;
  THD      *thd=current_thd;

  error = dsmTableAutoIncrement((dsmContext_t *)thd->gemini.context,
                                  tableNumber, (ULONG64 *)&nr);
  return nr;
}

/* Put or delete index entries for a row                          */
int ha_gemini::handleIndexEntries(const byte * record, dsmRecid_t recid,
                                  enum_key_string_options option)
{
  dsmStatus_t rc = 0;

  DBUG_ENTER("handleIndexEntries");

  for (uint i = 0; i < table->keys && rc == 0; i++)
  {
    rc = handleIndexEntry(record, recid,option, i);
  }
  DBUG_RETURN(rc);
}

int ha_gemini::handleIndexEntry(const byte * record, dsmRecid_t recid,
                                  enum_key_string_options option,uint keynr)
{
  dsmStatus_t rc = 0;
  KEY  *key_info;
  int   keyStringLen;
  bool  thereIsAnull;
  THD   *thd;

  AUTOKEY(theKey,keyBufSize);

  DBUG_ENTER("handleIndexEntry");

  thd = current_thd;
  key_info=table->key_info+keynr;
  thereIsAnull = false;
  rc = createKeyString(record, key_info, theKey.akey.keystr,
                          sizeof(theKey.apad),&keyStringLen,
                          (short)pindexNumbers[keynr],
                           &thereIsAnull);
  if(!rc)
  {
    theKey.akey.index = pindexNumbers[keynr];
    theKey.akey.keycomps = (COUNT)key_info->key_parts;

    /* We have to subtract three here since cxKeyPrepare
       expects that the three lead bytes of the header are
       not counted in this length -- But cxKeyPrepare also
       expects that these three bytes are present in the keystr */
    theKey.akey.keyLen = (COUNT)keyStringLen - 3;
    theKey.akey.unknown_comp = thereIsAnull;
    theKey.akey.word_index = 0;
    theKey.akey.descending_key =0;
    if(option == KEY_CREATE)
    {
      rc = dsmKeyCreate((dsmContext_t *)thd->gemini.context, &theKey.akey,
                        (dsmTable_t)tableNumber, recid, NULL);
      if(rc == DSM_S_IXDUPKEY)
      {
        last_dup_key=keynr;
        rc = HA_ERR_FOUND_DUPP_KEY;
      }
    }
    else if(option == KEY_DELETE)
    {
      rc = dsmKeyDelete((dsmContext_t *)thd->gemini.context, &theKey.akey,
                        (dsmTable_t)tableNumber, recid, 0, NULL);
    }
    else
    {
      /* KEY_CHECK  */
      dsmCursid_t  aCursorId;
      int          error;

      rc = dsmCursorCreate((dsmContext_t *)thd->gemini.context,
			   (dsmTable_t)tableNumber,
			   (dsmIndex_t)pindexNumbers[keynr],
			   &aCursorId,NULL);
                           
      rc = dsmCursorFind((dsmContext_t *)thd->gemini.context,
			 &aCursorId,&theKey.akey,NULL,DSMDBKEY,
			 DSMFINDFIRST,DSM_LK_SHARE,0,
			 &lastRowid,0);
      error = dsmCursorDelete((dsmContext_t *)thd->gemini.context,
                          &aCursorId, 0);

    }
  }
  DBUG_RETURN(rc);
}

int ha_gemini::createKeyString(const byte * record, KEY *pkeyinfo,
                               unsigned char *pkeyBuf, int bufSize,
                               int  *pkeyStringLen, 
                               short geminiIndexNumber, 
                               bool  *thereIsAnull)
{
  dsmStatus_t  rc = 0;
  int          componentLen;
  int          fieldType;
  int          isNull;

  KEY_PART_INFO *key_part;

  DBUG_ENTER("createKeyString");
  
  rc = gemKeyInit(pkeyBuf,pkeyStringLen, geminiIndexNumber);

  for(uint i = 0; i < pkeyinfo->key_parts && rc == 0; i++)
  {
    unsigned char *pos;

    key_part = pkeyinfo->key_part + i;
    fieldType = gemDataType(key_part->field->type());
    if(fieldType == GEM_CHAR)
    {
      /* Save the current ptr to the field in case we're building a key
         to remove an old key value when an indexed character column 
         gets updated.                                                 */
      char *ptr = key_part->field->ptr;
      key_part->field->ptr = (char *)record + key_part->offset;
      key_part->field->sort_string(rec_buff, key_part->length);
      key_part->field->ptr = ptr;
      pos = (unsigned char *)rec_buff;
    }
    else
    {
      pos = (unsigned char *)record + key_part->offset;
    }

    isNull = record[key_part->null_offset] & key_part->null_bit;
    if(isNull)
      *thereIsAnull = true;

    rc = gemFieldToIdxComponent(pos,
                                (unsigned long) key_part->length,
                                fieldType,
                                isNull ,
                                key_part->field->flags & UNSIGNED_FLAG,
                                pkeyBuf + *pkeyStringLen,
                                bufSize,
                                &componentLen);
    *pkeyStringLen += componentLen;
  }  
  DBUG_RETURN(rc);
} 


int ha_gemini::update_row(const byte * old_record, byte * new_record)
{
  int error = 0;
  dsmRecord_t dsmRecord;
  unsigned long savepoint;
  THD         *thd = current_thd;
  DBUG_ENTER("update_row");

  statistic_increment(ha_update_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(new_record+table->time_stamp-1);

  if(thd->gemini.needSavepoint || using_ignore)
  {
    thd->gemini.savepoint++;
    error = dsmTransaction((dsmContext_t *)thd->gemini.context,
                           &thd->gemini.savepoint,
                           DSMTXN_SAVE, 0, 0);
    if (error)
       DBUG_RETURN(error);
    thd->gemini.needSavepoint = 0;
  }
  for (uint keynr=0 ; keynr < table->keys ; keynr++)
  {
    if(key_cmp(keynr,old_record, new_record))
    {
      error = handleIndexEntry(old_record,lastRowid,KEY_DELETE,keynr);
      if(error)
        DBUG_RETURN(error);
      error = handleIndexEntry(new_record, lastRowid, KEY_CREATE, keynr);
      if(error)
      {
        if (using_ignore && error == HA_ERR_FOUND_DUPP_KEY)
        {
           dsmStatus_t rc;
           rc = dsmTransaction((dsmContext_t *)thd->gemini.context,
                        &thd->gemini.savepoint,DSMTXN_UNSAVE,0,NULL);
           thd->gemini.needSavepoint = 1;
        }
        DBUG_RETURN(error);
      }
    }
  }

  dsmRecord.table = tableNumber;
  dsmRecord.recid = lastRowid;
  dsmRecord.maxLength = table->reclength;

  if ((error=pack_row((byte **)&dsmRecord.pbuffer, (int *)&dsmRecord.recLength,
                      new_record)))
  {
    DBUG_RETURN(error);
  }
  error = dsmRecordUpdate((dsmContext_t *)thd->gemini.context,
                          &dsmRecord, 0, NULL);
                          
  DBUG_RETURN(error);
}


int ha_gemini::delete_row(const byte * record)
{
  int error = 0;
  dsmRecord_t  dsmRecord;
  THD         *thd = current_thd;
  DBUG_ENTER("delete_row");

  statistic_increment(ha_delete_count,&LOCK_status);

  if(thd->gemini.needSavepoint)
  {
    thd->gemini.savepoint++;
    error = dsmTransaction((dsmContext_t *)thd->gemini.context,
                           &thd->gemini.savepoint,
                           DSMTXN_SAVE, 0, 0);
    if (error)
       DBUG_RETURN(error);
    thd->gemini.needSavepoint = 0;
  }

  dsmRecord.table = tableNumber;
  dsmRecord.recid = lastRowid;

  error = handleIndexEntries(record, dsmRecord.recid,KEY_DELETE);
  if(!error)
  {
    error = dsmRecordDelete((dsmContext_t *)thd->gemini.context,
                          &dsmRecord, 0, NULL);
  }

  DBUG_RETURN(error);
}  

int ha_gemini::index_init(uint keynr)
{
  int error = 0;
  int keyStringLen;
  THD *thd;
  DBUG_ENTER("index_init");
  thd = current_thd;

  lastRowid = 0;
  active_index=keynr;
  error = dsmCursorCreate((dsmContext_t *)thd->gemini.context,
                           (dsmTable_t)tableNumber, 
                           (dsmIndex_t)pindexNumbers[keynr],
                           &cursorId,NULL); 
  pbracketBase = (dsmKey_t *)my_malloc(sizeof(dsmKey_t) + keyBufSize,
                           MYF(MY_WME));
  if(!pbracketBase)
    DBUG_RETURN(1);
  pbracketLimit = (dsmKey_t *)my_malloc(sizeof(dsmKey_t) + keyBufSize,MYF(MY_WME));
  if(!pbracketLimit)
  {
    my_free((char *)pbracketLimit,MYF(0));
    DBUG_RETURN(1);
  }
  pbracketBase->index = 0;
  pbracketLimit->index = (dsmIndex_t)pindexNumbers[keynr];
  pbracketLimit->keycomps = 1;
  keyStringLen = 0;
  error = gemKeyHigh(pbracketLimit->keystr, &keyStringLen,
                     pbracketLimit->index);
  
  /* We have to subtract three here since cxKeyPrepare
     expects that the three lead bytes of the header are
     not counted in this length -- But cxKeyPrepare also
     expects that these three bytes are present in the keystr */
  pbracketLimit->keyLen = (COUNT)keyStringLen - 3;

  pbracketBase->descending_key = pbracketLimit->descending_key = 0;
  pbracketBase->ksubstr = pbracketLimit->ksubstr = 0;

  pfoundKey = (dsmKey_t *)my_malloc(sizeof(dsmKey_t) + keyBufSize,MYF(MY_WME));
  if(!pfoundKey)
  {
    my_free((char *)pbracketLimit,MYF(0));
    my_free((char *)pbracketBase,MYF(0));
    DBUG_RETURN(1);
  }

  DBUG_RETURN(error);
}

int ha_gemini::index_end()
{
  int error = 0;
  THD *thd;
  DBUG_ENTER("index_end");
  thd = current_thd;
  error = dsmCursorDelete((dsmContext_t *)thd->gemini.context,
                          &cursorId, 0);
  if(pbracketLimit)
    my_free((char *)pbracketLimit,MYF(0));
  if(pbracketBase)
    my_free((char *)pbracketBase,MYF(0));
  if(pfoundKey)
    my_free((char *)pfoundKey,MYF(0));

  pbracketLimit = 0;
  pbracketBase = 0;
  pfoundKey = 0;
  DBUG_RETURN(error);
}

/* This is only used to read whole keys */

int ha_gemini::index_read_idx(byte * buf, uint keynr, const byte * key,
                              uint key_len, enum ha_rkey_function find_flag)
{
  int error = 0;
  DBUG_ENTER("index_read_idx");
  statistic_increment(ha_read_key_count,&LOCK_status);

  error = index_init(keynr);
  if (!error)
    error = index_read(buf,key,key_len,find_flag);

  if(error == HA_ERR_END_OF_FILE)
    error = HA_ERR_KEY_NOT_FOUND;

  table->status = error ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(error);
}

int ha_gemini::pack_key( uint keynr, dsmKey_t *pkey, 
                           const byte *key_ptr, uint key_length)
{
  KEY *key_info=table->key_info+keynr;
  KEY_PART_INFO *key_part=key_info->key_part;
  KEY_PART_INFO *end=key_part+key_info->key_parts;
  int rc;
  int componentLen;
  DBUG_ENTER("pack_key");

  rc = gemKeyInit(pkey->keystr,&componentLen,
             (short)pindexNumbers[active_index]);
  pkey->keyLen = componentLen;

  for (; key_part != end && (int) key_length > 0 && !rc; key_part++)
  {
    uint offset=0;
    unsigned char *pos;

    int fieldType; 
    if (key_part->null_bit)
    {
      offset=1;
      if (*key_ptr != 0)         // Store 0 if NULL
      {
        key_length-= key_part->store_length;
        key_ptr+=   key_part->store_length;
        rc = gemFieldToIdxComponent(
                 (unsigned char *)key_ptr + offset,
                                 (unsigned long) key_part->length,
                                  0,
                                  1 ,  /* Tells it to build a null component */
                                  key_part->field->flags & UNSIGNED_FLAG,
                                  pkey->keystr + pkey->keyLen,
                                  keyBufSize,
                                  &componentLen);
        pkey->keyLen += componentLen;        
        continue;
      }
    }
    fieldType = gemDataType(key_part->field->type());
    if(fieldType == GEM_CHAR)
    {
      key_part->field->store(key_ptr + offset, key_part->length); 
      key_part->field->sort_string(rec_buff, key_part->length);
      pos = (unsigned char *)rec_buff;
    }
    else
    {
      pos = (unsigned char *)key_ptr + offset;
    }

    rc = gemFieldToIdxComponent(
                                 pos,
                                 (unsigned long) key_part->length,
                                  fieldType,
                                  0 ,
                                  key_part->field->flags & UNSIGNED_FLAG,
                                  pkey->keystr + pkey->keyLen,
                                  keyBufSize,
                                  &componentLen);

    key_ptr+=key_part->store_length;
    key_length-=key_part->store_length;
    pkey->keyLen += componentLen;
  }
  DBUG_RETURN(rc);
}

void ha_gemini::unpack_key(char *record, dsmKey_t *key, uint index)
{
  KEY *key_info=table->key_info+index;
  KEY_PART_INFO *key_part= key_info->key_part,
                *end=key_part+key_info->key_parts;
  int fieldIsNull, fieldType;
  int rc = 0;

  char unsigned *pos= &key->keystr[7];

  for ( ; key_part != end; key_part++)
  {
    fieldType = gemDataType(key_part->field->type());
    if(fieldType == GEM_CHAR)
    {
      /* Can't get data from character indexes since the sort weights
         are in the index and not the characters.   */
      key_read = 0;
    }
    rc = gemIdxComponentToField(pos, fieldType,
                                (unsigned char *)record + key_part->field->offset(),
                                key_part->field->field_length,
                                key_part->field->decimals(),
                                &fieldIsNull);
    if(fieldIsNull)
    {
      record[key_part->null_offset] |= key_part->null_bit;
    }
    else if (key_part->null_bit)
    {
      record[key_part->null_offset]&= ~key_part->null_bit;
    }
    while(*pos++);       /* Advance to next field in key by finding */
                         /*   a null byte                             */
  }
}

int ha_gemini::index_read(byte * buf, const byte * key,
			    uint key_len, enum ha_rkey_function find_flag)
{
  int error = 0;
  THD   *thd;
  int componentLen;
 
  DBUG_ENTER("index_read");
  statistic_increment(ha_read_key_count,&LOCK_status);


  pbracketBase->index = (short)pindexNumbers[active_index];
  pbracketBase->keycomps =  1;
  

  /* Its a greater than operation so create a base bracket
       from the input key data.                              */
  error = pack_key(active_index, pbracketBase, key, key_len);
  if(error)
    goto errorReturn;

  if(find_flag == HA_READ_AFTER_KEY)
  {
    /* A greater than operation      */
    error = gemKeyAddLow(pbracketBase->keystr + pbracketBase->keyLen,
                         &componentLen);
    pbracketBase->keyLen += componentLen;
  }
  if(find_flag == HA_READ_KEY_EXACT)
  {
    /* Need to set up a high bracket for an equality operator 
       Which is a copy of the base bracket plus a hi lim term */
    bmove(pbracketLimit,pbracketBase,(size_t)pbracketBase->keyLen + sizeof(dsmKey_t));
    error = gemKeyAddHigh(pbracketLimit->keystr + pbracketLimit->keyLen,
                          &componentLen);
    if(error)
      goto errorReturn;
    pbracketLimit->keyLen += componentLen;
  }
  else
  {
    /* Always add a high range -- except for HA_READ_KEY_EXACT this
       is all we need for the upper index bracket    */
    error = gemKeyHigh(pbracketLimit->keystr, &componentLen,
		       pbracketLimit->index);
  
    pbracketLimit->keyLen = componentLen;
  }
  /* We have to subtract three here since cxKeyPrepare
     expects that the three lead bytes of the header are
     not counted in this length -- But cxKeyPrepare also
     expects that these three bytes are present in the keystr */
  pbracketBase->keyLen -= 3;
  pbracketLimit->keyLen -= 3;

  thd = current_thd;

  error = findRow(thd, DSMFINDFIRST, buf);

errorReturn:
  if (error == DSM_S_ENDLOOP)
    error = HA_ERR_KEY_NOT_FOUND;
 
  table->status = error ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(error);
}


int ha_gemini::index_next(byte * buf)
{
  THD  *thd;
  int error = 1;
  int keyStringLen=0;
  dsmMask_t   findMode;
  DBUG_ENTER("index_next");

  if(tableStatus)
    DBUG_RETURN(tableStatus);

  thd = current_thd;
  
  if(pbracketBase->index == 0)
  {
    error = gemKeyLow(pbracketBase->keystr, &keyStringLen, 
                      pbracketLimit->index);
    
    pbracketBase->keyLen = (COUNT)keyStringLen - 3;
    pbracketBase->index = pbracketLimit->index;
    pbracketBase->keycomps =  1;
    findMode = DSMFINDFIRST;
  }    
  else
    findMode = DSMFINDNEXT;

  error = findRow(thd,findMode,buf);

  if (error == DSM_S_ENDLOOP)
    error = HA_ERR_END_OF_FILE;

  table->status = error ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(error);
}

int ha_gemini::index_next_same(byte * buf, const byte *key, uint keylen)
{
  int error = 0;
  DBUG_ENTER("index_next_same");
  statistic_increment(ha_read_next_count,&LOCK_status);
  DBUG_RETURN(index_next(buf));
}


int ha_gemini::index_prev(byte * buf)
{
  int error = 0;
  THD *thd = current_thd;

  DBUG_ENTER("index_prev");
  statistic_increment(ha_read_prev_count,&LOCK_status);

  error = findRow(thd, DSMFINDPREV, buf);

  if (error == DSM_S_ENDLOOP)
    error = HA_ERR_END_OF_FILE;


  table->status = error ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(error);
}
  

int ha_gemini::index_first(byte * buf)
{
  DBUG_ENTER("index_first");
  statistic_increment(ha_read_first_count,&LOCK_status);
  DBUG_RETURN(index_next(buf));
}

int ha_gemini::index_last(byte * buf)
{
  int error = 0;
  THD  *thd;
  int keyStringLen;
  dsmMask_t   findMode;
  thd = current_thd;
 
  DBUG_ENTER("index_last");
  statistic_increment(ha_read_last_count,&LOCK_status);

  error = gemKeyLow(pbracketBase->keystr, &keyStringLen,
                      pbracketLimit->index);
  if(error)
    goto errorReturn;

  pbracketBase->keyLen = (COUNT)keyStringLen - 3;
  pbracketBase->index = pbracketLimit->index;
  pbracketBase->keycomps =  1;

  error = findRow(thd,DSMFINDLAST,buf);

errorReturn:
  if (error == DSM_S_ENDLOOP)
    error = HA_ERR_END_OF_FILE;

  table->status = error ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(error);
  
  table->status = error ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(error);
}

int ha_gemini::rnd_init(bool scan)
{
  THD  *thd = current_thd;

  lastRowid = 0;

  return 0;
}

int ha_gemini::rnd_end()
{
/*
  return gem_scan_end();
*/
  return 0;
}

int ha_gemini::rnd_next(byte *buf)
{
  int error = 0;
  dsmRecord_t  dsmRecord;
  THD  *thd;

  DBUG_ENTER("rnd_next");

  if(tableStatus)
    DBUG_RETURN(tableStatus);

  thd = current_thd;
  if(thd->gemini.tx_isolation == ISO_READ_COMMITTED && !(lockMode & DSM_LK_EXCL)
     && lastRowid)
    error = dsmObjectUnlock((dsmContext_t *)thd->gemini.context,
			    tableNumber, DSMOBJECT_RECORD, lastRowid, 
			    lockMode | DSM_UNLK_FREE, 0);

  statistic_increment(ha_read_rnd_next_count,&LOCK_status);
  dsmRecord.table = tableNumber;
  dsmRecord.recid = lastRowid;
  dsmRecord.pbuffer = (dsmBuffer_t *)rec_buff;
  dsmRecord.recLength = table->reclength;
  dsmRecord.maxLength = table->reclength;

  error = dsmTableScan((dsmContext_t *)thd->gemini.context,
                       &dsmRecord, DSMFINDNEXT, lockMode, 0);

  if(!error)
  {
     lastRowid = dsmRecord.recid;
     unpack_row((char *)buf,(char *)dsmRecord.pbuffer);
  }     
  if(!error)
    ;
  else if (error == DSM_S_ENDLOOP)
    error = HA_ERR_END_OF_FILE;
  else if (error ==  DSM_S_RQSTREJ)
    error = HA_ERR_LOCK_WAIT_TIMEOUT;
  else if (error == DSM_S_LKTBFULL)
    error = HA_ERR_LOCK_TABLE_FULL;

  table->status = error ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(error);
}


int ha_gemini::rnd_pos(byte * buf, byte *pos)
{
  int error;
  int rc;

  THD *thd;

  statistic_increment(ha_read_rnd_count,&LOCK_status);
  thd = current_thd;
  memcpy((void *)&lastRowid,pos,ref_length);
  if(thd->gemini.tx_isolation == ISO_READ_COMMITTED && !(lockMode & DSM_LK_EXCL))
  {
    /* Lock the row      */

    error = dsmObjectLock((dsmContext_t *)thd->gemini.context,
                       (dsmObject_t)tableNumber,DSMOBJECT_RECORD,lastRowid,
                       lockMode, 1, 0);
    if ( error ) 
      goto errorReturn;
  }
  error = fetch_row(thd->gemini.context, buf);  
  if(thd->gemini.tx_isolation == ISO_READ_COMMITTED && !(lockMode & DSM_LK_EXCL))
  {
    /* Unlock the row      */

    rc = dsmObjectUnlock((dsmContext_t *)thd->gemini.context,
                       (dsmObject_t)tableNumber,DSMOBJECT_RECORD,lastRowid,
                       lockMode | DSM_UNLK_FREE , 0);
  }
  if(error == DSM_S_RMNOTFND)
    error = HA_ERR_RECORD_DELETED;

 errorReturn:
  table->status = error ? STATUS_NOT_FOUND : 0;
  return error;
}

int ha_gemini::fetch_row(void *gemini_context,const byte *buf)
{
  dsmStatus_t  rc = 0;
  dsmRecord_t  dsmRecord;

  DBUG_ENTER("fetch_row");
  dsmRecord.table = tableNumber;
  dsmRecord.recid = lastRowid;
  dsmRecord.pbuffer = (dsmBuffer_t *)rec_buff;
  dsmRecord.recLength = table->reclength;
  dsmRecord.maxLength = table->reclength;

  rc = dsmRecordGet((dsmContext_t *)gemini_context,
                       &dsmRecord, 0);

  if(!rc)
  {
     unpack_row((char *)buf,(char *)dsmRecord.pbuffer);
  }

   DBUG_RETURN(rc);
}
int ha_gemini::findRow(THD *thd, dsmMask_t findMode, byte *buf)
{
  dsmStatus_t rc;
  dsmKey_t    *pkey;

  DBUG_ENTER("findRow"); 

  if(thd->gemini.tx_isolation == ISO_READ_COMMITTED && !(lockMode & DSM_LK_EXCL)
     && lastRowid)
    rc = dsmObjectUnlock((dsmContext_t *)thd->gemini.context,
			    tableNumber, DSMOBJECT_RECORD, lastRowid, 
			    lockMode | DSM_UNLK_FREE, 0);
  if( key_read )
    pkey = pfoundKey;
  else
    pkey = 0;
 
  rc = dsmCursorFind((dsmContext_t *)thd->gemini.context,
                        &cursorId,
                        pbracketBase,
                        pbracketLimit,
                        DSMPARTIAL,
                        findMode,
                        lockMode,
                        NULL,
                        &lastRowid,
                        pkey);
  if( rc )
    goto errorReturn;

  if(key_read)
  {
    unpack_key(buf, pkey, active_index);
  }
  if(!key_read)  /* unpack_key may have turned off key_read  */
  {
    rc = fetch_row((dsmContext_t *)thd->gemini.context,buf);
  }

errorReturn:
  if(!rc)
    ;
  else if(rc ==  DSM_S_RQSTREJ)
    rc = HA_ERR_LOCK_WAIT_TIMEOUT;
  else if (rc == DSM_S_LKTBFULL)
    rc = HA_ERR_LOCK_TABLE_FULL;

 DBUG_RETURN(rc);
}

void ha_gemini::position(const byte *record)
{
  memcpy(ref,&lastRowid,ref_length);
}


void ha_gemini::info(uint flag)
{
  DBUG_ENTER("info");

  if ((flag & HA_STATUS_VARIABLE))
  {
    THD  *thd = current_thd;
    dsmStatus_t error;
    ULONG64     rows;

    error = dsmRowCount((dsmContext_t *)thd->gemini.context,tableNumber,&rows);
    records = (ha_rows)rows;
    deleted = 0;
  }
  else if ((flag & HA_STATUS_CONST))
  {
    ;
  }
  else if ((flag & HA_STATUS_ERRKEY))
  {
    errkey=last_dup_key;
  }
  else if ((flag & HA_STATUS_TIME))
  {
    ;
  }
  else if ((flag & HA_STATUS_AUTO))
  {
    ;
  }

  DBUG_VOID_RETURN;
}


int ha_gemini::extra(enum ha_extra_function operation)
{
  switch (operation)
  {
  case HA_EXTRA_RESET:
  case HA_EXTRA_RESET_STATE:
    key_read=0;
    using_ignore=0;
    break;
  case HA_EXTRA_KEYREAD:
    key_read=1;					// Query satisfied with key
    break;
  case HA_EXTRA_NO_KEYREAD:
    key_read=0;
    break;
  case HA_EXTRA_IGNORE_DUP_KEY:
    using_ignore=1;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    using_ignore=0;
    break;

  default:
    break;
  }
  return 0;
}


int ha_gemini::reset(void)
{
  key_read=0;					// Reset to state after open
  return 0;
}


/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
*/

int ha_gemini::external_lock(THD *thd, int lock_type)
{
  dsmStatus_t rc = 0;
  LONG        txNumber;

  DBUG_ENTER("ha_gemini::external_lock");

  if (lock_type != F_UNLCK)
  {
    if (!thd->gemini.lock_count)
    {
      thd->gemini.lock_count = 1;
      thd->gemini.tx_isolation = thd->tx_isolation;
    }
    
    if(thd->gemini.context == NULL)
    {
      /* Need to get this thread a connection into the database */
      rc = gemini_connect(thd);
      if(rc)
        return rc;
    }
    /* Set need savepoint flag */
    thd->gemini.needSavepoint = 1;

    if(rc)
      DBUG_RETURN(rc);


    if( thd->in_lock_tables || thd->gemini.tx_isolation == ISO_SERIALIZABLE )
    {
      rc = dsmObjectLock((dsmContext_t *)thd->gemini.context,
			 (dsmObject_t)tableNumber,DSMOBJECT_TABLE,0,
			 lockMode, 1, 0);
    } 
  }
  else /* lock_type == F_UNLK  */
  {
    /* Commit the tx if we're in auto-commit mode */
    if (!(thd->options & OPTION_NOT_AUTO_COMMIT)&&
	!(thd->options & OPTION_BEGIN))
      gemini_commit(thd);
  }

  DBUG_RETURN(rc);
}  


THR_LOCK_DATA **ha_gemini::store_lock(THD *thd, THR_LOCK_DATA **to,
					enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /* If we are not doing a LOCK TABLE, then allow multiple writers */
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
	 lock_type <= TL_WRITE) &&
	!thd->in_lock_tables)
      lock_type = TL_WRITE_ALLOW_WRITE;
    lock.type=lock_type;

    if(thd->gemini.tx_isolation == ISO_READ_UNCOMMITTED)
      lockMode = DSM_LK_NOLOCK;
    else if(table->reginfo.lock_type > TL_WRITE_ALLOW_READ)
       lockMode = DSM_LK_EXCL;
    else
       lockMode = DSM_LK_SHARE;
  }
  *to++= &lock;
  return to;
}


int ha_gemini::create(const char *name, register TABLE *form,
                      HA_CREATE_INFO *create_info)
{
  THD *thd;
  char name_buff[FN_REFLEN];
  char dbname_buff[FN_REFLEN];
  DBUG_ENTER("ha_gemini::create");
  dsmContext_t  *pcontext;
  dsmStatus_t   rc;
  dsmArea_t     areaNumber;
  dsmObject_t   tableNumber = 0;
  dsmDbkey_t    dummy = 0;
  unsigned      i;
  int           baseNameLen;
  dsmObject_t   indexNumber;
 
  /* separate out the name of the table and the database (a VST must be
  ** created in the mysql database)
  */
  rc = gemini_parse_table_name(name, dbname_buff, name_buff);
  if (rc == 0)
  {
    /* If the table is a VST, don't create areas or extents */
    if (strcmp(dbname_buff, "mysql") == 0)
    {
      tableNumber = gemini_is_vst(name_buff);
      if (tableNumber)
      {
        return 0;
      }
    }
  }

  thd = current_thd;
  if(thd->gemini.context == NULL)
  {
    /* Need to get this thread a connection into the database */
    rc = gemini_connect(thd);
    if(rc)
      return rc;
  }
  pcontext = (dsmContext_t *)thd->gemini.context;

  if(thd->gemini.needSavepoint || using_ignore)
  {
    thd->gemini.savepoint++;
    rc = dsmTransaction((dsmContext_t *)thd->gemini.context,
                           &thd->gemini.savepoint,
                           DSMTXN_SAVE, 0, 0);
    if (rc)
       DBUG_RETURN(rc);
    thd->gemini.needSavepoint = 0;
  }

  fn_format(name_buff, name, "", ha_gemini_ext, 2 | 4);
  /* Create a storage area                               */
  rc = dsmAreaNew(pcontext,gemini_blocksize,DSMAREA_TYPE_DATA,
                  &areaNumber, gemini_recbits,
		(dsmText_t *)"gemini_data_area");
  if( rc != 0 )
  {
      printf("dsmAreaNew failed %ld\n",rc);
      return(rc);
  }
  
  /* Create an extent                                    */
  /* Don't pass in leading ./ in name_buff                  */
  rc = dsmExtentCreate(pcontext,areaNumber,1,15,5,
                       (dsmText_t *)&name_buff[start_of_name]);
  if( rc != 0 )
  {
      printf("dsmExtentCreate failed %ld\n",rc);
      return(rc);
  }

  /* Create the table storage object                     */
  /* Change slashes in the name to periods               */
  for(  i = 0; i < strlen(name_buff); i++)
    if(name_buff[i] == '/' || name_buff[i] == '\\')
      name_buff[i] = '.';
 
  /* Get rid of .gmd suffix      */
  name_buff[strlen(name_buff) - 4] = '\0';
 
  rc = dsmObjectCreate(pcontext, areaNumber, &tableNumber, 
                       DSMOBJECT_MIXTABLE,0,0,0,
		       (dsmText_t *)&name_buff[start_of_name],
                       &dummy,&dummy);

  if(rc == 0 && form->keys)
  {
    fn_format(name_buff, name, "", ha_gemini_idx_ext, 2 | 4); 
       /* Create a storage area                               */
    rc = dsmAreaNew(pcontext,gemini_blocksize,DSMAREA_TYPE_DATA,
                  &areaNumber, gemini_recbits,
                (dsmText_t *)"gemini_index_area");
    if( rc != 0 )
    {
        printf("dsmAreaNew failed %ld\n",rc);
        return(rc);
    } 
    /* Create an extent                                    */
    /* Don't pass in leading ./ in name_buff                  */
    rc = dsmExtentCreate(pcontext,areaNumber,1,15,5,
                       (dsmText_t *)&name_buff[start_of_name]);
    if( rc != 0 )
    {
        printf("dsmExtentCreate failed %ld\n",rc);
        return(rc);
    }
    
    /* Change slashes in the name to periods               */
    for(  i = 0; i < strlen(name_buff); i++)
        if(name_buff[i] == '/' || name_buff[i] == '\\')
          name_buff[i] = '.';

    /* Get rid of .gmi suffix      */
    name_buff[strlen(name_buff) - 4] = '\0';

    baseNameLen = strlen(name_buff);
    name_buff[baseNameLen] = '.';
    baseNameLen++;
    for( i = 0; i < form->keys; i++)
    {
       dsmObjectAttr_t  indexUnique;

       indexNumber = DSMINDEX_INVALID;    
       /* Create a storage object record for each index */
       /* Add the index name so the object name is in the form
          <db>.<table>.<index_name>                     */
       strcpy(&name_buff[baseNameLen],table->key_info[i].name);
       if(table->key_info[i].flags & HA_NOSAME)
         indexUnique = 1;
       else
         indexUnique = 0;
       rc = dsmObjectCreate(pcontext, areaNumber, &indexNumber,
                       DSMOBJECT_MIXINDEX,indexUnique,tableNumber,
                       DSMOBJECT_MIXTABLE,
                       (dsmText_t *)&name_buff[start_of_name],
                       &dummy,&dummy);
           
    }
  }
  rc = dsmTableAutoIncrementSet(pcontext,tableNumber,
                                create_info->auto_increment_value);


  
  /* Get a table lock on this table in case this table is being
     created as part of an alter table statement.  We don't want
     the alter table statement to abort because of a lock table overflow
  */
  if (thd->lex.sql_command == SQLCOM_CREATE_INDEX ||
      thd->lex.sql_command == SQLCOM_ALTER_TABLE ||
      thd->lex.sql_command == SQLCOM_DROP_INDEX)
  {
    rc = dsmObjectLock(pcontext,
                       (dsmObject_t)tableNumber,DSMOBJECT_TABLE,0,
                       DSM_LK_EXCL, 1, 0);
    /* and don't commit so we won't release the table on the table number
       of the table being altered */
  }
  else
  {
    if(!rc)
      rc = gemini_commit(thd);
  }

  DBUG_RETURN(rc);
}

int ha_gemini::delete_table(const char *pname)
{
    THD *thd;
    dsmStatus_t     rc;
    dsmContext_t   *pcontext;
    unsigned        i,nameLen;
    dsmArea_t       indexArea = 0;
    dsmArea_t       tableArea = 0;
    dsmObjectAttr_t objectAttr;
    dsmObject_t     associate;
    dsmObjectType_t associateType;
    dsmDbkey_t      block, root;
    int             need_txn = 0;
    dsmObject_t     tableNum = 0;
    char            name_buff[FN_REFLEN];
    char            dbname_buff[FN_REFLEN];
    DBUG_ENTER("ha_gemini::delete_table");
 
    /* separate out the name of the table and the database (a VST must be
    ** located in the mysql database)
    */
    rc = gemini_parse_table_name(pname, dbname_buff, name_buff);
    if (rc == 0)
    {
      /* If the table is a VST, there are no areas or extents to delete */
      if (strcmp(dbname_buff, "mysql") == 0)
      { 
        tableNum = gemini_is_vst(name_buff);
        if (tableNum)
        {
          return 0;
        }
      }
    }

    thd = current_thd;
    if(thd->gemini.context == NULL)
    {
        /* Need to get this thread a connection into the database */
        rc = gemini_connect(thd);
        if(rc)
        {
            DBUG_RETURN(rc);
        }
    }
    pcontext = (dsmContext_t *)thd->gemini.context;


    bzero(name_buff, FN_REFLEN);

    nameLen = strlen(pname);
    for( i = start_of_name; i < nameLen; i++)
    {
        if(pname[i] == '/' || pname[i] == '\\')
            name_buff[i-start_of_name] = '.';
        else
            name_buff[i-start_of_name] = pname[i];
    }

    rc = dsmObjectNameToNum(pcontext, (dsmText_t *)name_buff, 
               (dsmObject_t *)&tableNum);
    if (rc)
    {
        printf("Cound not find table number for %s with string %s, %ld\n",
                    pname,name_buff,rc);
        rc = gemini_rollback(thd);
        if (rc)
        {
            printf("Error in rollback %ld\n",rc);
        }
        DBUG_RETURN(rc);
    }

    rc = dsmObjectInfo(pcontext, tableNum, DSMOBJECT_MIXTABLE, &tableArea,
                &objectAttr, &associate, &associateType, &block, &root);
    if (rc)
    {
        printf("Failed to get area number for table %d, %s, return %ld\n",
                 tableNum, pname, rc);
        rc = gemini_rollback(thd);
        if (rc)
        {
            printf("Error in rollback %ld\n",rc);
        }
    }
                        
    indexArea = DSMAREA_INVALID;

    /* Delete the indexes and tables storage objects for with the table */
    rc = dsmObjectDeleteAssociate(pcontext, tableNum, &indexArea);
    if (rc)
    {
        printf("Error deleting storage objects for table number %d, return %ld\n", 
                       (int)tableNum, rc);

        /* roll back txn and return */
        rc = gemini_rollback(thd);
        if (rc)
        {
            printf("Error in rollback %ld\n",rc);
        }
        DBUG_RETURN(rc);
    }

    if (indexArea != DSMAREA_INVALID)
    {
        /* Delete the extents for both Index and Table */
        rc = dsmExtentDelete(pcontext, indexArea, 0);
        rc = dsmAreaDelete(pcontext, indexArea);
        if (rc)
        {
            printf("Error deleting Index Area %ld, return %ld\n", indexArea, rc);

            /* roll back txn and return */
            rc = gemini_rollback(thd);
            if (rc)
            {
                printf("Error in rollback %ld\n",rc);
            }
            DBUG_RETURN(rc);
        }
    }

    rc = dsmExtentDelete(pcontext, tableArea, 0);
    rc = dsmAreaDelete(pcontext, tableArea);
    if (rc)
    {
        printf("Error deleting table Area %ld, name %s, return %ld\n",
                    tableArea, pname, rc);
        /* roll back txn and return */
        rc = gemini_rollback(thd);
        if (rc)
        {
            printf("Error in rollback %ld\n",rc);
        }
        DBUG_RETURN(rc);
    }


    /* Commit the transaction */
    rc = gemini_commit(thd);
    if (rc)
    {
        printf("Failed to commit transaction %ld\n",rc);
    }

  
    /* now remove all the files that need to be removed and
       cause a checkpoint so recovery will work */
    rc = dsmExtentUnlink(pcontext);

    DBUG_RETURN(0);
}


int ha_gemini::rename_table(const char *pfrom, const char *pto)
{
    THD            *thd;
    dsmContext_t   *pcontext;
    dsmStatus_t     rc;
    char            tabname_buff[FN_REFLEN];
    char            dbname_buff[FN_REFLEN];
    char            name_buff[FN_REFLEN];
    char            newname_buff[FN_REFLEN];
    char            newextname_buff[FN_REFLEN];
    char            newidxextname_buff[FN_REFLEN];
    unsigned        i, nameLen;
    dsmObject_t     tableNum;
    dsmArea_t       indexArea = 0;

    DBUG_ENTER("ha_gemini::rename_table");

    /* don't allow rename of VSTs */
    rc = gemini_parse_table_name(pfrom, dbname_buff, name_buff);
    if (rc == 0)
    {
      /* If the table is a VST, don't create areas or extents */
      if (strcmp(dbname_buff, "mysql") == 0)
      {
        if (gemini_is_vst(name_buff))
        {
          return 0;
        }
      }
    }

    thd = current_thd;
    if (thd->gemini.context == NULL)
    {
      /* Need to get this thread a connection into the database */
      rc = gemini_connect(thd);
      if (rc)
      {
        DBUG_RETURN(rc);
      }
    }

    pcontext = (dsmContext_t *)thd->gemini.context;

    /* change the slashes to dots in the old and new names */
    nameLen = strlen(pfrom);
    for( i = start_of_name; i < nameLen; i++)
    {
        if(pfrom[i] == '/' || pfrom[i] == '\\')
            name_buff[i-start_of_name] = '.';
        else
            name_buff[i-start_of_name] = pfrom[i];
    }
    name_buff[i-start_of_name] = '\0';

    nameLen = strlen(pto);
    for( i = start_of_name; i < nameLen; i++)
    {
        if(pto[i] == '/' || pto[i] == '\\')
            newname_buff[i-start_of_name] = '.';
        else
            newname_buff[i-start_of_name] = pto[i];
    }
    newname_buff[i-start_of_name] = '\0';

    /* generate new extent names (for table and index extents) */
    fn_format(newextname_buff, pto, "", ha_gemini_ext, 2 | 4);
    fn_format(newidxextname_buff, pto, "", ha_gemini_idx_ext, 2 | 4);

    rc = dsmObjectNameToNum(pcontext, (dsmText_t *)name_buff, &tableNum);
    if (rc)
        goto errorReturn;

    rc = dsmObjectRename(pcontext, tableNum,
                         (dsmText_t *)newname_buff,
                         (dsmText_t *)&newidxextname_buff[start_of_name],
                         (dsmText_t *)&newextname_buff[start_of_name],
                         &indexArea);
    if (rc)
        goto errorReturn;

    /* rename the physical table and index files (if necessary) */
    rc = rename_file_ext(pfrom, pto, ha_gemini_ext);
    if (!rc && indexArea)
    {
        rc = rename_file_ext(pfrom, pto, ha_gemini_idx_ext);
    }

errorReturn:
    DBUG_RETURN(rc);
}


/*
  How many seeks it will take to read through the table
  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/

double ha_gemini::scan_time()
{
  return records / (gemini_blocksize / table->reclength);
}

int ha_gemini::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  int error;
  int checkStatus = HA_ADMIN_OK;
  ha_rows indexCount;
  byte         *buf = 0, *indexBuf = 0;
  int		errorCount = 0;

  /* Get a shared table lock                  */
  if(thd->gemini.needSavepoint)
  {
    /* We don't really need a savepoint here but do it anyway
       just to keep the savepoint number correct.             */
    thd->gemini.savepoint++;
    error = dsmTransaction((dsmContext_t *)thd->gemini.context,
                           &thd->gemini.savepoint,
                           DSMTXN_SAVE, 0, 0);
    if (error)
       return(error);
    thd->gemini.needSavepoint = 0;
  }
  buf = my_malloc(table->rec_buff_length,MYF(MY_WME));
  indexBuf = my_malloc(table->rec_buff_length,MYF(MY_WME));
  /* Lock the table    */
  error = dsmObjectLock((dsmContext_t *)thd->gemini.context,
                        (dsmObject_t)tableNumber,
                         DSMOBJECT_TABLE,0,
                         DSM_LK_SHARE, 1, 0);
  if(error)
    return error;

  info(HA_STATUS_VARIABLE);

  /* If quick option just scan along index converting and counting entries */
  for (uint i = 0; i < table->keys; i++)
  {
    key_read = 1;
    indexCount = 0;
    error = index_init(i);
    error = index_first(indexBuf);
    while(!error)
    {
      indexCount++;
      if(!check_opt->quick)
      {  
	/* Fetch row and compare to data produced from key */
	error = fetch_row(thd->gemini.context,buf);
        if(!error)
	{
	  if(key_cmp(i,buf,indexBuf))
	  {
	    print_msg(thd,table->real_name,"check","error",
                    "Key does not match row for rowid %d for index %s",
		    lastRowid,table->key_info[i].name);
	    checkStatus = HA_ADMIN_CORRUPT;
	    errorCount++;
	    if(errorCount > 1000)
	      goto error_return;
	  }
	  else if(error == DSM_S_RMNOTFND)
	  {
	    errorCount++;
	    checkStatus = HA_ADMIN_CORRUPT;
            print_msg(thd,table->real_name,"check","error",
		      "Key does not have a valid row pointer %d for index %s",
		      lastRowid,table->key_info[i].name);
            if(errorCount > 1000)
	      goto error_return;
	    error = 0;
	  }
	}
      }
      if(!error)
	error = index_next(indexBuf);
    }

    if(error == HA_ERR_END_OF_FILE)
    {
      /* Check count of rows   */

      if(records != indexCount)
      {
        /* Number of index entries does not agree with the number of
           rows in the index.                            */
        checkStatus = HA_ADMIN_CORRUPT;
        print_msg(thd,table->real_name,"check","error",
                "Total rows %d does not match total index entries %d for %s",
                   records, indexCount,
                    table->key_info[i].name);
      }
    }
    else
    {
      checkStatus = HA_ADMIN_FAILED;
      goto error_return;
    }
    index_end();
  }
  if(!check_opt->quick)
  {
    /* Now scan the table and for each row generate the keys
       and find them in the index                               */
    error = fullCheck(thd, buf);\
    if(error)
      checkStatus = error;
  }

error_return:
  my_free(buf,MYF(MY_ALLOW_ZERO_PTR));
  index_end();
  key_read = 0;
  error = dsmObjectUnlock((dsmContext_t *)thd->gemini.context,
                        (dsmObject_t)tableNumber,
                         DSMOBJECT_TABLE,0,
                         DSM_LK_SHARE,0);

  return checkStatus;
}

int ha_gemini::fullCheck(THD *thd,byte *buf)
{
  int error;
  int errorCount = 0;
  int checkStatus = 0;

  lastRowid = 0;

  while(((error = rnd_next( buf)) != HA_ERR_END_OF_FILE) && errorCount <= 1000)
  {
    if(!error)
    {
      error = handleIndexEntries(buf,lastRowid,KEY_CHECK);
      if(error)
	{
	/* Error finding an index entry for a row.   */
	print_msg(thd,table->real_name,"check","error",
		  "Unable to find all index entries for row %d",
		  lastRowid);
        errorCount++;
	checkStatus = HA_ADMIN_CORRUPT;
	error = 0;
      }
    }
    else
    {
      /* Error reading a row          */
      print_msg(thd,table->real_name,"check","error",
		  "Error reading row %d status = %d",
		  lastRowid,error);
      errorCount++;
      checkStatus = HA_ADMIN_CORRUPT;
      error = 0;
    }
  }

  return checkStatus;
}

int ha_gemini::repair(THD* thd,  HA_CHECK_OPT* check_opt)
{
  int error;
  dsmRecord_t  dsmRecord;
  byte         *buf;

  if(thd->gemini.needSavepoint) 
  {
    /* We don't really need a savepoint here but do it anyway
       just to keep the savepoint number correct.             */
    thd->gemini.savepoint++;
    error = dsmTransaction((dsmContext_t *)thd->gemini.context,
                           &thd->gemini.savepoint,
                           DSMTXN_SAVE, 0, 0);
    if (error)
       return(error);
    thd->gemini.needSavepoint = 0;
  }


  /* Lock the table    */
  error = dsmObjectLock((dsmContext_t *)thd->gemini.context,
                        (dsmObject_t)tableNumber,
                         DSMOBJECT_TABLE,0,
                         DSM_LK_EXCL, 1, 0);
  if(error)
    return error;
  
  error = dsmContextSetLong((dsmContext_t *)thd->gemini.context,
                             DSM_TAGCONTEXT_NO_LOGGING,1);

  error = dsmTableReset((dsmContext_t *)thd->gemini.context,
                        (dsmTable_t)tableNumber, table->keys,
                        pindexNumbers);

  buf = my_malloc(table->rec_buff_length,MYF(MY_WME));
  dsmRecord.table = tableNumber;
  dsmRecord.recid = 0;
  dsmRecord.pbuffer = (dsmBuffer_t *)rec_buff;
  dsmRecord.recLength = table->reclength;
  dsmRecord.maxLength = table->reclength;
  while(!error)
  {
    error = dsmTableScan((dsmContext_t *)thd->gemini.context,
                       &dsmRecord, DSMFINDNEXT, DSM_LK_NOLOCK,
                        1);
    if(!error)
    {
      unpack_row((char *)buf,(char *)dsmRecord.pbuffer);
      error = handleIndexEntries(buf,dsmRecord.recid,KEY_CREATE);
      if(error == HA_ERR_FOUND_DUPP_KEY)
      {
	/* We don't want to stop on duplicate keys -- we're repairing 
	   here so let's get as much repaired as possible.   */
	error = 0;
      }
    }
  } 
  error = dsmObjectUnlock((dsmContext_t *)thd->gemini.context,
                        (dsmObject_t)tableNumber,
                         DSMOBJECT_TABLE,0,
                         DSM_LK_EXCL,0);
  my_free(buf,MYF(MY_ALLOW_ZERO_PTR));  
 
  error = dsmContextSetLong((dsmContext_t *)thd->gemini.context,
                             DSM_TAGCONTEXT_NO_LOGGING,0);
    
  return error;
}

ha_rows ha_gemini::records_in_range(int keynr,
				      const byte *start_key,uint start_key_len,
				      enum ha_rkey_function start_search_flag,
				      const byte *end_key,uint end_key_len,
				      enum ha_rkey_function end_search_flag)
{
  int   error;
  int   componentLen;
  float pctInrange;
  ha_rows rows = 5;

  DBUG_ENTER("records_in_range");

  error = index_init(keynr);
  if(error)
    DBUG_RETURN(rows);

  pbracketBase->index = (short)pindexNumbers[keynr];
  pbracketBase->keycomps =  1;

  if(start_key)
  {
    error = pack_key(keynr, pbracketBase, start_key, start_key_len);  
    if(start_search_flag == HA_READ_AFTER_KEY)
    {
      /* A greater than operation      */
      error = gemKeyAddLow(pbracketBase->keystr + pbracketBase->keyLen,
                         &componentLen);
      pbracketBase->keyLen += componentLen;
    }
  }
  else
  {
    error = gemKeyLow(pbracketBase->keystr, &componentLen,
                      pbracketBase->index);
    pbracketBase->keyLen = componentLen;

  }
  pbracketBase->keyLen -= 3;

  if(end_key)
  {
    error = pack_key(keynr, pbracketLimit, end_key, end_key_len);
    if(!error && end_search_flag == HA_READ_AFTER_KEY)
    {
      error = gemKeyAddHigh(pbracketLimit->keystr + pbracketLimit->keyLen,
                            &componentLen);
      pbracketLimit->keyLen += componentLen; 
    }
  }
  else
  {
    error = gemKeyHigh(pbracketLimit->keystr,&componentLen,
                     pbracketLimit->index);
    pbracketLimit->keyLen = componentLen;
  }
  
  pbracketLimit->keyLen -= 3;
  error = dsmIndexRowsInRange((dsmContext_t *)current_thd->gemini.context,
                              pbracketBase,pbracketLimit,
                              &pctInrange);
  if(pctInrange >= 1)
    rows = (ha_rows)pctInrange;
  else
  {
     rows = (ha_rows)(records * pctInrange);
     if(!rows && pctInrange > 0)
        rows = 1;
  }
  index_end(); 

  DBUG_RETURN(rows);
}


/*
  Pack a row for storage.  If the row is of fixed length, just store the
  row 'as is'.
  If not, we will generate a packed row suitable for storage.
  This will only fail if we don't have enough memory to pack the row, which;
  may only happen in rows with blobs,  as the default row length is
  pre-allocated.
*/
int ha_gemini::pack_row(byte **pprow, int *ppackedLength, const byte *record)
{
  if (fixed_length_row)
  {
    *pprow = (byte *)record;
    *ppackedLength=(int)table->reclength;
    return 0;
  }
  if (table->blob_fields)
  {
    return HA_ERR_WRONG_COMMAND;
  }
  /* Copy null bits */
  memcpy(rec_buff, record, table->null_bytes);
  byte *ptr=rec_buff + table->null_bytes;

  for (Field **field=table->field ; *field ; field++)
    ptr=(byte*) (*field)->pack((char*) ptr,record + (*field)->offset());

  *pprow=rec_buff;
  *ppackedLength=  (ptr - rec_buff);
  return 0;
}

void ha_gemini::unpack_row(char *record, char *prow)
{
  if (fixed_length_row)
  {
    /* If the table is a VST, the row is in Gemini internal format.
    ** Convert the fields to MySQL format.
    */
    if (RM_IS_VST(tableNumber))
    {
      int i = 2; /* VST fields are numbered sequentially starting at 2 */
      long longValue;
      char *fld;
      unsigned long unknown;

      for (Field **field = table->field; *field; field++, i++)
      {
        switch ((*field)->type())
        {
          case FIELD_TYPE_LONG:
          case FIELD_TYPE_TINY:
          case FIELD_TYPE_SHORT:
          case FIELD_TYPE_TIMESTAMP:
          case FIELD_TYPE_LONGLONG:
          case FIELD_TYPE_INT24:
          case FIELD_TYPE_DATE:
          case FIELD_TYPE_TIME:
          case FIELD_TYPE_DATETIME:
          case FIELD_TYPE_YEAR:
          case FIELD_TYPE_NEWDATE:
          case FIELD_TYPE_ENUM:
          case FIELD_TYPE_SET:
            recGetLONG((dsmText_t *)prow, i, 0, &longValue, &unknown);
            if (unknown)
            {
              (*field)->set_null();
            }
            else
            {
              (*field)->set_notnull();
              (*field)->store((longlong)longValue);
            }
            break;

          case FIELD_TYPE_DECIMAL:
          case FIELD_TYPE_DOUBLE:
          case FIELD_TYPE_TINY_BLOB:
          case FIELD_TYPE_MEDIUM_BLOB:
          case FIELD_TYPE_LONG_BLOB:
          case FIELD_TYPE_BLOB:
          case FIELD_TYPE_VAR_STRING:
            break;

          case FIELD_TYPE_STRING:
            svcByteString_t stringFld;

            fld = (char *)my_malloc((*field)->field_length, MYF(MY_WME));
            stringFld.pbyte = (TEXT *)fld;
            stringFld.size = (*field)->field_length;
            recGetBYTES((dsmText_t *)prow, i, 0, &stringFld, &unknown);
            if (unknown)
            {
              (*field)->set_null();
            }
            else
            {
              (*field)->set_notnull();
              (*field)->store(fld, (*field)->field_length);
            }
            my_free(fld, MYF(MY_ALLOW_ZERO_PTR));
            break;

          default:
            break;
        }
      }
    }
    else
    {
      memcpy(record,(char*) prow,table->reclength);
    }
  }
  else
  {
    /* Copy null bits */
    const char *ptr= (const char*) prow;
    memcpy(record, ptr, table->null_bytes);
    ptr+=table->null_bytes;
    for (Field **field=table->field ; *field ; field++)
      ptr= (*field)->unpack(record + (*field)->offset(), ptr);
  }
}

int ha_gemini::key_cmp(uint keynr, const byte * old_row,
                         const byte * new_row)
{
  KEY_PART_INFO *key_part=table->key_info[keynr].key_part;
  KEY_PART_INFO *end=key_part+table->key_info[keynr].key_parts;

  for ( ; key_part != end ; key_part++)
  {
    if (key_part->null_bit)
    {
      if ((old_row[key_part->null_offset] & key_part->null_bit) !=
          (new_row[key_part->null_offset] & key_part->null_bit))
        return 1;
    }
    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH))
    {

      if (key_part->field->cmp_binary(old_row + key_part->offset,
                                      new_row + key_part->offset,
                                      (ulong) key_part->length))
        return 1;
    }
    else
    {
      if (memcmp(old_row+key_part->offset, new_row+key_part->offset,
                 key_part->length))
        return 1;
    }
  }
  return 0;
}

int gemini_parse_table_name(const char *fullname, char *dbname, char *tabname)
{
  char *namestart;
  char *nameend;

  /* separate out the name of the table and the database
  */
  namestart = strchr(fullname + start_of_name, '/');
  if (!namestart)
  {
    /* if on Windows, slashes go the other way */
    namestart = strchr(fullname + start_of_name, '\\');
  }
  nameend = strchr(fullname + start_of_name, '.');
  /* sometimes fullname has an extension, sometimes it doesn't */
  if (!nameend)
  {
    nameend = (char *)fullname + strlen(fullname);
  }
  strncpy(dbname, fullname + start_of_name,
          (namestart - fullname) - start_of_name);
  dbname[(namestart - fullname) - start_of_name] = '\0';
  strncpy(tabname, namestart + 1, (nameend - namestart) - 1);
  tabname[nameend - namestart - 1] = '\0';

  return 0;
}

/* PROGRAM: gemini_is_vst - if the name is the name of a VST, return
 *                          its number
 *
 * RETURNS:   Table number if a match is found
 *            0 if not a VST
 */
int
gemini_is_vst(const char *pname) /* IN the name */
{
    int tablenum = 0;

    for (int i = 0; i < vstnumfils; i++)
    {
        if (strcmp(pname, vstfil[i].filename) == 0)
        {
            tablenum = vstfil[i].filnum;
            break;
        }
    }

    return tablenum;
}

static void print_msg(THD *thd, const char *table_name, const char *op_name,
                      const char *msg_type, const char *fmt, ...)
{
  String* packet = &thd->packet;
  packet->length(0);
  char msgbuf[256];
  msgbuf[0] = 0;
  va_list args;
  va_start(args,fmt);

  my_vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
  msgbuf[sizeof(msgbuf) - 1] = 0; // healthy paranoia

  DBUG_PRINT(msg_type,("message: %s",msgbuf));

  net_store_data(packet, table_name);
  net_store_data(packet, op_name);
  net_store_data(packet, msg_type);
  net_store_data(packet, msgbuf);
  if (my_net_write(&thd->net, (char*)thd->packet.ptr(),
                   thd->packet.length()))
    thd->killed=1;
}

#endif /* HAVE_GEMINI_DB */
