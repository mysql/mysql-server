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

#ifndef BINLOG_STORAGE_ITERATOR_SERVICE_HEADERS_H
#define BINLOG_STORAGE_ITERATOR_SERVICE_HEADERS_H
#include <cstdint>
#include <string>

#include <mysql/components/service.h>
#include <mysql/components/services/dynamic_privilege.h>
#include <mysql/plugin_audit_message_types.h>

DEFINE_SERVICE_HANDLE(my_h_binlog_storage_iterator);

enum Binlog_iterator_service_init_status : uint16_t {
  /// @brief Iterator was successfully initialized.
  kBinlogIteratorInitOk = 0,

  /// @brief Returned when the required GTIDs have already been purged and
  /// therefore the iterator cannot fetch the needed entries. Caller should
  /// still call the deinit function on the iterator.
  kBinlogIteratorIniErrorPurgedGtids,

  /// @brief Returned when the log is closed and therefore the iterator cannot
  /// get the change entries. Caller should still call the deinit function on
  /// the iterator.
  kBinlogIteratorInitErrorLogClosed,

  /// @brief Failure to initialize iterator due to undefined error. Caller
  /// should still call the deinit function on the iterator.
  kBinlogIteratorInitErrorUnspecified
};

/// @brief This enumeration lists the possible return values for the get
/// function
enum Binlog_iterator_service_get_status : uint16_t {
  /// @brief returned when the get operation succeeded.
  ///
  /// If the get operation succeeds, this also means that the iterator advances.
  kBinlogIteratorGetOk = 0,

  /// @brief returned when there are no more entries to get.
  ///
  /// The iterator remains open and you can call get on it again. If
  /// more content has been created in the meantime, get will get it
  /// for you.
  kBinlogIteratorGetEndOfChanges,

  /// @brief returned whenever the get was called with an insufficient buffer.
  ///
  /// The iterator does not advance and the caller can call get again with a
  /// larger buffer.
  kBinlogIteratorGetInsufficientBuffer,

  /// @brief Returned when there is an unrecoverable error and this iterator has
  /// been closed. The caller still needs to deinitialize the iterator.
  kBinlogIteratorGetErrorClosed,

  /// @brief returned whenever the iterator context is invalid.
  ///
  /// The iterator became invalid and therefore it cannot be used successfully
  /// from now onwards. I must still be de-initialized to release resources.
  kBinlogIteratorGetErrorInvalid,

  /// @brief returned whenever there was an unspecified error attempting to get
  /// the next entry.
  ///
  /// In case of an unspecified error, the caller can retry but there is no
  /// guarantee whether the retry is successful or not.
  kBinlogIteratorGetErrorUnspecified
};

BEGIN_SERVICE_DEFINITION(binlog_storage_iterator)

/// @brief Initializes the iterator.
///
/// my_h_binlog_storage_iterator is the service handle defined and is an opaque
/// pointer to the stream state.
///
/// @param[out] iterator where the iterator created will be stored.
///
/// @param[in] excluded_gtids_as_string The set of GTIDs to filter out from the
/// iterator.
///
/// @retval kBinlogIteratorInitOk @see
/// Binlog_iterator_service_init_status#kBinlogIteratorInitOk
/// @retval kBinlogIteratorIniErrorPurgedGtids @see
/// Binlog_iterator_service_init_status#kBinlogIteratorIniErrorPurgedGtids
/// @retval kBinlogIteratorInitErrorLogClosed @see
/// Binlog_iterator_service_init_status#kBinlogIteratorInitErrorLogClosed
/// @retval kBinlogIteratorInitErrorUnspecified @see
/// Binlog_iterator_service_init_status#kBinlogIteratorGetErrorUnspecified
DECLARE_METHOD(Binlog_iterator_service_init_status, init,
               (my_h_binlog_storage_iterator * iterator,
                const char *excluded_gtids_as_string));

/// @brief Shall get the next event in the iterator.
///
/// Gets the next event in the iterator. If there are no more events in the
/// iterator, it just returns immediately. Note that this function will also
/// advance the iterator if the operation is successful or the next entries are
/// to be skipped.
///
/// In case the error is kBinlogIteratorGetErrorInvalid or
/// kBinlogIteratorGetErrorUnspecified the iterator must be de initialized by
/// calling deinit. If the caller attempts to call get again, then the
/// same error is returned.
///
/// @param iterator the iterator reference to use use during the get operation.
///
/// @param[in,out] buffer the buffer to store the raw change stream bytes for
/// the next entry fetched from the given iterator.
///
/// @param[in] buffer_capacity the capacity of the buffer where the bytes are to
/// be stored.
///
/// @param[out] bytes_read the amount of bytes read and put into the buffer.
///
/// @retval kBinlogIteratorGetOk
/// Binlog_iterator_service_get_status#kBinlogIteratorGetOk
/// @retval kBinlogIteratorGetEndOfChanges
/// Binlog_iterator_service_get_status#kBinlogIteratorGetEndOfChanges
/// @retval kBinlogIteratorGetInsufficientBuffer
/// Binlog_iterator_service_get_status#kBinlogIteratorGetInsufficientBuffer
/// @retval kBinlogIteratorGetErrorClosed @see
/// Binlog_iterator_service_get_status#kBinlogIteratorGetErrorClosed
/// @retval kBinlogIteratorGetErrorInvalid @see
/// Binlog_iterator_service_get_status#kBinlogIteratorGetErrorInvalid
/// @retval kBinlogIteratorGetErrorUnspecified @see
/// Binlog_iterator_service_get_status#kBinlogIteratorGetErrorUnspecified
DECLARE_METHOD(Binlog_iterator_service_get_status, get,
               (my_h_binlog_storage_iterator iterator, unsigned char *buffer,
                uint64_t buffer_capacity, uint64_t *bytes_read));

/// @brief Destroys the iterator and releases resources associated with it.
///
/// @param[in] iterator the iterator to destroy.
DECLARE_METHOD(void, deinit, (my_h_binlog_storage_iterator iterator));

/// @brief Gets details about the entry's storage in a JSON format.
///
/// Allows the caller to get information about the underlying storage. Some
/// implementations may return a name and a position, for instance.
///
/// @param[in] iterator the iterator handle.
///
/// @param[in,out] buffer The buffer to store the information in.
///
/// @param[in,out] size As input, the size of the buffer provided. As output,
/// the size of the data copied into the buffer.
///
/// @return true if there was an error, false otherwise.
DECLARE_BOOL_METHOD(get_storage_details, (my_h_binlog_storage_iterator iterator,
                                          char *buffer, uint64_t *size));

/// @brief Gets the size of the next entry to fetch from the iterator.
///
/// Useful to drive reallocations.
///
/// @param[in] iterator the iterator being operated.
/// @param[out] size a pointer to store the size of the next entry to get.
///
/// @return false on success, true otherwise. Note that if the iterator has
/// reached the end of changes, then it means that an error shall be returned.
DECLARE_BOOL_METHOD(get_next_entry_size,
                    (my_h_binlog_storage_iterator iterator, uint64_t *size));

END_SERVICE_DEFINITION(binlog_storage_iterator)

#endif /* BINLOG_STORAGE_ITERATOR_SERVICE_HEADERS_H */
