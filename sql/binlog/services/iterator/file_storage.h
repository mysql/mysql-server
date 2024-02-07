/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef BINLOG_SERVICE_ITERATOR_FILE_STORAGE_H_
#define BINLOG_SERVICE_ITERATOR_FILE_STORAGE_H_

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/binlog_storage_iterator.h>

namespace binlog::services::iterator {
class FileStorage {
 public:
  /// @brief The fully qualified service name: "binlog_storage_iterator.file".
  static const std::string SERVICE_NAME;

  /// @brief This registers runtime binary log related file services in
  /// the service registry.
  ///
  /// This function is called at server startup.
  ///
  /// @return true on failure
  /// @return false on success
  static bool register_service();

  /// @brief This unregisters runtime binary log related file storage services
  /// from the server registry.
  ///
  /// This function is called, when the binary log is closed or when the server
  /// is shutting down.
  ///
  /// @return true on failure.
  /// @return false on success.
  static bool unregister_service();

  /// @brief Initializes the iterator.
  ///
  /// This function must be called prior to using the iterator.
  ///
  /// @param[out] iterator a pointer to the iterator to initialize.
  ///
  /// @param[in] excluded_gtids_as_string the set of transaction identifiers to
  /// skip while reading from the log.
  ///
  /// @retval kBinlogIteratorInitOk if the operation concluded successfully.
  /// @retval kBinlogIteratorIniErrorPurgedGtids if the iterator was not
  /// successfully initialized due to transactions having been purged.
  /// @retval kBinlogIteratorInitErrorLogClosed if the iterator was not
  /// successfully initialized because the log is closed.
  /// @retval BINLOG_INIT_ITERATOR_ERROR_UNDEF if the iterator was not
  /// initialized due to an undefined error.
  static DEFINE_METHOD(Binlog_iterator_service_init_status, init,
                       (my_h_binlog_storage_iterator * iterator,
                        const char *excluded_gtids_as_string));

  /// @brief Returns the next entry in the log files, end-of-file, or an error.
  ///
  /// @param[in] iterator the iterator context
  ///
  /// @param[in,out] buffer the buffer where to put the next value.
  ///
  /// @param[in] buffer_capacity the buffer size.
  ///
  /// @param[out] bytes_read the size of the value added to the buffer. If the
  /// value is larger than the buffer length, then an error is returned and this
  /// value is undefined.
  ///
  /// @note If there are new transactions being written to the log file after
  /// the iterator has been opened, get will return them if the iterator is not
  /// disposed before reaching that point in the log.
  ///
  /// @note If the log file rotates after the iterator has been opened, the
  /// iterator itself shall rotate to the new file as well.
  ///
  /// @note The iterator will stop once it reached the end of the most recent
  /// log file.
  ///
  /// @retval kBinlogIteratorGetOk if the operation succeeded.
  /// @retval kBinlogIteratorGetEndOfChanges if there are no more events to
  /// read. The iterator is still valid and if you call get again you either get
  /// the same error back or new entries that may have been added in the
  /// meantime.
  /// @retval kBinlogIteratorGetInsufficientBuffer if the buffer is insufficient
  /// to store the next entry. The iterator is still valid and you can call try
  /// to get another entry with a larger buffer.
  /// @retval kBinlogIteratorGetErrorClosed if the log is closed. The iterator
  /// is still valid, but retrying will get the same error back as long as the
  /// change log is closed.
  /// @retval kBinlogIteratorGetErrorInvalid if the iterator itself has become
  /// invalid. This can happen if the memory structures of the iterator are
  /// tampered with. The iterator state is invalid and therefore the behavior is
  /// undefined if one retries getting the next entry. May result on the same
  /// error again. This iterator should be de-initialized and a new one created.
  /// @retval kBinlogIteratorGetErrorUnspecified if there was an undefined error
  /// while reading the next entry. The iterator state is invalid and therefore
  /// the behavior is undefined if one retries getting the next entry. May
  /// result on the same error again. This iterator should be de-initialized and
  /// a new one created.
  static DEFINE_METHOD(Binlog_iterator_service_get_status, get,
                       (my_h_binlog_storage_iterator iterator,
                        unsigned char *buffer, uint64_t buffer_capacity,
                        uint64_t *bytes_read));

  /// @brief Destroys the iterator.
  /// @param[in] iterator the iterator to destroy.
  static DEFINE_METHOD(void, deinit, (my_h_binlog_storage_iterator iterator));

  /// @brief Gets details about the entry's storage in a JSON format.
  ///
  /// @param[in] iterator a valid iterator, i.e., one that has been initialized
  /// and not destroyed yet.
  ///
  /// @param[in,out] buffer The buffer to store the information in.
  ///
  /// @param[in,out] size As input, the size of the buffer provided. As output,
  /// the size of the data copied into the buffer.
  ///
  /// @return false on success, true otherwise.
  static DEFINE_BOOL_METHOD(get_storage_details,
                            (my_h_binlog_storage_iterator iterator,
                             char *buffer, uint64_t *size));

  /// Gets the size of the next block to be read.
  ///
  /// This member function can be used to check how larger the size of the
  /// buffer to read the next block/event shall be. Note though that if the next
  /// block ends up being skipped the obtained via this function is obsolete and
  /// a new get_next_entry_size may have to be executed to fetch the buffer
  /// needed for the next entry. Therefore it is a good practice for the caller
  /// to loop over a get function while it returns insufficient buffer and thus
  /// allocate a bigger buffer in that case.
  ///
  /// @param[in] iterator the iterator context.
  ///
  /// @param[out] size a pointer that stores the size if the function returns
  /// successfully.
  ///
  /// @return false on success, true otherwise.
  static DEFINE_BOOL_METHOD(get_next_entry_size,
                            (my_h_binlog_storage_iterator iterator,
                             uint64_t *size));
};

/* clang-format off */
BEGIN_SERVICE_IMPLEMENTATION(server,
                             binlog_storage_iterator)

FileStorage::init,
FileStorage::get,
FileStorage::deinit,
FileStorage::get_storage_details,
FileStorage::get_next_entry_size,

END_SERVICE_IMPLEMENTATION();
/* clang-format on */

}  // namespace binlog::services::iterator
#endif /*  BINLOG_ITERATOR_SERVICE_FILE_IMPLEMENTATION_H */
