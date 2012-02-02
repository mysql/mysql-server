/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/pfs_digest.h
  Statement Digest data structures (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_instr.h"
#include "pfs_digest.h"
#include "pfs_global.h"
#include "table_helper.h"
#include "my_md5.h"
#include <string.h>

/* Generated code */
#define YYSTYPE_IS_DECLARED
#include "sql_yacc.h"
#include "pfs_lex_token.h"

/**
  Token array : 
  Token array is an array of bytes to store tokens recieved during parsing.
  Following is the way token array is formed.
     
      ...<non-id-token><non-id-token><id-token><id_len><id_text>...

  For Ex:
  SELECT * FROM T1;
  <SELECT_TOKEN><*><FROM_TOKEN><ID_TOKEN><2><T1>
*/

/** 
  Macro to read a single token from token array.
*/
#define READ_TOKEN(_dest, _index, _src)                                \
{                                                                      \
  short _sh;                                                           \
  _sh= ((0x00ff & _src[_index+1])<<8) | (0x00ff & _src[_index]);       \
  _dest= (int)(_sh);                                                   \
  _index+= PFS_SIZE_OF_A_TOKEN;                                        \
}

/**
  Macro to store a single token in token array.
*/
#define STORE_TOKEN(_dest, _index, _token)                             \
{                                                                      \
  short _sh= (short)_token;                                            \
  _dest[_index++]= (_sh) & 0xff;                                       \
  _dest[_index++]= (_sh>>8) & 0xff;                                    \
}

/**
  Macro to read an identifier from token array.
*/
#define READ_IDENTIFIER(_dest, _index, _src)                           \
{                                                                      \
  int _length;                                                         \
  int remaining_bytes= PFS_MAX_DIGEST_STORAGE_SIZE-_index;             \
  DBUG_ASSERT(remaining_bytes >= 0);                                   \
  /*                                                                   \
    Read ID's length.                                                  \
    Make sure that space, to read ID's length, is available.           \
  */                                                                   \
  if(remaining_bytes >= PFS_SIZE_OF_A_TOKEN)                           \
  {                                                                    \
    READ_TOKEN(_length, _index, _src);                                 \
    /*                                                                 \
      While storing ID length, it has already been stored              \
      in a way that ID doesn't go beyond the storage size,             \
      so no need to check length here.                                 \
    */                                                                 \
    strncpy(_dest, _src+_index, _length);                              \
    _index+= _length;                                                  \
    _dest+= _length;                                                   \
  }                                                                    \
}

/**
  Macro to store an identifier in token array.
*/
#define STORE_IDENTIFIER(_dest, _index, _length, _id)                  \
{                                                                      \
  int remaining_bytes= PFS_MAX_DIGEST_STORAGE_SIZE-_index;             \
  DBUG_ASSERT(remaining_bytes >= 0);                                   \
  /*                                                                   \
    Store ID's length.                                                 \
    Make sure that space, to store ID's length, is available.          \
  */                                                                   \
  if(remaining_bytes >= PFS_SIZE_OF_A_TOKEN)                           \
  {                                                                    \
    /*                                                                 \
       Make sure to store ID length/ID as per the space                \
       available.                                                      \
    */                                                                 \
    remaining_bytes-= PFS_SIZE_OF_A_TOKEN;                             \
    _length= _length>remaining_bytes?remaining_bytes:_length;          \
    STORE_TOKEN(_dest, _index, _length);                               \
    strncpy(_dest+_index, _id, _length);                               \
    _index+= _length;                                                  \
  }                                                                    \
}

/**
  Macro to read last two tokens from token array. If an identifier
  is found, do not look for token after that.
*/
#define READ_LAST_TWO_TOKENS(_t1, _t2, _id_index, _byte_count)         \
{                                                                      \
  int _last_token_index;                                               \
  if(_id_index <= _byte_count - PFS_SIZE_OF_A_TOKEN)                   \
  {                                                                    \
    /* Take last token. */                                             \
    _last_token_index= _byte_count - PFS_SIZE_OF_A_TOKEN;              \
    DBUG_ASSERT(_last_token_index >= 0);                               \
    READ_TOKEN(_t1, _last_token_index, digest_storage->m_token_array); \
  }                                                                    \
  if(_id_index <= _byte_count - 2*PFS_SIZE_OF_A_TOKEN)                 \
  {                                                                    \
    /* Take 2nd token from last. */                                    \
    _last_token_index= _byte_count - 2*PFS_SIZE_OF_A_TOKEN;            \
    DBUG_ASSERT(_last_token_index >= 0);                               \
    READ_TOKEN(_t2, _last_token_index, digest_storage->m_token_array); \
  }                                                                    \
}

unsigned int statements_digest_size= 0;
/** EVENTS_STATEMENTS_HISTORY_LONG circular buffer. */
PFS_statements_digest_stat *statements_digest_stat_array= NULL;
/** Consumer flag for table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
bool flag_statements_digest= true;
/** 
  Current index in Stat array where new record is to be inserted.
  index 0 is reserved for "all else" case when entire array is full.
*/
unsigned int digest_index= 1;

static LF_HASH digest_hash;
static bool digest_hash_inited= false;

/**
  Initialize table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST.
  @param digest_sizing      
*/
int init_digest(unsigned int statements_digest_sizing)
{
  unsigned int index;

  /* 
    TBD. Allocate memory for statements_digest_stat_array based on 
    performance_schema_digests_size values
  */
  statements_digest_size= statements_digest_sizing;
 
  if (statements_digest_size == 0)
    return 0;

  statements_digest_stat_array=
    PFS_MALLOC_ARRAY(statements_digest_size, PFS_statements_digest_stat,
                     MYF(MY_ZEROFILL));
   
  for (index= 0; index < statements_digest_size; index++)
  statements_digest_stat_array[index].m_stat.reset();

  return (statements_digest_stat_array ? 0 : 1);
}

/** Cleanup table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
void cleanup_digest(void)
{
  /* 
    TBD. Free memory allocated to statements_digest_stat_array. 
  */
  pfs_free(statements_digest_stat_array);
  statements_digest_stat_array= NULL;
}

C_MODE_START
static uchar *digest_hash_get_key(const uchar *entry, size_t *length,
                                my_bool)
{
  const PFS_statements_digest_stat * const *typed_entry;
  const PFS_statements_digest_stat *digest;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_statements_digest_stat*const*>(entry);
  DBUG_ASSERT(typed_entry != NULL);
  digest= *typed_entry;
  DBUG_ASSERT(digest != NULL);
  *length= 16; 
  result= digest->m_md5_hash.m_md5;
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}
C_MODE_END


/**
  Initialize the digest hash.
  @return 0 on success
*/
int init_digest_hash(void)
{
  if (! digest_hash_inited)
  {
    lf_hash_init(&digest_hash, sizeof(PFS_statements_digest_stat*),
                 LF_HASH_UNIQUE, 0, 0, digest_hash_get_key,
                 &my_charset_bin);
    digest_hash_inited= true;
  }
  return 0;
}

/** Cleanup the digest hash. */
void cleanup_digest_hash(void)
{
  if (digest_hash_inited)
  {
    lf_hash_destroy(&digest_hash);
    digest_hash_inited= false;
  }
}

static LF_PINS* get_digest_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_digest_hash_pins == NULL))
  {
    if (!digest_hash_inited)
      return NULL;
    thread->m_digest_hash_pins= lf_hash_get_pins(&digest_hash);
  }
  return thread->m_digest_hash_pins;
}

PFS_statements_digest_stat* 
find_or_create_digest(PFS_thread* thread, PFS_digest_storage* digest_storage)
{
  /* get digest pin. */
  LF_PINS *pins= get_digest_hash_pins(thread);
  /* There shoulod be at least one token. */
  if(unlikely(pins == NULL) || 
     !(digest_storage->m_byte_count >= PFS_SIZE_OF_A_TOKEN))
  {
    return NULL;
  }

  unsigned char* hash_key= digest_storage->m_digest_hash.m_md5;
 
  PFS_statements_digest_stat **entry;
  PFS_statements_digest_stat *pfs= NULL;

  /* Lookup LF_HASH using this new key. */
  entry= reinterpret_cast<PFS_statements_digest_stat**>
    (lf_hash_search(&digest_hash, pins,
                    hash_key, 16));

  if(!entry)
  {
    /* 
      If statement digest entry doesn't exist.
    */
    if(digest_index==0)
    {
      /*
        digest_stat array is full. Add stat at index 0 and return.
      */
      pfs= &statements_digest_stat_array[0];
      return pfs;
    }

    /* 
      Add a new record in digest stat array. 
    */
    pfs= &statements_digest_stat_array[digest_index];
    
    /* 
      Copy digest storage to statement_digest_stat_array so that it could be
      used later to generate digest text.
    */
    pfs->m_digest_storage.m_byte_count= digest_storage->m_byte_count;
    pfs->m_digest_storage.m_last_id_index= digest_storage->m_last_id_index;
    memcpy(pfs->m_digest_storage.m_token_array, digest_storage->m_token_array, PFS_MAX_DIGEST_STORAGE_SIZE);
    memcpy(pfs->m_digest_storage.m_digest_hash.m_md5, digest_storage->m_digest_hash.m_md5, 16);

    /* Set digest hash/LF Hash search key. */
    memcpy(pfs->m_md5_hash.m_md5, hash_key, 16);
    
    /* Increment index. */
    digest_index++;
    
    if(digest_index%statements_digest_size == 0)
    {
      /* 
        Digest stat array is full. Now all stat for all further 
        entries will go into index 0.
      */
      digest_index= 0;
    }
    
    /* Add this new digest into LF_HASH */
    int res;
    res= lf_hash_insert(&digest_hash, pins, &pfs);
    lf_hash_search_unpin(pins);
    if (res > 0)
    {
      /* TODO: Handle ERROR CONDITION */
      return NULL;
    }
    return pfs;
  }
  else if (entry && (entry != MY_ERRPTR))
  {
    /* 
      If stmt digest already exists, update stat and return 
    */
    pfs= *entry;
    lf_hash_search_unpin(pins);
    return pfs;
  }

  return NULL;
}
 
void reset_esms_by_digest()
{
  uint index;

  if(statements_digest_stat_array == NULL)
    return;

  /*
    Reset statements_digest_stat_array.
  */
  for (index= 0; index < statements_digest_size; index++)
  {
    statements_digest_stat_array[index].m_md5_hash.m_md5[0]= '\0';
    statements_digest_stat_array[index].m_stat.reset();
  }

  /* 
    Reset index which indicates where the next calculated digest informationi
    to be inserted in statements_digest_stat_array.
  */
  digest_index= 1;
}

/*
  This function, iterates token array and updates digest_text.
*/
void get_digest_text(char* digest_text,
                            char* token_array,
                            int byte_count)
{
  int tok= 0;
  int current_byte= 0;
  char *digest_text_start= digest_text;
  lex_token_string *tok_data;

  DBUG_ASSERT(byte_count <= PFS_MAX_DIGEST_STORAGE_SIZE);

  while(current_byte<byte_count &&
        (digest_text-digest_text_start)<COL_DIGEST_TEXT_SIZE-3)
  {
    READ_TOKEN(tok, current_byte, token_array);
    tok_data= & lex_token_array[tok];

    switch (tok)
    {
    /* All literals are printed as '?' */
    case BIN_NUM:
    case DECIMAL_NUM:
    case FLOAT_NUM:
    case HEX_NUM:
    case LEX_HOSTNAME:
    case LONG_NUM:
    case NUM:
    case NCHAR_STRING:
    case TEXT_STRING:
    case ULONGLONG_NUM:
      *digest_text= '?';
      digest_text++;
      break;

    /* All identifiers are printed with their name. */
    case IDENT:
    case IDENT_QUOTED:
      READ_IDENTIFIER(digest_text, current_byte, token_array);
      *digest_text= ' ';
      digest_text++;
      break;

    /* Everything else is printed as is. */
    default:
      strncpy(digest_text,
              tok_data->m_token_string,
              tok_data->m_token_length);
      digest_text+= tok_data->m_token_length;
      *digest_text= ' ';
      digest_text++;
    }
  }

  /* 
    Truncate digest text in case of long queries.
  */
  if(digest_text-digest_text_start == COL_DIGEST_TEXT_SIZE-3)
  {
    strcpy(digest_text,"...");
  }
  else
  {
    *digest_text= '\0';
  }
}

struct PSI_digest_locker* pfs_digest_start_v1(PSI_statement_locker *locker)
{
  PSI_statement_locker_state *statement_state= NULL;
  PSI_digest_locker_state    *state= NULL;
  PFS_events_statements      *pfs= NULL;
  PFS_digest_storage         *digest_storage= NULL;

  /*
    If current statement is not instrumented
    or if statement_digest consumer is not enabled.
  */
  if(!locker || !(flag_thread_instrumentation && flag_events_statements_current)
             || (!flag_statements_digest)
             || (!statements_digest_stat_array))
  {
    return NULL;
  }

  /*
    Get statement locker state from statement locker
  */
  statement_state= reinterpret_cast<PSI_statement_locker_state*> (locker);
  DBUG_ASSERT(statement_state != NULL);

  /*
    Get digest_locker_state from statement_locker_state.
  */
  state= &statement_state->m_digest_state;
  DBUG_ASSERT(state != NULL);

  /*
    Take out thread specific statement record. And then digest
    storage information for this statement from it.
  */
  pfs= reinterpret_cast<PFS_events_statements*>(statement_state->m_statement);
  digest_storage= &pfs->m_digest_storage;

  /*
    Initialize token array and token count to 0.
  */
  digest_storage->m_byte_count= PFS_MAX_DIGEST_STORAGE_SIZE;
  digest_storage->m_last_id_index= 0;
  while(digest_storage->m_byte_count)
    digest_storage->m_token_array[--digest_storage->m_byte_count]= 0;

  /*
    Set digest_locker_state's statement info pointer.
  */
  state->m_statement= pfs;

  return reinterpret_cast<PSI_digest_locker*> (state);
}

void pfs_digest_add_token_v1(PSI_digest_locker *locker,
                             uint token,
                             char *yytext,
                             int yylen)
{
  PSI_digest_locker_state *state= NULL;
  PFS_events_statements   *pfs= NULL;
  PFS_digest_storage      *digest_storage= NULL;

  if(!locker)
    return;

  state= reinterpret_cast<PSI_digest_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  pfs= reinterpret_cast<PFS_events_statements *>(state->m_statement);
  digest_storage= &pfs->m_digest_storage;

  if( PFS_MAX_DIGEST_STORAGE_SIZE - digest_storage->m_byte_count <
      PFS_SIZE_OF_A_TOKEN)
  {
    /*
      If digest storage record is full, do nothing.
    */
    return;
  }

  /* 
    Take last_token 2 tokens collected till now. These tokens will be used
    in reduce for normalisation. Make sure not to consider ID tokens in reduce.
  */
  uint last_token = TOK_PFS_UNUSED;
  uint last_token2= TOK_PFS_UNUSED;
  
  READ_LAST_TWO_TOKENS(last_token, last_token2,
                       digest_storage->m_last_id_index,
                       digest_storage->m_byte_count);

  switch (token)
  {
    case BIN_NUM:
    case DECIMAL_NUM:
    case FLOAT_NUM:
    case HEX_NUM:
    case LEX_HOSTNAME:
    case LONG_NUM:
    case NUM:
    case TEXT_STRING:
    case NCHAR_STRING:
    case ULONGLONG_NUM:
    {
      /*
        REDUCE:
        TOK_PFS_GENERIC_VALUE := BIN_NUM | DECIMAL_NUM | ... | ULONGLONG_NUM
      */
      token= TOK_PFS_GENERIC_VALUE;

      if ((last_token2 == TOK_PFS_GENERIC_VALUE ||
           last_token2 == TOK_PFS_GENERIC_VALUE_LIST) &&
          (last_token == ','))
      {
        /*
          REDUCE:
          TOK_PFS_GENERIC_VALUE_LIST :=
            TOK_PFS_GENERIC_VALUE ',' TOK_PFS_GENERIC_VALUE
          
          REDUCE:
          TOK_PFS_GENERIC_VALUE_LIST :=
            TOK_PFS_GENERIC_VALUE_LIST ',' TOK_PFS_GENERIC_VALUE
        */
        digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
        token= TOK_PFS_GENERIC_VALUE_LIST;
      }
      /*
        Add this token or the resulting reduce to digest storage.
      */
      STORE_TOKEN(digest_storage->m_token_array, digest_storage->m_byte_count, token);
      break;
    }
    case ')':
    {
      if(last_token == TOK_PFS_GENERIC_VALUE &&
         last_token2 == '(') 
      { 
        /*
          REDUCE:
            "(" "#" +  ")" => "(#)"
        */
        digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
        token= TOK_PFS_ROW_SINGLE_VALUE;
      
        /* Read last two tokens again */
        READ_LAST_TWO_TOKENS(last_token, last_token2,
                             digest_storage->m_last_id_index,
                             digest_storage->m_byte_count);

        if((last_token2 == TOK_PFS_ROW_SINGLE_VALUE ||
            last_token2 == TOK_PFS_ROW_SINGLE_VALUE_LIST) &&
           (last_token == ','))
        {
          /*
            REDUCE:
              "(#)" "," + "(#)" => "(#),(#)"
            REDUCE:
              "(#),(#)" "," + "(#)" => "(#),(#)"
          */
          digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
          token= TOK_PFS_ROW_SINGLE_VALUE_LIST;
        }
      }
      else if(last_token == TOK_PFS_GENERIC_VALUE_LIST &&
              last_token2 == '(') 
      {
        /*
          REDUCE:
            "(" "#,#" + ")" => "(#,#)"
        */
        digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
        token= TOK_PFS_ROW_MULTIPLE_VALUE;

        /* Read last two tokens again */
        READ_LAST_TWO_TOKENS(last_token, last_token2,
                             digest_storage->m_last_id_index,
                             digest_storage->m_byte_count);

        if((last_token2 == TOK_PFS_ROW_MULTIPLE_VALUE ||
            last_token2 == TOK_PFS_ROW_MULTIPLE_VALUE_LIST) &&
           (last_token == ','))
        {
          /*
            REDUCE:
              "(#,#)" "," + "(#,#)" ) => "(#,#),(#,#)"
            REDUCE:
              "(#,#),(#,#)" "," + "(#,#)" ) => "(#,#),(#,#)"
          */
          digest_storage->m_byte_count-= 2*PFS_SIZE_OF_A_TOKEN;
          token= TOK_PFS_ROW_MULTIPLE_VALUE_LIST;
        }
      }
      /*
        Add this token or the resulting reduce to digest storage.
      */
      STORE_TOKEN(digest_storage->m_token_array, digest_storage->m_byte_count, token);
      break;
    }
    case IDENT:
    case IDENT_QUOTED:
    {
      /*
        Add this token to digest storage.
      */
      STORE_TOKEN(digest_storage->m_token_array, digest_storage->m_byte_count, token);
      /*
        Add this identifier's lenght and string to digest storage.
      */
      STORE_IDENTIFIER(digest_storage->m_token_array, digest_storage->m_byte_count, yylen, yytext);
      /* 
        Update the index of last identifier found.
      */
      digest_storage->m_last_id_index= digest_storage->m_byte_count;
      break;
    }
    default:
    {
      /*
        Add this token to digest storage.
      */
      STORE_TOKEN(digest_storage->m_token_array, digest_storage->m_byte_count, token);
      break;
    }
  }
}

void pfs_digest_end_v1(PSI_digest_locker *locker)
{
  PSI_digest_locker_state *state= NULL;
  PFS_events_statements   *pfs= NULL;
  PFS_digest_storage      *digest_storage= NULL;

  if(!locker)
    return;

  state= reinterpret_cast<PSI_digest_locker_state*> (locker);
  DBUG_ASSERT(state != NULL);

  pfs= reinterpret_cast<PFS_events_statements *>(state->m_statement);
  digest_storage= &pfs->m_digest_storage;

  /*
    Calculate MD5 Hash of the tokens received.
  */
  MY_MD5_HASH(digest_storage->m_digest_hash.m_md5,
              (unsigned char *)digest_storage->m_token_array,
              (uint) sizeof(digest_storage->m_token_array));
}
