/* Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

/**
  constructor taking a single logger output and optional list of format versions

  @param logger     - extern logging object for tracing
  @param versions   - list of allowable file format versions
*/
Buffered_file_io::Buffered_file_io(ILogger *logger,
                                   std::vector<std::string> const *versions)
    : digest(SHA256, dummy_digest),
      memory_needed_for_buffer(0),
      file_version(keyring_file_version_2_0),
      logger(logger),
      file_io(logger),
      file_arch(Converter::Arch::UNKNOWN),
      native_arch(Converter::get_native_arch()) {
  // by default we support only default keyring version
  if (versions == nullptr)
    checkers.push_back(checker_factory.getCheckerForVersion(file_version));
  else
    for (auto const &version : *versions) {
      auto checker = checker_factory.getCheckerForVersion(version);
      DBUG_ASSERT(checker != nullptr);
      checkers.push_back(std::move(checker));
    }
}

/**
  builds backup file name on-the-fly
  - shouldn't be used for write access
*/
std::string *Buffered_file_io::get_backup_filename() {
  // do we have to "build" backup file name
  if (unlikely(backup_filename.empty())) {
    backup_filename.append(keyring_filename);
    backup_filename.append(".backup");
  }

  return &backup_filename;
}

/**
  tries opening backup file and passes file handle on success

  @param backup_file - handle to opened backup file (output)

  @return
    @retval false - backup handle was successfully passed via parameter
    @retval true  - backup file could not be opened (probably missing)
*/
bool Buffered_file_io::open_backup_file(File *backup_file) {
  // try opening backup file
  *backup_file = file_io.open(keyring_backup_file_data_key,
                              get_backup_filename()->c_str(), O_RDONLY, MYF(0));

  // most probably there was no backup file
  return likely(*backup_file < 0);
}

/**
  checks whether keyring file matches any known keyring file structure

  @param file         - file handle of keyring file
  @param file_size    - size of the keyring file

  @return
    @retval false     - keyring file has valid structure
    @retval true      - keyring file does not have valid structure
*/
bool Buffered_file_io::check_file_structure(File file, size_t file_size) {
  // check whether keyring file structure matches any of rules
  for (auto &checker : checkers)
    if (!checker->check_file_structure(file, file_size, &digest, &file_arch))
      return false;  // match is found

  // no match was found, keyring file can't be used
  logger->log(ERROR_LEVEL, ER_KEYRING_INCORRECT_FILE);
  return true;
}

/**
  loads keyring file content into a Buffer serialized object
  - only called when keyring is initalizing

  @param file       - file handle of keyring file
  @param buffer     - serializable object to store file content to

  @return
    @retval false   - file is loaded into buffer, or is empty
    @retval true    - an error has occured during buffer loading
*/
bool Buffered_file_io::load_file_into_buffer(File file, Buffer *buffer) {
  // position ourselves at file end, leave on failure
  if (file_io.seek(file, 0, MY_SEEK_END, MYF(MY_WME)) == MY_FILEPOS_ERROR)
    return true;

  // get current file position (size of file)
  my_off_t file_size = file_io.tell(file, MYF(MY_WME));
  if (file_size == ((my_off_t)-1)) return true;

  // we don't load if file's empty
  if (file_size == 0) return false;  // it is OK if file is empty

  // file structure has to be valid, we also check the architecture
  if (check_file_structure(file, file_size)) return true;

  // calculate size required for buffer when wrappers are removed
  const size_t digest_length = digest.is_empty ? 0 : SHA256_DIGEST_LENGTH;
  size_t input_buffer_size =
      file_size - Checker::EOF_TAG_SIZE - file_version.length() - digest_length;

  // we have to be able to move ourselves beyond file version in file
  if (file_io.seek(file, file_version.length(), MY_SEEK_SET, MYF(MY_WME)) ==
      MY_FILEPOS_ERROR)
    return true;

  // we'll read if there's something to read
  if (likely(input_buffer_size > 0)) {
    // do we have file format mismatch
    if (file_arch != native_arch) {
      // load data to temp buffer
      std::unique_ptr<uchar> tmp(new uchar[input_buffer_size]);
      if (file_io.read(file, tmp.get(), input_buffer_size, MYF(MY_WME)) !=
          input_buffer_size)
        return true;

      // convert data to appropriate format, leave on error
      std::string converted;
      if (Converter::convert_data(reinterpret_cast<const char *>(tmp.get()),
                                  input_buffer_size, file_arch, native_arch,
                                  converted))
        return true;

      // prepare buffer size and store converted data
      buffer->reserve(converted.length());
      memcpy(buffer->data, converted.c_str(), converted.length());
    } else {
      // buffer size has to be memory aligned to machine architecture (size_t)
      if (input_buffer_size % sizeof(size_t) != 0) return true;

      // reserve buffer memory and load directly to buffer
      buffer->reserve(input_buffer_size);
      if (file_io.read(file, buffer->data, input_buffer_size, MYF(MY_WME)) !=
          input_buffer_size)
        return true;
    }
  }

  // remember current buffer size
  memory_needed_for_buffer = buffer->size;
  return false;
}

/**
  restores keys from backup file if possible
  - backup file has to exist and have a good structure
  - backup file is always erased after the procedure

  @return
    @retval false - we successfully restored from backup, or no backup
    @retval true  - we encountered an error while trying to restore
*/
bool Buffered_file_io::recreate_keyring_from_backup_if_backup_exists() {
  // try opening backup file
  File backup_file;
  if (open_backup_file(&backup_file))
    return false;  // if there's no backup file, that's not an error

  // load keyring backup to a buffer
  Buffer buffer;
  if (load_file_into_buffer(backup_file, &buffer)) {
    // backup file is malformed, delete it and emit warning
    logger->log(WARNING_LEVEL, ER_KEYRING_FOUND_MALFORMED_BACKUP_FILE);
    file_io.close(backup_file, MYF(0));

    // remove invalid backup and leave, this is still not an error
    return remove_backup(MYF(MY_WME));
  }

  // do not create keyring file from the backup if the backup file is empty
  if (buffer.size == 0) {
    logger->log(WARNING_LEVEL, ER_KEYRING_FAILED_TO_RESTORE_FROM_BACKUP_FILE);
    remove_backup(MYF(MY_WME));
    return false;
  }

  // try opening or creating main keyring file
  File keyring_file =
      file_io.open(keyring_file_data_key, keyring_filename.c_str(),
                   O_RDWR | O_CREAT, MYF(MY_WME));

  // copy buffer with backup to main file, leave on any error
  if (keyring_file < 0 || flush_buffer_to_storage(&buffer, keyring_file) ||
      file_io.close(backup_file, MYF(MY_WME)) < 0 ||
      file_io.close(keyring_file, MYF(MY_WME)) < 0)

  {
    // failure to do any of previous steps is an error
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_RESTORE_FROM_BACKUP_FILE);
    return true;
  }

  // we restored keyring from backup, so we can remove backup
  return remove_backup(MYF(MY_WME));
}

/**
  checks if keyring file exists, whether empty or not
  - if keyring file is empty, it shall be deleted

  @return
    @retval false   - keyring file can be opened for appending
    @retval true    - file I/O error (unreadable, missing...)
*/
bool Buffered_file_io::check_if_keyring_file_can_be_opened_or_created() {
  // Check if the file exists
  int file_exist = !my_access(keyring_filename.c_str(), F_OK);

  // try creating file or opening existing
  File file = file_io.open(
      keyring_file_data_key, keyring_filename.c_str(),
      file_exist && keyring_open_mode ? O_RDONLY : O_RDWR | O_CREAT,
      MYF(MY_WME));

  // if we can't open file or position ourselves at end - it's an error
  if (file < 0 ||
      file_io.seek(file, 0, MY_SEEK_END, MYF(MY_WME)) == MY_FILEPOS_ERROR)
    return true;

  // get local file position (i.e. file size), leave on error
  my_off_t file_size = file_io.tell(file, MYF(MY_WME));
  if (((file_size == (my_off_t)-1)) || file_io.close(file, MYF(MY_WME)) < 0)
    return true;

  // empty file (any remnant) should be deleted
  if (file_size == 0 && file_io.remove(keyring_filename.c_str(), MYF(MY_WME)))
    return true;

  // keyring file is accessible (we can open or create it)
  return false;
}

/**
  (override) prepares keyring file for usage
  - initialization may include restoring keys from backup file, if available
  - if no backup is available, keyring file is either opened or created

  @param keyring_filename - file name of the keyring file to initialize

  @return
    @retval true      - there was an error with initializing keyring file
    @retval false     - keyring file has been initialized successfully
*/
bool Buffered_file_io::init(std::string *keyring_filename) {
  // file name can't be empty
  DBUG_ASSERT(keyring_filename->empty() == false);

#ifdef HAVE_PSI_INTERFACE
  keyring_init_psi_file_keys();
#endif

  // set internal filename to provided name
  this->keyring_filename = *keyring_filename;

  // keys are restored from backup (optional, if available),
  // after which we assure that keyring file is accessible
  return recreate_keyring_from_backup_if_backup_exists() ||
         check_if_keyring_file_can_be_opened_or_created();
}

/**
  stores serialized keys from buffer, and accompanied data to file

  @param buffer           - object with serialized keys
  @param buffer_digest    - file digest sum
  @param file             - handle of file to store keys to

  @return
    @retval false         - buffer content successfully copied to file
    @retval true          - errors occurred during file I/O
*/
bool Buffered_file_io::flush_buffer_to_file(Buffer *buffer,
                                            Digest *buffer_digest, File file) {
  const uchar *data = buffer->data;
  size_t data_size = buffer->size;
  std::string converted;

  // check whether format conversion is required
  if (native_arch != file_arch) {
    if (Converter::convert_data(reinterpret_cast<char const *>(buffer->data),
                                buffer->size, native_arch, file_arch,
                                converted))
      return true;

    data = (const uchar *)converted.c_str();
    data_size = converted.length();
  }

  // write file version, buffer content, EOF tag and digest sum to file
  if (file_io.write(file, reinterpret_cast<const uchar *>(file_version.c_str()),
                    file_version.length(),
                    MYF(MY_WME)) == file_version.length() &&
      file_io.write(file, data, data_size, MYF(MY_WME)) == data_size &&
      file_io.write(
          file, reinterpret_cast<const uchar *>(Checker::eofTAG.c_str()),
          Checker::eofTAG.length(), MYF(MY_WME)) == Checker::eofTAG.length() &&
      file_io.write(file, reinterpret_cast<const uchar *>(buffer_digest->value),
                    SHA256_DIGEST_LENGTH, MYF(0)) == SHA256_DIGEST_LENGTH)
    return false;

  // if we're here, some of upper operations failed
  logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_FLUSH_KEYRING_TO_FILE);
  return true;
}

/**
  verifies that file structure of keyring file is valid

  @param keyring_file     - file handle of keyring file

  @return
    @retval false   - keyring file structure is valid
    @retval true    - keyring file structure is invalid
*/
bool Buffered_file_io::check_keyring_file_structure(File keyring_file) {
  // if file is missing - either we are currently creating new one, which is ok
  // (digest should be set to dummy value), or it was deleted, which is not ok
  if (keyring_file < 0)
    return strncmp(reinterpret_cast<char *>(digest.value), dummy_digest,
                   SHA256_DIGEST_LENGTH) != 0;

  // try positioning ourself at file end, leave on error
  if (file_io.seek(keyring_file, 0, MY_SEEK_END, MYF(MY_WME)) ==
      MY_FILEPOS_ERROR)
    return true;

  // determine current location (i.e. file size), leave on error
  my_off_t file_size = file_io.tell(keyring_file, MYF(MY_WME));
  if (file_size == ((my_off_t)-1)) return true;

  // lets call all available rules to see if any checks out
  return check_file_structure(keyring_file, file_size);
}

/**
  (override) fetches serialized object and stores it to a backup file
  - serialized object has to point to Buffer implementation

  @param serialized_object - object holding keys to be backed-up

  @return
    @retval false - keys from serialized object were stored to backup
    @retval true  - error occurred during keys backup
*/
bool Buffered_file_io::flush_to_backup(ISerialized_object *serialized_object) {
  // First open backup file then check keyring file. This way we make sure that
  // media, where keyring file is written, is not replaced with some other media
  // before backup file is written. In case media was changed backup file
  // handler  becomes invalid
  File backup_file =
      file_io.open(keyring_backup_file_data_key, get_backup_filename()->c_str(),
                   O_WRONLY | O_TRUNC | O_CREAT, MYF(MY_WME));

  File keyring_file = file_io.open(keyring_file_data_key,
                                   keyring_filename.c_str(), O_RDONLY, MYF(0));

  // backup file must be available
  if (backup_file < 0) {
    // close the keyring file if necessary
    if (keyring_file >= 0) file_io.close(keyring_file, MYF(MY_WME));
    return true;  // failure - no backup file
  }

  // try checking keyring file structure and closing it after that
  if (check_keyring_file_structure(keyring_file) ||
      (keyring_file >= 0 && file_io.close(keyring_file, MYF(MY_WME)) < 0)) {
    // close keyring file if necessary
    if (keyring_file >= 0) file_io.close(keyring_file, MYF(MY_WME));

    // close backup and remove it
    file_io.close(backup_file, MYF(MY_WME));
    remove_backup(MYF(MY_WME));

    return true;  // failure - bad or missing keyring file
  }

  // upcast serialized object to buffer, verify result
  Buffer *buffer = dynamic_cast<Buffer *>(serialized_object);
  DBUG_ASSERT(buffer != NULL);

  // calculate digest sum of buffer
  Digest buffer_digest;
  buffer_digest.compute(buffer->data, buffer->size);

  // hook to crash the server before wiring the keyring backup file
  DBUG_EXECUTE_IF("keyring_file_backup_fail", DBUG_SUICIDE(););

  // store buffer to backup file and close it, leave on error
  return buffer == NULL ||
         flush_buffer_to_file(buffer, &buffer_digest, backup_file) ||
         file_io.close(backup_file, MYF(MY_WME)) < 0;
}

/**
  deletes backup file from file system

  @param my_flags - flags to be used during file removal

  @return
    @retval false - backup file was successfully removed
    @retval true  - backup file removal failed
*/
bool Buffered_file_io::remove_backup(myf my_flags) {
  return file_io.remove(get_backup_filename()->c_str(), my_flags);
}

/**
  overwrites file with keys from buffer, and associated wrapper data

  @param buffer    - object with serialized key to store
  @param file      - handle of file to store keys to

  @return
    @retval false  - buffer was successfully stored to a file
    @retval true   - there was an error during procedure
*/
bool Buffered_file_io::flush_buffer_to_storage(Buffer *buffer, File file) {
  // clear file content and go to start of file, leave on error
  if (file_io.truncate(file, MYF(MY_WME)) ||
      file_io.seek(file, 0, MY_SEEK_SET, MYF(MY_WME)) != 0)
    return true;

  // calculate buffer digest
  Digest buffer_digest;
  buffer_digest.compute(buffer->data, buffer->size);

  // store buffer to a file, leave on error
  if (flush_buffer_to_file(buffer, &buffer_digest, file)) return true;

  // remember current file digest
  digest = buffer_digest;
  return false;
}

/**
  (override) stores keys from serialized object to keyring file
  - serialized object has to point to Buffer implementation

  @param serialized_object - object holding keys to be stored

  @return
    @retval false - keys from serialized object were stored to keyring file
    @retval true  - error occurred during store operation
*/
bool Buffered_file_io::flush_to_storage(ISerialized_object *serialized_object) {
  Buffer *buffer = dynamic_cast<Buffer *>(serialized_object);
  DBUG_ASSERT(buffer != NULL);
  DBUG_ASSERT(serialized_object->get_key_operation() != NONE);

  // open keyring file
  File keyring_file =
      file_io.open(keyring_file_data_key, keyring_filename.c_str(),
                   O_CREAT | O_RDWR, MYF(MY_WME));

  // we need valid keyring file, and writing has to succeed
  if (keyring_file < 0 || check_keyring_file_structure(keyring_file) ||
      flush_buffer_to_storage(buffer, keyring_file)) {
    // close keyring file and return error
    file_io.close(keyring_file, MYF(MY_WME));
    return true;
  }

  // close file and remove backup, leave on error
  if (file_io.close(keyring_file, MYF(MY_WME)) < 0 ||
      remove_backup(MYF(MY_WME)))
    return true;

  // store information about memory required for keys in buffer
  memory_needed_for_buffer = buffer->size;
  return false;
}

/**
  (override) retrieves serialized which creates Buffer implementation from key
  hash map
  - used to prepare appropriate serialized object for class (Buffer)

  @return - serializer which converts key hash map to a Buffer implementation
*/
ISerializer *Buffered_file_io::get_serializer() {
  hash_to_buffer_serializer.set_memory_needed_for_buffer(
      memory_needed_for_buffer);
  return &hash_to_buffer_serializer;
}

/**
  (override) retrieves serialized implementation of keys stored in keyring file
  - keys object could be empty, if keyring file is empty

  @param serialized_object - pointer-to-pointer of location to store keys

  @return false - set of keys was successfully retrieved from keyring file
  @return true  - we were not able to retrieve serialized keys
*/
bool Buffered_file_io::get_serialized_object(
    ISerialized_object **serialized_object) {
  // Check if the file exists
  int file_exist = !my_access(keyring_filename.c_str(), F_OK);

  // try opening keyring file, leave on error
  File file = file_io.open(
      keyring_file_data_key, keyring_filename.c_str(),
      file_exist && keyring_open_mode ? O_RDONLY : O_RDWR | O_CREAT,
      MYF(MY_WME));
  if (file < 0) return true;

  // try loading file content into a Buffer implementation
  std::unique_ptr<Buffer> buffer(new Buffer);
  if (load_file_into_buffer(file, buffer.get())) {
    // didn't work - close file and pass a null pointer
    file_io.close(file, MYF(MY_WME));
    *serialized_object = NULL;
    return true;
  }

  // close keyring file, leave on error
  if (file_io.close(file, MYF(MY_WME)) < 0) return true;

  // if there were no keys in keyring file, we reset it
  if (buffer->size == 0) buffer.reset(NULL);

  // pass buffer memory to the caller
  *serialized_object = buffer.release();
  return false;
}

/**
  (override) verifies if there is more serialized objects in I/O object

  @return
    @retval true  - there is at least one more serialized object to get
    @retval false - there is no more serialized objects to get
*/
bool Buffered_file_io::has_next_serialized_object() {
  // Buffered_file_io implementation uses a single serialized object
  return false;
}

}  // namespace keyring
