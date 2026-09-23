// Copyright 2026 Google LLC

#include "vector0pk.h"

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "data0data.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0types.h"
#include "include/base64.h"
#include "mach0data.h"
#include "mem0mem.h"
#include "rem0cmp.h"
#include "rem0rec.h"
#include "row0sel.h"
#include "sql/item.h"
#include "sql/iterators/cloudsql_vector_iterators.h"
#include "sql/sql_const.h"
#include "univ.i"
#include "my_dbug.h"
#include "ut0core.h"
#include "ut0dbg.h"
#include "ut0log.h"
#include "ut0new.h"

namespace ib_vector {

/* ==========Serialization of primary key===========

KMeans stores docids in std::string format. We need to store primary key as
docid. This module implements a scheme to serialize a primary key to string.
We have not been very frugal in terms of bytes needed to serialize a primary
key. But we should work fine for most typical cases e.g.: where we have single
column BIGINT primary key, we'll need nine bytes to serialize.

Note that we can have maximum of MAX_REF_PARTS (16) columns in primary key and
the total prefix length of prefixes is limited to MAX_KEY_LENGTH (3072). We
create a byte array from the primary key and then convert it into a base64
string. The format of byte array is:
* First byte stores number of variable length columns
* 2 bytes each for prefix lengths of each variable length column in the same
order as they appear in the index
* prefix data of each field

Note we don't store the length of fixed length columns as we can always get this
from index definition.

Example:
PK = c1 BIGINT, c2(30) VARCHAR, c3(500) VARBINARY, c4 INT
Serialized PK:
byte offset  value
0             2
1 - 2         prefix len of c2
3 - 4         prefix len of c3
5 -           c1, c2, c3, c4 values concatenated */

/** Marker for fixed field lenghts. Note it is safe to use this value
as primary key can have a maximum prefix length of 3072 */
#define FIXED_FIELD_LEN_MARKER std::numeric_limits<uint16_t>::max()

/* Size in bytes needed to serialize primary key
@param[in]    prefix_len    total length of all prefixes in the primary key
@param[in]    var_fields    number of variable length fields
@return number of bytes neeed to serialize */
static uint16_t pk_serialized_size(uint16_t prefix_len, uint8_t var_fields) {
  /* First byte to store number of variable length fields. Two bytes each to
   * store the length of variable length fields and then the field data. */
  ut_ad(prefix_len <= MAX_KEY_LENGTH);
  ut_ad(var_fields <= MAX_REF_PARTS);
  return var_fields * 2 + prefix_len + 1;
}

/* Write first byte and lengths of variable length fields to the buffer
@param[in]        index       clustered index handle
@param[in]        var_fields  number of variable length fields
@param[in]        var_lens    array of holding lengths of variable length fields
@param[in, out]   buf         buffer to write the lengths to
@return pointing to first byte after the writes */
static byte* pk_write_lengths(const dict_index_t* index, uint8_t var_fields,
                              uint16_t* var_lens, byte* buf) {
  ut_ad(var_fields <= MAX_REF_PARTS);
  byte* ptr = buf;
  ulint n_fields = dict_index_get_n_unique_in_tree(index);
  mach_write_to_1(ptr, var_fields);
  ++ptr;
  for (ulint i = 0; i < n_fields; i++) {
    if (var_lens[i] == FIXED_FIELD_LEN_MARKER) {
      ut_ad(index->get_field(i)->fixed_len);
    } else {
      ut_ad(!index->get_field(i)->fixed_len);
      ut_ad(var_lens[i] <= MAX_KEY_LENGTH);
      mach_write_to_2(ptr, var_lens[i]);
      ptr += 2;
    }
  }
  return ptr;
}

/* Reads primary key from a physical record and serialize it to a buffer.
@param[in]  rec    physical record on index page
@param[in]  index  clustered index handle
@param[in]  heap   heap to allocate memory
@return [own] pointer to the buffer and its length */
std::pair<byte*, uint16_t> write_pk_from_rec(const rec_t* rec,
                                             const dict_index_t* index,
                                             mem_heap_t* heap) {
  const byte* lens;

  ut_a(index->is_clustered());
  ut_a(dict_table_is_comp(index->table));
  ut_ad(rec_get_status(rec) == REC_STATUS_ORDINARY);

  ulint n_fields = dict_index_get_n_unique_in_tree(index);
  ut_a(n_fields);

  {
    uint16_t ndf = 0;
    uint16_t n_null = 0;
    row_version_t row_version = MAX_ROW_VERSION;
    const byte* nulls = nullptr;
    /* We are really only interested in lens. This takes us to the location on
    physical record where lengths of variable length fields are stored. Note
    that rec_t pointer grows both ways. Forward is the data while meta data
    including the lengths of variable length fields are backwards.
    Why don't we care about null fields? Because all primary key fields must be
    non-null. We check for that below. */
    rec_init_null_and_len_comp(rec, index, &nulls, &lens, &n_null, ndf,
                               row_version);
  }

  ulint prefix_len = 0;
  uint8_t var_fields = 0;
  uint16_t var_lens[MAX_REF_PARTS];
  ut_a(n_fields <= MAX_REF_PARTS);

  /* read the lengths of fields 0..n */
  for (ulint i = 0; i < n_fields; i++) {
    const dict_field_t* field;
    const dict_col_t* col;

    field = index->get_field(i);
    col = field->col;

    ut_a(col->prtype & DATA_NOT_NULL);
    if (field->fixed_len) {
      prefix_len += field->fixed_len;
      /* We don't store the length of fixed length columns. */
      var_lens[i] = FIXED_FIELD_LEN_MARKER;
    } else {
      ulint len = *lens--;
      /* If the maximum length of the column is up
      to 255 bytes, the actual length is always
      stored in one byte. If the maximum length is
      more than 255 bytes, the actual length is
      stored in one byte for 0..127.  The length
      will be encoded in two bytes when it is 128 or
      more, or when the column is stored externally. */
      if (DATA_BIG_COL(col)) {
        if (len & 0x80) {
          /* 1exxxxxx */
          len &= 0x3f;
          len <<= 8;
          len |= *lens--;
          UNIV_PREFETCH_R(lens);
        }
      }
      ut_ad(len <= MAX_KEY_LENGTH);
      prefix_len += len;
      var_lens[i] = len;
      ++var_fields;
    }
  }

  uint16_t serialized_size = pk_serialized_size(prefix_len, var_fields);
  auto buf = static_cast<byte*>(mem_heap_alloc(heap, serialized_size));
  ut_a(buf);
  auto ptr = pk_write_lengths(index, var_fields, var_lens, buf);
  memcpy(ptr, rec, prefix_len);
  return std::pair<byte*, uint16_t>(buf, serialized_size);
}

/* Reads primary key from a data tuple and serialize it to a buffer.
@param[in]  tuple  tuple holding row data
@param[in]  index  clustered index handle
@param[in]  heap   heap to allocate memory
@return [own] pointer to the buffer and its length */
std::pair<byte*, uint16_t> write_pk_from_tuple(const dtuple_t* tuple,
                                               const dict_index_t* index,
                                               mem_heap_t* heap) {
  ut_a(index->is_clustered());
  ut_a(dict_table_is_comp(index->table));

  ulint n_fields = dict_index_get_n_unique_in_tree(index);
  ut_a(n_fields);

  ulint prefix_len = 0;
  uint8_t var_fields = 0;
  uint16_t var_lens[MAX_REF_PARTS];
  ut_a(n_fields <= MAX_REF_PARTS);

  /* read the lengths of fields 0..n */
  for (ulint i = 0; i < n_fields; i++) {
    const dict_field_t* dict_field = index->get_field(i);
    [[maybe_unused]] const dict_col_t* col = dict_field->col;
    ut_ad(col->prtype & DATA_NOT_NULL);

    const dfield_t* field = &tuple->fields[i];
    ut_ad(!dfield_is_ext(field));
    ut_ad(!dfield_is_null(field));

    if (dict_field->fixed_len) {
      ut_ad(dfield_get_len(field) == dict_field->fixed_len);
      prefix_len += dict_field->fixed_len;
      var_lens[i] = FIXED_FIELD_LEN_MARKER;
    } else {
      ulint len = dfield_get_len(field);
      prefix_len += len;
      var_lens[i] = len;
      ++var_fields;
    }
  }

  uint16_t serialized_size = pk_serialized_size(prefix_len, var_fields);
  auto buf = static_cast<byte*>(mem_heap_alloc(heap, serialized_size));
  ut_a(buf);
  auto ptr = pk_write_lengths(index, var_fields, var_lens, buf);
  /* Write fields 0..n */
  for (ulint i = 0; i < n_fields; i++) {
    const dfield_t* field = &tuple->fields[i];

    ulint len = dfield_get_len(field);
    ut_ad(ptr + len <= buf + serialized_size);
    memcpy(ptr, dfield_get_data(field), len);
    ptr += len;
  }
  return std::pair<byte*, uint16_t>(buf, serialized_size);
}

/* Reads field lengths from a serialized primary key buffer
@param[in]  buf         buffer holding serialized primary key
@param[in]  index       clustered index handle
@param[out] field_lens  length of each field is populated here
@return pointer to the fist byte after the lengths in the buffer. */
static byte* read_field_lens(byte* buf, const dict_index_t* index,
                             uint16_t* field_lens) {
  ut_a(index->is_clustered());
  ut_a(field_lens);

  ulint n_fields = dict_index_get_n_unique_in_tree(index);
  ut_a(n_fields);
  byte* ptr = buf;
  uint8_t var_fields = mach_read_from_1(ptr);
  ut_a(var_fields <= MAX_REF_PARTS);
  ++ptr;
  for (uint8_t i = 0; i < n_fields; i++) {
    const dict_field_t* dict_field = index->get_field(i);
    const ulint len = dict_field->fixed_len;
    ut_ad(dict_field->col->prtype & DATA_NOT_NULL);
    ut_a(len <= MAX_KEY_LENGTH);

    if (len) {
      field_lens[i] = len;
    } else {
      field_lens[i] = mach_read_from_2(ptr);
      ptr += 2;
    }
  }
  /* We must have processed all variable length fields. */
  ut_a(ptr - buf == var_fields * 2 + 1);

  return ptr;
}

/* Encode serialized priamry key to base64 string
@param[in]    buf   buffer where serialized PK resides
@param[in     sz    size of the serialized primary key
@return base64 repreasentation of the buffer */
static std::string encode_pk_to_string(byte* buf, size_t sz) {
  /* How many bytes we need in base64 string. Note this implementation adds
  \n and ending null byte. */
  size_t base64_len = base64_needed_encoded_length(sz);
  char* ptr = static_cast<char*>(ut::zalloc(base64_len));
  base64_encode(buf, sz, ptr);
  std::string ret(ptr, base64_len);
  ut::free(ptr);
  return ret;
}

/* Converts a primary key on a physical record to base64 string
@param[in]    rec   physical record on index page
@param[in]    index clustered index handle
@param[in]    heap   heap to allocate memory
@return base64 string representation of the primary key */
std::string pk_rec_to_string(const rec_t* rec, const dict_index_t* index,
                             mem_heap_t* heap) {
  ut_a(index->is_clustered());
  ulint n_fields = dict_index_get_n_unique_in_tree(index);
  ut_a(n_fields);

  /* ib::info() << "serializing rec: "; rec_print(stderr, rec, index); */

  auto pk_data = write_pk_from_rec(rec, index, heap);
  auto ret = encode_pk_to_string(pk_data.first, pk_data.second);
  return ret;
}

/* Converts a primary key in a data tuple to base64 string
@param[in]    dt    data tuple
@param[in]    index clustered index handle
@param[in]    heap   heap to allocate memory
@return base64 string representation of the primary key */
std::string pk_tuple_to_string(const dtuple_t* dt, const dict_index_t* index,
                               mem_heap_t* heap) {
  ut_a(index->is_clustered());
  ulint n_fields = dict_index_get_n_unique_in_tree(index);
  ut_a(n_fields);

  /* ib::info() << "serializing tuple: " << *dt; */
  auto pk_data = write_pk_from_tuple(dt, index, heap);
  auto ret = encode_pk_to_string(pk_data.first, pk_data.second);
  return ret;
}

/* Deserialize a base64 string representing a primary key to a data tuple
containing just the primary key
@param[in]    input   serialized base64 string
@param[in]    index   clustered index handle
@param[in]    heap    heap from which to allocate memory
@return[own] tuple representing the primary key. The caller owns the memory */
dtuple_t* pk_string_to_tuple(const std::string& input,
                             const dict_index_t* index, mem_heap_t* heap) {
  ut_a(index->is_clustered());
  ulint n_fields = dict_index_get_n_unique_in_tree(index);
  ut_a(n_fields);

  /* Excluding the nullptr at the end. */
  size_t total_size = base64_needed_decoded_length(input.size() - 1);
  byte* buf = static_cast<byte*>(mem_heap_alloc(heap, total_size));
  int ret = base64_decode(input.c_str(), input.size() - 1, buf, nullptr, 0);
  if (ret == -1) {
    ib::error() << "base64_decode failed to read serialized PK: " << input;
    return nullptr;
  }

  uint16_t field_lens[MAX_REF_PARTS];
  byte* ptr = read_field_lens(buf, index, field_lens);

  dtuple_t* tuple = dtuple_create(heap, n_fields);
  dict_index_copy_types(tuple, index, n_fields);
  ut_ad(dtuple_check_typed(tuple));

  /* Point the fields in the tuple to data in the buf. */
  for (uint8_t i = 0; i < n_fields; i++) {
    dfield_t* field = dtuple_get_nth_field(tuple, i);
    uint16_t len = field_lens[i];
    ut_ad(ptr + len <= buf + total_size);
    dfield_set_data(field, ptr, len);
    ptr += len;
  }
  /* ib::info() << "deserialized tuple: " << *tuple; */
  return tuple;
}

/* Prints a deserialized PK tuple.
@param[in]    dt      tuple containing primary key
@param[in]    index   handle to clustered index */
void print_pk_tuple(const dtuple_t* dt, const dict_index_t* index) {
  if (!dt) {
    ib::info() << "PK tuple is NULL";
    return;
  }

  /* If PK is just a single column with DATA_INT type we extract the value
  and print it nicely. For the rest, we just print the entire tuple. */
  if (dict_index_get_n_unique_in_tree(index) == 1 &&
      index->get_field(0)->col->mtype & DATA_INT) {
    bool is_unsigned = (index->get_field(0)->col->prtype & DATA_UNSIGNED) != 0;
    const dfield_t* field = dtuple_get_nth_field(dt, 0);
    uint64_t value =
        mach_read_int_type(static_cast<const byte*>(dfield_get_data(field)),
                           dfield_get_len(field), is_unsigned);
    ib::info() << "PK value: " << value;
  } else {
    ib::info() << "PK tuple: " << *dt;
  }
}

/** Builds a template for a field to be converted in MySQL format. For composite
primary keys we'll need one such template for each column. This essentially
defines the mapping between an InnoDB column and the corresponding MySQL
field.
@param[in]    index       InnoDB clustered index
@param[in]    table       MySQL table struct
@param[in]    field       MySQL column definition
@param[in]    col_index   Index of column in InnoDB
@param[out]   templ       template to be populated */
static void build_template_field(const dict_index_t* index, const TABLE* table,
                                 const Field* field, ulint col_index,
                                 mysql_row_templ_t* templ) {
  ut_ad(index->is_clustered());
  UNIV_MEM_INVALID(templ, sizeof *templ);

  const dict_col_t* col = index->table->get_col(col_index);

  /* No virtual columns in primary key */
  templ->is_virtual = false;

  /* Set in set_templ_icp(). */
  templ->icp_rec_field_no = ULINT_UNDEFINED;
  templ->mysql_mvidx_len = 0;
  templ->is_multi_val = false;

  /* Primary key is not nullable. */
  ut_a(!field->is_nullable());
  templ->mysql_null_bit_mask = 0;

  /* No BLOBs or big datatype in Primary key. */
  ut_ad(!DATA_LARGE_MTYPE(col->mtype));

  templ->col_no = col_index;
  auto clust_pos = dict_col_get_clust_pos(col, index);
  ut_a(clust_pos != ULINT_UNDEFINED);
  if (clust_pos >= dict_index_get_n_unique_in_tree(index)) {
    /* This column has a prefix in primary key. In InnoDB a PK column with a
    prefix will be stored in two locations. The clust_pos is the position of
    actual column. The prefix is stored as part of PK separately. Since we
    only store PK in vector index, at this point we have only access to the
    prefix. And MySQL is only interested in the prefix as well. */
    ut_a(col->has_prefix_phy_pos());
    clust_pos = col->get_prefix_phy_pos();
  }
  templ->clust_rec_field_no = clust_pos;
  templ->rec_field_no = clust_pos;

  /* Note that though we might be reading from the prefix, at MySQL layer
  there is only one representation of a column in record[0]. */
  templ->mysql_col_offset = static_cast<ulint>(field->offset(table->record[0]));
  templ->mysql_col_len = static_cast<ulint>(field->pack_length());

  templ->type = col->mtype;
  templ->mysql_type = static_cast<ulint>(field->type());

  if (templ->mysql_type == DATA_MYSQL_TRUE_VARCHAR) {
    templ->mysql_length_bytes = field->get_length_bytes();
  } else {
    templ->mysql_length_bytes = 0;
  }

  templ->charset = dtype_get_charset_coll(col->prtype);
  templ->mbminlen = col->get_mbminlen();
  templ->mbmaxlen = col->get_mbmaxlen();
  templ->is_unsigned = col->prtype & DATA_UNSIGNED;
}

bool convert_docid_to_pk(std::string_view docid, std::string& key) {
  size_t total_size = base64_needed_decoded_length(docid.size() - 1);
  byte* buf = static_cast<byte*>(ut::zalloc(total_size));
  int64 len = base64_decode(docid.data(), docid.size() - 1, buf, nullptr, 0);

  bool converted = false;
  if (len >= 0) {
    ut_a(len <= 3072);
    key.reserve(len);
    key.assign((const char*)buf, (size_t)len);
    converted = true;
  }
  ut::free(buf);

  return converted;
}

bool convert_search_results_to_mysql_format(
    const dict_index_t* index,
    const std::vector<std::pair<std::string, VectorDistT>>& vector_results,
    VectorSearchResults* results, bool decode_base64) {
  bool success = true;
  if (vector_results.empty()) {
    ib::error() << "vector_results is empty";
    success = false;
  }

  ut_a(index->is_clustered());
  ulint n_fields = dict_index_get_n_unique_in_tree(index);
  ut_a(n_fields);

  /* Allocate templates to be used. One for each field. */
  auto templates = static_cast<mysql_row_templ_t*>(
      ut::zalloc(sizeof(mysql_row_templ_t) * n_fields));

  auto mysql_table = results->table();
  auto n_columns = mysql_table->s->fields;

  /* Find out which columns are part of PK and initialize the template for
  each field. */
  ulint n_virtual_columns = 0;
  ulint n_pk_columns = 0;
  for (ulint i = 0; i < n_columns; i++) {
    if (innobase_is_v_fld(mysql_table->field[i])) {
      /* Virtual column at MySQL level. These columns (generated columns) are
      not present in physical InnoDB record. */
      n_virtual_columns++;
    } else if (dict_table_col_in_clustered_key(index->table,
                                               i - n_virtual_columns)) {
      /* It is a PK column. Initialize the template. */
      auto templ = templates + n_pk_columns;
      build_template_field(index, mysql_table, mysql_table->field[i],
                           i - n_virtual_columns, templ);
      ut_ad(templ->clust_rec_field_no < n_fields);
      n_pk_columns++;
    }
  }
  ut_a(n_pk_columns == n_fields);

  // If there are more results than expected, issue a warning and process only
  // the expected number of results.
  if (vector_results.size() > results->capacity()) {
    ib::warn() << "Similarity search returned"  << vector_results.size()
        << "results more than expected. Only processing "
        << results->capacity() << "results.";
  }

  for (const auto& [key_str, distance] : vector_results) {
    byte* buf = (byte*)key_str.data();
    if (decode_base64) {
      /* Deserialize base64 PK. */
      size_t total_size = base64_needed_decoded_length(key_str.size() - 1);
      buf = static_cast<byte*>(ut::zalloc(total_size));
      int ret =
          base64_decode(key_str.c_str(), key_str.size() - 1, buf, nullptr, 0);
      if (ret == -1) {
        ib::error() << "base64_decode failed to read serialized PK: "
                    << key_str;
        ut::free(buf);
        success = false;
        goto func_exit;
      }
    }
    uint16_t field_lens[MAX_REF_PARTS];
    byte* data = read_field_lens(buf, index, field_lens);

    // Exit if there is no more capacity.
    if (!results->has_capacity())
      break;

    /* Store each column value in mysql_record */
    auto mysql_rec = results->get_next_record();
    for (ulint i = 0; i < n_pk_columns; ++i) {
      auto templ = templates + i;
      /* The record in deserialized PK are not necessarily in the same order
      as columns in the table. This is because in InnoDB the PK columns are
      stored in the PK ordering sequence. These columns may be have been
      defined in a different order in the table.
      We need to move to the pointer to data to the right location in PK. */
      byte* ptr = data;
      for (ulint j = 0; j < templ->clust_rec_field_no; ++j) {
        ptr += field_lens[j];
      }
      ulint len = field_lens[templ->clust_rec_field_no];
      ut_ad(len <= templ->mysql_col_len);

      /* Do the conversion to MySQL format. */
      row_sel_field_store_in_mysql_format(
          mysql_rec + templ->mysql_col_offset, templ, index,
          templ->clust_rec_field_no, ptr, len, ULINT_UNDEFINED);
    }
    results->add_record(mysql_rec, distance);
    if (decode_base64) {
      ut::free(buf);
    }
  }

  // ignore the results size check in the test simulation, which purposely
  // uses the stacking feature of VectorSearchResults.
  DBUG_EXECUTE_IF(
      "vector_stream_query_simulate_sparse_partitions", {
        ut_ad(results->count() >= vector_results.size());
         goto func_exit;
        });

  ut_ad(results->count() == vector_results.size());

func_exit:
  ut::free(templates);
  return success;
}

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
    mem_heap_t* heap) {
  bool success = true;
  for (const auto& result : vector_results) {
    dtuple_t* tuple = pk_string_to_tuple(result.first, index, heap);
    tuples.push_back(std::make_pair(tuple, result.second));
    if (!tuple) {
      /* The serialized PK string is already logged to the error log. */
      success = false;
    }
  }
  return success;
}

uint16_t get_pk_fixed_len(const dict_index_t* index) {
  ut_a(index->is_clustered());
  ut_a(dict_table_is_comp(index->table));

  ulint n_fields = dict_index_get_n_unique_in_tree(index);
  ut_a(n_fields);
  ut_a(n_fields <= MAX_REF_PARTS);

  uint16_t fixed_len = 0;
  for (ulint i = 0; i < n_fields; i++) {
    const dict_field_t* dict_field = index->get_field(i);
    ut_ad(dict_field->col->prtype & DATA_NOT_NULL);
    if (dict_field->fixed_len) {
      fixed_len += dict_field->fixed_len;
    } else {
      /* If there are any variable length fields, return 0. */
      return 0;
    }
  }
  /* We have only fixed length fields. We add one extra byte for PK header. */
  return fixed_len + 1;
}
} /* namespace ib_vector */
