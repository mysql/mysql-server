// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_SERIALIZATION_ARCHIVE_H
#define MYSQL_SERIALIZATION_ARCHIVE_H

#include "mysql/serialization/field_wrapper.h"
#include "mysql/serialization/serialization_error.h"
#include "mysql/serialization/serialization_types.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Interface for archives (file archive, byte vector archive,
/// string archive etc.), available only to instances implementing Serializable
/// interface and Archive interface
/// @details Archive is responsible for primitive types byte-level formatting
/// and data storage. It also provides implementation of separation between
/// serializable types, serializable type fields, separate entries in
/// containers (repeated fields). Derived class should implement
/// all methods of provided interface (except methods for error handling).
/// @tparam Archive_derived_type Archive derived that is implementing Archive
/// interface (CRTP)
template <class Archive_derived_type>
class Archive {
 public:
  /// @brief Ingests argument into this archive
  /// @tparam Type type of the argument
  /// @param [in] arg Argument to read data from
  /// @return This archive reference
  /// @note To be implemented in Archive_derived_type
  template <typename Type>
  Archive_derived_type &operator<<(Type &&arg) {
    return get_derived()->operator<<(std::forward<Type>(arg));
  }

  /// @brief Reads argument from this archive
  /// @tparam Type type of the argument
  /// @param [in] arg Argument to store data into
  /// @return This archive reference
  /// @note To be implemented in Archive_derived_type
  template <typename Type>
  Archive_derived_type &operator>>(Type &&arg) {
    return get_derived()->operator>>(std::forward<Type>(arg));
  }

  /// @brief Function for the API user to access data in the archive
  /// @returns Archive data, defined in the Archive (e.g. pointer to bytes ...)
  /// @note To be implemented in Archive_derived_type
  decltype(auto) get_raw_data() { return get_derived()->get_raw_data(); }

  /// @brief Function returns size of serialized argument
  /// @tparam Type type of the argument
  /// @param [in] arg serialized argument
  /// @return size of serialized argument
  /// @note To be implemented in Archive_derived_type
  template <typename Type>
  static std::size_t get_size(Type &&arg) {
    return Archive_derived_type::template get_size(std::forward<Type>(arg));
  }

  /// @brief Returns archive size - size of data written to the archive
  /// @return archive size - size of data written to the archive
  /// @note To be implemented in Archive_derived_type
  inline std::size_t get_size_written() const {
    return Archive_derived_type::template get_size_written();
  }

  /// @brief Function returns maximum size of the Type
  /// @tparam Type serialized type
  /// @return maximum size of the Type in the stream
  /// @note To be implemented in Archive_derived_type
  template <typename Type>
  static constexpr std::size_t get_max_size() {
    return Archive_derived_type::template get_max_size<Type>();
  }

  // used to access the protected API of an archive (peek/seek_to/error methods)
  // by any implementation of a Serializer class
  template <class Serializer_derived_current_type, class Archive_current_type>
  friend class Serializer;

  // available for serializers

  /// @brief This method needs to define field separator to be inserted
  /// after the field, note that some formats won't contain separators
  /// Used mainly for text formatters
  /// @details Field is defined a a single field in object of serializable class
  // which is identified by an unique id within this object
  virtual void put_field_separator() {}

  /// @brief This method needs to define field entry separator to be inserted
  /// after the field entry, note that some formats won't contain separators
  /// Used mainly for text formatters
  /// @details Each field may have a several entries (e.g. vector)
  virtual void put_entry_separator() {}

  /// @brief This method needs to define level separator to be inserted
  /// after the level, note that some formats won't contain separators
  /// Used mainly for text formatters
  /// @details Each field that is an object of serializable class creates a new
  /// level
  virtual void put_level_separator() {}

  /// @brief This method needs to define how to process field separator during
  /// decoding. Used mainly for text formatters
  /// @details Field is defined a a single field in object of serializable class
  // which is identified by an unique id within this object
  virtual void process_field_separator() {}

  /// @brief This method needs to define how to process field entry separator
  /// during decoding. Used mainly for text formatters
  /// @details Each field may have a several entries (e.g. vector)
  virtual void process_entry_separator() {}

  /// @brief This method needs to define how to process level separator during
  /// decoding. Used mainly for text formatters
  /// @details Each field that is an object of serializable class creates a new
  /// level
  virtual void process_level_separator() {}

 protected:
  /// @brief Casts this to derived type
  /// @return Derived class pointer to this
  /// @note To be implemented in Archive_derived_type
  const Archive_derived_type *get_derived_const() {
    return static_cast<const Archive_derived_type *>(this);
  }

  /// @brief Casts this to derived type
  /// @return Derived class pointer to this
  /// @note To be implemented in Archive_derived_type
  Archive_derived_type *get_derived() {
    return static_cast<Archive_derived_type *>(this);
  }

  /// @brief This method decodes field id, without moving stream positions
  /// @returns Serialized field id
  /// @note To be implemented in Archive_derived_type
  Field_id_type peek_type_field_id() {
    return get_derived()->peek_type_field_id();
  }

  /// @brief Peeks selected field wrapper (reads data without updating
  /// read stream position)
  /// @note To be implemented in Archive_derived_type
  template <class Field_type>
  void peek(Field_type &&field) {
    return get_derived()->peek(std::forward<Field_type>(field));
  }

  /// @brief Moves the current read position to current position + size
  /// @param[in] num_pos Number of positions to be skipped
  /// @note To be implemented in Archive_derived_type
  void seek_to(std::size_t num_pos) { get_derived()->seek_to(num_pos); }

  /// @brief Gets current read pos
  /// @return Current read pos
  /// @note To be implemented in Archive_derived_type
  std::size_t get_read_pos() const {
    return get_derived_const()->get_read_pos();
  }

  bool is_error() const { return m_error.is_error(); }
  bool is_good() const { return !m_error.is_error(); }
  const Serialization_error &get_error() { return m_error; }
  void clear_error() { m_error = Serialization_error(); }

  /// @brief Destructor
  /// @note Cannot create an object of 'abstract' Archive type
  virtual ~Archive() = default;
  Serialization_error m_error;  ///< Holds information about error
};

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_ARCHIVE_H
