// Copyright 2026 Google LLC

#ifndef _INCLUDE_VECTOR0PK_H_
#define _INCLUDE_VECTOR0PK_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "vector0types.h"

#include "data0data.h"
#include "db0err.h"
#include "dict0mem.h"
#include "mem0mem.h"
#include "rem0types.h"
#include "univ.i"

namespace ib_vector {
/* Reads primary key from a physical record and serialize it to a buffer.
@param[in]  rec    physical record on index page
@param[in]  index  clustered index handle
@param[in]  heap   heap to allocate memory
@return [own] pointer to the buffer and its length */
std::pair<byte*, uint16_t> write_pk_from_rec(const rec_t* rec,
                                             const dict_index_t* index,
                                             mem_heap_t* heap);
/* Reads primary key from a data tuple and serialize it to a buffer.
@param[in]  tuple  tuple holding row data
@param[in]  index  clustered index handle
@param[in]  heap   heap to allocate memory
@return [own] pointer to the buffer and its length */
std::pair<byte*, uint16_t> write_pk_from_tuple(const dtuple_t* tuple,
                                               const dict_index_t* index,
                                               mem_heap_t* heap);

/* Converts a primary key on a physical record to base64 string
@param[in]    rec   physical record on index page
@param[in]    index clustered index handle
@param[in]    heap   heap to allocate memory
@return base64 string representation of the primary key */
std::string pk_rec_to_string(const rec_t* rec, const dict_index_t* index,
                             mem_heap_t* heap);

/* Converts a primary key in a data tuple to base64 string
@param[in]    dt    data tuple
@param[in]    index clustered index handle
@param[in]    heap   heap to allocate memory
@return base64 string representation of the primary key */
std::string pk_tuple_to_string(const dtuple_t* dt, const dict_index_t* index,
                               mem_heap_t* heap);

/* Deserialize a base64 string representing a primary key to a data tuple
containing just the primary key
@param[in]    input   serialized base64 string
@param[in]    index   clustered index handle
@param[in]    heap    heap from which to allocate memory
@return[own] tuple representing the primary key. The caller owns the memory */
dtuple_t* pk_string_to_tuple(const std::string& input,
                             const dict_index_t* index, mem_heap_t* heap);
/* Prints a deserialized PK tuple.
@param[in]    dt      tuple containing primary key
@param[in]    index   handle to clustered index */
void print_pk_tuple(const dtuple_t* dt, const dict_index_t* index);

/* Converts the search results obtained from vector to PK tuples.
@param[in]    vector_results  search results from vector. PK strings
@param[out]   tuples          PK tuples formed from the search results
@param[in]    index           clustered index handle
@param[in]    heap            heap to allocate memory for the tuples
@return false if any of the conversion fails. true otherwise */
bool get_pk_tuples(
    const std::vector<std::pair<std::string, VectorDistT>>& vector_results,
    std::vector<std::pair<dtuple_t*, VectorDistT>>& tuples,
    const dict_index_t* index,
    mem_heap_t* heap);

/** Converts the docid (base64'ed PK) used for in-memory index to PK
@param[in]    docid           base64 encoded PK
@param[out]   key             PK in its native format */
bool convert_docid_to_pk(std::string_view docid, std::string& key);

/* Converts the search results obtained from vector to MySQL format.
@param[in]    index            clustered index handle
@param[in]    vector_results   search results from vector. PK strings
@param[out]   results          MySQL format search results
@param[in]    decode_base64    whether to decode base64 string to PK tuple
@return false if any of the conversion fails. true otherwise */
bool convert_search_results_to_mysql_format(
    const dict_index_t* index,
    const std::vector<std::pair<std::string, VectorDistT>>& vector_results,
    VectorSearchResults* results,
    bool decode_base64 = true);

/** Returns the fixed length of the primary key iff entire PK consists of only
fixed length fields.
@param[in]    index   clustered index handle
@return the fixed length of the primary key or zero if the key has variable
length fields. */
uint16_t get_pk_fixed_len(const dict_index_t* index);
} /* namespace ib_vector */
#endif /* _INCLUDE_VECTOR0PK_H_ */
