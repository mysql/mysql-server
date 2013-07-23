/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include "sql_priv.h"
#include "rpl_slave.h"
#include "rpl_info_factory.h"

/*
  Defines meta information on diferent repositories.
*/
Rpl_info_factory::struct_table_data Rpl_info_factory::rli_table_data;
Rpl_info_factory::struct_file_data Rpl_info_factory::rli_file_data;
Rpl_info_factory::struct_table_data Rpl_info_factory::mi_table_data;
Rpl_info_factory::struct_file_data Rpl_info_factory::mi_file_data;
Rpl_info_factory::struct_file_data Rpl_info_factory::worker_file_data;
Rpl_info_factory::struct_table_data Rpl_info_factory::worker_table_data;

/**
  Creates both a Master info and a Relay log info repository whose types are
  defined as parameters. Nothing is done for Workers here.

  @todo Make the repository a pluggable component.
  @todo Use generic programming to make it easier and clearer to
        add a new repositories' types and Rpl_info objects.
  
  @param[in]  mi_option  Type of the Master info repository.
  @param[out] mi         Reference to the Master_info.
  @param[in]  rli_option Type of the Relay log info repository.
  @param[out] rli        Reference to the Relay_log_info.

  @retval FALSE No error
  @retval TRUE  Failure
*/ 
bool Rpl_info_factory::create_coordinators(uint mi_option, Master_info **mi,
                                           uint rli_option, Relay_log_info **rli)
{
  DBUG_ENTER("Rpl_info_factory::create_coordinators");

  Rpl_info_factory::init_repository_metadata();

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
  
  @param[in]  mi_option Type of the repository, e.g. FILE TABLE.

  The execution fails if a user requests a type but a different type
  already exists in the system. This is done to avoid that a user
  accidentally accesses the wrong repository and makes the slave go out
  of sync.

  @retval Pointer to Master_info Success
  @retval NULL  Failure
*/ 
Master_info *Rpl_info_factory::create_mi(uint mi_option)
{
  Master_info* mi= NULL;
  Rpl_info_handler*  handler_src= NULL;
  Rpl_info_handler*  handler_dest= NULL;
  uint instances= 1;
  const char *msg= "Failed to allocate memory for the master info "
                   "structure";

  DBUG_ENTER("Rpl_info_factory::create_mi");

  if (!(mi= new Master_info(
#ifdef HAVE_PSI_INTERFACE
                            &key_master_info_run_lock,
                            &key_master_info_data_lock,
                            &key_master_info_sleep_lock,
                            &key_master_info_data_cond,
                            &key_master_info_start_cond,
                            &key_master_info_stop_cond,
                            &key_master_info_sleep_cond,
#endif
                            instances
                           )))
    goto err;

  if(init_repositories(mi_table_data, mi_file_data, mi_option, instances,
                       &handler_src, &handler_dest, &msg))
    goto err;

  if (decide_repository(mi, mi_option, &handler_src, &handler_dest, &msg))
    goto err;

  DBUG_RETURN(mi);

err:
  delete handler_src;
  delete handler_dest;
  if (mi)
  {
    /*
      The handler was previously deleted so we need to remove
      any reference to it.  
    */
    mi->set_rpl_info_handler(NULL);
    delete mi;
  }
  sql_print_error("Error creating master info: %s.", msg);
  DBUG_RETURN(NULL);
}

/**
  Allows to change the master info repository after startup.

  @param[in]  mi        Reference to Master_info.
  @param[in]  mi_option Type of the repository, e.g. FILE TABLE.
  @param[out] msg       Error message if something goes wrong.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::change_mi_repository(Master_info *mi,
                                            uint mi_option,
                                            const char **msg)
{
  Rpl_info_handler*  handler_src= mi->get_rpl_info_handler();
  Rpl_info_handler*  handler_dest= NULL;
  uint instances= 1;
  DBUG_ENTER("Rpl_info_factory::change_mi_repository");

  DBUG_ASSERT(handler_src);
  if (handler_src->get_rpl_info_type() == mi_option)
    DBUG_RETURN(false);

  if (init_repositories(mi_table_data, mi_file_data, mi_option, instances,
                        NULL, &handler_dest, msg))
    goto err;

  if (decide_repository(mi, mi_option, &handler_src, &handler_dest, msg))
    goto err;

  DBUG_RETURN(FALSE);

err:
  delete handler_dest;
  handler_dest= NULL;

  sql_print_error("Error changing the type of master info's repository: %s.", *msg);
  DBUG_RETURN(TRUE);
}

/**
  Creates a Relay log info repository whose type is defined as a parameter.
  
  @param[in]  rli_option        Type of the Relay log info repository
  @param[in]  is_slave_recovery If the slave should try to start a recovery
                                process to get consistent relay log files

  The execution fails if a user requests a type but a different type
  already exists in the system. This is done to avoid that a user
  accidentally accesses the wrong repository and make the slave go out
  of sync.

  @retval Pointer to Relay_log_info Success
  @retval NULL  Failure
*/ 
Relay_log_info *Rpl_info_factory::create_rli(uint rli_option, bool is_slave_recovery)
{
  Relay_log_info *rli= NULL;
  Rpl_info_handler* handler_src= NULL;
  Rpl_info_handler* handler_dest= NULL;
  uint instances= 1;
  uint worker_repository= INVALID_INFO_REPOSITORY;
  uint worker_instances= 1;
  const char *msg= NULL;
  const char *msg_alloc= "Failed to allocate memory for the relay log info "
    "structure";

  DBUG_ENTER("Rpl_info_factory::create_rli");

  /*
    Returns how many occurrences of rli's repositories exist. For example,
    if the repository is a table, this retrieves the number of rows in it.
    Besides, it also returns the type of the repository where entries were
    found.
  */
  if (rli_option != INFO_REPOSITORY_DUMMY &&
      scan_repositories(&worker_instances, &worker_repository,
                        worker_table_data, worker_file_data, &msg))
    goto err;

  if (!(rli= new Relay_log_info(is_slave_recovery
#ifdef HAVE_PSI_INTERFACE
                                ,&key_relay_log_info_run_lock,
                                &key_relay_log_info_data_lock,
                                &key_relay_log_info_sleep_lock,
                                &key_relay_log_info_data_cond,
                                &key_relay_log_info_start_cond,
                                &key_relay_log_info_stop_cond,
                                &key_relay_log_info_sleep_cond
#endif
                                , instances
                                )))
  {
    msg= msg_alloc;
    goto err;
  }

  if(init_repositories(rli_table_data, rli_file_data, rli_option, instances,
                       &handler_src, &handler_dest, &msg))
    goto err;

  if (rli_option != INFO_REPOSITORY_DUMMY &&
      worker_repository != INVALID_INFO_REPOSITORY &&
      worker_repository != rli_option)
  {
    opt_rli_repository_id= rli_option= worker_repository;
    sql_print_warning("It is not possible to change the type of the relay log "
                      "repository because there are workers repositories with "
                      "possible execution gaps. "
                      "The value of --relay_log_info_repository is altered to "
                      "one of the found Worker repositories. "
                      "The gaps have to be sorted out before resuming with "
                      "the type change.");
    std::swap(handler_src, handler_dest);
  }
  if (decide_repository(rli, rli_option, &handler_src, &handler_dest, &msg))
    goto err;

  DBUG_RETURN(rli);

err:
  delete handler_src;
  delete handler_dest;
  if (rli) 
  {
    /*
      The handler was previously deleted so we need to remove
      any reference to it.  
    */
    rli->set_rpl_info_handler(NULL);
    delete rli;
  }
  sql_print_error("Error creating relay log info: %s.", msg);
  DBUG_RETURN(NULL);
}

/**
  Allows to change the relay log info repository after startup.

  @param[in]  mi        Pointer to Relay_log_info.
  @param[in]  mi_option Type of the repository, e.g. FILE TABLE.
  @param[out] msg       Error message if something goes wrong.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::change_rli_repository(Relay_log_info *rli,
                                             uint rli_option,
                                             const char **msg)
{
  Rpl_info_handler*  handler_src= rli->get_rpl_info_handler();
  Rpl_info_handler*  handler_dest= NULL;
  uint instances= 1;
  DBUG_ENTER("Rpl_info_factory::change_rli_repository");

  DBUG_ASSERT(handler_src != NULL);
  
  if (handler_src->get_rpl_info_type() == rli_option)
    DBUG_RETURN(false);

  if (init_repositories(rli_table_data, rli_file_data, rli_option,
                        instances, NULL, &handler_dest, msg))
    goto err;

  if (decide_repository(rli, rli_option, &handler_src, &handler_dest,
                        msg))
    goto err;

  DBUG_RETURN(FALSE);

err:
  delete handler_dest;
  handler_dest= NULL;

  sql_print_error("Error changing the type of relay log info's repository: %s.", *msg);
  DBUG_RETURN(TRUE);
}

/**
   Delete all info from Worker info tables to render them useless in 
   future MTS recovery, and indicate that in Coordinator info table.

   @return false on success, true when a failure in deletion or writing
           to Coordinator table fails. 
*/
bool Rpl_info_factory::reset_workers(Relay_log_info *rli)
{
  bool error= true;

  DBUG_ENTER("Rpl_info_factory::reset_workers");

  if (rli->recovery_parallel_workers == 0)
    DBUG_RETURN(0);

  if (Rpl_info_file::do_reset_info(Slave_worker::get_number_worker_fields(),
                                   worker_file_data.pattern,
                                   worker_file_data.name_indexed))
    goto err;

  if (Rpl_info_table::do_reset_info(Slave_worker::get_number_worker_fields(),
                                    MYSQL_SCHEMA_NAME.str, WORKER_INFO_NAME.str))
    goto err;

  error= false;

  DBUG_EXECUTE_IF("mts_debug_reset_workers_fails", error= true;);

err:
  if (error)
    sql_print_error("Could not delete from Slave Workers info repository.");
  rli->recovery_parallel_workers= 0;
  if (rli->flush_info(true))
  {
    error= true;
    sql_print_error("Could not store the reset Slave Worker state into "
                    "the slave info repository.");
  }
  DBUG_RETURN(error);
}

/**
  Creates a Slave worker repository whose type is defined as a parameter.
  
  @param[in]  rli_option Type of the repository, e.g. FILE TABLE.
  @param[in]  rli        Pointer to Relay_log_info.

  The execution fails if a user requests a type but a different type
  already exists in the system. This is done to avoid that a user
  accidentally accesses the wrong repository and make the slave go out
  of sync.

  @retval Pointer to Slave_worker Success
  @retval NULL  Failure
*/ 
Slave_worker *Rpl_info_factory::create_worker(uint rli_option, uint worker_id,
                                              Relay_log_info *rli,
                                              bool is_gaps_collecting_phase)
{
  Rpl_info_handler* handler_src= NULL;
  Rpl_info_handler* handler_dest= NULL;
  Slave_worker* worker= NULL;
  const char *msg= "Failed to allocate memory for the worker info "
                   "structure";

  DBUG_ENTER("Rpl_info_factory::create_worker");

  /*
    Define the name of the worker and its repository.
  */
  char *pos= strmov(worker_file_data.name, worker_file_data.pattern);
  sprintf(pos, "%u", worker_id + 1);

  if (!(worker= new Slave_worker(rli
#ifdef HAVE_PSI_INTERFACE
                                 ,&key_relay_log_info_run_lock,
                                 &key_relay_log_info_data_lock,
                                 &key_relay_log_info_sleep_lock,
                                 &key_relay_log_info_data_cond,
                                 &key_relay_log_info_start_cond,
                                 &key_relay_log_info_stop_cond,
                                 &key_relay_log_info_sleep_cond
#endif
                                 , worker_id
                                )))
    goto err;


  if(init_repositories(worker_table_data, worker_file_data, rli_option,
                       worker_id + 1, &handler_src, &handler_dest, &msg))
    goto err;

  if (decide_repository(worker, rli_option, &handler_src, &handler_dest, &msg))
    goto err;
       
  if (worker->rli_init_info(is_gaps_collecting_phase))
  {
    msg= "Failed to initialize the worker info structure";
    goto err;
  }

  if (rli->info_thd && rli->info_thd->is_error())
  {
    msg= "Failed to initialize worker info table";
    goto err;
  }

  DBUG_RETURN(worker);

err:
  delete handler_src;
  delete handler_dest;
  if (worker)
  {
    /*
      The handler was previously deleted so we need to remove
      any reference to it.  
    */
    worker->set_rpl_info_handler(NULL);
    delete worker;
  }
  sql_print_error("Error creating relay log info: %s.", msg);
  DBUG_RETURN(NULL);
}

static void build_worker_info_name(char* to,
                                   const char* path,
                                   const char* fname)
{
  DBUG_ASSERT(to);
  char* pos= to;
  if (path[0])
    pos= strmov(pos, path);
  pos= strmov(pos, "worker-");
  pos= strmov(pos, fname);
  strmov(pos, ".");
}

/**
  Initializes startup information on diferent repositories.
*/
void Rpl_info_factory::init_repository_metadata()
{
  /* Needed for the file names and paths for worker info files. */
  size_t len;
  char* relay_log_info_file_name;
  char relay_log_info_file_dirpart[FN_REFLEN];

  /* Extract the directory name from relay_log_info_file */
  dirname_part(relay_log_info_file_dirpart, relay_log_info_file, &len);
  relay_log_info_file_name= relay_log_info_file + len;

  rli_table_data.n_fields= Relay_log_info::get_number_info_rli_fields();
  rli_table_data.schema= MYSQL_SCHEMA_NAME.str;
  rli_table_data.name= RLI_INFO_NAME.str;
  rli_file_data.n_fields= Relay_log_info::get_number_info_rli_fields();
  strmov(rli_file_data.name, relay_log_info_file);
  strmov(rli_file_data.pattern, relay_log_info_file);
  rli_file_data.name_indexed= false;

  mi_table_data.n_fields= Master_info::get_number_info_mi_fields();
  mi_table_data.schema= MYSQL_SCHEMA_NAME.str;
  mi_table_data.name= MI_INFO_NAME.str;
  mi_file_data.n_fields= Master_info::get_number_info_mi_fields();
  strmov(mi_file_data.name, master_info_file);
  strmov(mi_file_data.pattern, master_info_file);
  rli_file_data.name_indexed= false;

  worker_table_data.n_fields= Slave_worker::get_number_worker_fields();
  worker_table_data.schema= MYSQL_SCHEMA_NAME.str;
  worker_table_data.name= WORKER_INFO_NAME.str;
  worker_file_data.n_fields= Slave_worker::get_number_worker_fields();
  build_worker_info_name(worker_file_data.name,
                         relay_log_info_file_dirpart,
                         relay_log_info_file_name);
  build_worker_info_name(worker_file_data.pattern,
                         relay_log_info_file_dirpart,
                         relay_log_info_file_name);
  worker_file_data.name_indexed= true;
}


/**
  Decides during startup what repository will be used based on the following
  decision table:

  \code
  |--------------+-----------------------+-----------------------|
  | Exists \ Opt |         SOURCE        |      DESTINATION      |
  |--------------+-----------------------+-----------------------|
  | ~is_s, ~is_d |            -          | Create/Update D       |
  | ~is_s,  is_d |            -          | Continue with D       |
  |  is_s, ~is_d | Copy S into D         | Create/Update D       |
  |  is_s,  is_d | Error                 | Error                 |
  |--------------+-----------------------+-----------------------|
  \endcode

  @param[in]  info         Either master info or relay log info.
  @param[in]  option       Identifies the type of the repository that will
                           be used, i.e., destination repository.
  @param[out] handler_src  Source repository from where information is
                           copied into the destination repository.
  @param[out] handler_dest Destination repository to where informaiton is
                           copied.
  @param[out] msg          Error message if something goes wrong.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::decide_repository(Rpl_info *info, uint option,
                                         Rpl_info_handler **handler_src,
                                         Rpl_info_handler **handler_dest,
                                         const char **msg)
{
  bool error= true;
  enum_return_check return_check_src= ERROR_CHECKING_REPOSITORY;
  enum_return_check return_check_dst= ERROR_CHECKING_REPOSITORY;
  DBUG_ENTER("Rpl_info_factory::decide_repository");

  if (option == INFO_REPOSITORY_DUMMY)
  {
    delete (*handler_src);
    *handler_src= NULL;
    info->set_rpl_info_handler(*handler_dest);
    error = false;
    goto err;
  }

  DBUG_ASSERT((*handler_src) != NULL && (*handler_dest) != NULL &&
              (*handler_src) != (*handler_dest));

  return_check_src= check_src_repository(info, option, handler_src);
  return_check_dst= (*handler_dest)->do_check_info(info->get_internal_id());

  if (return_check_src == ERROR_CHECKING_REPOSITORY ||
      return_check_dst == ERROR_CHECKING_REPOSITORY)
  {
    /*
      If there is a problem with one of the repositories we print out
      more information and exit.
    */
    DBUG_RETURN(check_error_repository(info, *handler_src, *handler_dest,
                                       return_check_src,
                                       return_check_dst, msg));
  }
  else
  {
    if ((return_check_src == REPOSITORY_EXISTS &&
        return_check_dst == REPOSITORY_DOES_NOT_EXIST) ||
        (return_check_src == REPOSITORY_EXISTS &&
        return_check_dst == REPOSITORY_EXISTS))
    {
      /*
        If there is no error, we can proceed with the normal operation.
        However, if both repositories are set an error will be printed
        out.
      */
      if (return_check_src == REPOSITORY_EXISTS &&
        return_check_dst == REPOSITORY_EXISTS)
      {
        *msg= "Multiple replication metadata repository instances "
              "found with data in them. Unable to decide which is "
              "the correct one to choose";
        goto err;
      }

      /*
        Do a low-level initialization to be able to do a state transfer. 
      */
      if (init_repositories(info, handler_src, handler_dest, msg))
        goto err;

      /*
        Transfer information from source to destination and delete the
        source. Note this is not fault-tolerant and a crash before removing
        source may cause the next restart to fail as is_src and is_dest may
        be true. Moreover, any failure in removing the source may lead to
        the same.
        /Alfranio
      */
      if (info->copy_info(*handler_src, *handler_dest) ||
        (*handler_dest)->flush_info(true))
      {
        *msg= "Error transfering information";
        goto err;
      }
      (*handler_src)->end_info();
      if ((*handler_src)->remove_info())
      {
        *msg= "Error removing old repository";
        goto err;
      }
    }
    else if (return_check_src == REPOSITORY_DOES_NOT_EXIST &&
             return_check_dst == REPOSITORY_EXISTS)
    {
      DBUG_ASSERT(info->get_rpl_info_handler() == NULL);
      if ((*handler_dest)->do_init_info(info->get_internal_id()))
      {
        *msg= "Error reading repository";
        goto err;
      }
    }
    else
    {
      DBUG_ASSERT(return_check_src == REPOSITORY_DOES_NOT_EXIST &&
                  return_check_dst == REPOSITORY_DOES_NOT_EXIST);
    }

    delete (*handler_src);
    *handler_src= NULL;
    info->set_rpl_info_handler(*handler_dest);
    error= false;
  }

err:
  DBUG_RETURN(error); 
}


/**
  This method is called by the decide_repository() and is used to check if
  the source repository exits.

  @param[in]  info         Either master info or relay log info.
  @param[in]  option       Identifies the type of the repository that will
                           be used, i.e., destination repository.
  @param[out] handler_src  Source repository from where information is

  @return enum_return_check The repository's status.
*/
enum_return_check
Rpl_info_factory::check_src_repository(Rpl_info *info,
                                       uint option,
                                       Rpl_info_handler **handler_src)
{
  enum_return_check return_check_src= ERROR_CHECKING_REPOSITORY;
  bool live_migration = info->get_rpl_info_handler() != NULL;

  if (!live_migration)
  {
    /*
      This is not a live migration and we don't know whether the repository
      exists or not.
    */
    return_check_src= (*handler_src)->do_check_info(info->get_internal_id());

    /*
      Since this is not a live migration, if we are using file repository
      and there is some error on table repository (for instance, engine
      disabled) we can ignore it instead of stopping replication.
      A warning saying that table is not ready to be used was logged.
    */
    if (ERROR_CHECKING_REPOSITORY == return_check_src &&
        INFO_REPOSITORY_FILE == option &&
        INFO_REPOSITORY_TABLE == (*handler_src)->do_get_rpl_info_type())
    {
      return_check_src= REPOSITORY_DOES_NOT_EXIST;
      /*
        If a already existent thread was used to access info tables,
        current_thd will point to it and we must clear access error on
        it. If a temporary thread was used, then there is nothing to
        clean because the thread was already deleted.
        See Rpl_info_table_access::create_thd().
      */
      if (current_thd)
        current_thd->clear_error();
    }
  }
  else
  {
    /*
      This is a live migration as the repository is already associated to.
      However, we cannot assume that it really exists, for instance, if a
      file was really created.

      This situation may happen when we start a slave for the first time
      but skips its initialization and tries to migrate it.
    */
    return_check_src= (*handler_src)->do_check_info();
  }

  return return_check_src;
}


/**
  This method is called by the decide_repository() and is used print out
  information on errors.

  @param  info         Either master info or relay log info.
  @param  handler_src  Source repository from where information is
                       copied into the destination repository.
  @param  handler_dest Destination repository to where informaiton is
                       copied.
  @param  err_src      Possible error status of the source repo check
  @param  err_dst      Possible error status of the destination repo check
  @param[out] msg      Error message if something goes wrong.

  @retval TRUE  Failure
*/
bool Rpl_info_factory::check_error_repository(Rpl_info *info,
                                              Rpl_info_handler *handler_src,
                                              Rpl_info_handler *handler_dest,
                                              enum_return_check err_src,
                                              enum_return_check err_dst,
                                              const char **msg)
{
  bool error = true;

  /*
    If there is an error in any of the source or destination
    repository checks, the normal operation can't be proceeded.
    The runtime repository won't be initialized.
  */
  if (err_src == ERROR_CHECKING_REPOSITORY)
    sql_print_error("Error in checking %s repository info type of %s.",
                    handler_src->get_description_info(),
                    handler_src->get_rpl_info_type_str());
  if (err_dst == ERROR_CHECKING_REPOSITORY)
    sql_print_error("Error in checking %s repository info type of %s.",
                    handler_dest->get_description_info(),
                    handler_dest->get_rpl_info_type_str());
  *msg= "Error checking repositories";
  return error;
}


/**
  This method is called by the decide_repository() and is used to initialize
  the repositories through a low-level interfacei, which means that if they
  do not exist nothing will be created.

  @param[in]  info         Either master info or relay log info.
  @param[out] handler_src  Source repository from where information is
                           copied into the destination repository.
  @param[out] handler_dest Destination repository to where informaiton is
                           copied.
  @param[out] msg          Error message if something goes wrong.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::init_repositories(Rpl_info *info,
                                         Rpl_info_handler **handler_src,
                                         Rpl_info_handler **handler_dest,
                                         const char **msg)
{
  bool live_migration = info->get_rpl_info_handler() != NULL;

  if (!live_migration)
  {
    if ((*handler_src)->do_init_info(info->get_internal_id()) ||
        (*handler_dest)->do_init_info(info->get_internal_id()))
    {
      *msg= "Error transfering information";
      return true;
    }
  }
  else
  {
    if ((*handler_dest)->do_init_info(info->get_internal_id()))
    {
      *msg= "Error transfering information";
      return true;
    }
  }
  return false;
}


/**
  Creates repositories that will be associated to either the Master_info
  or Relay_log_info.

  @param[in] table_data    Defines information to create a table repository.
  @param[in] file_data     Defines information to create a file repository.
  @param[in] rep_option    Identifies the type of the repository that will
                           be used, i.e., destination repository.
  @param[in] instance      Identifies the instance of the repository that
                           will be used.
  @param[out] handler_src  Source repository from where information is
                           copied into the destination repository.
  @param[out] handler_dest Destination repository to where informaiton is
                           copied.
  @param[out] msg          Error message if something goes wrong.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::init_repositories(const struct_table_data table_data,
                                         const struct_file_data file_data,
                                         uint rep_option,
                                         uint instance,
                                         Rpl_info_handler **handler_src,
                                         Rpl_info_handler **handler_dest,
                                         const char **msg)
{
  bool error= TRUE;
  *msg= "Failed to allocate memory for master info repositories";

  DBUG_ENTER("Rpl_info_factory::init_mi_repositories");

  DBUG_ASSERT(handler_dest != NULL);
  switch (rep_option)
  {
    case INFO_REPOSITORY_FILE:
      if (!(*handler_dest= new Rpl_info_file(file_data.n_fields,
                                             file_data.pattern,
                                             file_data.name,
                                             file_data.name_indexed)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_table(table_data.n_fields,
                                             table_data.schema,
                                             table_data.name)))
        goto err;
    break;

    case INFO_REPOSITORY_TABLE:
      if (!(*handler_dest= new Rpl_info_table(table_data.n_fields,
                                              table_data.schema,
                                              table_data.name)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_file(file_data.n_fields,
                                            file_data.pattern,
                                            file_data.name,
                                            file_data.name_indexed)))
        goto err;
    break;

    case INFO_REPOSITORY_DUMMY:
      if (!(*handler_dest= new Rpl_info_dummy(Master_info::get_number_info_mi_fields())))
        goto err;
    break;

    default:
      DBUG_ASSERT(0);
  }
  error= FALSE;

err:
  DBUG_RETURN(error);
}

bool Rpl_info_factory::scan_repositories(uint* found_instances,
                                         uint* found_rep_option,
                                         const struct_table_data table_data,
                                         const struct_file_data file_data,
                                         const char **msg)
{
  bool error= false;
  uint file_instances= 0;
  uint table_instances= 0;
  DBUG_ASSERT(found_rep_option != NULL);

  DBUG_ENTER("Rpl_info_factory::scan_repositories");

  if (Rpl_info_table::do_count_info(table_data.n_fields, table_data.schema,
                                    table_data.name, &table_instances))
  {
    error= true;
    goto err;
  }

  if (Rpl_info_file::do_count_info(file_data.n_fields, file_data.pattern,
                                   file_data.name_indexed, &file_instances))
  {
    error= true;
    goto err;
  }

  if (file_instances != 0 && table_instances != 0)
  {
    error= true;
    *msg= "Multiple repository instances found with data in "
      "them. Unable to decide which is the correct one to "
      "choose";
    goto err;
  }

  if (table_instances != 0)
  {
    *found_instances= table_instances;
    *found_rep_option= INFO_REPOSITORY_TABLE;
  }
  else if (file_instances != 0)
  {
    *found_instances= file_instances;
    *found_rep_option= INFO_REPOSITORY_FILE;
  }
  else
  {
    *found_instances= 0;
    *found_rep_option= INVALID_INFO_REPOSITORY;
  }

err:
  DBUG_RETURN(error);
}
