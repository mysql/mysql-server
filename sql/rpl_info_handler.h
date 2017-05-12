/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_INFO_HANDLER_H
#define RPL_INFO_HANDLER_H

#include <stddef.h>
#include <sys/types.h>

#include "my_inttypes.h"

class Rpl_info_values;
class Server_ids;


enum enum_info_repository
{
  INFO_REPOSITORY_FILE= 0,
  INFO_REPOSITORY_TABLE,
  INFO_REPOSITORY_DUMMY,
  /*
    Add new types of repository before this
    entry.
  */
  INVALID_INFO_REPOSITORY
};

/*
  Defines status on the repository.
*/
enum enum_return_check { REPOSITORY_DOES_NOT_EXIST= 1, REPOSITORY_EXISTS, ERROR_CHECKING_REPOSITORY };

class Rpl_info_handler
{
  friend class Rpl_info_factory;

public:
  /**
    After creating an object and assembling components, this method is
    used to initialize internal structures. Everything that does not
    depend on other components (e.g. mutexes) should be placed in the
    object's constructor though.

    @retval FALSE success,
    @retval TRUE  otherwise error.
  */
  int init_info()
  {
    return do_init_info();
  }

  /**
    Checks the repository's status.
    
    @retval REPOSITORY_EXISTS         reposistory is ready to
                                      be used.
    @retval REPOSITORY_DOES_NOT_EXIST repository needs to be 
                                      configured.
    @retval ERROR_CHECKING_REPOSITORY error while checking the
                                      reposistory.
  */
  enum_return_check check_info()
  {
    return do_check_info();
  }

  /**
    Flushes and syncs in-memory information into a stable storage (i.e.
    repository). Usually, syncing after flushing depends on other options
    such as @c relay-log-info-sync, @c master-info-sync. These options
    dictate after how many events or transactions the information
    should be synced. We can ignore them and always sync by setting the
    parameter @c force, which is by default @c false, to @c true.

    So if the number of events is below a threshold, the parameter
    @c force is FALSE and we are using a file system as a storage
    system, it may happen that the changes will only end up in the
    operating system's cache and a crash may lead to inconsistencies.

    @retval FALSE No error
    @retval TRUE  Failure
  */
  int flush_info(const bool force)
  {
    return do_flush_info(force);
  }

  /**
    Deletes any information in it and in some cases the repository.
    The decision to remove the repository is delegated to the
    developer.

    @retval FALSE No error
    @retval TRUE  Failure
  */
  int remove_info()
  {
    return do_remove_info();
  }

  /**
    Deletes any information in the repository. In contrast to the
    @c remove_info() method, the repository is not removed.

    @retval FALSE No error
    @retval TRUE  Failure
  */
  int clean_info()
  {
    return do_clean_info();
  }

  /**
    Closes access to the repository.

    @retval FALSE No error
    @retval TRUE  Failure
  */
  void end_info()
  {
    do_end_info();
  }

  /**
    Enables the storage system to receive reads, i.e.
    getters.
 
    @retval FALSE No error
    @retval TRUE  Failure
  */
  int prepare_info_for_read()
  {
    return (do_prepare_info_for_read());
  }

  /**
    Enables the storage system to receive writes, i.e.
    setters.
 
    @retval FALSE No error
    @retval TRUE  Failure
  */
  int prepare_info_for_write()
  {
    return (do_prepare_info_for_write());
  }

  /**
    Gets the type of the repository that is used.

    @return Type of repository.
  */
  uint get_rpl_info_type()
  {
     return (do_get_rpl_info_type());
  }
  /**
     Returns a string corresponding to the type.
  */
  const char* get_rpl_info_type_str();

  /**
    Sets the value of a field to @c value.
    Any call must be done in the right order which
    is defined by the caller that wants to persist
    the information.

    @param[in] value Value to be set.

    @retval FALSE No error
    @retval TRUE Failure
  */
  template <class TypeHandler>
  bool set_info(TypeHandler const value)
  {
    if (cursor >= ninfo || prv_error)
      return TRUE;

    if (!(prv_error= do_set_info(cursor, value)))
      cursor++;

    return(prv_error);
  }

  template <class TypeHandler>
  bool set_info(TypeHandler const value, const size_t size)
  {
    if (cursor >= ninfo || prv_error)
      return TRUE;

    if (!(prv_error= do_set_info(cursor, value, size)))
      cursor++;

    return(prv_error);
  }

  /**
    set the value of a field pointed at @c pk_cursor to
    @ value.

    @param[in]   pk_cursor   cursor for the filed value.
    @param[in]   value       fieled[pk_cursor] would be set
                             this value.

    @retval      FALSE       ok
    @retval      TRUE       error.
  */

  template <class TypeHandler>
  bool set_info(int pk_cursor, TypeHandler const value)
  {
    if (pk_cursor >= ninfo)
      return TRUE;

    return (do_set_info(pk_cursor, value));
  }

  /**
    Returns the value of a field.
    Any call must be done in the right order which
    is defined by the caller that wants to return
    the information.

    @param[in] value Value to be set.
    @param[in] default_value Returns a default value
                             if the field is empty.

    @retval FALSE No error
    @retval TRUE Failure
  */
  template <class TypeHandlerPointer, class TypeHandler>
  bool get_info(TypeHandlerPointer value,
                TypeHandler const default_value)
  {
    if (cursor >= ninfo || prv_error)
      return TRUE;

    if (!(prv_error= do_get_info(cursor, value, default_value)))
      cursor++;

    return(prv_error);
  }

  /**
    Returns the value of a string field.
    Any call must be done in the right order which
    is defined by the caller that wants to return
    the information.

    @param[in] value Value to be returned.
    @param[in] size  Max size of the string to be
                     returned.
    @param[in] default_value Returns a default value
                             if the field is empty.

    @retval FALSE No error
    @retval TRUE Failure
  */
  template <class TypeHandler>
  bool get_info(TypeHandler value, const size_t size,
                TypeHandler const default_value)
  {
    if (cursor >= ninfo || prv_error)
      return TRUE;

    if (!(prv_error= do_get_info(cursor, value, size, default_value)))
      cursor++;

    return(prv_error);
  }
 
  /**
    Returns the value of a Server_id field.
    Any call must be done in the right order which
    is defined by the caller that wants to return
    the information.

    @param[out] value Value to be return.
    @param[in] default_value Returns a default value
                             if the field is empty.

    @retval FALSE No error
    @retval TRUE Failure
  */
  bool get_info(Server_ids *value,
                const Server_ids *default_value)
  {
    if (cursor >= ninfo || prv_error)
      return TRUE;

    if (!(prv_error= do_get_info(cursor, value, default_value)))
      cursor++;

    return(prv_error);
  }

  /**
    Returns the number of fields handled by this handler.

    @return Number of fields handled by the handler.
  */
  int get_number_info() { return ninfo; }

  /**
    Configures the number of events after which the info (e.g.
    master info, relay log info) must be synced when flush() is
    called.
 
    @param[in] period Number of events.
  */
  void set_sync_period(uint period);

  /**
    Returns a string describing the repository. For instance, if the
    repository is a file, the returned string is path where data is
    stored.

    @return a pointer to a string.
  */
  char *get_description_info()
  {
    return (do_get_description_info());
  }

  /**
    Any transactional repository may have its updates rolled back in case
    of a failure. If this is possible, the repository is classified as
    transactional.

    @retval TRUE If transactional.
    @retval FALSE Otherwise.
  */
  bool is_transactional() { return do_is_transactional(); }

  /**
    Updates the value returned by the member function is_transactional()
    because it may be expensive to compute it whenever is_transactional()
    is called.

    In the current implementation, the type of the repository can only be
    changed when replication, i.e. slave, is stopped. For that reason,
    this member function, i.e. update_is__transactional(), must be called
    when slave is starting.

    @retval FALSE No error
    @retval TRUE Failure
  */
  bool update_is_transactional() { return do_update_is_transactional(); }

  /*                                                                                                                                    
    Pre-store information before writing it to the repository and if
    necessary after reading it from the repository. The decision is
    delegated to the sub-classes.
  */
  Rpl_info_values *field_values;

  virtual ~Rpl_info_handler();

protected:
  /* Number of fields to be stored in the repository. */
  int ninfo;

  /* From/To where we should start reading/writing. */
  int cursor;

  /* Registers if there was failure while accessing a field/information. */
  bool prv_error;

  /*
   Keeps track of the number of events before fsyncing. The option
   --sync-master-info and --sync-relay-log-info determine how many
   events should be processed before fsyncing.
  */
  uint sync_counter;

  /*
   The number of events after which we should fsync.
  */
  uint sync_period;

  Rpl_info_handler(const int nparam);

private:
  virtual int do_init_info()= 0;
  virtual int do_init_info(uint instance)= 0;
  virtual enum_return_check do_check_info()= 0;
  virtual enum_return_check do_check_info(uint instance)= 0;
  virtual int do_flush_info(const bool force)= 0;
  virtual int do_remove_info()= 0;
  virtual int do_clean_info()= 0;
  virtual void do_end_info()= 0;
  virtual int do_prepare_info_for_read()= 0;
  virtual int do_prepare_info_for_write()= 0;

  virtual bool do_set_info(const int pos, const char *value)= 0;
  virtual bool do_set_info(const int pos, const uchar *value,
                           const size_t size)= 0;
  virtual bool do_set_info(const int pos, const ulong value)= 0;
  virtual bool do_set_info(const int pos, const int value)= 0;
  virtual bool do_set_info(const int pos, const float value)= 0;
  virtual bool do_set_info(const int pos, const Server_ids *value)= 0;
  virtual bool do_get_info(const int pos, char *value,
                           const size_t size,
                           const char *default_value)= 0;
  virtual bool do_get_info(const int pos, uchar *value,
                           const size_t size,
                           const uchar *default_value)= 0;
  virtual bool do_get_info(const int pos, ulong *value,
                           const ulong default_value)= 0;
  virtual bool do_get_info(const int pos, int *value,
                           const int default_value)= 0;
  virtual bool do_get_info(const int pos, float *value,
                           const float default_value)= 0;
  virtual bool do_get_info(const int pos, Server_ids *value,
                           const Server_ids *default_value)= 0;
  virtual char* do_get_description_info()= 0;
  virtual bool do_is_transactional()= 0;
  virtual bool do_update_is_transactional()= 0;
  virtual uint do_get_rpl_info_type()= 0;

  Rpl_info_handler(const Rpl_info_handler& handler);

  Rpl_info_handler& operator=(const Rpl_info_handler& handler);
};
#ifndef DBUG_OFF
 extern ulong w_rr;
 extern uint mts_debug_concurrent_access;
#endif
#endif /* RPL_INFO_HANDLER_H */
