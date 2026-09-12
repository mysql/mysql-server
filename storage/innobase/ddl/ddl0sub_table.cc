#include "ddl0sub_table.h"

#include <cstddef>
#include <cstdlib>
#include <ctime>

#include "data0data.h"
#include "data0type.h"
#include "db0err.h"
#include "ddl0ddl.h"
#include "ddl0impl-builder.h"
#include "ddl0impl-loader.h"
#include "ddl0sub_table.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "ha_prototypes.h"
#include "include/my_compiler.h"
#include "mach0data.h"
#include "rem0rec.h"
#include "vector0build.h"
#include "vector0pk.h"
// TODO: optimize the abstraction here
//       1. ideally we should use <vector0table.h> and be agnostic about the
//          table shape, i.e., index persistence implementation details
//       2. or we should specialize it to <kmeans0table.h> and have other
//          versions of dd0sub_table.h to fit other index types
#include "kmeans0table.h"
#include "trx0types.h"
#include "trx0undo.h"
#include "univ.i"
#include "ut0dbg.h"
#include "ut0ut.h"

namespace ddl {

using ib_vector::KMeansIndex;
using ib_vector::KMeansTable;

VectorIndexBuilder::VectorIndexBuilder(
    ddl::Context &ctx, Loader &loader, size_t i,
    VectorIndexBuilder *clust_builder) noexcept
    : Builder::Builder(ctx, loader, i),
      m_base_index(ctx.old_table()->first_index()),
      m_clust_builder(clust_builder) {
  /* Invariants */
  ut_a(m_ctx.m_vec_index_build);
  ut_a(dict_table_is_comp(m_ctx.old_table()));
  ut_a(dict_table_is_comp(m_ctx.new_table()));
  ut_a(m_ctx.old_table() != m_ctx.new_table());
  ut_a(!is_fts_index());
  ut_a(!is_spatial_index());

  m_tmpdir = thd_innodb_tmpdir(m_ctx.thd());
  m_sort_index = m_index;

  ut_a(m_index->is_clustered() || m_clust_builder != nullptr);

  /* Validate that the base table has a ready vector index */
  ut_a(m_ctx.old_table());
  auto vec_index =
      ib_vector::dict_table_get_vector_index_ptr<KMeansIndex>(m_ctx.old_table());
  ut_a(vec_index);

  /* Write the transaction id and rollback pointer for all threads to use. */
  auto trx_id = m_ctx.m_trx->id;
  ut_a(trx_id != 0);
  mach_write_to_6(m_trx_id_buf, trx_id);

  roll_ptr_t roll_ptr = trx_undo_build_roll_ptr(true, 0, 0, 0);
  mach_write_to_7(m_roll_ptr_buf, roll_ptr);
}

dberr_t VectorIndexBuilder::build_sec_index_tuple(Copy_ctx &ctx) noexcept {
  auto thread_ctx = m_thread_ctxs[ctx.m_thread_id];
  auto key_buffer = thread_ctx->m_key_buffer;
  dfield_t *fields;
  auto field = fields = key_buffer->alloc(ctx.m_n_fields);
  key_buffer->m_dtuples.push_back(fields);

  /* The only secondary index on the sub table is the base_pk index. */
  using FLD = KMeansTable::SecIndexFields;
  ut_a(ctx.m_n_fields == FLD::SEC_NUM_FIELDS);

  ut_a(!m_index->is_clustered());
  ut_a(m_clust_builder != nullptr);

  /* We don't need to consult the base table row that we are pointing to.
  Our secondary index has two fields. base_pk and partition_id in that order.
  Both of these values are already computed and stored in the clustered index
  builder's thread_ctx. */
  auto [pk_val, pk_len] = m_clust_builder->get_pk_buf(ctx.m_thread_id);
  auto partition_buf = m_clust_builder->get_partition_buf(ctx.m_thread_id);

  ut_a(pk_val != nullptr);
  ut_a(pk_len != UNIV_SQL_NULL);
  ut_a(partition_buf != nullptr);

  for (size_t i = 0; i < ctx.m_n_fields; ++i, ++field) {
    field->reset();
    m_index->fields[i].col->copy_type(&field->type);
    switch (i) {
      case FLD::SEC_BASE_PK:
        /* base_pk is a binary field. */
        ut_ad(dfield_get_type(field)->mtype == DATA_BINARY);
        dfield_set_data(field, pk_val, pk_len);
        ctx.m_data_size += pk_len;
        if (pk_len < 128 || !DATA_BIG_COL(m_index->table->get_col(
                                KMeansTable::Cols::BASE_PK))) {
          ++ctx.m_extra_size;
        } else {
          ctx.m_extra_size += 2;
        }
        break;
      case FLD::SEC_PARTITION_ID:
        /* partition_id is an integer field. */
        ut_ad(dfield_get_type(field)->mtype == DATA_INT);
        dfield_set_data(field, partition_buf,
                        ib_vector::KMeansTable::partition_id_len);
        ctx.m_data_size += ib_vector::KMeansTable::partition_id_len;
        break;
    }
  }
  return DB_SUCCESS;
}

dberr_t VectorIndexBuilder::build_clust_index_tuple(Copy_ctx &ctx) noexcept {
  auto thread_ctx =
      static_cast<SubTableThreadCtx *>(m_thread_ctxs[ctx.m_thread_id]);
  auto key_buffer = thread_ctx->m_key_buffer;
  dfield_t *fields;
  auto field = fields = key_buffer->alloc(ctx.m_n_fields);
  key_buffer->m_dtuples.push_back(fields);

  /* Reset the heap used to store the base_pk and partition_id. We go through
  builder of each index in a loop. The idea is we populate the base_pk and
  partition_id in each iteration. The secondary index builder uses these values
  and in next iteration, we clear the heap and populate the values again. */
  thread_ctx->reset_sub_table_heap();

  /* Extract PK from the base table row */
  auto [pk_val, pk_len] = ib_vector::write_pk_from_rec(
      ctx.m_row.m_rec, m_base_index, thread_ctx->m_sub_table_heap.get());
  ut_a(pk_len != 0);
  thread_ctx->m_pk_buf.first = pk_val;
  thread_ctx->m_pk_buf.second = pk_len;

  /* Get embedding value from the vector column. */
  auto vec_col_pos = ib_vector::get_vector_col_pos(m_base_index);
  bool vec_col_ext =
      rec_offs_nth_extern(m_base_index, ctx.m_row.m_offsets, vec_col_pos);
  auto [vec_data, vec_len] = ib_vector::get_embedding_from_rec(
      m_base_index, ctx.m_row.m_rec, ctx.m_row.m_offsets, vec_col_pos,
      thread_ctx->m_sub_table_heap.get());

  auto vec_index =
      ib_vector::dict_table_get_vector_index_ptr<KMeansIndex>(m_ctx.old_table());


  thread_ctx->m_partition_buf = KMeansTable::get_partition_id(
      vec_index.get(), vec_data, vec_len, thread_ctx->m_sub_table_heap.get());

  /* Base table can have arbitrary number of fields. We are only interested
  in the PK and embedding. sub_table has five fields which we need to
  populate. */
  using FLD = ib_vector::KMeansTable::ClustIndexFields;
  ut_a(ctx.m_n_fields == FLD::CLUST_NUM_FIELDS);
  for (size_t i = 0; i < ctx.m_n_fields; ++i, ++field) {
    field->reset();
    m_index->fields[i].col->copy_type(&field->type);
    switch (i) {
      case FLD::CLUST_PARTITION_ID:
        dfield_set_data(field, thread_ctx->m_partition_buf,
                        ib_vector::KMeansTable::partition_id_len);
        ctx.m_data_size += ib_vector::KMeansTable::partition_id_len;
        break;
      case FLD::CLUST_BASE_PK:
        dfield_set_data(field, pk_val, pk_len);
        ctx.m_data_size += pk_len;
        if (pk_len < 128 || !DATA_BIG_COL(m_index->table->get_col(
                                ib_vector::KMeansTable::Cols::BASE_PK))) {
          ++ctx.m_extra_size;
        } else {
          ctx.m_extra_size += 2;
        }
        break;
      case FLD::CLUST_TRX_ID:
        dfield_set_data(field, m_trx_id_buf, DATA_TRX_ID_LEN);
        ctx.m_data_size += DATA_TRX_ID_LEN;
        break;
      case FLD::CLUST_ROLL_PTR:
        dfield_set_data(field, m_roll_ptr_buf, DATA_ROLL_PTR_LEN);
        ctx.m_data_size += DATA_ROLL_PTR_LEN;
        break;
      case FLD::CLUST_CONTENT:
        auto [q_vec, q_len] = KMeansTable::get_quantized_vector(
                vec_index.get(), vec_data, vec_len,
                thread_ctx->m_sub_table_heap.get());

        if (q_len == UNIV_SQL_NULL) {
          dfield_set_null(field);
          continue;
        }

        if ((ctx.m_data_size + ctx.m_extra_size + q_len) >= srv_page_size / 2) {
          /* Quantized vector is too big to fit in the row. It has to be
          stored externally. Here, we assume that if quantized vector cannot
          fit locally then the vector column itself must be external.
          TODO: The above invariant can change if we decide to store
          additional stuff in CONTENT column. */
          ut_a(vec_col_ext);

          /* We'll store the lob reference of the vector column only. During
          the build we'll read the vector column from the base table and
          recompute the quantized vector. This is not ideal but we do have a
          limit on record size which builder has to adhere to. And we won't be
          hit by this for typical dimensions of vector e.g.: 768 or 1536. */
          ulint ref_len;
          auto ref_data = const_cast<byte *>(
              rec_get_nth_field(m_base_index, ctx.m_row.m_rec,
                                ctx.m_row.m_offsets, vec_col_pos, &ref_len));

          /* We don't allow any secondary index on the vector column. No
          prefix should have been stored inline. */
          ut_a(ref_len == BTR_EXTERN_FIELD_REF_SIZE);
          ut_a(memcmp(ref_data, field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE));

          dfield_set_data(field, ref_data, ref_len);
          dfield_set_ext(field);
          ctx.m_data_size += ref_len;
        } else {
          dfield_set_data(field, q_vec, q_len);
          ctx.m_data_size += q_len;
        }

        if (dfield_is_ext(field)) {
          ctx.m_extra_size += 2;
        } else if (dfield_get_len(field) < 128 ||
                   !DATA_BIG_COL(m_index->table->get_col(
                       KMeansTable::Cols::CONTENT))) {
          ++ctx.m_extra_size;
        } else {
          ctx.m_extra_size += 2;
        }
        break;
    }
  }
  return DB_SUCCESS;
}

dberr_t VectorIndexBuilder::copy_row(Copy_ctx &ctx,
                                     size_t &mv_rows_added) noexcept {
  ut_a(!m_index->is_multi_value());
  ut_a(!is_spatial_index());
  ut_a(!is_fts_index());
  ut_a(m_conv_heap.is_null());
  ut_a(ctx.m_n_rows_added == 0);

  auto key_buffer = m_thread_ctxs[ctx.m_thread_id]->m_key_buffer;

  ctx.m_data_size = 0;
  ctx.m_n_fields = dict_index_get_n_fields(m_index);
  ctx.m_extra_size = UT_BITS_IN_BYTES(m_index->n_nullable);

  auto err{DB_SUCCESS};
  if (m_index->is_clustered()) {
    err = build_clust_index_tuple(ctx);
  } else {
    err = build_sec_index_tuple(ctx);
  }
  if (unlikely(err != DB_SUCCESS)) {
    ib::error() << "Failed building vector sub_table key, " << ut_strerr(err);
    return err;
  }

#ifdef UNIV_DEBUG
  {
    ulint extra;
    auto fields = key_buffer->m_dtuples[key_buffer->size()];

    auto size = rec_get_serialize_size(m_index, fields, ctx.m_n_fields, nullptr,
                                       &extra, MAX_ROW_VERSION);

    ut_a(ctx.m_data_size + ctx.m_extra_size == size);
    ut_a(ctx.m_extra_size == extra);
  }
#endif /* UNIV_DEBUG */

  /* Add to the total size of the record in the output buffer,
  the encoded length of extra_size and the extra bytes (extra_size).
  See Key_sort_buffer::write() for the variable-length encoding
  of extra_size. */
  ctx.m_data_size += (ctx.m_extra_size + 1) + ((ctx.m_extra_size + 1) >= 0x80);

  if (unlikely(!key_buffer->will_fit(ctx.m_data_size))) {
    ctx.m_n_rows_added = 0;
    return DB_OVERFLOW;
  }

  key_buffer->deep_copy(ctx.m_n_fields, ctx.m_data_size);

  /* Note row added and all fields copied. */
  ctx.m_n_fields = 0;
  ++ctx.m_n_rows_added;

  return DB_SUCCESS;
}

dberr_t VectorIndexBuilder::dtuple_copy_blobs(dtuple_t *dtuple, ulint *offsets,
                                              const mrec_t *mrec,
                                              mem_heap_t *heap) noexcept {
  const auto field_pos = KMeansTable::ClustIndexFields::CLUST_CONTENT;
  dfield_t *content_field = nullptr;

  /* We can only have one external field i.e.: the content field. */
  if (dtuple->has_ext()) {
    ut_a(m_index->is_clustered());
    ut_a(field_pos < dtuple->n_fields);

    content_field = dtuple_get_nth_field(dtuple, field_pos);
    ut_a(content_field->ext);
    ut_a(content_field->data != nullptr);
    ut_a(content_field->len == BTR_EXTERN_FIELD_REF_SIZE);
  }

  /* Potentially read the vector column from the base table. */
  auto err = Builder::dtuple_copy_blobs(dtuple, offsets, mrec, heap);
  if (err != DB_SUCCESS) {
    ib::error() << "Error copying vector data from blob, " << ut_strerr(err);
    return err;
  }

  if (content_field != nullptr) {
    auto vec_index = ib_vector::dict_table_get_vector_index_ptr<KMeansIndex>(
        m_ctx.old_table());
    /* Recompute the quantized vector. */
    auto [q_vec, q_len] = KMeansTable::get_quantized_vector(
            vec_index.get(),
            static_cast<const byte *>(content_field->data),
            content_field->len,
            heap);
    dfield_set_data(content_field, q_vec, q_len);
  }

  return DB_SUCCESS;
}

}  // namespace ddl
