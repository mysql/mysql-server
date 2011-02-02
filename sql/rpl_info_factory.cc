/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include "sql_priv.h"
#include "rpl_slave.h"
#include "rpl_info_factory.h"

/*
  We need to replace these definitions by an option that states the
  engine one wants to use in the master info repository.
*/
#define master_info_engine NULL
#define relay_log_info_engine NULL

/**
  Creates both a Master info and a Relay log info repository whose types are
  defined as parameters.

  @todo Make the repository a pluggable component.
  
  @param[in]  mi_option  Type of the Master info repository
  @param[out] mi         Reference to the Master info repository
  @param[in]  rli_option Type of the Relay log info repository
  @param[out] rli        Reference to the Relay log info repository

  @retval FALSE No error
  @retval TRUE  Failure
*/ 
bool Rpl_info_factory::create(uint mi_option, Master_info **mi,
                              uint rli_option, Relay_log_info **rli)
{
  DBUG_ENTER("Rpl_info_factory::Rpl_info_factory");

  if (!((*mi)= Rpl_info_factory::create_mi(mi_option)))
    DBUG_RETURN(TRUE);
    
  if (!((*rli)= Rpl_info_factory::create_rli(rli_option, relay_log_recovery)))
  {
    delete *mi;
    *mi= NULL;
    DBUG_RETURN(TRUE);
  }

  /*
    Setting the cross dependency used all over the code.
  */
  (*mi)->set_relay_log_info(*rli);
  (*rli)->set_master_info(*mi);

  DBUG_RETURN(FALSE); 
}

/**
  Creates a Master info repository whose type is defined as a parameter.
  
  @param[in]  mi_option  Type of the Master info repository
  @param[out] mi         Reference to the Master info repository

  The execution fails if a user requests a type but a different type
  already exists in the system. This is done to avoid that a user
  accidentally accesses the wrong repository and make the slave go out
  of sync.

  @retval FALSE No error
  @retval TRUE  Failure
*/ 
Master_info *Rpl_info_factory::create_mi(uint mi_option)
{
  Master_info* mi= NULL;
  Rpl_info_file*  mi_file= NULL;
  Rpl_info_table*  mi_table= NULL;
  const char *msg= "Failed to allocate memory for the master info "
                   "structure";

  DBUG_ENTER("Rpl_info_factory::Rpl_info_factory");

  if (!(mi= new Master_info(
#ifdef HAVE_PSI_INTERFACE
                            &key_master_info_run_lock,
                            &key_master_info_data_lock,
                            &key_master_info_data_cond,
                            &key_master_info_start_cond,
                            &key_master_info_stop_cond
#endif
                           )))
    goto err;

  /*
    Now we instantiate all info repos and later decide which one to take,
    but not without first checking if there is already existing data for
    a repo different from the one that is being requested.
  */
  if (!(mi_file= new Rpl_info_file(mi->get_number_info_mi_fields(),
                                   master_info_file)))
    goto err;

  if (!(mi_table= new Rpl_info_table(mi->get_number_info_mi_fields() + 1,
                                     MI_FIELD_ID, MI_SCHEMA, MI_TABLE)))
    goto err;

  DBUG_ASSERT(mi_option == MI_REPOSITORY_FILE ||
              mi_option == MI_REPOSITORY_TABLE);

  if (decide_repository(mi, &mi_table, &mi_file,
                        mi_option == MI_REPOSITORY_TABLE, &msg))
    goto err;

  if ((mi_option == MI_REPOSITORY_TABLE) &&
       change_engine(static_cast<Rpl_info_table *>(mi_table),
                     master_info_engine, &msg))
    goto err;

  DBUG_RETURN(mi);

err:
  if (mi_file) delete mi_file;
  if (mi_table) delete mi_table;
  if (mi)
  {
    /*
      The handler was previously deleted so we need to remove
      any reference to it.  
    */
    mi->set_rpl_info_handler(NULL);
    delete (mi);
  }
  sql_print_error("%s", msg);
  DBUG_RETURN(NULL);
}

/**
  Creates a Relay log info repository whose type is defined as a parameter.
  
  @param[in]  rli_option        Type of the Relay log info repository
  @param[in]  is_slave_recovery If the slave should try to start a recovery
                                process to get consistent relay log files
  @param[out] rli               Reference to the Relay log info repository

  The execution fails if a user requests a type but a different type
  already exists in the system. This is done to avoid that a user
  accidentally accesses the wrong repository and make the slave go out
  of sync.

  @retval FALSE No error
  @retval TRUE  Failure
*/ 
Relay_log_info *Rpl_info_factory::create_rli(uint rli_option, bool is_slave_recovery)
{
  Relay_log_info *rli= NULL;
  Rpl_info_file* rli_file= NULL;
  Rpl_info_table* rli_table= NULL;
  const char *msg= "Failed to allocate memory for the relay log info "
                   "structure";

  DBUG_ENTER("Rpl_info_factory::create_rli");

  if (!(rli= new Relay_log_info(is_slave_recovery
#ifdef HAVE_PSI_INTERFACE
                                ,&key_relay_log_info_run_lock,
                                &key_relay_log_info_data_lock,
                                &key_relay_log_info_data_cond,
                                &key_relay_log_info_start_cond,
                                &key_relay_log_info_stop_cond
#endif
                               )))
    goto err;

  /*
    Now we instantiate all info repos and later decide which one to take,
    but not without first checking if there is already existing data for
    a repo different from the one that is being requested.
  */
  if (!(rli_file= new Rpl_info_file(rli->get_number_info_rli_fields(),
                                    relay_log_info_file)))
    goto err;

  if (!(rli_table= new Rpl_info_table(rli->get_number_info_rli_fields() + 1,
                                      RLI_FIELD_ID, RLI_SCHEMA, RLI_TABLE)))
    goto err;

  DBUG_ASSERT(rli_option == RLI_REPOSITORY_FILE ||
              rli_option == RLI_REPOSITORY_TABLE);

  if (decide_repository(rli, &rli_table, &rli_file,
                        rli_option == RLI_REPOSITORY_TABLE, &msg))
    goto err;

  if ((rli_option == RLI_REPOSITORY_TABLE) &&
      change_engine(static_cast<Rpl_info_table *>(rli_table),
                    relay_log_info_engine, &msg))
    goto err;

  DBUG_RETURN(rli);

err:
  if (rli_file) delete rli_file;
  if (rli_table) delete rli_table;
  if (rli) 
  {
    /*
      The handler was previously deleted so we need to remove
      any reference to it.  
    */
    rli->set_rpl_info_handler(NULL);
    delete (rli);
  }
  sql_print_error("%s", msg);
  DBUG_RETURN(NULL);
}

/**
  Decides what repository will be used based on the following decision table:

  \code
  |--------------+-----------------------+-----------------------|
  | Exists \ Opt |         TABLE         |          FILE         |
  |--------------+-----------------------+-----------------------|
  | ~is_t,  is_f | Update T and delete F | Read F                |
  |  is_t,  is_f | ERROR                 | ERROR                 |
  | ~is_t, ~is_f | Fill in T             | Create and Fill in F  |
  |  is_t, ~is_f | Read T                | Update F and delete T |
  |--------------+-----------------------+-----------------------|
  \endcode

  <ul>
    \li F     --> file

    \li T     --> table

    \li is_t  --> table with data

    \li is_f  --> file with data

    \li ~is_t --> no data in the table

    \li ~is_f --> no file
  </ul> 

  @param[in] info     Either master info or relay log info.
  @param[in] table    Table handler.
  @param[in] file     File handler.
  @param[in] is_table True if a table handler was requested.
  @param[out] msg     Message specifying what went wrong, if there is any error.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::decide_repository(Rpl_info *info, Rpl_info_table **table,
                                         Rpl_info_file **file, bool is_table,
                                         const char **msg)
{

  DBUG_ENTER("Rpl_info_factory::decide_repository");
 
  bool error= TRUE;
  bool is_t= !((*table)->check_info());
  bool is_f= !((*file)->check_info());

  if (is_t && is_f)
  {
    *msg= "Multiple replication metadata repository instances "
          "found with data in them. Unable to decide which is "
          "the correct one to choose.";
    DBUG_RETURN(error);
  }

  if (is_table)
  {
    if (!is_t && is_f)
    {
      if ((*table)->init_info() || (*file)->init_info())
      {
        *msg= "Error transfering information from a file to a table.";
        goto err;
      }
      /*
        Transfer the information from the file to the table and delete the
        file, i.e. Update the table (T) and delete the file (F).
      */
      if (info->copy_info(*file, *table) || (*file)->remove_info())
      {
        *msg= "Error transfering information from a file to a table.";
        goto err;
      }
    }
    delete (*file);
    info->set_rpl_info_handler(*table);
    error= FALSE;
    *file= NULL;
  }
  else
  {
    if (is_t && !is_f)
    {
      if ((*table)->init_info() || (*file)->init_info())
      {
        *msg= "Error transfering information from a file to a table.";
        goto err;
      }
      /*
        Transfer the information from the table to the file and delete 
        entries in the table, i.e. Update the file (F) and delete the
        table (T).
      */
      if (info->copy_info(*table, *file) || (*table)->remove_info())
      {
        *msg= "Error transfering information from a table to a file.";
        goto err;
      } 
    }
    delete (*table);
    info->set_rpl_info_handler(*file);
    error= FALSE;
    *table= NULL;
  }

err:
  DBUG_RETURN(error); 
}

/**
  Changes the engine in use by a handler.
  
  @param[in]  handler Reference to a handler.
  @param[in]  engine  Type of the engine, e.g. Innodb, MyIsam.
  @param[out] msg     Message specifying what went wrong, if there is any error.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::change_engine(Rpl_info_table *table, const char *engine,
                                     const char **msg)
{
  DBUG_ENTER("Rpl_info_factory::decide_engine");

  if (engine && table->change_engine(engine))
  {
    *msg= "Error changing the engine for a respository.";
    DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}
