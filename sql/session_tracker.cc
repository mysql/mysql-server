/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#include "session_tracker.h"

#include "hash.h"
#include "rpl_gtid.h"
#include "sql_class.h"
#include "sql_show.h"
#include "xa.h"

static void store_lenenc_string(String &to, const char *from,
                                size_t length);

/**
  Session_sysvars_tracker
  -----------------------
  This is a tracker class that enables & manages the tracking of session
  system variables. It internally maintains a hash of user supplied variable
  names and a boolean field to store if the variable was changed by the last
  statement.
*/

class Session_sysvars_tracker : public State_tracker
{
private:

  struct sysvar_node_st {
    LEX_STRING m_sysvar_name;
    bool m_changed;
  };

  class vars_list
  {
  private:
    /**
      Registered system variables. (@@session_track_system_variables)
      A hash to store the name of all the system variables specified by the
      user.
    */
    HASH m_registered_sysvars;
    char *variables_list;
    /**
      The boolean which when set to true, signifies that every variable
      is to be tracked.
    */
    bool track_all;
    void init(const CHARSET_INFO *char_set)
    {
      variables_list= NULL;
      my_hash_init(&m_registered_sysvars,
		   const_cast<CHARSET_INFO *>(char_set),
		   4, 0, 0, (my_hash_get_key) sysvars_get_key,
		   my_free, HASH_UNIQUE,
                   key_memory_THD_Session_tracker);
    }
    void free_hash()
    {
      if (my_hash_inited(&m_registered_sysvars))
      {
	my_hash_free(&m_registered_sysvars);
      }
    }

    uchar* search(const uchar* token, size_t length)
    {
      return (my_hash_search(&m_registered_sysvars, token,
			     length));
    }

  public:
    vars_list(const CHARSET_INFO *char_set)
    {
      init(char_set);
    }

    void claim_memory_ownership()
    {
      my_claim(variables_list);
      my_hash_claim(&m_registered_sysvars);
    }

    ~vars_list()
    {
      /* free the allocated hash. */
      if (my_hash_inited(&m_registered_sysvars))
      {
	my_hash_free(&m_registered_sysvars);
      }
      if (variables_list)
	my_free(variables_list);
      variables_list= NULL;
    }

    uchar* search(sysvar_node_st *node, LEX_STRING tmp)
    {
      uchar *res;
      res= search((const uchar *)tmp.str, tmp.length);
      if (!res)
      {
	if (track_all)
	{
	  insert(node, tmp);
	  return search((const uchar *)tmp.str, tmp.length);
	}
      }
      return res;
    }

    uchar* operator[](ulong idx)
    {
      return my_hash_element(&m_registered_sysvars, idx);
    }
    bool insert(sysvar_node_st *node, LEX_STRING var);
    void reset();
    bool update(vars_list* from, THD *thd);
    bool parse_var_list(THD *thd, LEX_STRING var_list, bool throw_error,
	                const CHARSET_INFO *char_set, bool session_created);
  };
  /**
    Two objects of vars_list type are maintained to manage
    various operations on variables_list.
  */
  vars_list *orig_list, *tool_list;

public:
  /** Constructor */
  Session_sysvars_tracker(const CHARSET_INFO *char_set)
  {
    orig_list= new (std::nothrow) vars_list(char_set);
    tool_list= new (std::nothrow) vars_list(char_set);
  }

  /** Destructor */
  ~Session_sysvars_tracker()
  {
    if (orig_list)
      delete orig_list;
    if (tool_list)
      delete tool_list;
  }

  /**
    Method used to check the validity of string provided
    for session_track_system_variables during the server
    startup.
  */
  static bool server_init_check(const CHARSET_INFO *char_set, LEX_STRING var_list)
  {
    vars_list dummy(char_set);
    bool result;
    result= dummy.parse_var_list(NULL, var_list, false, char_set, true);
    return result;
  }

  void reset();
  bool enable(THD *thd);
  bool check(THD *thd, set_var *var);
  bool update(THD *thd);
  bool store(THD *thd, String &buf);
  void mark_as_changed(THD *thd, LEX_CSTRING *tracked_item_name);
  /* callback */
  static uchar *sysvars_get_key(const char *entry, size_t *length,
                                my_bool not_used MY_ATTRIBUTE((unused)));

  virtual void claim_memory_ownership()
  {
    if (orig_list != NULL)
      orig_list->claim_memory_ownership();
    if (tool_list != NULL)
      tool_list->claim_memory_ownership();
  }
};


/**
  Current_schema_tracker
  ----------------------
  This is a tracker class that enables & manages the tracking of current
  schema for a particular connection.
*/

class Current_schema_tracker : public State_tracker
{
private:
  bool schema_track_inited;
  void reset();

public:

  /** Constructor */
  Current_schema_tracker()
  {
    schema_track_inited= false;
  }

  bool enable(THD *thd)
  { return update(thd); }
  bool check(THD *thd, set_var *var)
  { return false; }
  bool update(THD *thd);
  bool store(THD *thd, String &buf);
  void mark_as_changed(THD *thd, LEX_CSTRING *tracked_item_name);
};

/* To be used in expanding the buffer. */
static const unsigned int EXTRA_ALLOC= 1024;

///////////////////////////////////////////////////////////////////////////////

/**
  This is an interface for encoding the gtids in the payload of the
  the OK packet.

  In the future we may have different types of payloads, thence we may have
  different encoders specifications/types. This implies changing either, the
  encoding specification code, the actual encoding procedure or both at the
  same time.

  New encoders can extend this interface/abstract class or extend
  other encoders in the hierarchy.
*/
class Session_gtids_ctx_encoder
{
public:
  Session_gtids_ctx_encoder() {}
  virtual ~Session_gtids_ctx_encoder() {};

  /*
   This function SHALL encode the collected GTIDs into the buffer.
   @param thd The session context.
   @param buf The buffer that SHALL contain the encoded data.
   @return false if the contents were successfully encoded, true otherwise.
           if the return value is true, then the contents of the buffer is
           undefined.
   */
  virtual bool encode(THD *thd, String& buf)= 0;

  /*
   This function SHALL return the encoding specification used in the
   packet sent to the client. The format of the encoded data will differ
   according to the specification set here.

   @return the encoding specification code.
   */
  virtual ulonglong encoding_specification()= 0;
private:
  // not implemented
  Session_gtids_ctx_encoder(const Session_gtids_ctx_encoder& rsc);
  Session_gtids_ctx_encoder& operator=(const Session_gtids_ctx_encoder& rsc);
};

class Session_gtids_ctx_encoder_string : public Session_gtids_ctx_encoder
{
public:

  Session_gtids_ctx_encoder_string() {}
  ~Session_gtids_ctx_encoder_string() {}

  ulonglong encoding_specification() { return 0; }

  bool encode(THD *thd, String& buf)
  {
    const Gtid_set* state=
      thd->rpl_thd_ctx.session_gtids_ctx().state();

    if (!state->is_empty())
    {
      /*
        No need to use net_length_size in the following two fields.
        These are constants in this class and will both be encoded using
        only 1 byte.
      */
      ulonglong tracker_type_enclen= 1 /* net_length_size((ulonglong)SESSION_TRACK_GTIDS); */;
      ulonglong encoding_spec_enclen= 1 /* net_length_size(encoding_specification()); */;
      ulonglong gtids_string_len= state->get_string_length(&Gtid_set::default_string_format);
      ulonglong gtids_string_len_enclen= net_length_size(gtids_string_len);
      ulonglong entity_len= encoding_spec_enclen + gtids_string_len_enclen +
                            gtids_string_len;
      ulonglong entity_len_enclen= net_length_size(entity_len);
      ulonglong total_enclen= tracker_type_enclen + entity_len_enclen +
                              encoding_spec_enclen + gtids_string_len_enclen +
                              gtids_string_len;

      /* prepare the buffer */
      uchar *to= (uchar *) buf.prep_append(total_enclen, EXTRA_ALLOC);

     /* format of the payload is as follows:
       [ tracker type] [len] [ encoding spec ] [gtid string len] [gtid string]
      */

      /* Session state type (SESSION_TRACK_SCHEMA) */
      *to= (uchar) SESSION_TRACK_GTIDS; to++;

      /* Length of the overall entity. */
      to= net_store_length(to, entity_len);

      /* encoding specification */
      *to= (uchar) encoding_specification(); to++;

      /* the length of the gtid set string*/
      to= net_store_length(to, gtids_string_len);

      /* the actual gtid set string */
      state->to_string((char*)to);

    }
    return false;
  }
private:
  // not implemented
  Session_gtids_ctx_encoder_string(const Session_gtids_ctx_encoder_string& rsc);
  Session_gtids_ctx_encoder_string& operator=(const Session_gtids_ctx_encoder_string& rsc);
};

/**
  Session_gtids_tracker
  ---------------------------------
  This is a tracker class that enables & manages the tracking of gtids for
  relaying to the connectors the information needed to handle session
  consistency.
*/

class Session_gtids_tracker : public State_tracker, Session_consistency_gtids_ctx::Ctx_change_listener
{
private:
  void reset();
  Session_gtids_ctx_encoder* m_encoder;

public:

  /** Constructor */
  Session_gtids_tracker() : Session_consistency_gtids_ctx::Ctx_change_listener(),
          m_encoder(NULL)
  { }

  ~Session_gtids_tracker()
  {
    /*
     Unregister the listener if the tracker is being freed. This is needed
     since this may happen after a change user command.
     */
    if (m_enabled && current_thd)
      current_thd->rpl_thd_ctx.session_gtids_ctx().unregister_ctx_change_listener(this);
    if (m_encoder)
      delete m_encoder;
  }

  bool enable(THD *thd)
  { return update(thd); }
  bool check(THD *thd, set_var *var)
  { return false; }
  bool update(THD *thd);
  bool store(THD *thd, String &buf);
  void mark_as_changed(THD *thd, LEX_CSTRING *tracked_item_name);

  // implementation of the Session_gtids_ctx::Ctx_change_listener
  void notify_session_gtids_ctx_change()
  {
    mark_as_changed(NULL, NULL);
  }
};

void Session_sysvars_tracker::vars_list::reset()
{
  if (m_registered_sysvars.records)
    my_hash_reset(&m_registered_sysvars);
  if (variables_list)
  {
    my_free(variables_list);
    variables_list=NULL;
  }
}

/**
  This function is used to update the members of one vars_list object with
  the members from the other.

  @@param  from    Source vars_list object.
  @@param  thd     THD handle to retrive the charset in use.

  @@return    true if the m_registered_sysvars hash has any records.
              Else the value of track_all.
*/

bool Session_sysvars_tracker::vars_list::update(vars_list* from, THD *thd)
{
  reset();
  variables_list= from->variables_list;
  track_all= from->track_all;
  free_hash();
  m_registered_sysvars= from->m_registered_sysvars;
  from->init(thd->charset());
  return (m_registered_sysvars.records)? true : track_all;
}

/**
  Inserts the variable to be tracked into m_registered_sysvars hash.

  @@param   node   Node to be inserted.
  @@param   var    LEX_STRING which has the name of variable.

  @@return  false  success
            true   error
*/
bool Session_sysvars_tracker::vars_list::insert(sysvar_node_st *node,
                                                LEX_STRING var)
{
  if (!node)
  {
    if (!(node= (sysvar_node_st *) my_malloc(key_memory_THD_Session_tracker,
					     sizeof(sysvar_node_st), MY_WME)))
    {
      reset();
      return true;                            /* Error */
    }
  }

  node->m_sysvar_name.str= var.str;
  node->m_sysvar_name.length= var.length;
  node->m_changed= false;
  if (my_hash_insert(&m_registered_sysvars, (uchar *) node))
  {
    /* Duplicate entry. */
    my_error(ER_DUP_LIST_ENTRY, MYF(0), var.str);
    reset();
    my_free(node);
    return true;
  }                          /* Error */
  return false;
}

/**
  @brief Parse the specified system variables list. While parsing raise
         warning/error on invalid/duplicate entries.

         * In case of duplicate entry ER_DUP_LIST_ENTRY is raised.
         * In case of invalid entry a warning is raised per invalid entry.
           This is done in order to handle 'potentially' valid system
           variables from uninstalled plugins which might get installed in
           future.

	Value of @@session_track_system_variables is initially put into
	variables_list. This string is used to update the hash with valid
	system variables.

  @param thd             [IN]    The thd handle.
  @param var_list        [IN]    System variable list.
  @param throw_error     [IN]    bool when set to true, returns an error
                                 in case of invalid/duplicate values.
  @param char_set	 [IN]	 charecter set information used for string
				 manipulations.
  @param session_created [IN]    bool variable which says if the parse is
                                 already executed once. The mutex on variables
				 is not acquired if this variable is false.

  @return
    true                    Error
    false                   Success
*/
bool Session_sysvars_tracker::vars_list::parse_var_list(THD *thd, LEX_STRING var_list,
                                                        bool throw_error,
							const CHARSET_INFO *char_set,
							bool session_created)
{
  const char *separator= ",";
  char *token, *lasts= NULL;                    /* strtok_r */

  if (!var_list.str)
  {
    variables_list= NULL;
    return false;
  }

  /*
    Storing of the session_track_system_variables option
    string to be used by strtok_r().
  */
  variables_list= my_strndup(key_memory_THD_Session_tracker,
			     var_list.str, var_list.length,
			     MYF(0));
  if (variables_list)
  {
    if(!strcmp(variables_list,(const char *)"*"))
    {
      track_all= true;
      return false;
    }
  }

  token= my_strtok_r(variables_list, separator, &lasts);

  track_all= false;
  /*
    If Lock to the plugin mutex is not acquired here itself, it results
    in having to acquire it multiple times in find_sys_var_ex for each
    token value. Hence the mutex is handled here to avoid a performance
    overhead.
  */
  if (!thd || session_created)
    lock_plugin_mutex();
  while(token)
  {
    LEX_STRING var;
    var.str= token;
    var.length= strlen(token);

    /* Remove leading/trailing whitespace. */
    trim_whitespace(char_set, &var);

    if (!thd || session_created)
    {
      if (find_sys_var_ex(thd, var.str, var.length, throw_error, true))
      {
	if (insert(NULL, var) == TRUE)
	{
	/* Error inserting into the hash. */
	  unlock_plugin_mutex();
	  return true;                            /* Error */
	}
      }

      else if (throw_error)
      {
	DBUG_ASSERT(thd);
	push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_VALUE_FOR_VAR,
			    "%s is not a valid system variable and will be ignored.", token);
      }
      else
      {
	unlock_plugin_mutex();
	return true;
      }
    }
    else
    {
      if (insert(NULL, var) == TRUE)
      {
      /* Error inserting into the hash. */
	return true;                            /* Error */
      }
    }

    token= my_strtok_r(NULL, separator, &lasts);
  }
  if (!thd || session_created)
    unlock_plugin_mutex();

  return false;
}

/**
  @brief It is responsible for enabling this tracker when a session starts.
         During the initialization, a session's system variable gets a copy
         of the global variable. The new value of session_track_system_variables
         is then verified & tokenized to create a hash, which is then updated to
	 orig_list which represents all the systems variables to be tracked.

  @param thd    [IN]        The thd handle.

  @return
    true                    Error
    false                   Success
*/

bool Session_sysvars_tracker::enable(THD *thd)
{
  LEX_STRING var_list;

  if (!thd->variables.track_sysvars_ptr)
    return false;

  var_list.str= thd->variables.track_sysvars_ptr;
  var_list.length= strlen(thd->variables.track_sysvars_ptr);

  if (tool_list->parse_var_list(thd, var_list, true, thd->charset(), false) == true)
    return true;

  m_enabled= orig_list->update(tool_list,thd);

  return false;
}


/**
  @brief Check if any of the system variable name(s) in the given list of
         system variables is duplicate/invalid.

         When the value of @@session_track_system_variables system variable is
         updated, the new value is first verified in this function (called from
         ON_CHECK()) and a hash is populated in tool_list.

  @note This function is called from the ON_CHECK() function of the
        session_track_system_variables' sys_var class.

  @param thd    [IN]        The thd handle.
  @param var    [IN]        A pointer to set_var holding the specified list of
                            system variable names.

  @return
    true                    Error
    false                   Success
*/

inline bool Session_sysvars_tracker::check(THD *thd, set_var *var)
{
  tool_list->reset();
  return tool_list->parse_var_list(thd, var->save_result.string_value, true,
                                   thd->charset(), true);
}


/**
  @brief Once the value of the @@session_track_system_variables has been
         successfully updated, this function calls
	 Session_sysvars_tracker::vars_list::update updating the hash in
         orig_list which represents the system variables to be tracked.

  @note This function is called from the ON_UPDATE() function of the
        session_track_system_variables' sys_var class.

  @param thd    [IN]        The thd handle.

  @return
    true                    Error
    false                   Success
*/

bool Session_sysvars_tracker::update(THD *thd)
{
  if (!thd->variables.track_sysvars_ptr)
    return false;
  m_enabled= orig_list->update(tool_list,thd);
  return false;
}


/**
  @brief Store the data for changed system variables in the specified buffer.
         Once the data is stored, we reset the flags related to state-change
         (see reset()).

  @param thd [IN]           The thd handle.
  @paran buf [INOUT]        Buffer to store the information to.

  @return
    false                   Success
    true                    Error
*/

bool Session_sysvars_tracker::store(THD *thd, String &buf)
{
  char val_buf[1024];
  const char *value;
  sysvar_node_st *node;
  SHOW_VAR *show;
  sys_var *var;
  const CHARSET_INFO *charset;
  size_t val_length, length;
  uchar *to;
  int idx= 0;

  if (!(show= (SHOW_VAR *) thd->alloc(sizeof(SHOW_VAR))))
    return true;

  /* As its always system variable. */
  show->type= SHOW_SYS;

  while ((node= (sysvar_node_st *) (*orig_list)[idx]))
  {
    if (node->m_changed &&
        (var= find_sys_var_ex(thd, node->m_sysvar_name.str,
                              node->m_sysvar_name.length, true, false)))
    {
      show->name= var->name.str;
      show->value= (char *) var;

      value= get_one_variable(thd, show, OPT_SESSION, show->type, NULL,
                              &charset, val_buf, &val_length);

      length= net_length_size(node->m_sysvar_name.length) +
              node->m_sysvar_name.length +
              net_length_size(val_length) +
              val_length;

      to= (uchar *) buf.prep_append(net_length_size(length) + 1, EXTRA_ALLOC);

      /* Session state type (SESSION_TRACK_SYSTEM_VARIABLES) */
      to= net_store_length(to, (ulonglong)SESSION_TRACK_SYSTEM_VARIABLES);

      /* Length of the overall entity. */
      net_store_length(to, (ulonglong)length);

      /* System variable's name (length-encoded string). */
      store_lenenc_string(buf, node->m_sysvar_name.str,
                          node->m_sysvar_name.length);

      /* System variable's value (length-encoded string). */
      store_lenenc_string(buf, value, val_length);
    }
    ++ idx;
  }

  reset();

  return false;
}


/**
  @brief Mark the system variable with the specified name as changed.

  @param name [IN]          Name of the system variable which got changed.

  @return                   void
*/

void Session_sysvars_tracker::mark_as_changed(THD *thd, LEX_CSTRING *tracked_item_name)
{
  DBUG_ASSERT(tracked_item_name->str);
  sysvar_node_st *node= NULL;
  LEX_STRING tmp;
  tmp.str= (char *) tracked_item_name->str;
  tmp.length= tracked_item_name->length;
  /*
    Check if the specified system variable is being tracked, if so
    mark it as changed and also set the class's m_changed flag.
  */
  if ((node= (sysvar_node_st *) (orig_list->search(node, tmp))))
  {
    node->m_changed= true;
    m_changed= true;
    /* do not cache the statement when there is change in session state */
    thd->lex->safe_to_cache_query= 0;
  }
}


/**
  @brief Supply key to the hash implementation (to be used internally by the
         implementation).

  @param entry  [IN]        A single entry.
  @param length [OUT]       Length of the key.
  @param not_used           Unused.

  @return                   Pointer to the key buffer.
*/

uchar *Session_sysvars_tracker::sysvars_get_key(const char *entry,
                                                size_t *length,
                                                my_bool not_used MY_ATTRIBUTE((unused)))
{
  char *key;
  key= ((sysvar_node_st *) entry)->m_sysvar_name.str;
  *length= ((sysvar_node_st *) entry)->m_sysvar_name.length;
  return (uchar *) key;
}


/**
  @brief Prepare/reset the m_registered_sysvars hash for next statement.

  @return                   void
*/

void Session_sysvars_tracker::reset()
{
  sysvar_node_st *node;
  int idx= 0;

  while ((node= (sysvar_node_st *) (*orig_list)[idx]))
  {
    node->m_changed= false;
    ++ idx;
  }
  m_changed= false;
}

///////////////////////////////////////////////////////////////////////////////

/**
  @brief Enable/disable the tracker based on @@session_track_schema's value.

  @param thd [IN]           The thd handle.

  @return
    false (always)
*/

bool Current_schema_tracker::update(THD *thd)
{
  m_enabled= (thd->variables.session_track_schema)? true: false;
  return false;
}


/**
  @brief Store the schema name as length-encoded string in the specified
         buffer.  Once the data is stored, we reset the flags related to
         state-change (see reset()).


  @param thd [IN]           The thd handle.
  @paran buf [INOUT]        Buffer to store the information to.

  @return
    false                   Success
    true                    Error
*/

bool Current_schema_tracker::store(THD *thd, String &buf)
{
  ulonglong db_length, length;

  length= db_length= thd->db().length;
  length += net_length_size(length);

  uchar *to= (uchar *) buf.prep_append(net_length_size(length) + 1,
                                       EXTRA_ALLOC);

  /* Session state type (SESSION_TRACK_SCHEMA) */
  to= net_store_length(to, (ulonglong)SESSION_TRACK_SCHEMA);

  /* Length of the overall entity. */
  to= net_store_length(to, length);

  /* Length of the changed current schema name. */
  net_store_length(to, db_length);

  /* Current schema name (length-encoded string). */
  store_lenenc_string(buf, thd->db().str, thd->db().length);

  reset();

  return false;
}


/**
  @brief Mark the tracker as changed.

  @param name [IN]          Always null.

  @return void
*/

void Current_schema_tracker::mark_as_changed(THD *thd,
                                             LEX_CSTRING *tracked_item_name
                                             MY_ATTRIBUTE((unused)))
{
  m_changed= true;
  thd->lex->safe_to_cache_query= 0;
}


/**
  @brief Reset the m_changed flag for next statement.

  @return                   void
*/

void Current_schema_tracker::reset()
{
  m_changed= false;
}

///////////////////////////////////////////////////////////////////////////////

/**
   @brief Constructor for transaction state tracker.
*/

Transaction_state_tracker::Transaction_state_tracker()
{
  m_enabled        = false;
  tx_changed       = TX_CHG_NONE;
  tx_curr_state    =
  tx_reported_state= TX_EMPTY;
  tx_read_flags    = TX_READ_INHERIT;
  tx_isol_level    = TX_ISOL_INHERIT;
}

/**
  @brief Enable/disable the tracker based on @@session_track_transaction_info's value.

  @param thd [IN]           The thd handle.

  @return
    true if updating the tracking level failed, false otherwise
*/

bool Transaction_state_tracker::update(THD *thd)
{
#ifdef EMBEDDED_LIBRARY
  return true;

#else
  if (thd->variables.session_track_transaction_info != TX_TRACK_NONE)
  {
    /*
      If we only just turned reporting on (rather than changing between
      state and chistics reporting), start from a defined state.
    */
    if (!m_enabled)
    {
      tx_curr_state     =
      tx_reported_state = TX_EMPTY;
      tx_changed       |= TX_CHG_STATE;
      m_enabled= true;
    }
    if (thd->variables.session_track_transaction_info == TX_TRACK_CHISTICS)
      tx_changed       |= TX_CHG_CHISTICS;
    mark_as_changed(thd, NULL);
  }
  else
    m_enabled= false;

  return false;
#endif
}


/**
  @brief Store the transaction state (and, optionally, characteristics)
         as length-encoded string in the specified buffer.  Once the data
         is stored, we reset the flags related to state-change (see reset()).


  @param thd [IN]           The thd handle.
  @paran buf [INOUT]        Buffer to store the information to.

  @return
    false                   Success
    true                    Error
*/

bool Transaction_state_tracker::store(THD *thd, String &buf)
{
  /* STATE */
  if (tx_changed & TX_CHG_STATE)
  {
    uchar *to= (uchar *) buf.prep_append(11, EXTRA_ALLOC);

    to= net_store_length((uchar *) to,
                         (ulonglong) SESSION_TRACK_TRANSACTION_STATE);

    to= net_store_length((uchar *) to, (ulonglong) 9);
    to= net_store_length((uchar *) to, (ulonglong) 8);

    *(to++)=  (tx_curr_state & TX_EXPLICIT)       ? 'T' :
             ((tx_curr_state & TX_IMPLICIT)       ? 'I' : '_');
    *(to++)=  (tx_curr_state & TX_READ_UNSAFE)    ? 'r' : '_';
    *(to++)= ((tx_curr_state & TX_READ_TRX) ||
              (tx_curr_state & TX_WITH_SNAPSHOT)) ? 'R' : '_';
    *(to++)=  (tx_curr_state & TX_WRITE_UNSAFE)   ? 'w' : '_';
    *(to++)=  (tx_curr_state & TX_WRITE_TRX)      ? 'W' : '_';
    *(to++)=  (tx_curr_state & TX_STMT_UNSAFE)    ? 's' : '_';
    *(to++)=  (tx_curr_state & TX_RESULT_SET)     ? 'S' : '_';
    *(to++)=  (tx_curr_state & TX_LOCKED_TABLES)  ? 'L' : '_';
  }

  /* CHARACTERISTICS -- How to restart the transaction */

  if ((thd->variables.session_track_transaction_info == TX_TRACK_CHISTICS) &&
      (tx_changed & TX_CHG_CHISTICS))
  {
    bool is_xa= !thd->get_transaction()->xid_state()
                    ->has_state(XID_STATE::XA_NOTR);

    // worst case: READ UNCOMMITTED + READ WRITE + CONSISTENT SNAPSHOT
    char buffer[110];
    String tx(buffer, sizeof(buffer), &my_charset_bin);
    tx.length(0);

    // Any characteristics to print?
    {
      /*
        We have four basic replay scenarios:

        a) SET TRANSACTION was used, but before an actual transaction
           was started, the load balancer moves the connection elsewhere.
           In that case, the same one-shots should be set up in the
           target session.  (read-only/read-write; isolation-level)

        b) The initial transaction has begun; the relevant characteristics
           are the session defaults, possibly overridden by previous
           SET TRANSACTION statements, possibly overridden or extended
           by options passed to the START TRANSACTION statement.
           If the load balancer wishes to move this transaction,
           it needs to be replayed with the correct characteristics.
           (read-only/read-write from SET or START;
           isolation-level from SET only, snapshot from START only)

        c) A subsequent transaction started with START TRANSACTION
           (which is legal syntax in lieu of COMMIT AND CHAIN in MySQL)
           may add/modify the current one-shots:

           - It may set up a read-only/read-write one-shot.
             This one-shot will override the value used in the previous
             transaction (whether that came from the default or a one-shot),
             and, like all one-shots currently do, it will carry over into
             any subsequent transactions that don't explicitly override them
             in turn. This behavior is not guaranteed in the docs and may
             change in the future, but the tracker item should correctly
             reflect whatever behavior a given version of mysqld implements.

           - It may also set up a WITH CONSISTENT SNAPSHOT one-shot.
             This one-shot does not currently carry over into subsequent
             transactions (meaning that with "traditional syntax", WITH
             CONSISTENT SNAPSHOT can only be requested for the first part
             of a transaction chain). Again, the tracker item should reflect
             mysqld behavior.

        d) A subsequent transaction started using COMMIT AND CHAIN
           (or, for that matter, BEGIN WORK, which is currently
           legal and equivalent syntax in MySQL, or START TRANSACTION
           sans options) will re-use any one-shots set up so far
           (with SET before the first transaction started, and with
           all subsequent STARTs), except for WITH CONSISTANT SNAPSHOT,
           which will never be chained and only applies when explicitly
           given.

        It bears noting that if we switch sessions in a follow-up
        transaction, SET TRANSACTION would be illegal in the old
        session (as a transaction is active), whereas in the target
        session which is being prepared, it should be legal, as no
        transaction (chain) should have started yet.

        Therefore, we are free to generate SET TRANSACTION as a replay
        statement even for a transaction that isn't the first in an
        ongoing chain. Consider

          SET TRANSACTION ISOLATION LEVEL READ UNCOMMITED;
          START TRANSACTION READ ONLY, WITH CONSISTENT SNAPSHOT;
          # work
          COMMIT AND CHAIN;

        If we switch away at this point, the replay in the new session
        needs to be

          SET TRANSACTION ISOLATION LEVEL READ UNCOMMITED;
          START TRANSACTION READ ONLY;

        When a transaction ends (COMMIT/ROLLBACK sans CHAIN), all
        per-transaction characteristics are reset to the session's
        defaults.

        This also holds for a transaction ended implicitly!  (transaction.cc)
        Once again, the aim is to have the tracker item reflect on a
        given mysqld's actual behavior.
      */

      /*
        "ISOLATION LEVEL"
        Only legal in SET TRANSACTION, so will always be replayed as such.
      */
      if (tx_isol_level != TX_ISOL_INHERIT)
      {
        /*
          Unfortunately, we can't re-use tx_isolation_names /
          tx_isolation_typelib as it hyphenates its items.
        */
        LEX_CSTRING isol[]= {
          { STRING_WITH_LEN("READ UNCOMMITTED") },
          { STRING_WITH_LEN("READ COMMITTED") },
          { STRING_WITH_LEN("REPEATABLE READ") },
          { STRING_WITH_LEN("SERIALIZABLE") }
        };

        tx.append(STRING_WITH_LEN("SET TRANSACTION ISOLATION LEVEL "));
        tx.append(isol[tx_isol_level - 1].str, isol[tx_isol_level - 1].length);
        tx.append(STRING_WITH_LEN("; "));
      }

      /*
        Start transaction will usually result in TX_EXPLICIT (transaction
        started, but no data attached yet), except when WITH CONSISTENT
        SNAPSHOT, in which case we may have data pending.
        If it's an XA transaction, we don't go through here so we can
        first print the trx access mode ("SET TRANSACTION READ ...")
        separately before adding XA START (whereas with START TRANSACTION,
        we can merge the access mode into the same statement).
      */
      if ((tx_curr_state & TX_EXPLICIT) && !is_xa)
      {
        tx.append(STRING_WITH_LEN("START TRANSACTION"));

        /*
          "WITH CONSISTENT SNAPSHOT"
          Defaults to no, can only be enabled.
          Only appears in START TRANSACTION.
        */
        if (tx_curr_state & TX_WITH_SNAPSHOT)
        {
          tx.append(STRING_WITH_LEN(" WITH CONSISTENT SNAPSHOT"));
          if (tx_read_flags != TX_READ_INHERIT)
            tx.append(STRING_WITH_LEN(","));
        }

        /*
          "READ WRITE / READ ONLY" can be set globally, per-session,
          or just for one transaction.

          The latter case can take the form of
          START TRANSACTION READ (WRITE|ONLY), or of
          SET TRANSACTION READ (ONLY|WRITE).
          (Both set thd->read_only for the upcoming transaction;
          it will ultimately be re-set to the session default.)

          As the regular session-variable tracker does not monitor the one-shot,
          we'll have to do it here.

          If READ is flagged as set explicitly (rather than just inherited
          from the session's default), we'll get the actual bool from the THD.
        */
        if (tx_read_flags != TX_READ_INHERIT)
        {
          if (tx_read_flags == TX_READ_ONLY)
            tx.append(STRING_WITH_LEN(" READ ONLY"));
          else
            tx.append(STRING_WITH_LEN(" READ WRITE"));
        }
        tx.append(STRING_WITH_LEN("; "));
      }
      else if (tx_read_flags != TX_READ_INHERIT)
      {
        /*
          "READ ONLY" / "READ WRITE"
          We could transform this to SET TRANSACTION even when it occurs
          in START TRANSACTION, but for now, we'll resysynthesize the original
          command as closely as possible.
        */
        tx.append(STRING_WITH_LEN("SET TRANSACTION "));
        if (tx_read_flags == TX_READ_ONLY)
          tx.append(STRING_WITH_LEN("READ ONLY; "));
        else
          tx.append(STRING_WITH_LEN("READ WRITE; "));
      }

      if ((tx_curr_state & TX_EXPLICIT) && is_xa)
      {
        XID *xid=thd->get_transaction()->xid_state()->get_xid();
        long glen, blen;

        tx.append(STRING_WITH_LEN("XA START"));

        if ((glen= xid->get_gtrid_length()) > 0)
        {
          tx.append(STRING_WITH_LEN(" '"));
          tx.append(xid->get_data(), glen);

          if ((blen= xid->get_bqual_length()) > 0)
          {
            tx.append(STRING_WITH_LEN("','"));
            tx.append(xid->get_data() + glen, blen);
          }
          tx.append(STRING_WITH_LEN("'"));

          if (xid->get_format_id() != 1)
          {
            tx.append(STRING_WITH_LEN(","));
            tx.append_ulonglong(xid->get_format_id());
          }
        }

        tx.append(STRING_WITH_LEN("; "));
      }

      // discard trailing space
      if (tx.length() > 0)
        tx.chop();
    }

    {
      ulonglong length= tx.length();      // length of the string payload ...
      /*
        We've set up the info string, now turn it into a proper tracker
        item we can send!
      */
      length += net_length_size(length);  // ... plus that of its length

      uchar *to= (uchar *) buf.prep_append(net_length_size(length) + 1,
                                           EXTRA_ALLOC);

      /* Session state type (SESSION_TRACK_TRANSACTION_CHARACTERISTICS) */
      to= net_store_length((uchar *) to,
                         (ulonglong) SESSION_TRACK_TRANSACTION_CHARACTERISTICS);

      /* Length of the overall entity. */
      to= net_store_length((uchar *) to, length);

      /* Transaction characteristics (length-encoded string). */
      store_lenenc_string(buf, tx.ptr(), tx.length());
    }
  }

  reset();

  return false;
}


/**
  @brief Mark the tracker as changed.

  @param name [IN]          Always null.

  @return void
*/

void Transaction_state_tracker::mark_as_changed(THD *thd,
                                                LEX_CSTRING *tracked_item_name
                                                MY_ATTRIBUTE((unused)))
{
  m_changed                    = true;
}


/**
  @brief Reset the m_changed flag for next statement.

  @return                   void
*/

void Transaction_state_tracker::reset()
{
  m_changed         =  false;
  tx_reported_state =  tx_curr_state;
  tx_changed        =  TX_CHG_NONE;
}


/**
  @brief  Helper function: turn table info into table access flag.
          Accepts table lock type and engine type flag (transactional/
          non-transactional), and returns the corresponding access flag
          out of TX_READ_TRX, TX_READ_UNSAFE, TX_WRITE_TRX, TX_WRITE_UNSAFE.

  @param thd [IN]           The thd handle
  @param set [IN]           The table's access/lock type
  @param set [IN]           Whether the table's engine is transactional

  @return                   The table access flag
*/
enum_tx_state Transaction_state_tracker::calc_trx_state(THD *thd,
                                                        thr_lock_type l,
                                                        bool has_trx)
{
  enum_tx_state      s;
  bool               read= (l <= TL_READ_NO_INSERT);

  if (read)
    s= has_trx ? TX_READ_TRX  : TX_READ_UNSAFE;
  else
    s= has_trx ? TX_WRITE_TRX : TX_WRITE_UNSAFE;

  return s;
}

/**
  @brief  Register the end of an (implicit or explicit) transaction.

  @param thd [IN]           The thd handle
*/
void Transaction_state_tracker::end_trx(THD *thd)
{
  DBUG_ASSERT(thd->variables.session_track_transaction_info > TX_TRACK_NONE);

  if ((!m_enabled) || (thd->state_flags & Open_tables_state::BACKUPS_AVAIL))
    return;

  if (tx_curr_state != TX_EMPTY)
  {
    if (tx_curr_state & TX_EXPLICIT)
      tx_changed  |= TX_CHG_CHISTICS;
    tx_curr_state &= TX_LOCKED_TABLES;
  }
  update_change_flags(thd);
}

/**
  @brief Clear flags pertaining to the current statement or transaction.
         May be called repeatedly within the same execution cycle.

  @param thd [IN]           The thd handle.
  @param set [IN]           The flags to clear
*/
void Transaction_state_tracker::clear_trx_state(THD *thd, uint clear)
{
  if ((!m_enabled) || (thd->state_flags & Open_tables_state::BACKUPS_AVAIL))
    return;

  tx_curr_state &= ~clear;
  update_change_flags(thd);
}

/**
  @brief Add flags pertaining to the current statement or transaction.
         May be called repeatedly within the same execution cycle,
         e.g. to add access info for more tables.

  @param thd [IN]           The thd handle.
  @param set [IN]           The flags to add
*/
void Transaction_state_tracker::add_trx_state(THD *thd, uint add)
{
  if ((!m_enabled) || (thd->state_flags & Open_tables_state::BACKUPS_AVAIL))
    return;

  if (add == TX_EXPLICIT)
  {
    /*
      Always send chistics item (if tracked), always replace state.
    */
    tx_changed |= TX_CHG_CHISTICS;
    tx_curr_state = TX_EXPLICIT;
  }

  /*
    If we're not in an implicit or explicit transaction, but
    autocommit==0 and tables are accessed, we flag "implicit transaction."
  */
  else if (!(tx_curr_state & (TX_EXPLICIT|TX_IMPLICIT)) &&
           (thd->variables.option_bits & OPTION_NOT_AUTOCOMMIT) &&
           (add &
            (TX_READ_TRX | TX_READ_UNSAFE | TX_WRITE_TRX | TX_WRITE_UNSAFE)))
    tx_curr_state |= TX_IMPLICIT;

  /*
    Only flag state when in transaction or LOCK TABLES is added.
  */
  if ((tx_curr_state & (TX_EXPLICIT | TX_IMPLICIT)) ||
      (add & TX_LOCKED_TABLES))
    tx_curr_state |= add;

  update_change_flags(thd);
}

/**
  @brief Add "unsafe statement" flag if applicable.

  @param thd [IN]           The thd handle.
  @param set [IN]           The flags to add
*/
void Transaction_state_tracker::add_trx_state_from_thd(THD *thd)
{
  if (m_enabled)
  {
    if (thd->lex->is_stmt_unsafe())
      add_trx_state(thd, TX_STMT_UNSAFE);

//    update_change_flags(thd); ###
  }
}

/**
  @brief Set read flags (read only/read write) pertaining to the next
         transaction.

  @param thd [IN]           The thd handle.
  @param set [IN]           The flags to set
*/
void Transaction_state_tracker::set_read_flags(THD *thd,
                                               enum enum_tx_read_flags flags)
{
  if (m_enabled && (tx_read_flags != flags))
  {
    tx_read_flags = flags;
    tx_changed   |= TX_CHG_CHISTICS;
    mark_as_changed(thd, NULL);
  }
}

/**
  @brief Set isolation level pertaining to the next transaction.

  @param thd [IN]           The thd handle.
  @param set [IN]           The isolation level to set
*/
void Transaction_state_tracker::set_isol_level(THD *thd,
                                               enum enum_tx_isol_level level)
{
  if (m_enabled && (tx_isol_level != level))
  {
    tx_isol_level = level;
    tx_changed   |= TX_CHG_CHISTICS;
    mark_as_changed(thd, NULL);
  }
}


///////////////////////////////////////////////////////////////////////////////
 
/** Constructor */
Session_state_change_tracker::Session_state_change_tracker()
{
  m_changed= false;
}

/**
  @brief Initiate the value of m_enabled based on
  @@session_track_state_change value.

  @param thd [IN]           The thd handle.
  @return                   false (always)

**/

bool Session_state_change_tracker::enable(THD *thd)
{
  m_enabled= (thd->variables.session_track_state_change)? true: false;
  return false;
}

/**
  @Enable/disable the tracker based on @@session_track_state_change value.

  @param thd [IN]           The thd handle.
  @return                   false (always)

**/

bool Session_state_change_tracker::update(THD *thd)
{
  return enable(thd);
}

/**
  @brief Store the 1byte boolean flag in the specified buffer. Once the
         data is stored, we reset the flags related to state-change. If
         1byte flag valie is 1 then there is a session state change else
         there is no state change information.

  @param thd [IN]           The thd handle.
  @paran buf [INOUT]        Buffer to store the information to.

  @return
    false                   Success
    true                    Error
**/

bool Session_state_change_tracker::store(THD *thd, String &buf)
{
  /* since its a boolean tracker length is always 1 */
  const ulonglong length= 1;

  uchar *to= (uchar *) buf.prep_append(3,EXTRA_ALLOC);

  /* format of the payload is as follows:
     [ tracker type] [length] [1 byte flag] */

  /* Session state type (SESSION_TRACK_STATE_CHANGE) */
  to= net_store_length(to, (ulonglong)SESSION_TRACK_STATE_CHANGE);

  /* Length of the overall entity it is always 1 byte */
  to= net_store_length(to, length);

  /* boolean tracker will go here */
  *to= (is_state_changed(thd) ? '1' : '0');

  reset();

  return false;
}

/**
  @brief Mark the tracker as changed and associated session
         attributes accordingly.

  @param name [IN]          Always null.
  @return void
*/

void Session_state_change_tracker::mark_as_changed(THD *thd,
                                                   LEX_CSTRING *tracked_item_name)
{
  /* do not send the boolean flag for the tracker itself
     in the OK packet */
  if(tracked_item_name &&
     (strncmp(tracked_item_name->str, "session_track_state_change", 26) == 0))
    m_changed= false;
  else
  {
    m_changed= true;
    thd->lex->safe_to_cache_query= 0;
  }
}

/**
  @brief Reset the m_changed flag for next statement.

  @return                   void
*/

void Session_state_change_tracker::reset()
{
  m_changed= false;
}

/**
  @brief find if there is a session state change

  @return
  true  - if there is a session state change
  false - if there is no session state change
**/

bool Session_state_change_tracker::is_state_changed(THD* thd)
{
  return m_changed;
}

///////////////////////////////////////////////////////////////////////////////

/**
  @brief Initialize session tracker objects.

  @param char_set [IN]      The character set info.

  @return                   void
*/

void Session_tracker::init(const CHARSET_INFO *char_set)
{
  m_trackers[SESSION_SYSVARS_TRACKER]=
    new (std::nothrow) Session_sysvars_tracker(char_set);
  m_trackers[CURRENT_SCHEMA_TRACKER]=
    new (std::nothrow) Current_schema_tracker;
  m_trackers[SESSION_STATE_CHANGE_TRACKER]=
    new (std::nothrow) Session_state_change_tracker;
  m_trackers[SESSION_GTIDS_TRACKER]=
    new (std::nothrow) Session_gtids_tracker;
  m_trackers[TRANSACTION_INFO_TRACKER]=
    new (std::nothrow) Transaction_state_tracker;
}

void Session_tracker::claim_memory_ownership()
{
  for (int i= 0; i <= SESSION_TRACKER_END; i ++)
    m_trackers[i]->claim_memory_ownership();
}

/**
  @brief Enables the tracker objects.

  @param thd [IN]    The thread handle.

  @return            void
*/
void Session_tracker::enable(THD *thd)
{
  for (int i= 0; i <= SESSION_TRACKER_END; i ++)
    m_trackers[i]->enable(thd);
}

/**
  @brief Method called during the server startup to verify the contents
         of @@session_track_system_variables.

  @param    char_set        The character set info.
  @param    var_list        Value of @@session_track_system_variables.

  @return   false           Success
            true            failure
*/
bool Session_tracker::server_boot_verify(const CHARSET_INFO *char_set,
                                         LEX_STRING var_list)
{
  Session_sysvars_tracker *server_tracker;
  bool result;
  server_tracker= new (std::nothrow) Session_sysvars_tracker(char_set);
  result= server_tracker->server_init_check(char_set, var_list);
  delete server_tracker;
  return result;
}



/**
  @brief Returns the pointer to the tracker object for the specified tracker.

  @param tracker [IN]       Tracker type.

  @return                   Pointer to the tracker object.
*/

State_tracker *
Session_tracker::get_tracker(enum_session_tracker tracker) const
{
  return m_trackers[tracker];
}


/**
  @brief Checks if m_enabled flag is set for any of the tracker objects.

  @return
    true  - At least one of the trackers is enabled.
    false - None of the trackers is enabled.

*/

bool Session_tracker::enabled_any()
{
  for (int i= 0; i <= SESSION_TRACKER_END; i ++)
  {
    if (m_trackers[i]->is_enabled())
      return true;
  }
  return false;
}

/**
  @brief Checks if m_changed flag is set for any of the tracker objects.

  @return
    true                    At least one of the entities being tracker has
                            changed.
    false                   None of the entities being tracked has changed.
*/

bool Session_tracker::changed_any()
{
  for (int i= 0; i <= SESSION_TRACKER_END; i ++)
  {
    if (m_trackers[i]->is_changed())
      return true;
  }
  return false;
}


/**
  @brief Store all change information in the specified buffer.

  @param thd [IN]           The thd handle.
  @param buf [OUT]          Reference to the string buffer to which the state
                            change data needs to be written.

  @return                   void
*/

void Session_tracker::store(THD *thd, String &buf)
{
  /* Temporary buffer to store all the changes. */
  String temp;
  size_t length;

  /* Get total length. */
  for (int i= 0; i <= SESSION_TRACKER_END; i ++)
  {
    if (m_trackers[i]->is_changed())
      m_trackers[i]->store(thd, temp);
  }

  length= temp.length();
  /* Store length first.. */
  char *to= buf.prep_append(net_length_size(length), EXTRA_ALLOC);
  net_store_length((uchar *) to, length);

  /* .. and then the actual info. */
  buf.append(temp);
  temp.mem_free();
}


/**
  @brief Stores the given string in length-encoded format into the specified
         buffer.

  @param to     [IN]        Buffer to store the given string in.
  @param from   [IN]        The give string to be stored.
  @param length [IN]        Length of the above string.

  @return                   void.
*/

static
void store_lenenc_string(String &to, const char *from, size_t length)
{
  char *ptr;
  ptr= to.prep_append(net_length_size(length), EXTRA_ALLOC);
  net_store_length((uchar *) ptr, length);
  to.append(from, length);
}

/**
  @brief Enable/disable the tracker based on @@session_track_gtids's value.

  @param thd [IN]           The thd handle.

  @return
    false (always)
*/

bool Session_gtids_tracker::update(THD *thd)
{
  /*
    We are updating this using the previous value. No change needed.
    Bailing out.
  */
  if (m_enabled == (thd->variables.session_track_gtids != OFF))
    return false;

  m_enabled= thd->variables.session_track_gtids != OFF &&
             /* No need to track GTIDs for system threads. */
             thd->system_thread == NON_SYSTEM_THREAD;
  if (m_enabled)
  {
    // register to listen to gtids context state changes
    thd->rpl_thd_ctx.session_gtids_ctx().register_ctx_change_listener(this, thd);

    // instantiate the encoder if needed
    if (m_encoder == NULL)
    {
      /*
       TODO: in the future, there can be a variable to control which
       encoder instance to instantiate here.

       This means that if we ever make the server encode deltas instead,
       or compressed GTIDS we want to change the encoder instance below.

       Right now, by default we instantiate the encoder that has.
      */
      m_encoder= new Session_gtids_ctx_encoder_string();
    }
  }
  // else /* break the bridge between tracker and collector */
  return false;
}

/**
  @brief Store the collected gtids as length-encoded string in the specified
         buffer.  Once the data is stored, we reset the flags related to
         state-change (see reset()).


  @param thd [IN]           The thd handle.
  @paran buf [INOUT]        Buffer to store the information to.

  @return
    false                   Success
    true                    Error
*/

bool Session_gtids_tracker::store(THD *thd, String &buf)
{
  if (m_encoder && m_encoder->encode(thd, buf))
    return true;
  reset();
  return false;
}

/**
  @brief Mark the tracker as changed.

  @param thd               [IN]          Always null.
  @param tracked_item_name [IN]          Always null.

  @return void
*/

void Session_gtids_tracker::mark_as_changed(THD *thd MY_ATTRIBUTE((unused)),
                                            LEX_CSTRING *tracked_item_name
                                            MY_ATTRIBUTE((unused)))
{
  m_changed= true;
}


/**
  @brief Reset the m_changed flag for next statement.

  @return                   void
*/

void Session_gtids_tracker::reset()
{
  /*
   Delete the encoder and remove the listener if this had been previously
   deactivated.
   */
  if (!m_enabled && m_encoder)
  {
    /* No need to listen to gtids context state changes */
    current_thd->rpl_thd_ctx.session_gtids_ctx().unregister_ctx_change_listener(this);

    // delete the encoder (just to free memory)
    delete m_encoder; // if not tracking, delete the encoder
    m_encoder= NULL;
  }
  m_changed= false;
}
