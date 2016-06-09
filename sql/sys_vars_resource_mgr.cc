/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "sys_vars_resource_mgr.h"
#include <set_var.h>
#include "mysqld.h"

/**
  Returns the member that contains the given key (address).

  @parma key    [IN]        Key (address) to look for in the list.
  @param length [IN]        Length of the key.

  @return
    Success - Address of the member containing the specified key (address).
    Failure - 0, key not found.
*/

uchar *Session_sysvar_resource_manager::find(void *key, size_t length)
{
  return (my_hash_search(&m_sysvar_string_alloc_hash, (const uchar *) key,
	                 length));
}


/**
  Allocates memory for Sys_var_charptr session variable during session
  initialization.

  @param var     [IN]     The variable.
  @param charset [IN]     Character set information.

  @return
  Success - false
  Failure - true
*/

bool Session_sysvar_resource_manager::init(char **var, const CHARSET_INFO * charset)
{
  if (*var)
  {
    sys_var_ptr *element;
    char *ptr;

    if (!my_hash_inited(&m_sysvar_string_alloc_hash))
      my_hash_init(&m_sysvar_string_alloc_hash,
	           const_cast<CHARSET_INFO *> (charset),
		   4, 0, 0, (my_hash_get_key) sysvars_mgr_get_key,
		   my_free, HASH_UNIQUE,
                   key_memory_THD_Session_sysvar_resource_manager);
    /* Create a new node & add it to the hash. */
    if ( !(element=
           (sys_var_ptr *) my_malloc(key_memory_THD_Session_sysvar_resource_manager,
                                     sizeof(sys_var_ptr), MYF(MY_WME))) ||
         !(ptr=
           (char *) my_memdup(key_memory_THD_Session_sysvar_resource_manager,
                              *var, strlen(*var) + 1, MYF(MY_WME))))
      return true;                            /* Error */
    element->data= (void *) ptr;
    my_hash_insert(&m_sysvar_string_alloc_hash, (uchar *) element);

    /* Update the variable to point to the newly alloced copy. */
    *var= ptr;
  }
  return false;
}


/**
  Frees the old alloced memory, memdup()'s the given val to a new memory
  address & updated the session variable pointer.

  @param var     [IN]     The variable.
  @param val     [IN]     The new value.
  @param val_len [IN]     Length of the new value.

  @return
  Success - false
  Failure - true
*/

bool Session_sysvar_resource_manager::update(char **var, char *val,
                                             size_t val_len)
{
  sys_var_ptr *element= NULL;
  char *ptr= NULL;
  char *old_key= NULL;

  /*
    Memory allocation for the new value of the variable and
    copying the value in it.
  */
  if (val)
  {
    if ( !(ptr=
           (char *) my_memdup(PSI_NOT_INSTRUMENTED,
                              val, val_len + 1, MYF(MY_WME))))
      return true;
    ptr[val_len]= 0;
  }

  /* Get the handle for existing value in hash. */
  if (*var)
  {
    element= (sys_var_ptr *) find(*var, strlen(*var));
    if (element)
      old_key= (char *) element->data;
  }

  /*
    Update the value in hash when both the existing value
    and the new value are not null.
  */
  if (val && *var)
  {
    /* Free the existing one & update the current address. */
    element->data= ptr;
    my_hash_update(&m_sysvar_string_alloc_hash, (uchar *) element,
	           (uchar *)old_key, strlen(old_key));
    if (old_key)
      my_free(old_key);
  }

  /*
    Delete the existing value from the hash when the new value is NULL.
  */
  else if ((val == NULL) && *var)
  {
    if (element)
    {
      my_hash_delete(&m_sysvar_string_alloc_hash, (uchar *)element);
      if (old_key)
	my_free(old_key);
    }
  }

  /*
    Insert the new value into the hash when it is not NULL, but the
    existing value is.
  */
  else if ((*var == NULL) && val)
  {
    /* Create a new node & add it to the list. */
    if( !(element=
          (sys_var_ptr*) my_malloc(key_memory_THD_Session_sysvar_resource_manager,
				   sizeof(sys_var_ptr), MYF(MY_WME))))
      return true;                            /* Error */
    element->data= ptr;
    my_hash_insert(&m_sysvar_string_alloc_hash, (uchar *) element);
  }

  /*
    Update the variable to point to the newly alloced copy.
    
    When current value and the new value are both NULL,
    the control directly reaches here. In that case this
    function effectively does nothing.
  */
  *var= ptr;
  return false;
}

void Session_sysvar_resource_manager::claim_memory_ownership()
{
  /* Release Sys_var_charptr resources here. */
  sys_var_ptr *ptr;
  int i= 0;
  while ((ptr= (sys_var_ptr*)my_hash_element(&m_sysvar_string_alloc_hash, i)))
  {
    if (ptr->data)
      my_claim(ptr->data);
    i++;
  }

  if (m_sysvar_string_alloc_hash.records)
  {
    my_hash_claim(&m_sysvar_string_alloc_hash);
  }
}


/**
  @brief Frees the memory allocated for Sys_var_charptr session variables.
*/

void Session_sysvar_resource_manager::deinit()
{
  /* Release Sys_var_charptr resources here. */
  sys_var_ptr *ptr;
  int i= 0;
  while ((ptr= (sys_var_ptr*)my_hash_element(&m_sysvar_string_alloc_hash, i)))
  {
    if(ptr->data)
      my_free(ptr->data);
    i++;
  }

  if (m_sysvar_string_alloc_hash.records)
  {
    my_hash_free(&m_sysvar_string_alloc_hash);
  }
}

uchar *Session_sysvar_resource_manager::sysvars_mgr_get_key(const char *entry,
							    size_t *length,
							    my_bool not_used MY_ATTRIBUTE((unused)))
{
  char *key;
  key= (char *) ((sys_var_ptr *) entry)->data;
  *length= strlen(key);
  return (uchar *) key;
}
