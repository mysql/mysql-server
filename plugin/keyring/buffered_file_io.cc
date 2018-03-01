/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/keyring/buffered_file_io.h"

#include <fcntl.h>
#include <mysql/psi/mysql_file.h>
#include <stdio.h>
#include <algorithm>
#include <memory>

#include "my_compiler.h"
#include "my_dbug.h"
#include "mysqld_error.h"

namespace keyring {

extern PSI_memory_key key_memory_KEYRING;
#ifdef HAVE_PSI_INTERFACE
PSI_file_key keyring_file_data_key;
PSI_file_key keyring_backup_file_data_key;

static PSI_file_info all_keyring_files[] = {
    {&keyring_file_data_key, "keyring_file_data", 0, 0, PSI_DOCUMENT_ME},
    {&keyring_backup_file_data_key, "keyring_backup_file_data", 0, 0,
     PSI_DOCUMENT_ME}};

void keyring_init_psi_file_keys(void) {
  const char *category = "keyring_file";
  int count;

  count = static_cast<int>(array_elements(all_keyring_files));
  mysql_file_register(category, all_keyring_files, count);
}
#endif

Buffered_file_io::Buffered_file_io(
    ILogger *logger, std::vector<std::string> *allowedFileVersionsToInit)
    : digest(SHA256, dummy_digest),
      memory_needed_for_buffer(0),
      file_version(keyring_file_version_2_0),
      logger(logger),
      file_io(logger) {
  if (allowedFileVersionsToInit == NULL)
    checkers.push_back(checker_factory.getCheckerForVersion(file_version));
  else
    std::for_each(
        allowedFileVersionsToInit->begin(), allowedFileVersionsToInit->end(),
        [this](std::string version) {
          Checker *checker = checker_factory.getCheckerForVersion(version);
          DBUG_ASSERT(checker != NULL);
          checkers.push_back(checker);
        });
}

Buffered_file_io::~Buffered_file_io() {
  std::for_each(checkers.begin(), checkers.end(),
                [](Checker *checker) { delete checker; });
}

std::string *Buffered_file_io::get_backup_filename() {
  if (backup_filename.empty() == false) return &backup_filename;
  backup_filename.append(keyring_filename);
  backup_filename.append(".backup");
  return &backup_filename;
}

bool Buffered_file_io::open_backup_file(File *backup_file) {
  *backup_file = file_io.open(keyring_backup_file_data_key,
                              get_backup_filename()->c_str(), O_RDONLY, MYF(0));

  if (likely(*backup_file < 0)) return true;
  return false;
}

bool Buffered_file_io::check_file_structure(File file, size_t file_size) {
  if (std::find_if(checkers.begin(), checkers.end(), [&](Checker *checker) {
        return checker->check_file_structure(file, file_size, &digest) == false;
      }) == checkers.end()) {
    logger->log(ERROR_LEVEL, ER_KEYRING_INCORRECT_FILE);
    return true;
  }
  return false;
}

// Only called when keyring is initalizing
bool Buffered_file_io::load_file_into_buffer(File file, Buffer *buffer) {
  if (file_io.seek(file, 0, MY_SEEK_END, MYF(MY_WME)) == MY_FILEPOS_ERROR)
    return true;
  my_off_t file_size = file_io.tell(file, MYF(MY_WME));
  if (file_size == ((my_off_t)-1)) return true;
  if (file_size == 0) return false;  // it is OK if file is empty
  if (check_file_structure(file, file_size)) return true;
  // result has to be positive, digest (if exists) was already read by checker
  int digest_length = digest.is_empty ? 0 : SHA256_DIGEST_LENGTH;
  size_t input_buffer_size =
      file_size - Checker::EOF_TAG_SIZE - file_version.length() - digest_length;
  if (input_buffer_size % sizeof(size_t) != 0)
    return true;  // buffer size in the keyring file must be multiplication of
                  // size_t
  if (file_io.seek(file, file_version.length(), MY_SEEK_SET, MYF(MY_WME)) ==
      MY_FILEPOS_ERROR)  // skip file version
    return true;
  if (likely(input_buffer_size > 0)) {
    buffer->reserve(input_buffer_size);
    if (file_io.read(file, buffer->data, input_buffer_size, MYF(MY_WME)) !=
        input_buffer_size)
      return true;
  }
  memory_needed_for_buffer = buffer->size;
  return false;
}

/*!
  Recovers from backup if backup file exists
  if backup is malformed - remove it,
  else if backup is good restore keyring file from it.
*/
bool Buffered_file_io::recreate_keyring_from_backup_if_backup_exists() {
  Buffer buffer;
  File backup_file;
  if (open_backup_file(&backup_file))
    return false;  // no backup file to recover from
  if (load_file_into_buffer(backup_file, &buffer)) {
    logger->log(WARNING_LEVEL, ER_KEYRING_FOUND_MALFORMED_BACKUP_FILE);
    file_io.close(backup_file, MYF(0));
    // if backup file was successfully removed then we have one keyring file
    return remove_backup(MYF(MY_WME));
  }
  // Do not create keyring file from the backup if the backup file is empty
  if (buffer.size == 0) {
    logger->log(WARNING_LEVEL, ER_KEYRING_FAILED_TO_RESTORE_FROM_BACKUP_FILE);
    remove_backup(MYF(MY_WME));
    return false;
  }
  File keyring_file =
      file_io.open(keyring_file_data_key, this->keyring_filename.c_str(),
                   O_RDWR | O_CREAT, MYF(MY_WME));

  if (keyring_file < 0 || flush_buffer_to_storage(&buffer, keyring_file) ||
      file_io.close(backup_file, MYF(MY_WME)) < 0 ||
      file_io.close(keyring_file, MYF(MY_WME)) < 0)

  {
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_RESTORE_FROM_BACKUP_FILE);
    return true;
  }
  return remove_backup(MYF(MY_WME));
}

/*!
  Recovers from backup if backup file exists
  if backup is malformed - remove it,
  else if backup is good restore keyring file from it.
*/
bool Buffered_file_io::check_if_keyring_file_can_be_opened_or_created() {
  File file =
      file_io.open(keyring_file_data_key, this->keyring_filename.c_str(),
                   O_RDWR | O_CREAT, MYF(MY_WME));
  if (file < 0 ||
      file_io.seek(file, 0, MY_SEEK_END, MYF(MY_WME)) == MY_FILEPOS_ERROR)
    return true;
  my_off_t file_size = file_io.tell(file, MYF(MY_WME));
  if (((file_size == (my_off_t)-1)) || file_io.close(file, MYF(MY_WME)) < 0)
    return true;
  if (file_size == 0 && file_io.remove(this->keyring_filename.c_str(),
                                       MYF(MY_WME)))  // remove empty file
    return true;
  return false;
}

bool Buffered_file_io::init(std::string *keyring_filename) {
  DBUG_ASSERT(keyring_filename->empty() == false);
#ifdef HAVE_PSI_INTERFACE
  keyring_init_psi_file_keys();
#endif
  this->keyring_filename = *keyring_filename;
  return recreate_keyring_from_backup_if_backup_exists() ||
         check_if_keyring_file_can_be_opened_or_created();
}

bool Buffered_file_io::flush_buffer_to_file(Buffer *buffer,
                                            Digest *buffer_digest, File file) {
  if (file_io.write(file, reinterpret_cast<const uchar *>(file_version.c_str()),
                    file_version.length(),
                    MYF(MY_WME)) == file_version.length() &&
      file_io.write(file, buffer->data, buffer->size, MYF(MY_WME)) ==
          buffer->size &&
      file_io.write(
          file, reinterpret_cast<const uchar *>(Checker::eofTAG.c_str()),
          Checker::eofTAG.length(), MYF(MY_WME)) == Checker::eofTAG.length() &&
      file_io.write(file, reinterpret_cast<const uchar *>(buffer_digest->value),
                    SHA256_DIGEST_LENGTH, MYF(0)) == SHA256_DIGEST_LENGTH)
    return false;

  logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_FLUSH_KEYRING_TO_FILE);
  return true;
}

bool Buffered_file_io::check_keyring_file_structure(File keyring_file) {
  if (keyring_file >= 0)  // keyring file exists
  {
    if (file_io.seek(keyring_file, 0, MY_SEEK_END, MYF(MY_WME)) ==
        MY_FILEPOS_ERROR)
      return true;
    my_off_t file_size = file_io.tell(keyring_file, MYF(MY_WME));
    if (file_size == ((my_off_t)-1)) return true;
    return check_file_structure(keyring_file, file_size);
  }
  // if keyring_file doest not exist, we should be initializing and digest
  // should  be set to dummy. Otherwise the keyring file was removed.
  return strncmp(reinterpret_cast<char *>(digest.value), dummy_digest,
                 SHA256_DIGEST_LENGTH) != 0;
}

bool Buffered_file_io::flush_to_backup(ISerialized_object *serialized_object) {
  // First open backup file then check keyring file. This way we make sure that
  // media, where keyring file is written, is not replaced with some other media
  // before backup file is written. In case media was changed backup file
  // handler  becomes invalid
  File backup_file =
      file_io.open(keyring_backup_file_data_key, get_backup_filename()->c_str(),
                   O_WRONLY | O_TRUNC | O_CREAT, MYF(MY_WME));

  File keyring_file = file_io.open(
      keyring_file_data_key, this->keyring_filename.c_str(), O_RDONLY, MYF(0));
  if (backup_file < 0) {
    if (keyring_file >= 0) file_io.close(keyring_file, MYF(MY_WME));
    return true;
  }
  if (check_keyring_file_structure(keyring_file) ||
      (keyring_file >= 0 && file_io.close(keyring_file, MYF(MY_WME)) < 0)) {
    if (keyring_file >= 0) file_io.close(keyring_file, MYF(MY_WME));
    file_io.close(backup_file, MYF(MY_WME));
    remove_backup(MYF(MY_WME));
    return true;
  }

  Buffer *buffer = dynamic_cast<Buffer *>(serialized_object);
  DBUG_ASSERT(buffer != NULL);
  Digest buffer_digest;
  buffer_digest.compute(buffer->data, buffer->size);

  // Hook to crash the server before wiring the keyring backup file
  DBUG_EXECUTE_IF("keyring_file_backup_fail", DBUG_SUICIDE(););
  return buffer == NULL ||
         flush_buffer_to_file(buffer, &buffer_digest, backup_file) ||
         file_io.close(backup_file, MYF(MY_WME)) < 0;
}

bool Buffered_file_io::remove_backup(myf myFlags) {
  return file_io.remove(get_backup_filename()->c_str(), myFlags);
}

bool Buffered_file_io::flush_buffer_to_storage(Buffer *buffer, File file) {
  Digest buffer_digest;
  if (file_io.truncate(file, MYF(MY_WME)) ||
      file_io.seek(file, 0, MY_SEEK_SET, MYF(MY_WME)) != 0)
    return true;
  buffer_digest.compute(buffer->data, buffer->size);
  if (flush_buffer_to_file(buffer, &buffer_digest, file)) return true;
  digest = buffer_digest;
  return false;
}

bool Buffered_file_io::flush_to_storage(ISerialized_object *serialized_object) {
  Buffer *buffer = dynamic_cast<Buffer *>(serialized_object);
  DBUG_ASSERT(buffer != NULL);
  DBUG_ASSERT(serialized_object->get_key_operation() != NONE);

  File keyring_file =
      file_io.open(keyring_file_data_key, this->keyring_filename.c_str(),
                   O_CREAT | O_RDWR, MYF(MY_WME));

  if (keyring_file < 0 || check_keyring_file_structure(keyring_file) ||
      flush_buffer_to_storage(buffer, keyring_file)) {
    file_io.close(keyring_file, MYF(MY_WME));
    return true;
  }
  if (file_io.close(keyring_file, MYF(MY_WME)) < 0 ||
      remove_backup(MYF(MY_WME)))
    return true;

  memory_needed_for_buffer = buffer->size;
  return false;
}

ISerializer *Buffered_file_io::get_serializer() {
  hash_to_buffer_serializer.set_memory_needed_for_buffer(
      memory_needed_for_buffer);
  return &hash_to_buffer_serializer;
}

bool Buffered_file_io::get_serialized_object(
    ISerialized_object **serialized_object) {
  File file = file_io.open(keyring_file_data_key, keyring_filename.c_str(),
                           O_CREAT | O_RDWR, MYF(MY_WME));
  if (file < 0) return true;

  std::unique_ptr<Buffer> buffer(new Buffer);
  if (load_file_into_buffer(file, buffer.get())) {
    file_io.close(file, MYF(MY_WME));
    *serialized_object = NULL;
    return true;
  }
  if (file_io.close(file, MYF(MY_WME)) < 0) return true;
  if (buffer->size == 0)  // empty keyring file
    buffer.reset(NULL);
  *serialized_object = buffer.release();
  return false;
}

bool Buffered_file_io::has_next_serialized_object() { return false; }

}  // namespace keyring
