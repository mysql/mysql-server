/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifdef IN_DOXYGEN
#include <mysql/components/service_implementation.h>
#endif

/**
  Creates iterator that iterates through all metadata for object pointed by the
  specified iterator. If successful it leaves read lock on the registry until
  the iterator is released.

  @param iterator A iterator that points to object to get the metadata iterator
    for.
  @param [out] out_iterator Pointer to metadata iterator handle.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(REGISTRY_IMP::metadata_iterator_create,
                   (OBJECT_ITERATOR iterator,
                    METADATA_ITERATOR *out_iterator)) {
  try {
    *out_iterator = nullptr;

    REGISTRY_TYPE::const_iterator &iter =
        reinterpret_cast<ITERATOR_TYPE *>(iterator)->m_it;
    if (iter == REGISTRY.cend()) {
      return true;
    }
    /* This read lock on the whole registry will be held, until the iterator is
      released. */
    *out_iterator= new METADATA_ITERATOR_TYPE{
      iter->second->create_iterator(),
      minimal_chassis::rwlock_scoped_lock(&LOCK, false,
        __FILE__, __LINE__) };
    return false;
  } catch (...) {
  }
  return true;
}

/**
  Releases the specified iterator. Releases read lock on the registry.

  @param iterator Metadata iterator handle.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_METHOD(void, REGISTRY_IMP::metadata_iterator_release,
              (METADATA_ITERATOR iterator)) {
  METADATA_ITERATOR_TYPE *iter =
      reinterpret_cast<METADATA_ITERATOR_TYPE *>(iterator);

  if (!iter) return;

  delete iter;
}

/**
  Gets the key and value of the metadata pointed to by the specified iterator.

  @param iterator Metadata iterator handle.
  @param [out] out_name A pointer to the string with the key to set the result
    pointer to.
  @param [out] out_value A pointer to the string with the metadata value to set
    the result pointer to.
  @return Status of performed operation
  @retval false success
  @retval true Failure, may be caused when called on the iterator that went
    through all values already.
*/
DEFINE_BOOL_METHOD(REGISTRY_IMP::metadata_iterator_get,
                   (METADATA_ITERATOR iterator, const char **out_name,
                    const char **out_value)) {
  try {
    *out_name = nullptr;
    *out_value = nullptr;

    if (!iterator) return true;

    my_metadata::const_iterator &iter =
        reinterpret_cast<METADATA_ITERATOR_TYPE *>(iterator)->m_it;

    if (!iter.is_at_end()) {
      *out_name = iter->first.c_str();
      *out_value = iter->second.c_str();

      return false;
    }
  } catch (...) {
  }
  return true;
}

/**
  Advances specified iterator to next element. Will fail if it reaches
  one-past-last element.

  @param iterator Metadata iterator handle.
  @return Status of performed operation
  @retval false success
    @retval true Failure, may be caused when called on iterator that was on last
      element.
*/
DEFINE_BOOL_METHOD(REGISTRY_IMP::metadata_iterator_next,
                   (METADATA_ITERATOR iterator)) {
  try {
    if (!iterator) return true;

    my_metadata::const_iterator &iter =
        reinterpret_cast<METADATA_ITERATOR_TYPE *>(iterator)->m_it;

    if (!iter.is_at_end()) {
      ++iter;
      return iter.is_at_end();
    }
  } catch (...) {
  }
  return true;
}

/**
  Checks if specified iterator is valid, i.e. have not reached one-past-last
  element.

  @param iterator Metadata iterator handle.
  @return Validity of iterator
  @retval false Valid
  @retval true Invalid or reached one-past-last element.
*/
DEFINE_BOOL_METHOD(REGISTRY_IMP::metadata_iterator_is_valid,
                   (METADATA_ITERATOR iterator)) {
  try {
    if (!iterator) return true;

    my_metadata::const_iterator &iter =
        reinterpret_cast<METADATA_ITERATOR_TYPE *>(iterator)->m_it;

    return iter.is_at_end();
  } catch (...) {
  }
  return true;
}

/**
  Gets the key and value of the metadata pointed to by the specified object
  iterator.

  @param iterator A iterator that points to object to get the metadata iterator
  for.
  @param name A pointer to the string with the key to set the result
    pointer to.
  @param [out] out_value A pointer to the string with the metadata value to set
    the result pointer to.
  @return Status of performed operation
  @retval false success
  @retval true Failure, may be caused when called on the iterator that went
    through all values already.
*/
DEFINE_BOOL_METHOD(REGISTRY_IMP::metadata_get_value,
                   (OBJECT_ITERATOR iterator, const char *name,
                    const char **out_value)) {
  try {
    *out_value = nullptr;

    if (iterator == nullptr) return true;

    REGISTRY_TYPE::const_iterator &iter =
        reinterpret_cast<ITERATOR_TYPE *>(iterator)->m_it;
    if (iter == REGISTRY.cend()) return true;

    return iter->second->get_value(name, out_value);
  }
  catch (...)
  {
  }
  return true;
}

#undef REGISTRY
#undef REGISTRY_TYPE
#undef LOCK
#undef ITERATOR_TYPE
#undef METADATA_ITERATOR_TYPE
#undef OBJECT_ITERATOR
#undef METADATA_ITERATOR
