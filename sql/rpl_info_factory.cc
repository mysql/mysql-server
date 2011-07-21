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
  DBUG_ENTER("Rpl_info_factory::create");

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

  if(init_mi_repositories(mi, mi_option, &handler_src, &handler_dest, &msg))
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

  if(init_rli_repositories(rli, rli_option, &handler_src, &handler_dest, &msg))
    goto err;

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

  DBUG_ENTER("Rpl_info_factory::decide_repository");
 
  bool error= TRUE;
  /*
    check_info() returns FALSE if the repository exists. If a FILE, 
    for example, this means that FALSE is returned if a file exists.
    If a TABLE, for example, this means that FALSE is returned if
    the table exists and is populated. Otherwise, TRUE is returned.

    The check_info() behavior is odd and we are going to fix this
    in the future.

    So,

      . is_src  == TRUE, means that the source repository exists.
      . is_dest == TRUE, means that the destination repository
        exists.

    /Alfranio
  */
  bool is_src= !((*handler_src)->check_info());
  bool is_dest= !((*handler_dest)->check_info());

  DBUG_ASSERT((*handler_dest) != NULL && (*handler_dest) != NULL);
  if (is_src && is_dest)
  {
    *msg= "Multiple replication metadata repository instances "
          "found with data in them. Unable to decide which is "
          "the correct one to choose";
    DBUG_RETURN(error);
  }

  if (!is_dest && is_src)
  {
    if ((*handler_src)->init_info() || (*handler_dest)->init_info())
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
    if (info->copy_info(*handler_src, *handler_dest))
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

  delete (*handler_src);
  *handler_src= NULL;
  info->set_rpl_info_handler(*handler_dest);
  info->set_rpl_info_type(option);
  error= FALSE;

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

  DBUG_ENTER("Rpl_info_factory::change_repository");

  DBUG_ASSERT((*handler_dest) != NULL && (*handler_dest) != NULL);
  if (!(*handler_src)->check_info())
  {
    if ((*handler_dest)->init_info())
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
    if (info->copy_info(*handler_src, *handler_dest))
    {
      *msg= "Error transfering information";
      goto err;
    }
  }
  (*handler_src)->end_info();
  if ((*handler_src)->remove_info())
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
                                             MI_FIELD_ID, MYSQL_SCHEMA_NAME.str, MI_INFO_NAME.str)))
        goto err;
    break;

    case INFO_REPOSITORY_TABLE:
      if (!(*handler_dest= new Rpl_info_table(mi->get_number_info_mi_fields() + 1,
                                              MI_FIELD_ID, MYSQL_SCHEMA_NAME.str, MI_INFO_NAME.str)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_file(mi->get_number_info_mi_fields(),
                                            master_info_file)))
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
                                             RLI_FIELD_ID, MYSQL_SCHEMA_NAME.str, RLI_INFO_NAME.str)))
        goto err;
    break;

    case INFO_REPOSITORY_TABLE:
      if (!(*handler_dest= new Rpl_info_table(rli->get_number_info_rli_fields() + 1,
                                              RLI_FIELD_ID, MYSQL_SCHEMA_NAME.str, RLI_INFO_NAME.str)))
        goto err;
      if (handler_src &&
          !(*handler_src= new Rpl_info_file(rli->get_number_info_rli_fields(),
                                            relay_log_info_file)))
        goto err;
    break;
    default:
      DBUG_ASSERT(0);
  }
  error= FALSE;

err:
  DBUG_RETURN(error);
}
