/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BUFFEREDFILEIO_INCLUDED
#define BUFFEREDFILEIO_INCLUDED

#include <mysql/plugin.h>
#include <stddef.h>

#include "my_inttypes.h"
#include "my_io.h"
#include "plugin/keyring/buffer.h"
#include "plugin/keyring/checker/checker_factory.h"
#include "plugin/keyring/common/i_keyring_io.h"
#include "plugin/keyring/common/keyring_memory.h"
#include "plugin/keyring/common/logger.h"
#include "plugin/keyring/digest.h"
#include "plugin/keyring/file_io.h"
#include "plugin/keyring/hash_to_buffer_serializer.h"

namespace keyring {

class Buffered_file_io : public IKeyring_io {
 public:
  Buffered_file_io(ILogger *logger,
                   std::vector<std::string> *allowedFileVersionsToInit = NULL);

  ~Buffered_file_io();

  bool init(std::string *keyring_filename);

  bool flush_to_backup(ISerialized_object *serialized_object);
  bool flush_to_storage(ISerialized_object *serialized_object);

  ISerializer *get_serializer();
  bool get_serialized_object(ISerialized_object **serialized_object);
  bool has_next_serialized_object();

 protected:
  virtual bool remove_backup(myf myFlags);
  Buffer buffer;
  Digest digest;
  size_t memory_needed_for_buffer;

 private:
  bool recreate_keyring_from_backup_if_backup_exists();

  std::string *get_backup_filename();
  bool open_backup_file(File *backup_file);
  bool load_file_into_buffer(File file, Buffer *buffer);
  bool flush_buffer_to_storage(Buffer *buffer, File file);
  bool flush_buffer_to_file(Buffer *buffer, Digest *buffer_digest, File file);
  bool check_keyring_file_structure(File keyring_file);
  bool check_file_structure(File file, size_t file_size);
  bool check_if_keyring_file_can_be_opened_or_created();

  std::string keyring_filename;
  std::string backup_filename;
  const std::string file_version;
  ILogger *logger;
  Hash_to_buffer_serializer hash_to_buffer_serializer;
  std::vector<Checker *> checkers;
  CheckerFactory checker_factory;
  File_io file_io;
  File keyring_file;
};

}  // namespace keyring

#endif  // BUFFEREDFILEIO_INCLUDED
