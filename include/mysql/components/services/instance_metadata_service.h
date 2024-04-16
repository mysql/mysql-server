/* Copyright (c) 2024, Oracle and/or its affiliates.
 */
#pragma once

#include <mysql/components/service.h>

// clang-format off
/**
  @ingroup group_instance_metadata_service

  The Instance Metadata service provides methods to access the metadata
  of an active MySQL instance. Metadata entries can be retrieved using
  predefined or specified keys. The service also provides an iterator
  to enumerate multiple metadata entries.

  @code
  REQUIRES_SERVICE_PLACEHOLDER_AS(instance_metadata, imds_srv);

  BEGIN_COMPONENT_REQUIRES(<your component>)
    REQUIRES_SERVICE_AS(instance_metadata, imds_srv),
  END_COMPONENT_REQUIRES();

  // Get the max key and value sizes and total number of metadata entries

  size_t number_of_entries{0}, max_key_size{0}, max_value_size{0};
  imds_srv->get_metadata_info(&number_of_entries, &max_key_size, &max_value_size);

  // Create buffers to store the key and value

  std::string key_buffer(max_key_size+1, '\0');     // +1 for null terminator
  std::string value_buffer(max_value_size+1, '\0'); // +1 for null terminator
  
  size_t key_len{0}, value_len{0};    // String length of returned key and value

  //============================================================================
  // Example 1: Get a metadata entry with a complete key
  //============================================================================
  retval = imds_srv->get("regionInfo:regionIdentifier",
                         value_buffer.data(), value_buffer.size(), &value_len);

  //============================================================================
  // Example 2: Get multiple entries with a partial key
  //============================================================================

  // 2.1 Create an iterator to enumerate metadata entries

  my_h_imds_iterator iterator;
  int retval = imds_srv->create_iterator(&iterator);
  size_t key_len{0}, value_len{0};

  // 2.2 Get the first metadata entry for entries containing 'freeformTags'
  //     If the target key is nullptr or empty, then all metadata entries are considered a match

  int retval = imds_srv->get_first(iterator, "freeformTags",
                                   key_buffer.data(), key_buffer.size(), &key_len,
                                   value_buffer.data(), value_buffer.size(), &value_len);
  // Do something with key and value
  ...

  // 2.3 Get remaining matching entries, target key not required
  //     Handle entries that are not available via the service API

  while (retval == 0 || retval == -2) {
    retval = imds_srv->get_next(iterator,
                                key_buffer.data(), key_buffer.size(), &key_len,
                                value_buffer.data(), value_buffer.size(), &value_len);
    if (retval == 0) {
      // Do something with key and value
      ...
    } else if (retval == 1) {
      // Error: Check buffer sizes against key_len and value_len and try again
    } else if (retval == -1) {
      // No more matching entries
    } else if (retval == -2) {
      // Entry found but not available, get next entry
    }
  }

  // 2.4 Destroy the iterator
  imds_srv->destroy_iterator(&iterator);

  //============================================================================
  // Example 3: Get the DbSystem OCID
  //============================================================================

  // If optional key_buffer is provided, then the full key 'freeformTags:dbSystemId' is returned 

  retval = imds_srv->get_dbsystem_ocid(key_buffer.data(), key_buffer.size(), &key_len,
                                       value_buffer.data(), value_buffer.size(), &value_len);
  @endcode
*/
// clang-format on

DEFINE_SERVICE_HANDLE(my_h_imds_iterator);

BEGIN_SERVICE_DEFINITION(instance_metadata)

/**
  Get max key and value lengths and total number of metadata entries.
  This information can be used to determine the size of the buffers needed
  to retrieve metadata.

  @param[in,out] number_of_entries  Number of available metadata entries
  @param[in,out] max_key_len        Maximum key string length
  @param[in,out] max_value_len      Maximum value string length
  @return 0 success, 1 error

  Usage notes:
  - The number of metadata entries is the number of entries available through
    the service, including nested objects and arrays.
  - Metadata entries that are filtered by the service are not included, such as
    customer-defined entries or entries with very large values.
  - The max key and value lengths are given in bytes, not including
    the null terminator.
 */
DECLARE_METHOD(int, get_metadata_info,
               (size_t * number_of_entries, size_t *max_key_len,
                size_t *max_value_len));

/**
  Get a metadata value given a key.

  @param[in]     target_key         Key string, complete key, null-terminated
  @param[in,out] value_buffer       Buffer to store the value
  @param[in]     value_buffer_size  Size of the buffer in bytes
  @param[out]    value_len          String length of the value
  @return 0 success, 1 error, -1 not found, -2 found but not available, -2 found
  but not available

  Usage notes:
  - The key must be an exact match. Only a single value is returned.
  - If the target key is longer than 255 bytes, an error will be returned.
  - If the key is found, value_len will be set to the string length of the
  value.
  - If the key is not found, then the function will return -1.
  - If the entry is found but is not available via the service API, then the
    function will return -2.
  - If value_buffer_size is too small, the function will return an error.
  - If value_buffer is null, the function will return success, but value_len
    will be set to the string length of the value.
 */
DECLARE_METHOD(int, get,
               (const char *key, char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Create a new metadata iterator.

  @param[in,out]  iterator  Pointer to metadata iterator
  @return 0 success, 1 error

  Usage notes:
  - An iterator is required to enumerate multiple metadata entries.
  - The iterator must be initialized with get_first() before calling get_next().
  - The iterator must be destroyed with destroy_iterator() when not needed.
 */
DECLARE_METHOD(int, create_iterator, (my_h_imds_iterator * iterator));

/**
  Destroy a metadata iterator.

  @param[in,out]  iterator  Pointer to metadata iterator
  @return 0 success, 1 error

  Usage notes:
  - An iterator is required to enumerate multiple metadata entries.
  - The iterator must be created with create_iterator() and initialized with
    get_first() before calling get_next().
  - The iterator must be destroyed with destroy_iterator() when not needed.
  - The iterator is not valid after being destroyed.
 */
DECLARE_METHOD(int, destroy_iterator, (my_h_imds_iterator * iterator));

/**
  Get the first metadata entry matching a key.

  @param[in]     iterator          Metadata iterator pointer
  @param[in]     target_key        Target key string, null-terminated, complete,
                                   partial or nullptr, max length 255 bytes
  @param[in,out] key_buffer        Buffer to store the null-terminated key
  @param[in]     key_buffer_size   Size of the key buffer in bytes
  @param[out]    key_len           String length of the key
  @param[in,out] value_buffer      Buffer to store the null-terminated value
  @param[in]     value_buffer_size Size of the value buffer in bytes
  @param[out]    value_len         String length of the value
  @return 0 success, 1 error, -1 not found, -2 found but not available

  Usage notes:
  - The target key can be complete, partial or nullptr.
  - If the target key is longer than 255 bytes, an error will be returned.
  - If a matching metadata entry is found, the key and value will be written to
    the buffers as null-terminated strings.
  - If a matching entry is not found, the function will return -1.
  - If the entry is found but is not available via the service API, then the
    function will return -2.
  - If target_key is nullptr or empty, then all metadata entries will be
    considered a match.
  - If a matching key is found, then key_len and value_len will be set.
  - If either buffer is too small, then the function will return an error,
    but the key and value lengths will be set.
  - If either buffer is null, then the function will return success and the key
    and value lengths will be set.

  - This function initializes the iterator used by get_next().
  - The iterator retains the position of the current metadata entry.
  - The iterator can be reset by calling get_first() again.
 */
DECLARE_METHOD(int, get_first,
               (my_h_imds_iterator iterator, const char *target_key,
                char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the next metadata entry.

  @param[in]     iterator          Metadata iterator initialized by get_first()
  @param[in,out] key_buffer        Buffer to store the null-terminated key
  @param[in]     key_buffer_size   Size of the key buffer in bytes
  @param[out]    key_len           String length of the key
  @param[in,out] value_buffer      Buffer to store the null-terminated value
  @param[in]     value_buffer_size Size of the value buffer in bytes
  @param[out]    value_len         String length of the value
  @return 0 success, 1 error, -1 not found, -2 found but not available

  - The iterator is required and must be created with create_iterator()
    and then initialized by get_first().
  - If a matching metadata entry is found, the key and value will be written to
    the buffers as null-terminated strings.
  - If there are no more matching entries, then the function will return -1.
  - If the entry is found but is not available via the service API, then the
    function will return -2.
  - If either buffer is too small, then the function will return an error,
    but the key and value lengths will be set.
  - If either buffer is null, then the function will return success and the key
    and value lengths will be set.
 */
DECLARE_METHOD(int, get_next,
               (my_h_imds_iterator iterator, char *key_buffer,
                size_t key_buffer_size, size_t *key_len, char *value_buffer,
                size_t value_buffer_size, size_t *value_len));

/**
  Get the display name of the instance.

  @param[in,out] key_buffer        Buffer to store the null-terminated key
  @param[in]     key_buffer_size   Size of the key buffer in bytes
  @param[out]    key_len           String length of the key
  @param[in,out] value_buffer      Buffer to store the null-terminated value
  @param[in]     value_buffer_size Size of the value buffer in bytes
  @param[out]    value_len         String length of the value
  @return 0 success, 1 error, -1 not found, -2 found but not available

  Usage notes:
  - This function returns the key and value of the 'displayName' metadata entry.
  - If either buffer is too small, the function will return an error but the key
    and value lengths will be set.
  - If either buffer is null, the function will return success and the key and
    value lengths will be set.
  - If the entry is not found, the function will return -1.

  Sample display name metadata entry:
    key = "displayName", value = "mysqlinstance20231211234353"

 */
DECLARE_METHOD(int, get_display_name,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/** The following methods share the same function prototype. **/

/**
  Get the hostname of the instance.

  @sa get_display_name

  Sample hostname metadata entry:
    key = "hostname", value = "jt8guunjl0fvjzho"
*/
DECLARE_METHOD(int, get_hostname,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the OCID of the host where the instance is running.

  @sa get_display_name

  Sample host OCID metadata entry:
    key = "id", value = "ocid1.instance.oc1.iad.anuwcljts6i..."
*/
DECLARE_METHOD(int, get_host_ocid,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the realm key of the instance.

  @sa get_display_name

  Sample realm key metadata entry:
    key = "regionInfo:realmKey", value = "ocid1.realm.oc1..aaaaaaa..."
*/
DECLARE_METHOD(int, get_realm_key,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the region key of the instance.

  @sa get_display_name

  Sample region key metadata entry:
    key = "regionInfo:regionKey", value = "ocid1.region.oc1..aaaaaaa..."
*/
DECLARE_METHOD(int, get_region_key,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the region identifier of the instance.

  @sa get_display_name

  Sample region identifier metadata entry:
    key = "regionInfo:regionIdentifier", value = "us-ashburn-1"
*/
DECLARE_METHOD(int, get_region_id,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the DB system OCID of the instance.

  @sa get_display_name

  Sample DbSystem OCID metadata entry:
    key = "freeformTags:dbSystemId", value = "ocid1.mysqldbsystem..."
*/
DECLARE_METHOD(int, get_dbsystem_ocid,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the MySQL instance OCID of the instance.

  @sa get_display_name

  Sample MySQL instance OCID metadata entry:
    key = "freeformTags:mysqlInstanceId", value = "ocid1.mysqlinstance..."
*/
DECLARE_METHOD(int, get_mysql_instance_ocid,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the shape name.

  @sa get_display_name

  Sample shape details metadata entry:
    key = "shape", value = "VM.Standard.E4.Flex"
*/
DECLARE_METHOD(int, get_shape_name,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the shape details of the instance.

  @sa get_display_name

  Sample shape details metadata entry:
    key = "freeformTags:mysqlShapeDetails", value =
  "{'name':'MySQL.VM.Standard.E4.1.16GB','ocpu':1,...}"
*/
DECLARE_METHOD(int, get_shape_details,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the memory in GBs of the instance.

  @sa get_display_name

  Sample memory in GBs metadata entry:
    key = "shapeConfig:memoryInGbs", value = "16.0"
*/
DECLARE_METHOD(int, get_memory_in_gbs,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the number of OCPUs of the instance.

  @sa get_display_name

  Sample number of OCPUs metadata entry:
    key = "shapeConfig:ocpus", value = "4.0"
*/
DECLARE_METHOD(int, get_ocpus,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the state of the instance.

  @sa get_display_name

  Sample state metadata entry:
    key = "state", value = "Running"
*/
DECLARE_METHOD(int, get_state,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));

/**
  Get the time when the instance was created.

  @sa get_display_name

  Sample timeCreated metadata entry:
    key = "timeCreated", value = "1708340966316"
*/
DECLARE_METHOD(int, get_time_created,
               (char *key_buffer, size_t key_buffer_size, size_t *key_len,
                char *value_buffer, size_t value_buffer_size,
                size_t *value_len));
END_SERVICE_DEFINITION(instance_metadata)
