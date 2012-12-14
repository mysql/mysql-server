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
#include <sql_priv.h>
#include <my_dir.h>
#include "rpl_info_file.h"
#include "mysqld.h"
#include "log.h"

int init_ulongvar_from_file(ulong* var, IO_CACHE* f, ulong default_val);
int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
                          const char *default_val);
int init_intvar_from_file(int* var, IO_CACHE* f, int default_val);
int init_floatvar_from_file(float* var, IO_CACHE* f, float default_val);
bool init_dynarray_intvar_from_file(char *buffer, size_t size, 
                                    char **buffer_act, IO_CACHE* f);

Rpl_info_file::Rpl_info_file(const int nparam,
                             const char* param_pattern_fname,
                             const char* param_info_fname,
                             bool indexed_arg)
  :Rpl_info_handler(nparam), info_fd(-1), name_indexed(indexed_arg)
{
  DBUG_ENTER("Rpl_info_file::Rpl_info_file");

  memset(&info_file, 0, sizeof(info_file));
  fn_format(pattern_fname, param_pattern_fname, mysql_data_home, "", 4 + 32);
  fn_format(info_fname, param_info_fname, mysql_data_home, "", 4 + 32);

  DBUG_VOID_RETURN;
}

int Rpl_info_file::do_init_info(uint instance)
{
  DBUG_ENTER("Rpl_info_file::do_init_info(uint)");

  char fname_local[FN_REFLEN];
  char *pos= strmov(fname_local, pattern_fname);
  if (name_indexed)
    sprintf(pos, "%u", instance);

  fn_format(info_fname, fname_local, mysql_data_home, "", 4 + 32);
  DBUG_RETURN(do_init_info());
}

int Rpl_info_file::do_init_info()
{
  int error= 0;
  DBUG_ENTER("Rpl_info_file::do_init_info");

  /* does info file exist ? */
  enum_return_check ret_check= do_check_info();
  if (ret_check == REPOSITORY_DOES_NOT_EXIST)
  {
    /*
      If someone removed the file from underneath our feet, just close
      the old descriptor and re-create the old file
    */
    if (info_fd >= 0)
    {
      if (my_b_inited(&info_file))
        end_io_cache(&info_file);
      my_close(info_fd, MYF(MY_WME));
    }
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
  else if (ret_check == REPOSITORY_EXISTS)
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
  else
    error= 1;
  DBUG_RETURN(error);
}

int Rpl_info_file::do_prepare_info_for_read()
{
  cursor= 0;
  prv_error= FALSE;
  return (reinit_io_cache(&info_file, READ_CACHE, 0L, 0, 0));
}

int Rpl_info_file::do_prepare_info_for_write()
{
  cursor= 0;
  prv_error= FALSE;
  return (reinit_io_cache(&info_file, WRITE_CACHE, 0L, 0, 1));
}

inline enum_return_check do_check_repository_file(const char *fname)
{
  if (my_access(fname, F_OK))
    return REPOSITORY_DOES_NOT_EXIST;

  if (my_access(fname, F_OK | R_OK | W_OK))
    return ERROR_CHECKING_REPOSITORY;
    
  return REPOSITORY_EXISTS;
}

/*
  The method verifies existence of an instance of the repository.

  @param instance  an index in the repository
  @retval REPOSITORY_EXISTS when the check is successful
  @retval REPOSITORY_DOES_NOT_EXIST otherwise

  @note This method also verifies overall integrity
  of the repositories to make sure they are indexed without any gaps.
*/
enum_return_check Rpl_info_file::do_check_info(uint instance)
{
  uint i;
  enum_return_check last_check= REPOSITORY_EXISTS;
  char fname_local[FN_REFLEN];
  char *pos= NULL;

  for (i= 1; i <= instance && last_check == REPOSITORY_EXISTS; i++)
  {
    pos= strmov(fname_local, pattern_fname);
    if (name_indexed)
      sprintf(pos, "%u", i);
    fn_format(fname_local, fname_local, mysql_data_home, "", 4 + 32);
    last_check= do_check_repository_file(fname_local);
  }
  return last_check;
}

enum_return_check Rpl_info_file::do_check_info()
{
  return do_check_repository_file(info_fname);
}

/*
  The function counts number of files in a range starting
  from one. The range degenerates into one item when @c indexed is false.
  Scanning ends once the next indexed file is not found.

  @param      nparam    Number of fields
  @param      param_pattern  
                        a string pattern to generate
                        the actual file name
  @param      indexed   indicates whether the file is indexed and if so
                        there is a range to count in.
  @param[out] counter   the number of discovered instances before the first
                        unsuccess in locating the next file.

  @retval false     All OK
  @retval true      An error
*/
bool Rpl_info_file::do_count_info(const int nparam,
                                  const char* param_pattern,
                                  bool  indexed,
                                  uint* counter)
{
  uint i= 0;
  Rpl_info_file* info= NULL;

  char fname_local[FN_REFLEN];
  char *pos= NULL;
  enum_return_check last_check= REPOSITORY_EXISTS;

  DBUG_ENTER("Rpl_info_file::do_count_info");

  if (!(info= new Rpl_info_file(nparam, param_pattern, "", indexed)))
    DBUG_RETURN(true);

  for (i= 1; last_check == REPOSITORY_EXISTS; i++)
  {
    pos= strmov(fname_local, param_pattern);
    if (indexed)
    {  
      sprintf(pos, "%u", i);
    }
    fn_format(fname_local, fname_local, mysql_data_home, "", 4 + 32);
    if ((last_check= do_check_repository_file(fname_local)) == REPOSITORY_EXISTS)
      (*counter)++;
    // just one loop pass for MI and RLI file
    if (!indexed)
      break;
  }
  delete info;

  DBUG_RETURN(false);
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
    if (my_b_inited(&info_file))
      end_io_cache(&info_file);
    my_close(info_fd, MYF(MY_WME));
    info_fd = -1;
  }

  DBUG_VOID_RETURN;
}

int Rpl_info_file::do_remove_info()
{
  MY_STAT stat_area;
  int error= 0;

  DBUG_ENTER("Rpl_info_file::do_remove_info");

  if (my_stat(info_fname, &stat_area, MYF(0)) && my_delete(info_fname, MYF(MY_WME)))
    error= 1;

  DBUG_RETURN(error);
}

int Rpl_info_file::do_clean_info()
{
  /*
    There is nothing to do here. Maybe we can truncate the
    file in the future. Howerver, for now, there is no need.
  */
  return 0;
}

int Rpl_info_file::do_reset_info(const int nparam,
                                       const char* param_pattern,
                                       bool indexed)
{
  int error= false;
  uint i= 0;
  Rpl_info_file* info= NULL;
  char fname_local[FN_REFLEN];
  char *pos= NULL;
  enum_return_check last_check= REPOSITORY_EXISTS;

  DBUG_ENTER("Rpl_info_file::do_count_info");

  if (!(info= new Rpl_info_file(nparam, param_pattern, "", indexed)))
    DBUG_RETURN(true);

  for (i= 1; last_check == REPOSITORY_EXISTS; i++)
  {
    pos= strmov(fname_local, param_pattern);
    if (indexed)
    {  
      sprintf(pos, "%u", i);
    }
    fn_format(fname_local, fname_local, mysql_data_home, "", 4 + 32);
    if ((last_check= do_check_repository_file(fname_local)) == REPOSITORY_EXISTS)
      if (my_delete(fname_local, MYF(MY_WME)))
        error= true;
    // just one loop pass for MI and RLI file
    if (!indexed)
      break;
  }
  delete info;

  DBUG_RETURN(error);
}

bool Rpl_info_file::do_set_info(const int pos, const char *value)
{
  return (my_b_printf(&info_file, "%s\n", value) > (size_t) 0 ?
          FALSE : TRUE);
}

bool Rpl_info_file::do_set_info(const int pos, const uchar *value,
                                const size_t size)
{
  return (my_b_write(&info_file, value, size));
}

bool Rpl_info_file::do_set_info(const int pos, const ulong value)
{
  return (my_b_printf(&info_file, "%lu\n", value) > (size_t) 0 ?
          FALSE : TRUE);
}

bool Rpl_info_file::do_set_info(const int pos, const int value)
{
  return (my_b_printf(&info_file, "%d\n", value) > (size_t) 0 ?
          FALSE : TRUE);
}

bool Rpl_info_file::do_set_info(const int pos, const float value)
{
  /*
    64 bytes provide enough space considering that the precision is 3
    bytes (See the appropriate set funciton):

    FLT_MAX  The value of this macro is the maximum number representable
             in type float. It is supposed to be at least 1E+37.
    FLT_MIN  Similar to the FLT_MAX, we have 1E-37.

    If a file is manually and not properly changed, this function may
    crash the server.
  */
  char buffer[64];

  sprintf(buffer, "%.3f", value);

  return (my_b_printf(&info_file, "%s\n", buffer) > (size_t) 0 ?
          FALSE : TRUE);
}

bool Rpl_info_file::do_set_info(const int pos, const Dynamic_ids *value)
{
  bool error= TRUE;
  String buffer;

  /*
    This produces a line listing the total number and all the server_ids.
  */
  if (const_cast<Dynamic_ids *>(value)->pack_dynamic_ids(&buffer))
    goto err;

  error= (my_b_printf(&info_file, "%s\n", buffer.c_ptr_safe()) >
          (size_t) 0 ? FALSE : TRUE);
err:
  return error;
}

bool Rpl_info_file::do_get_info(const int pos, char *value, const size_t size,
                                const char *default_value)
{
  return (init_strvar_from_file(value, size, &info_file,
                                default_value));
}

bool Rpl_info_file::do_get_info(const int pos, uchar *value, const size_t size,
                                const uchar *default_value)
{
  return(my_b_read(&info_file, value, size));
}

bool Rpl_info_file::do_get_info(const int pos, ulong *value,
                                const ulong default_value)
{
  return (init_ulongvar_from_file(value, &info_file,
                                  default_value));
}

bool Rpl_info_file::do_get_info(const int pos, int *value,
                                const int default_value)
{
  return (init_intvar_from_file((int *) value, &info_file, 
                                (int) default_value));
}

bool Rpl_info_file::do_get_info(const int pos, float *value,
                                const float default_value)
{
  return (init_floatvar_from_file(value, &info_file,
                                  default_value));
}

bool Rpl_info_file::do_get_info(const int pos, Dynamic_ids *value,
                                const Dynamic_ids *default_value __attribute__((unused)))
{
  /*
    Static buffer to use most of the times. However, if it is not big
    enough to accommodate the server ids, a new buffer is allocated.
  */
  const int array_size= 16 * (sizeof(long) * 3 + 1);
  char buffer[array_size];
  char *buffer_act= buffer;

  bool error= init_dynarray_intvar_from_file(buffer, sizeof(buffer),
                                             &buffer_act,
                                             &info_file);
  if (!error)
    value->unpack_dynamic_ids(buffer_act);

  if (buffer != buffer_act)
  {
    /*
      Release the buffer allocated while reading the server ids
      from the file.
    */
    my_free(buffer_act);
  }

  return error;
}

char* Rpl_info_file::do_get_description_info()
{
  return info_fname;
}

bool Rpl_info_file::do_is_transactional()
{
  return FALSE;
}

bool Rpl_info_file::do_update_is_transactional()
{
  DBUG_EXECUTE_IF("simulate_update_is_transactional_error",
                  {
                  return TRUE;
                  });
  return FALSE;
}

uint Rpl_info_file::do_get_rpl_info_type()
{
  return INFO_REPOSITORY_FILE;
}

int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
                          const char *default_val)
{
  uint length;
  DBUG_ENTER("init_strvar_from_file");

  if ((length=my_b_gets(f,var, max_size)))
  {
    char* last_p = var + length -1;
    if (*last_p == '\n')
      *last_p = 0; // if we stopped on newline, kill it
    else
    {
      /*
        If we truncated a line or stopped on last char, remove all chars
        up to and including newline.
      */
      int c;
      while (((c=my_b_get(f)) != '\n' && c != my_b_EOF)) ;
    }
    DBUG_RETURN(0);
  }
  else if (default_val)
  {
    strmake(var,  default_val, max_size-1);
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}

int init_intvar_from_file(int* var, IO_CACHE* f, int default_val)
{
  /*
    32 bytes provide enough space:

    INT_MIN    –2,147,483,648
    INT_MAX    +2,147,483,647
  */
  char buf[32];
  DBUG_ENTER("init_intvar_from_file");

  if (my_b_gets(f, buf, sizeof(buf)))
  {
    *var = atoi(buf);
    DBUG_RETURN(0);
  }
  else if (default_val)
  {
    *var = default_val;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}

int init_ulongvar_from_file(ulong* var, IO_CACHE* f, ulong default_val)
{
  /* 
    32 bytes provide enough space:

    ULONG_MAX   32 bit compiler   +4,294,967,295
                64 bit compiler   +18,446,744,073,709,551,615
  */
  char buf[32];
  DBUG_ENTER("init_ulongvar_from_file");

  if (my_b_gets(f, buf, sizeof(buf)))
  {
    *var = strtoul(buf, 0, 10);
    DBUG_RETURN(0);
  }
  else if (default_val)
  {
    *var = default_val;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}

int init_floatvar_from_file(float* var, IO_CACHE* f, float default_val)
{
  /*
    64 bytes provide enough space considering that the precision is 3
    bytes (See the appropriate set funciton):

    FLT_MAX  The value of this macro is the maximum number representable
             in type float. It is supposed to be at least 1E+37.
    FLT_MIN  Similar to the FLT_MAX, we have 1E-37.

    If a file is manually and not properly changed, this function may
    crash the server.
  */
  char buf[64];
  DBUG_ENTER("init_floatvar_from_file");

  if (my_b_gets(f, buf, sizeof(buf)))
  {
    if (sscanf(buf, "%f", var) != 1)
      DBUG_RETURN(1);
    else
      DBUG_RETURN(0);
  }
  else if (default_val != 0.0)
  {
    *var = default_val;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}

/**
   TODO - Improve this function to use String and avoid this weird computation
   to calculate the size of the buffers.

   Particularly, this function is responsible for restoring IGNORE_SERVER_IDS
   list of servers whose events the slave is going to ignore (to not log them
   in the relay log).

   Items being read are supposed to be decimal output of values of a  type
   shorter or equal of @c long and separated by the single space.

   @param  buffer      Put the read values in this static buffer
   @param  buffer      Size of the static buffer
   @param  buffer_act  Points to the final buffer as dynamic buffer may
                       be used if the static buffer is not big enough.

   @retval 0           All OK
   @retval non-zero  An error
*/
bool init_dynarray_intvar_from_file(char *buffer, size_t size,
                                    char **buffer_act, IO_CACHE* f)
{
  char *buf= buffer; // actual buffer can be dynamic if static is short
  char *buf_act= buffer;
  char *last;
  uint num_items;   // number of items of `arr'
  size_t read_size;

  DBUG_ENTER("init_dynarray_intvar_from_file");

  if ((read_size= my_b_gets(f, buf_act, size)) == 0)
  {
    DBUG_RETURN(FALSE); // no line in master.info
  }
  if (read_size + 1 == size && buf[size - 2] != '\n')
  {
    /*
      short read happend; allocate sufficient memory and make the 2nd read
    */
    char buf_work[(sizeof(long) * 3 + 1) * 16];
    memcpy(buf_work, buf, sizeof(buf_work));
    num_items= atoi(strtok_r(buf_work, " ", &last));
    size_t snd_size;
    /*
      max size upper bound approximate estimation bases on the formula:
      (the items number + items themselves) * 
          (decimal size + space) - 1 + `\n' + '\0'
    */
    size_t max_size= (1 + num_items) * (sizeof(long) * 3 + 1) + 1;
    if (! (buf_act= (char*) my_malloc(max_size, MYF(MY_WME))))
      DBUG_RETURN(TRUE);
    *buffer_act= buf_act;
    memcpy(buf_act, buf, read_size);
    snd_size= my_b_gets(f, buf_act + read_size, max_size - read_size);
    if (snd_size == 0 ||
        ((snd_size + 1 == max_size - read_size) &&  buf[max_size - 2] != '\n'))
    {
      /*
        failure to make the 2nd read or short read again
      */
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
}
