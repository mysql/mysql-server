/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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
#include <mysql/psi/mysql_file.h>
#include "buffered_file_io.h"

namespace keyring {

extern PSI_memory_key key_memory_KEYRING;
const my_off_t EOF_TAG_SIZE= 3;
#ifdef HAVE_PSI_INTERFACE
PSI_file_key keyring_file_data_key;
PSI_file_key keyring_backup_file_data_key;

static PSI_file_info all_keyring_files[]=
{
  { &keyring_file_data_key, "keyring_file_data", 0},
  { &keyring_backup_file_data_key, "keyring_backup_file_data", 0}
};

void keyring_init_psi_file_keys(void)
{
  const char *category = "keyring_file";
  int count;

  count= array_elements(all_keyring_files);
  mysql_file_register(category, all_keyring_files, count);
}
#endif

std::string*Buffered_file_io::get_backup_filename()
{
  if(backup_filename.empty() == FALSE)
    return &backup_filename;
  backup_filename.append(keyring_filename);
  backup_filename.append(".backup");
  return &backup_filename;
}

my_bool Buffered_file_io::open_backup_file(File *backup_file)
{
  *backup_file= mysql_file_open(keyring_backup_file_data_key, get_backup_filename()->c_str(),
                                O_RDONLY, MYF(0));
  if (likely(*backup_file < 0))
    return TRUE;
  return FALSE;
}

my_bool Buffered_file_io::is_file_tag_correct(File file)
{
  uchar tag[EOF_TAG_SIZE+1];
  mysql_file_seek(file, 0, MY_SEEK_END, MYF(0));
  if (unlikely(mysql_file_tell(file, MYF(0)) < EOF_TAG_SIZE))
    return FALSE; // File does not contain tag

  mysql_file_seek(file, -static_cast<int>(EOF_TAG_SIZE), MY_SEEK_END, MYF(0));
  if (unlikely(mysql_file_read(file, tag, EOF_TAG_SIZE, MYF(0)) != EOF_TAG_SIZE))
    return FALSE;
  tag[3]='\0';
  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  return eofTAG == reinterpret_cast<char*>(tag);
}

my_bool Buffered_file_io::is_file_version_correct(File file)
{
  boost::movelib::unique_ptr<uchar[]> version(new uchar[file_version.length()+1]);
  version.get()[file_version.length()]= '\0';
  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  if (unlikely(mysql_file_read(file, version.get(), file_version.length(), MYF(0)) !=
      file_version.length() || file_version != reinterpret_cast<char*>(version.get())))
  {
    logger->log(MY_ERROR_LEVEL, "Incorrect Keyring file version");
    return FALSE;
  }
  mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0));
  return TRUE;
}

my_bool Buffered_file_io::check_file_structure(File file, size_t file_size)
{
  return file_size < ((size_t)EOF_TAG_SIZE + file_version.length()) ||
         is_file_tag_correct(file) == FALSE ||
         is_file_version_correct(file) == FALSE;
}

my_bool Buffered_file_io::load_file_into_buffer(File file, Buffer *buffer)
{
  mysql_file_seek(file, 0, MY_SEEK_END, MYF(0));
  size_t file_size= mysql_file_tell(file, MYF(0));
  if (file_size == 0)
    return FALSE; //it is OK if file is empty
  if (check_file_structure(file, file_size))
    return TRUE;
  size_t input_buffer_size= file_size - EOF_TAG_SIZE - file_version.length(); //result has to be positive
  if (input_buffer_size % sizeof(size_t) != 0)
    return TRUE; //buffer size in the keyring file must be multiplication of size_t
  mysql_file_seek(file, file_version.length(), MY_SEEK_SET, MYF(0)); //skip file version
  if (likely(input_buffer_size > 0))
  {
    buffer->reserve(input_buffer_size);
    if (mysql_file_read(file, buffer->data, input_buffer_size, MYF(0)) !=
        input_buffer_size)
      return TRUE;
  }
  memory_needed_for_buffer= buffer->size;
  return FALSE;
}

/*!
  Recovers from backup if backup file exists
  if backup is malformed - remove it,
  else if backup is good restore keyring file from it.
*/
my_bool Buffered_file_io::recreate_keyring_from_backup_if_backup_exists()
{
  Buffer buffer;
  File backup_file;
  if (open_backup_file(&backup_file))
    return FALSE; //no backup file to recover from
  if (load_file_into_buffer(backup_file, &buffer))
  {
    logger->log(MY_WARNING_LEVEL, "Found malformed keyring backup file - "
                                  "removing it");
    mysql_file_close(backup_file, MYF(0));
    // if backup file was successfully removed then we have one keyring file
    return remove_backup();
  }
  if (flush_buffer_to_storage(&buffer) || mysql_file_close(backup_file, MYF(0)) < 0)
  {
    logger->log(MY_ERROR_LEVEL, "Error while restoring keyring from backup file"
                                " cannot overwrite keyring with backup");
    return TRUE;
  }
  return remove_backup();
}

my_bool Buffered_file_io::init(std::string *keyring_filename)
{
  DBUG_ASSERT(keyring_filename->empty() == FALSE);
#ifdef HAVE_PSI_INTERFACE
  keyring_init_psi_file_keys();
#endif
  this->keyring_filename= *keyring_filename;
  return recreate_keyring_from_backup_if_backup_exists();
}

my_bool Buffered_file_io::flush_buffer_to_file(Buffer *buffer,
                                               PSI_file_key *file_key,
                                               const std::string* filename)
{
  File file;
  my_bool was_error= TRUE;
  file= mysql_file_open(*file_key, filename->c_str(),
                        O_TRUNC | O_WRONLY | O_CREAT, MYF(0));
  if (file >= 0 &&
    mysql_file_write(file, reinterpret_cast<const uchar*>(file_version.c_str()),
                     file_version.length(), MYF(0)) == file_version.length() &&
    mysql_file_write(file, buffer->data, buffer->size,
                     MYF(0)) == buffer->size &&
    mysql_file_write(file, reinterpret_cast<const uchar*>(eofTAG.c_str()),
                     eofTAG.length(), MYF(0)) == eofTAG.length() &&
    mysql_file_close(file, MYF(0)) >= 0)
  {
    was_error= FALSE;
  }
  return was_error;
}

my_bool Buffered_file_io::flush_to_backup(ISerialized_object *serialized_object)
{
  Buffer *buffer= dynamic_cast<Buffer*>(serialized_object);
  DBUG_ASSERT(buffer != NULL);
  return buffer == NULL ||
         flush_buffer_to_file(buffer, &keyring_backup_file_data_key,
                              get_backup_filename());
}

my_bool Buffered_file_io::remove_backup()
{
  return remove(get_backup_filename()->c_str()) == 0 ? FALSE : TRUE;
}

my_bool Buffered_file_io::flush_buffer_to_storage(Buffer *buffer)
{
  return flush_buffer_to_file(buffer, &keyring_file_data_key, &keyring_filename);
}

my_bool Buffered_file_io::flush_to_storage(ISerialized_object *serialized_object)
{
  Buffer *buffer= dynamic_cast<Buffer*>(serialized_object);
  DBUG_ASSERT(buffer != NULL);
  DBUG_ASSERT(serialized_object->get_key_operation() != NONE);

  if (flush_buffer_to_storage(buffer) || remove_backup())
    return TRUE;

  memory_needed_for_buffer= buffer->size;
  return FALSE;
}

ISerializer* Buffered_file_io::get_serializer()
{
  hash_to_buffer_serializer.set_memory_needed_for_buffer(memory_needed_for_buffer);
  return &hash_to_buffer_serializer;
}

my_bool Buffered_file_io::get_serialized_object(ISerialized_object **serialized_object)
{
  File file= mysql_file_open(keyring_file_data_key, keyring_filename.c_str(),
                             O_CREAT | O_RDWR, MYF(0));
  if (file < 0)
    return TRUE;

  Buffer *buffer= new Buffer;
  if (load_file_into_buffer(file, buffer) || mysql_file_close(file, MYF(0)) < 0)
  {
    delete buffer;
    *serialized_object= NULL;
    return TRUE;
  }
  if (buffer->size == 0)  //empty keyring file
  {
    delete buffer;
    buffer= NULL;
  }
  *serialized_object= buffer;
  return FALSE;
}

my_bool Buffered_file_io::has_next_serialized_object()
{
  return FALSE;
}

} //namespace keyring
