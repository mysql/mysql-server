/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

#define NUMBER_OF_FIELDS_TO_IDENTIFY_COORDINATOR 1
#define NUMBER_OF_FIELDS_TO_IDENTIFY_WORKER 2

/**
  Creates both a Master info and a Relay log info repository whose types are
  defined as parameters. Nothing is done for Workers here.

  @todo Make the repository a pluggable component.
  
  @param[in]  mi_option  Type of the Master info repository
  @param[out] mi         Reference to the Master info repository
  @param[in]  rli_option Type of the Relay log info repository
  @param[out] rli        Reference to the Relay log info repository

  @retval FALSE No error
  @retval TRUE  Failure
*/ 
bool Rpl_info_factory::create_coordinators(uint mi_option, Master_info **mi,
                                           uint rli_option, Relay_log_info **rli)
{
  DBUG_ENTER("Rpl_info_factory::create_coordinators");

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
  Rpl_info_handler*  handler_src= NULL;
  Rpl_info_handler*  handler_dest= NULL;
  ulong *key_info_idx= NULL;
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
                            &key_master_info_sleep_cond
#endif
                           )))
    goto err;

  if (!(key_info_idx= new ulong[NUMBER_OF_FIELDS_TO_IDENTIFY_COORDINATOR]))
     goto err;

  key_info_idx[0]= server_id;
  mi->set_idx_info(key_info_idx, NUMBER_OF_FIELDS_TO_IDENTIFY_COORDINATOR);

  if(init_mi_repositories(mi, mi_option, &handler_src, &handler_dest, &msg))
    goto err;

  if (decide_repository(mi, mi_option, &handler_src, &handler_dest, &msg))
    goto err;

  DBUG_RETURN(mi);

err:
  delete handler_src;
  delete handler_dest;
  delete []key_info_idx;
  if (mi)
  {
    /*
      The handler was previously deleted so we need to remove
      any reference to it.  
    */
    mi->set_idx_info(NULL, 0);
    mi->set_rpl_info_handler(NULL);
    mi->set_rpl_info_type(INVALID_INFO_REPOSITORY);
    delete mi;
  }
  sql_print_error("Error creating master info: %s.", msg);
  DBUG_RETURN(NULL);
}

/**
  Allows to change the master info repository after startup.

  @param[in]  mi        Pointer to Master_info.
  @param[in]  mi_option Type of the repository, e.g. FILE TABLE.
  @param[out] msg       Error message if something goes wrong.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::change_mi_repository(Master_info *mi,
                                            const uint mi_option,
                                            const char **msg)
{
  DBUG_ENTER("Rpl_info_factory::change_mi_repository");

  Rpl_info_handler*  handler_src= mi->get_rpl_info_handler();
  Rpl_info_handler*  handler_dest= NULL;

  if (mi->get_rpl_info_type()  == mi_option)
    DBUG_RETURN(FALSE);

  if (init_mi_repositories(mi, mi_option, NULL, &handler_dest, msg))
    goto err;

  if (change_repository(mi, mi_option, &handler_src, &handler_dest, msg))
    goto err;

  DBUG_RETURN(FALSE);

err:
  delete handler_dest;

  sql_print_error("Error changing the type of master info's repository: %s.", *msg);
  DBUG_RETURN(TRUE);
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
  Rpl_info_handler* handler_src= NULL;
  Rpl_info_handler* handler_dest= NULL;
  ulong *key_info_idx= NULL;
  const char *msg= "Failed to allocate memory for the relay log info "
                   "structure";

  DBUG_ENTER("Rpl_info_factory::create_rli");

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
                               )))
    goto err;

  if (!(key_info_idx= new ulong[NUMBER_OF_FIELDS_TO_IDENTIFY_COORDINATOR]))
     goto err;
  key_info_idx[0]= server_id;
  rli->set_idx_info(key_info_idx, NUMBER_OF_FIELDS_TO_IDENTIFY_COORDINATOR);

  if(init_rli_repositories(rli, rli_option, &handler_src, &handler_dest, &msg))
    goto err;

  if (decide_repository(rli, rli_option, &handler_src, &handler_dest, &msg))
    goto err;

  DBUG_RETURN(rli);

err:
  delete handler_src;
  delete handler_dest;
  delete []key_info_idx;
  if (rli) 
  {
    /*
      The handler was previously deleted so we need to remove
      any reference to it.  
    */
    rli->set_idx_info(NULL, 0);
    rli->set_rpl_info_handler(NULL);
    rli->set_rpl_info_type(INVALID_INFO_REPOSITORY);
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
                                             const uint rli_option,
                                             const char **msg)
{
  DBUG_ENTER("Rpl_info_factory::change_rli_repository");

  Rpl_info_handler*  handler_src= rli->get_rpl_info_handler();
  Rpl_info_handler*  handler_dest= NULL;

  if (rli->get_rpl_info_type()  == rli_option)
    DBUG_RETURN(FALSE);

  if (init_rli_repositories(rli, rli_option, NULL, &handler_dest, msg))
    goto err;

  if (change_repository(rli, rli_option, &handler_src, &handler_dest, msg))
    goto err;

  DBUG_RETURN(FALSE);

err:
  delete handler_dest;
  handler_dest= NULL;

  sql_print_error("Error changing the type of relay log info's repository: %s.", *msg);
  DBUG_RETURN(TRUE);
}

Slave_worker *Rpl_info_factory::create_worker(uint worker_option, uint worker_id,
                                              Relay_log_info *rli)
{
  char info_fname[FN_REFLEN];
  char info_name[FN_REFLEN];
  Rpl_info_handler* handler_src= NULL;
  Rpl_info_handler* handler_dest= NULL;
  ulong *key_info_idx= NULL;
  Slave_worker* worker= NULL;
  const char *msg= "Failed to allocate memory for the worker info "
                   "structure";

  DBUG_ENTER("Rpl_info_factory::create_worker");

  /*
    Defining the name of the worker and its repository.
  */
  char *pos= strmov(info_fname, relay_log_info_file);
  sprintf(pos, ".%u", worker_id);
  pos= strmov(info_name, "worker");
  sprintf(pos, ".%u", worker_id);

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
                                )))
    goto err;

  if (!(key_info_idx= new ulong[NUMBER_OF_FIELDS_TO_IDENTIFY_WORKER]))
     goto err;
  key_info_idx[0]= server_id;
  key_info_idx[1]= worker_id;
  worker->set_idx_info(key_info_idx, NUMBER_OF_FIELDS_TO_IDENTIFY_WORKER);
  worker->id= worker_id;

  if(init_worker_repositories(worker, worker_option, info_fname, &handler_src,
                              &handler_dest, &msg))
    goto err;

  if (decide_repository(worker, worker_option, &handler_src, &handler_dest,
                        &msg))
    goto err;

  DBUG_RETURN(worker);

err:
  delete handler_src;
  delete handler_dest;
  delete []key_info_idx;
  if (worker)
  {
    /*
      The handler was previously deleted so we need to remove
      any reference to it.  
    */
    worker->set_idx_info(NULL, 0);
    worker->set_rpl_info_handler(NULL);
    worker->set_rpl_info_type(INVALID_INFO_REPOSITORY);
    delete worker;
  }
  sql_print_error("Error creating relay log info: %s.", msg);
  DBUG_RETURN(NULL);
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
bool Rpl_info_factory::decide_repository(Rpl_info *info,
                                         uint option,
                                         Rpl_info_handler **handler_src,
                                         Rpl_info_handler **handler_dest,
                                         const char **msg)
{
  bool error= TRUE;
  enum_return_check return_check_src= ERROR_CHECKING_REPOSITORY;
  enum_return_check return_check_dst= ERROR_CHECKING_REPOSITORY;
  DBUG_ENTER("Rpl_info_factory::decide_repository");

  if (option == INFO_REPOSITORY_DUMMY)
    DBUG_RETURN(FALSE);

  DBUG_ASSERT((*handler_src) != NULL && (*handler_dest) != NULL &&
              (*handler_src) != (*handler_dest));

  return_check_src= (*handler_src)->check_info(info->uidx, info->nidx);
  return_check_dst= (*handler_dest)->check_info(info->uidx, info->nidx);
  if (return_check_src == ERROR_CHECKING_REPOSITORY ||
      return_check_dst == ERROR_CHECKING_REPOSITORY)
  {
    /*
      If there is an error, we cannot proceed with the normal operation.
      In this case, we just pick the dest repository if check_info() has
      not failed to execute against it in order to give users the chance
      to fix the problem and restart the server. One particular case can
      happen when there is an inplace upgrade: no source table (it did 
      not exist in 5.5) and the default destination is a file.

      Notice that migration will not take place and the destination may
      be empty.
    */
    if (opt_skip_slave_start && return_check_dst != ERROR_CHECKING_REPOSITORY)
    {
      sql_print_warning("Error while checking replication metadata. "
                        "Setting the requested repository in order to "
                        "give users the chance to fix the problem and "
                        "restart the server. If this is a live upgrade "
                        "please consider using mysql_upgrade to fix the "
                        "problem.");
      delete (*handler_src);
      *handler_src= NULL;
      info->set_rpl_info_handler(*handler_dest);
      info->set_rpl_info_type(option);
      error= FALSE;
      goto err;
    }
    else
    {
      *msg= "Error while checking replication metadata. This might also happen "
            "when doing a live upgrade from a version that did not make use "
            "of the replication metadata tables. If that was the case, consider "
            "starting the server with the option --skip-slave-start which "
            "causes the server to bypass the replication metadata tables check "
            "while it is starting up";
      goto err;
    }
  }
  else
  {
    /*
      If there is no error, we can proceed with the normal operation and
      check if we need to do some migration between repositories.
      However, if both repositories are set an error will be printed out.
    */
    if (return_check_src == REPOSITORY_EXISTS &&
        return_check_dst == REPOSITORY_EXISTS)
    {
      *msg= "Multiple replication metadata repository instances "
            "found with data in them. Unable to decide which is "
            "the correct one to choose";
      DBUG_RETURN(error);
    }

    if (return_check_src == REPOSITORY_EXISTS &&
        return_check_dst == REPOSITORY_DOES_NOT_EXIST)
    {
      if ((*handler_src)->init_info(info->uidx, info->nidx) ||
          (*handler_dest)->init_info(info->uidx, info->nidx))
      {
        *msg= "Error transfering information";
        goto err;
      }
      /*
        Transfer the information from source to destination and delete the
        source. Note this is not fault-tolerant and a crash before removing
        source may cause the next restart to fail as is_src and is_dest may
        be true. Moreover, any failure in removing the source may lead to
        the same.

        /Alfranio
      */
      if (info->copy_info(*handler_src, *handler_dest) ||
          (*handler_dest)->flush_info(info->uidx, info->nidx, TRUE))
      {
        *msg= "Error transfering information";
        goto err;
      }
      (*handler_src)->end_info(info->uidx, info->nidx);
      if ((*handler_src)->remove_info(info->uidx, info->nidx))
      {
        *msg= "Error removing old repository";
        goto err;
      }
    }

    delete (*handler_src);
    *handler_src= NULL;
    info->set_rpl_info_handler(*handler_dest);
    info->set_rpl_info_type(option);
    error= FALSE;
  }

err:
  DBUG_RETURN(error); 
}

/**
  Changes the type of the repository after startup based on the following
  decision table:

  \code
  |--------------+-----------------------+-----------------------|
  | Exists \ Opt |         SOURCE        |      DESTINATION      |
  |--------------+-----------------------+-----------------------|
  | ~is_s, ~is_d |            -          | Create/Update D       |
  | ~is_s,  is_d |            -          | Continue with D       |
  |  is_s, ~is_d | Copy S into D         | Create/Update D       |
  |  is_s,  is_d | Copy S into D         | Continue with D       |
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
bool Rpl_info_factory::change_repository(Rpl_info *info,
                                         uint option,
                                         Rpl_info_handler **handler_src,
                                         Rpl_info_handler **handler_dest,
                                         const char **msg)
{
  bool error= TRUE;
  enum_return_check return_check_src= ERROR_CHECKING_REPOSITORY;
  DBUG_ENTER("Rpl_info_factory::change_repository");
  DBUG_ASSERT((*handler_src) != NULL && (*handler_dest) != NULL &&
              (*handler_src) != (*handler_dest));

  return_check_src= (*handler_src)->check_info(info->uidx, info->nidx);
  if (return_check_src == REPOSITORY_EXISTS)
  {
    if ((*handler_dest)->init_info(info->uidx, info->nidx))
    {
      *msg= "Error initializing new repository";
      goto err;
    }

    /*
      Transfer the information from source to destination and delete the
      source. Note this is not fault-tolerant and a crash before removing
      source may cause the next restart to fail as is_src and is_dest may
      be true. Moreover, any failure in removing the source may lead to
      the same.

      /Alfranio
    */
    if (info->copy_info(*handler_src, *handler_dest) ||
        (*handler_dest)->flush_info(info->uidx, info->nidx, true))
    {
      *msg= "Error transfering information";
      goto err;
    }
  }

  if (return_check_src == ERROR_CHECKING_REPOSITORY)
  {
    *msg= "Error checking old repository";
    goto err;
  }

  (*handler_src)->end_info(info->uidx, info->nidx);
  if ((*handler_src)->remove_info(info->uidx, info->nidx))
  {
    *msg= "Error removing old repository";
    goto err;
  }

  info->set_rpl_info_handler(NULL);
  delete (*handler_src);
  *handler_src= NULL;
  info->set_rpl_info_handler(*handler_dest);
  info->set_rpl_info_type(option);
  error= FALSE;

err:
  DBUG_RETURN(error);
}

/**
  Creates repositories that will be associated to the master info.

  @param[in] mi            Pointer to the class Master info.
  @param[in] rli_option    Identifies the type of the repository that will
                           be used, i.e., destination repository.
  @param[out] handler_src  Source repository from where information is
                           copied into the destination repository.
  @param[out] handler_dest Destination repository to where informaiton is
                           copied.
  @param[out] msg          Error message if something goes wrong.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::init_mi_repositories(Master_info *mi,
                                            uint mi_option,
                                            Rpl_info_handler **handler_src,
                                            Rpl_info_handler **handler_dest,
                                            const char **msg)
{
  bool error= TRUE;
  *msg= "Failed to allocate memory for master info repositories";

  DBUG_ENTER("Rpl_info_factory::init_mi_repositories");

  DBUG_ASSERT(handler_dest != NULL);
  switch (mi_option)
  {
    case INFO_REPOSITORY_FILE:
      if (!(*handler_dest= new Rpl_info_file(mi->get_number_info_mi_fields(),
                                             master_info_file)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_table(mi->get_number_info_mi_fields() + 1,
                                             MYSQL_SCHEMA_NAME.str, MI_INFO_NAME.str)))
        goto err;
    break;

    case INFO_REPOSITORY_TABLE:
      if (!(*handler_dest= new Rpl_info_table(mi->get_number_info_mi_fields() + 1,
                                              MYSQL_SCHEMA_NAME.str, MI_INFO_NAME.str)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_file(mi->get_number_info_mi_fields(),
                                            master_info_file)))
        goto err;
    break;

    case INFO_REPOSITORY_DUMMY:
      if (!(*handler_dest= new Rpl_info_dummy(mi->get_number_info_mi_fields())))
        goto err;
    break;

    default:
      DBUG_ASSERT(0);
  }
  error= FALSE;

err:
  DBUG_RETURN(error);
}

/**
  Creates repositories that will be associated to the relay log info.

  @param[in] rli           Pointer to the class Relay_log_info.
  @param[in] rli_option    Identifies the type of the repository that will
                           be used, i.e., destination repository.
  @param[out] handler_src  Source repository from where information is
                           copied into the destination repository.
  @param[out] handler_dest Destination repository to where informaiton is
                           copied.
  @param[out] msg          Error message if something goes wrong.

  @retval FALSE No error
  @retval TRUE  Failure
*/
bool Rpl_info_factory::init_rli_repositories(Relay_log_info *rli,
                                             uint rli_option,
                                             Rpl_info_handler **handler_src,
                                             Rpl_info_handler **handler_dest,
                                             const char **msg)
{
  bool error= TRUE;
  *msg= "Failed to allocate memory for relay log info repositories";

  DBUG_ENTER("Rpl_info_factory::init_rli_repositories");

  DBUG_ASSERT(handler_dest != NULL);
  switch (rli_option)
  {
    case INFO_REPOSITORY_FILE:
      if (!(*handler_dest= new Rpl_info_file(rli->get_number_info_rli_fields(),
                                             relay_log_info_file)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_table(rli->get_number_info_rli_fields() + 1,
                                             MYSQL_SCHEMA_NAME.str, RLI_INFO_NAME.str)))
        goto err;
    break;

    case INFO_REPOSITORY_TABLE:
      if (!(*handler_dest= new Rpl_info_table(rli->get_number_info_rli_fields() + 1,
                                              MYSQL_SCHEMA_NAME.str, RLI_INFO_NAME.str)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_file(rli->get_number_info_rli_fields(),
                                            relay_log_info_file)))
        goto err;
    break;

    case INFO_REPOSITORY_DUMMY:
      if (!(*handler_dest= new Rpl_info_dummy(rli->get_number_info_rli_fields())))
        goto err;
    break;

    default:
      DBUG_ASSERT(0);
  }
  error= FALSE;

err:
  DBUG_RETURN(error);
}

bool Rpl_info_factory::init_worker_repositories(Slave_worker *worker,
                                                uint worker_option,
                                                const char* info_fname,
                                                Rpl_info_handler **handler_src,
                                                Rpl_info_handler **handler_dest,
                                                const char **msg)
{
  bool error= TRUE;
  *msg= "Failed to allocate memory for worker info repositories";

  DBUG_ENTER("Rpl_info_factory::init_worker_repositories");

  DBUG_ASSERT(handler_dest != NULL);
  switch (worker_option)
  {
    case INFO_REPOSITORY_FILE:
      if (!(*handler_dest= new Rpl_info_file(worker->get_number_worker_fields(),
                                             info_fname)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_table(worker->get_number_worker_fields() + 2,
                                             MYSQL_SCHEMA_NAME.str, WORKER_INFO_NAME.str)))
        goto err;
    break;

    case INFO_REPOSITORY_TABLE:
      if (!(*handler_dest= new Rpl_info_table(worker->get_number_worker_fields() + 2,
                                              MYSQL_SCHEMA_NAME.str, WORKER_INFO_NAME.str)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_file(worker->get_number_info_rli_fields(),
                                            info_fname)))
        goto err;
    break;

    case INFO_REPOSITORY_DUMMY:
      if (!(*handler_dest= new Rpl_info_dummy(worker->get_number_worker_fields())))
        goto err;
    break;

    default:
      DBUG_ASSERT(0);
  }
  error= FALSE;

err:
  DBUG_RETURN(error);
}
