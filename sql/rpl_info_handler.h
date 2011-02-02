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

#ifndef RPL_INFO_HANDLER_H
#define RPL_INFO_HANDLER_H

#include <my_global.h>
#include <server_ids.h>
#include "rpl_info_values.h"

class Rpl_info_handler
{
public:
  Rpl_info_handler(const int nparam);
  virtual ~Rpl_info_handler();

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
    Checks if any necessary dependency is satisfied such as a
    file exists.

    @retval FALSE success,
    @retval TRUE  otherwise error.
  */
  int check_info()
  {
    return do_check_info();
  }

  /**
    Flushes and syncs in-memory information into a stable storage (i.e.
    repository). Usually, syncing after flushing depends on other options
    such as @code relay-log-info-sync, master-info-sync. These options
    dictate after how many events or transactions the information
    should be synced. We can ignore them and always sync by setting the
    parameter @code force, which is by default false, to @code true.

    So if the number of events is below a threshold, the parameter
    @code force is FALSE and we are using a file system as a storage
    system, it may happen that the changes will only end up in the
    operating system's cache and a crash may lead to inconsistencies.

    @param[in] force Always sync the information.

    @retval FALSE No error
    @retval TRUE  Failure
  */
  int flush_info(const bool force)
  {
    return do_flush_info(force);
  }

  /**
    Deletes any information in the repository.

    @retval FALSE No error
    @retval TRUE  Failure
  */
  int remove_info()
  {
    return do_remove_info();
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
  bool get_info(char *value, const size_t size,
                const char *default_value)
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

  /*                                                                                                                                    
    Pre-store information before writing it to the repository and if
    necessary after reading it from the repository. The decision is
    delegated to the sub-classes.
  */
  Rpl_info_values *field_values;

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

private:
  virtual int do_init_info()= 0;
  virtual int do_check_info()= 0;
  virtual int do_flush_info(const bool force)= 0;
  virtual int do_remove_info()= 0;
  virtual void do_end_info()= 0;
  virtual int do_prepare_info_for_read()= 0;
  virtual int do_prepare_info_for_write()= 0;

  virtual bool do_set_info(const int pos, const char *value)= 0;
  virtual bool do_set_info(const int pos, const ulong value)= 0;
  virtual bool do_set_info(const int pos, const int value)= 0;
  virtual bool do_set_info(const int pos, const float value)= 0;
  virtual bool do_set_info(const int pos, const Server_ids *value)= 0;
  virtual bool do_get_info(const int pos, char *value, const size_t size,
                           const char *default_value)= 0;
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

  Rpl_info_handler& operator=(const Rpl_info_handler& handler);
  Rpl_info_handler(const Rpl_info_handler& handler);
};
#endif /* RPL_INFO_HANDLER_H */
