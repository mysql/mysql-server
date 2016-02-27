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

#ifndef BUFFEREDFILEIO_INCLUDED
#define BUFFEREDFILEIO_INCLUDED

#include <my_global.h>
#include <mysql/plugin.h>
#include "i_keyring_io.h"
#include "logger.h"
#include "keyring_memory.h"
#include "buffer.h"

namespace keyring {

class Buffered_file_io : public IKeyring_io
{
public:
  Buffered_file_io(ILogger *logger)
    : eofTAG("EOF"),
      file_version("Keyring file version:1.0"),
      logger(logger)
    , backup_exists(FALSE)
  {}

  ~Buffered_file_io();

  my_bool init(std::string *keyring_filename);
  my_bool open(std::string *keyring_filename);
  void reserve_buffer(size_t memory_size);
  my_bool close();
  my_bool flush_to_backup();
  /* Both attributes are unused */
  my_bool flush_to_keyring(IKey *key = NULL, Flush_operation operation = STORE_KEY);
  /**
   * Writes key into the buffer
   * @param key the key to be written to the buffer
   * @return TRUE on success
  */
  my_bool operator<<(const IKey* key);
  /**
   * Reads key from the buffer
   * @param key the key where memory from the buffer is going to be placed
   * @return TRUE on success
  */
  my_bool operator>>(IKey* key);
protected:
  Buffer buffer;
private:
  my_bool recreate_keyring_from_backup_if_backup_exists();

  std::string* get_backup_filename();
  my_bool remove_backup();
  my_bool open_backup_file(File *backup_file);
  my_bool load_keyring_into_input_buffer(File file);
  my_bool flush_to_file(PSI_file_key *file_key, const std::string* filename);
  inline my_bool check_file_structure(File file, size_t file_size);
  my_bool is_file_tag_correct(File file);
  my_bool is_file_version_correct(File file);

  std::string keyring_filename;
  std::string backup_filename;
  const std::string eofTAG;
  const std::string file_version;
  ILogger *logger;
  my_bool backup_exists;
};

}//namespace keyring

#endif //BUFFEREDFILEIO_INCLUDED
