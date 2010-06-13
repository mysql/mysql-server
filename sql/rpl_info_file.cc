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
#include <mysql_priv.h>
#include <my_dir.h>
#include "rpl_info_file.h"

/* These functions are defined in slave.cc */
int init_longvar_from_file(long* var, IO_CACHE* f, long default_val);
int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
                          const char *default_val);
int init_intvar_from_file(int* var, IO_CACHE* f, int default_val);
int init_floatvar_from_file(float* var, IO_CACHE* f, float default_val);
bool init_dynarray_intvar_from_file(char *buffer, char **buffer_act, IO_CACHE* f);

Rpl_info_file::Rpl_info_file(const int nparam, const char* param_info_fname)
  :Rpl_info_handler(nparam), info_fd(-1)
{
  DBUG_ENTER("Rpl_info_file::Rpl_info_file");

  bzero((char*) &info_file, sizeof(info_file));
  fn_format(info_fname, param_info_fname, mysql_data_home, "", 4 + 32);

  DBUG_VOID_RETURN;
}

int Rpl_info_file::do_init_info()
{
  int error= 0;

  /* Don't init if there is no storage */
  DBUG_ENTER("Rpl_info_file::do_init_info");

  /* does info file exist ? */
  if (do_check_info())
  {
    /*
      If someone removed the file from underneath our feet, just close
      the old descriptor and re-create the old file
    */
    if (info_fd >= 0)
      my_close(info_fd, MYF(MY_WME));
    if ((info_fd = my_open(info_fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME))) < 0)
    {
      sql_print_error("Failed to create a new info file (\
file '%s', errno %d)", info_fname, my_errno);
      error= 1;
    }
    else if (init_io_cache(&info_file, info_fd, IO_SIZE*2, READ_CACHE, 0L,0,
                      MYF(MY_WME)))
    {
      sql_print_error("Failed to create a cache on info file (\
file '%s')", info_fname);
      error= 1;
    }
    if (error)
    {
      if (info_fd >= 0)
        my_close(info_fd, MYF(0));
      info_fd= -1;
    }
  }
  /* file exists */
  else
  {
    if (info_fd >= 0)
      reinit_io_cache(&info_file, READ_CACHE, 0L,0,0);
    else
    {
      if ((info_fd = my_open(info_fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0 )
      {
        sql_print_error("Failed to open the existing info file (\
file '%s', errno %d)", info_fname, my_errno);
        error= 1;
      }
      else if (init_io_cache(&info_file, info_fd, IO_SIZE*2, READ_CACHE, 0L,
                        0, MYF(MY_WME)))
      {
        sql_print_error("Failed to create a cache on info file (\
file '%s')", info_fname);
        error= 1;
      }
      if (error)
      {
        if (info_fd >= 0)
          my_close(info_fd, MYF(0));
        info_fd= -1;
      }
    }
  }
  DBUG_RETURN(error);
}

int Rpl_info_file::do_prepare_info_for_read()
{
  cursor= 0;
  prv_error= FALSE;
  return (reinit_io_cache(&info_file, READ_CACHE, 0L,0,0));
}

int Rpl_info_file::do_prepare_info_for_write()
{
  cursor= 0;
  prv_error= FALSE;
  return (reinit_io_cache(&info_file, WRITE_CACHE, 0L, 0, 1));
}

int Rpl_info_file::do_check_info()
{
  return (access(info_fname,F_OK));
}

int Rpl_info_file::do_flush_info(const bool force)
{
  int error= 0;

  DBUG_ENTER("Rpl_info_file::do_flush_info");

  if (flush_io_cache(&info_file))
    error= 1;
  if (!error && (force ||
      (sync_period &&
      ++(sync_counter) >= sync_period)))
  {
    if (my_sync(info_fd, MYF(MY_WME)))
      error= 1;
    sync_counter= 0;
  }

  DBUG_RETURN(error);
}

void Rpl_info_file::do_end_info()
{
  DBUG_ENTER("Rpl_info_file::do_end_info");

  if (info_fd >= 0)
  {
    end_io_cache(&info_file);
    my_close(info_fd, MYF(MY_WME));
    info_fd = -1;
  }

  DBUG_VOID_RETURN;
}

int Rpl_info_file::do_reset_info()
{
  MY_STAT stat_area;
  int error= 0;

  DBUG_ENTER("Rpl_info_file::do_reset_info");

  if (my_stat(info_fname, &stat_area, MYF(0)) && my_delete(info_fname, MYF(MY_WME)))
    error= 1;

  DBUG_RETURN(error);
}

bool Rpl_info_file::do_set_info(const int pos, const char *value)
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;
  
  return (my_b_printf(&info_file, "%s\n", value) > (size_t) 0 ?
          FALSE : TRUE);
}

bool Rpl_info_file::do_set_info(const int pos, const ulong value)
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;
 
  return (my_b_printf(&info_file, "%ld\n", value) > (size_t) 0 ?
          FALSE : TRUE);
}

bool Rpl_info_file::do_set_info(const int pos, const int value)
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;
 
  return (my_b_printf(&info_file, "%d\n", value) > (size_t) 0 ?
          FALSE : TRUE);
}

bool Rpl_info_file::do_set_info(const int pos, const float value)
{
  /* This is enough to handle the float conversion */
  const int array_size= sizeof(value) * 32;
  char buffer[array_size];

  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;

  my_sprintf(buffer, (buffer, "%.3f", value));

  return (my_b_printf(&info_file, "%s\n", buffer) > (size_t) 0 ?
          FALSE : TRUE);
}

bool Rpl_info_file::do_set_info(const int pos, const my_off_t value)
{
  /* This is enough to handle the my_off_t conversion */
  char buffer[22];

  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;

  llstr(value, buffer);

  return (my_b_printf(&info_file, "%s\n", buffer) > (size_t) 0 ?
          FALSE : TRUE);
}

bool Rpl_info_file::do_set_info(const int pos, const Server_ids *value)
{
  bool error= TRUE;
  char *server_ids_buffer= (char*) my_malloc((sizeof(::server_id) * 3 + 1) *
                                   (1 + value->server_ids.elements), MYF(0));

  if (server_ids_buffer == NULL)
    return error;

  if (pos >= ninfo || pos != cursor || prv_error)
    goto err;
    
  /*
    This produces a line listing the total number and all the server_ids.
  */
  if (const_cast<Server_ids *>(value)->pack_server_ids(server_ids_buffer))
    goto err;

  error= (my_b_printf(&info_file, "%s\n", server_ids_buffer) >
          (size_t) 0 ? FALSE : TRUE);

err:
  my_free(server_ids_buffer, MYF(0));
  return error;
}

bool Rpl_info_file::do_get_info(const int pos, char *value, const size_t size,
                                const char *default_value)
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;
      
  return (init_strvar_from_file(value, size, &info_file,
                                default_value));
}

bool Rpl_info_file::do_get_info(const int pos, ulong *value,
                                const ulong default_value)
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;

  return (init_longvar_from_file((long *) value, &info_file, 
                                 (long) default_value));
}

bool Rpl_info_file::do_get_info(const int pos, int *value,
                                const int default_value)
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;

  return (init_intvar_from_file(value, &info_file, 
                                default_value));
}

bool Rpl_info_file::do_get_info(const int pos, float *value,
                                const float default_value)
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;

  return (init_floatvar_from_file(value, &info_file,
                                  default_value));
}

bool Rpl_info_file::do_get_info(const int pos, my_off_t *value,
                                const my_off_t default_value)
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;

  return (init_intvar_from_file((int *)value, &info_file,
                                (int) default_value));
}

bool Rpl_info_file::do_get_info(const int pos, Server_ids *value,
                                const Server_ids *default_value __attribute__((unused)))
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;
  /*
    Static buffer to use most of times. However, if it is not big
    enough to accommodate the server ids, a new buffer is allocated.
  */
  const int array_size= 16 * (sizeof(long)*4 + 1);
  char buffer[array_size];
  char *buffer_act= buffer;

  bool error= init_dynarray_intvar_from_file(buffer, &buffer_act,
                                                       &info_file);
  if (!error)
    value->unpack_server_ids(buffer_act);

  if (buffer != buffer_act)
  {
    /*
      Release the buffer allocated while reading the server ids
      from the file.
    */
    my_free(buffer_act, MYF(0));
  }

  return error;
}
