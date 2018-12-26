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
#include "file_io.h"

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
  *backup_file= file_io.open(keyring_backup_file_data_key, get_backup_filename()->c_str(),
                             O_RDONLY, MYF(0));

  if (likely(*backup_file < 0))
    return TRUE;
  return FALSE;
}

my_bool Buffered_file_io::is_file_tag_correct(File file)
{
  uchar tag[EOF_TAG_SIZE+1];
  if (unlikely(file_io.seek(file, 0, MY_SEEK_END, MYF(MY_WME)) == MY_FILEPOS_ERROR ||
               file_io.tell(file, MYF(MY_WME)) < EOF_TAG_SIZE) ||
               file_io.seek(file, -static_cast<int>(EOF_TAG_SIZE), MY_SEEK_END, MYF(MY_WME)) ==
                            MY_FILEPOS_ERROR ||
               file_io.read(file, tag, EOF_TAG_SIZE, MYF(MY_WME)) != EOF_TAG_SIZE ||
               file_io.seek(file, 0, MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR)
    return FALSE; // File does not contain tag

  tag[3]='\0';
  return eofTAG == reinterpret_cast<char*>(tag);
}

my_bool Buffered_file_io::is_file_version_correct(File file)
{
  boost::movelib::unique_ptr<uchar[]> version(new uchar[file_version.length()+1]);
  version.get()[file_version.length()]= '\0';
  if (unlikely(file_io.seek(file, 0, MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR ||
               file_io.read(file, version.get(), file_version.length(), MYF(MY_WME)) !=
                            file_version.length() ||
               file_version != reinterpret_cast<char*>(version.get()) ||
               file_io.seek(file, 0, MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR))
  {
    logger->log(MY_ERROR_LEVEL, "Incorrect Keyring file version");
    return FALSE;
  }
  return TRUE;
}

my_bool Buffered_file_io::check_if_keyring_file_can_be_opened_or_created()
{
  File file= file_io.open(keyring_file_data_key, this->keyring_filename.c_str(),
                          O_RDWR | O_CREAT, MYF(MY_WME));
  if (file < 0 ||
      file_io.seek(file, 0, MY_SEEK_END, MYF(MY_WME)) == MY_FILEPOS_ERROR)
    return TRUE;
  my_off_t file_size= file_io.tell(file, MYF(MY_WME));
  if ((file_size == ((my_off_t) - 1)) || file_io.close(file, MYF(MY_WME)) < 0)
    return TRUE;
  if (file_size == 0 && file_io.remove(this->keyring_filename.c_str(), MYF(MY_WME))) //remove empty file
    return TRUE;
  return FALSE;
}

my_bool Buffered_file_io::check_file_structure(File file, size_t file_size)
{
  return file_size < ((size_t)EOF_TAG_SIZE + file_version.length()) ||
         is_file_tag_correct(file) == FALSE ||
         is_file_version_correct(file) == FALSE;
}

my_bool Buffered_file_io::check_keyring_file_stat(File file)
{
  if (file >= 0 && saved_keyring_stat.is_initialized == TRUE)
  {
    static MY_STAT keyring_file_stat;
    memset(&keyring_file_stat, 0, sizeof(MY_STAT));
    if (file_io.fstat(file, &keyring_file_stat, MYF(MY_WME)))
      return TRUE;
    if (saved_keyring_stat != keyring_file_stat)
    {
      logger->log(MY_ERROR_LEVEL, "Keyring file has been changed outside the "
                                  "server.");
      return TRUE;
    }
    return FALSE;
  }
  //if keyring_file does not exist it means saved_keyring_stat cannot
  //be initialized - i.e. we are initializing keyring_file
  return saved_keyring_stat.is_initialized == TRUE;
}

my_bool Buffered_file_io::load_file_into_buffer(File file, Buffer *buffer)
{
  if (file_io.seek(file, 0, MY_SEEK_END, MYF(MY_WME)) == MY_FILEPOS_ERROR)
    return TRUE;
  my_off_t file_size= file_io.tell(file, MYF(MY_WME));
  if (file_size == ((my_off_t) - 1))
    return TRUE;
  if (file_size == 0)
    return FALSE; //it is OK if file is empty
  if (check_file_structure(file, file_size))
    return TRUE;
  size_t input_buffer_size= file_size - EOF_TAG_SIZE - file_version.length(); //result has to be positive
  if (input_buffer_size % sizeof(size_t) != 0)
    return TRUE; //buffer size in the keyring file must be multiplication of size_t
  if (file_io.seek(file, file_version.length(), MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR) //skip file version
    return TRUE;
  if (likely(input_buffer_size > 0))
  {
    buffer->reserve(input_buffer_size);
    if (file_io.read(file, buffer->data, input_buffer_size, MYF(MY_WME)) !=
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
    file_io.close(backup_file, MYF(0));
    // if backup file was successfully removed then we have one keyring file
    return remove_backup(MYF(MY_WME));
  }
  File keyring_file= file_io.open(keyring_file_data_key,
                                  this->keyring_filename.c_str(),
                                  O_RDWR | O_CREAT, MYF(MY_WME));

  if (keyring_file < 0 ||
      flush_buffer_to_storage(&buffer, keyring_file) ||
      file_io.close(backup_file, MYF(MY_WME)) < 0 ||
      file_io.close(keyring_file, MYF(MY_WME)) < 0)

  {
    logger->log(MY_ERROR_LEVEL, "Error while restoring keyring from backup file"
                                " cannot overwrite keyring with backup");
    return TRUE;
  }
  return remove_backup(MYF(MY_WME));
}

my_bool Buffered_file_io::init(std::string *keyring_filename)
{
  DBUG_ASSERT(keyring_filename->empty() == FALSE);
#ifdef HAVE_PSI_INTERFACE
  keyring_init_psi_file_keys();
#endif
  this->keyring_filename= *keyring_filename;
  if (recreate_keyring_from_backup_if_backup_exists() ||
      check_if_keyring_file_can_be_opened_or_created())
    return TRUE;
  File keyring_file = file_io.open(keyring_file_data_key,
                                   this->keyring_filename.c_str(), O_RDONLY,
                                   MYF(0));

  return (keyring_file >= 0 && (read_keyring_stat(keyring_file) ||
          file_io.close(keyring_file, MYF(MY_WME)) < 0));
}

my_bool Buffered_file_io::flush_buffer_to_file(Buffer *buffer,
                                               File file)
{
  if (file_io.write(file, reinterpret_cast<const uchar*>(file_version.c_str()),
                    file_version.length(), MYF(MY_WME)) == file_version.length() &&
    file_io.write(file, buffer->data, buffer->size, MYF(MY_WME)) == buffer->size &&
    file_io.write(file, reinterpret_cast<const uchar*>(eofTAG.c_str()),
                  eofTAG.length(), MYF(MY_WME)) == eofTAG.length())
      return FALSE;

  logger->log(MY_ERROR_LEVEL, "Error while flushing in-memory keyring into "
                              "keyring file");
  return TRUE;
}

my_bool Buffered_file_io::flush_to_backup(ISerialized_object *serialized_object)
{
  //First open backup file then check keyring file. This way we make sure that
  //media, where keyring file is written, is not replaced with some other media
  //before backup file is written. In case media was changed backup file handler
  //becomes invalid
  File backup_file= file_io.open(keyring_backup_file_data_key,
                                 get_backup_filename()->c_str(),
                                 O_WRONLY | O_TRUNC | O_CREAT, MYF(MY_WME));

  File keyring_file= file_io.open(keyring_file_data_key,
                                  this->keyring_filename.c_str(), O_RDONLY,
                                  MYF(0));
  if (backup_file < 0)
  {
    if (keyring_file >= 0)
      file_io.close(keyring_file, MYF(MY_WME));
    return TRUE;
  }
  if (check_keyring_file_stat(keyring_file) ||
      (keyring_file >= 0 && file_io.close(keyring_file, MYF(MY_WME)) < 0))
  {
    if (keyring_file >= 0)
      file_io.close(keyring_file, MYF(MY_WME));
    file_io.close(backup_file, MYF(MY_WME));
    remove_backup(MYF(MY_WME));
    return TRUE;
  }

  Buffer *buffer= dynamic_cast<Buffer*>(serialized_object);
  DBUG_ASSERT(buffer != NULL);
  return buffer == NULL ||
         flush_buffer_to_file(buffer, backup_file) ||
         file_io.close(backup_file, MYF(MY_WME)) < 0;
}

my_bool Buffered_file_io::remove_backup(myf myFlags)
{
  return file_io.remove(get_backup_filename()->c_str(), myFlags);
}

my_bool Buffered_file_io::flush_buffer_to_storage(Buffer *buffer, File file)
{
  return file_io.truncate(file, MYF(MY_WME)) ||
         file_io.seek(file, 0, MY_SEEK_SET, MYF(MY_WME)) != 0 ||
         flush_buffer_to_file(buffer, file);
}

my_bool Buffered_file_io::read_keyring_stat(File file)
{
  file_io.sync(file, MYF(0));
  if (file_io.fstat(file, &saved_keyring_stat, MYF(MY_WME)) < 0)
    return TRUE;
  saved_keyring_stat.is_initialized= TRUE;
  return FALSE;
}

my_bool Buffered_file_io::flush_to_storage(ISerialized_object *serialized_object)
{
  Buffer *buffer= dynamic_cast<Buffer*>(serialized_object);
  DBUG_ASSERT(buffer != NULL);
  DBUG_ASSERT(serialized_object->get_key_operation() != NONE);

  File keyring_file= file_io.open(keyring_file_data_key,
                                  this->keyring_filename.c_str(), O_CREAT | O_RDWR,
                                  MYF(MY_WME));

  if (keyring_file < 0 || check_keyring_file_stat(keyring_file) ||
      flush_buffer_to_storage(buffer, keyring_file) ||
      read_keyring_stat(keyring_file))
  {
    file_io.close(keyring_file,MYF(MY_WME));
    return TRUE;
  }
  if (file_io.close(keyring_file, MYF(MY_WME)) < 0 || remove_backup(MYF(MY_WME)))
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
  File file= file_io.open(keyring_file_data_key, keyring_filename.c_str(),
                          O_CREAT | O_RDWR, MYF(MY_WME));

  *serialized_object= NULL;

  if (file < 0) //nothing to read
    return TRUE;

  Buffer *buffer= new Buffer;
  if (check_keyring_file_stat(file) || load_file_into_buffer(file, buffer) ||
      read_keyring_stat(file))
  {
    file_io.close(file, MYF(MY_WME));
    delete buffer;
    return TRUE;
  }
  if (file_io.close(file, MYF(MY_WME)) < 0)
  {
    delete buffer;
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
