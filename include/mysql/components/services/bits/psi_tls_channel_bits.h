/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef COMPONENT_SERVICES_BITS_PSI_TLS_CHANNEL_BITS_H
#define COMPONENT_SERVICES_BITS_PSI_TLS_CHANNEL_BITS_H

/**
  @file mysql/components/services/bits/psi_tls_channel_bits.h
  Instrumentation helpers for TLS channels.
  This header file provides necessary declarations to instrument
  TLS context information.
*/

/**
  @defgroup psi_abi_tls_channel TLS Channel Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

/**
  @def PSI_TLS_CHANNEL_VERSION_1
  Performance Schema TLS Channel Interface number for version 1.
  This version is supported.
*/
#define PSI_TLS_CHANNEL_VERSION_1 1

/**
  @def PSI_CURRENT_TLS_CHANNEL_VERSION
  Performance Schema TLS Channel Interface number for the most recent version.
  The most current version is @c PSI_TLS_CHANNEL_VERSION_1
*/
#define PSI_CURRENT_TLS_CHANNEL_VERSION 1

const size_t MAX_CHANNEL_NAME_SIZE = 64;
const size_t MAX_PROPERTY_NAME_SIZE = 64;
const size_t MAX_PROPERTY_VALUE_SIZE = 512;

/** TLS property */
class TLS_channel_property {
 public:
  /** Channel name */
  char channel_name[MAX_CHANNEL_NAME_SIZE + 1];
  /** Property name */
  char property_name[MAX_PROPERTY_NAME_SIZE + 1];
  /** Property value */
  char property_value[MAX_PROPERTY_VALUE_SIZE + 1];
};

/** Iterator object */
typedef struct property_iterator_imp *property_iterator;

/**
  Initialize TLS property iterator

  @param [out] iterator TLS Property iterator object

  @returns Result of iterator creation
    @retval true  Success
    @retval false Failure
*/
typedef bool (*init_tls_property_iterator_t)(property_iterator *iterator);

/**
  De-initialize TLS property iterator

  @param [in] iterator TLS Property iterator object
*/
typedef void (*deinit_tls_property_iterator_t)(property_iterator iterator);

/**
  Get one TLS property information from current iterator position

  @param [in]  iterator TLS Property iterator object
  @param [out] property Property details

  @returns status of fetch operation
    @retval true  Success
    @retval false Failure
*/
typedef bool (*get_tls_property_t)(property_iterator iterator,
                                   TLS_channel_property *property);

/**
  Move TLS Property iterator to next position

  @param [in] iterator TLS Property iterator object

  @returns Status of operation
    @retval true  Success
    @retval false Iterator reached at the end
*/
typedef bool (*next_tls_property_t)(property_iterator iterator);

/** Property iterator callbacks */
struct TLS_channel_property_iterator {
  init_tls_property_iterator_t init_tls_property_iterator;
  deinit_tls_property_iterator_t deinit_tls_property_iterator;
  get_tls_property_t get_tls_property;
  next_tls_property_t next_tls_property;
};

typedef struct TLS_channel_property_iterator TLS_channel_property_iterator;

/**
  TLS channel information registration API
*/
typedef void (*register_tls_channel_v1_t)(
    TLS_channel_property_iterator *provider);

/**
  TLS channel information un registration API
*/
typedef void (*unregister_tls_channel_v1_t)(
    TLS_channel_property_iterator *provider);

/** @} (end of group psi_abi_tls_channel) */
#endif  // !COMPONENT_SERVICES_BITS_PSI_TLS_CHANNEL_BITS_H
