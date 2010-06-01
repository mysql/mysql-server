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
#include "mysql_priv.h"
#include "rpl_info_factory.h"
#include "rpl_info_file.h"
#include "rpl_info_table.h"
#include "rpl_mi.h"
#include "rpl_rli.h"

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
  bool error= FALSE;

  DBUG_ENTER("Rpl_info_factory::Rpl_info_factory");

  if (!(error= (Rpl_info_factory::create_mi(mi_option, mi) ||
       Rpl_info_factory::create_rli(rli_option, relay_log_recovery, rli))))
  {
    /*
      Setting the cross dependency used all over the code.
    */
    (*mi)->set_relay_log_info(*rli);
    (*rli)->set_master_info(*mi);
  }

  DBUG_RETURN(error); 
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
bool Rpl_info_factory::create_mi(uint mi_option, Master_info **mi)
{
  bool error= TRUE;

  DBUG_ENTER("Rpl_info_factory::Rpl_info_factory");

  Rpl_info_file*  mi_file= NULL;
  Rpl_info_table*  mi_table= NULL;

  *mi= new Master_info();
  if (!(*mi))
  {
    sql_print_error("Failed to allocate memory for the master info "
                    "structure");
    goto err;
  }

  /*
    Now we instantiate all info repos and later decide which one to take,
    but not without first checking if there is already existing data for
    a repo different from the one that is being requested.
  */
  mi_file= new Rpl_info_file((*mi)->get_number_info_mi_fields(),
                             master_info_file);
  if (!mi_file)
  {
    sql_print_error("Failed to allocate memory for the master info "
                    "structure");
    goto err;
  }

  mi_table= new Rpl_info_table((*mi)->get_number_info_mi_fields() + 1,
                               MI_FIELD_ID, MI_SCHEMA, MI_TABLE);
  if (!mi_table)
  {
    sql_print_error("Failed to allocate memory for the master info "
                    "structure");
    goto err;
  }

  DBUG_ASSERT(mi_option == MI_REPOSITORY_FILE ||
              mi_option == MI_REPOSITORY_UNSPEC ||
              mi_option == MI_REPOSITORY_TABLE);
  if (mi_option == MI_REPOSITORY_FILE ||
      mi_option == MI_REPOSITORY_UNSPEC)
  {
    if (!mi_table->check_info())
    {
      sql_print_error("We cannot use a file as repository to store master "
                      "info positions because a table is active.");
      goto err;
    }

    (*mi)->set_rpl_info_handler(mi_file);
    delete mi_table;
  }
  else if (mi_option == MI_REPOSITORY_TABLE)
  {
    if (!mi_file->check_info())
    {
      sql_print_error("We cannot use a table as repository to store master "
                      "info positions because a file is active.");
      goto err;
    }

    (*mi)->set_rpl_info_handler(mi_table);
    delete mi_file;
  }
  error= FALSE;

  DBUG_RETURN(error);
err:
  if (*mi) delete (*mi);
  if (mi_file) delete mi_file;
  if (mi_table) delete mi_table;
  DBUG_RETURN(error);
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
bool Rpl_info_factory::create_rli(uint rli_option, bool is_slave_recovery,
                                  Relay_log_info **rli)
{
  bool error= TRUE;
  Rpl_info_file* rli_file= NULL;
  Rpl_info_table* rli_table= NULL;

  DBUG_ENTER("Rpl_info_factory::create_rli");

  (*rli)= new Relay_log_info(is_slave_recovery);
  if (!(*rli))
  {
    sql_print_error("Failed to allocate memory for the relay log info "
                    "structure");
    goto err;
  }

  /*
    Now we instantiate all info repos and later decide which one to take,
    but not without first checking if there is already existing data for
    a repo different from the one that is being requested.
  */
  rli_file= new Rpl_info_file((*rli)->get_number_info_rli_fields(),
                              relay_log_info_file);
  if (!rli_file)
  {
    sql_print_error("Failed to allocate memory for the relay log info "
                    "structure");
    goto err;
  }

  rli_table= new Rpl_info_table((*rli)->get_number_info_rli_fields() + 1,
                                RLI_FIELD_ID, RLI_SCHEMA, RLI_TABLE);
  if (!rli_table)
  {
    sql_print_error("Failed to allocate memory for the relay log info "
                    "structure");
    goto err;
  }

  DBUG_ASSERT(rli_option == MI_REPOSITORY_FILE ||
              rli_option == MI_REPOSITORY_UNSPEC ||
              rli_option == MI_REPOSITORY_TABLE);
  if (rli_option == RLI_REPOSITORY_FILE ||
      rli_option == RLI_REPOSITORY_UNSPEC)
  {
    if (!rli_table->check_info())
    {
      sql_print_error("We cannot use a file as repository to store relay log "
                      "info positions because a table is active");
      goto err;
    }

    (*rli)->set_rpl_info_handler(rli_file);
    delete rli_table;
  }
  else if (rli_option == RLI_REPOSITORY_TABLE)
  {
    if (!rli_file->check_info())
    {
      sql_print_error("We cannot use a table as repository to store relay log "
                      "info positions because a file is active.");
      goto err;
    }

    (*rli)->set_rpl_info_handler(rli_table);
    delete rli_file;
  }
  error= FALSE;

  DBUG_RETURN(error);
err:
  if (*rli) delete (*rli);
  if (rli_file) delete rli_file;
  DBUG_RETURN(error);
}
