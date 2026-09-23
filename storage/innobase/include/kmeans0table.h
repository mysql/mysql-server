// Copyright 2026 Google LLC

#ifndef _KMEANS0TABLE_H_
#define _KMEANS0TABLE_H_

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

#include "vector0persist.h"
#include "kmeans0types.h"
#include "kmeans0index.h"

#include "data0data.h"
#include "data0type.h"
#include "db0err.h"
#include "dict0dd.h"
#include "dict0mem.h"
#include "mem0mem.h"
#include "mtr0mtr.h"
#include "que0que.h"
#include "rem0types.h"
#include "row0ins.h"
#include "row0upd.h"
#include "sql/current_thd.h"
#include "sql/table.h"

namespace ib_vector {

/** Table based KMeans index persistence
Table based KMeans index persistence uses a separate standalone innodb table,
a.k.a. sub_table, for each vector index. This sub_table will be modified when
creating the index or when DMLs happen on the base table. This class
encapsulates the operations on the sub_table, which are built on top of below
fundamental assumptions (invariants):
    - sub_table must be present when DML happens on the base table
    - there is a 1:1 mapping between the base table and the sub_table
    - no direct DMLs on sub_table. All DMLs must happen via the base table
    - no direct DDLs on the sub_table

Row Locking:
sub_table acts like any other secondary index on the base_table. Just like
other secondary indexes, it is required to get proper row level locks on
sub_table before performing any DML operation. */
class KMeansTable : public VectorPersist{
 public:

  // Table structure definition

  /** List of columns in the sub_table */
  enum Cols {
    PARTITION_ID = 0,           ///< partition of this vector in the KMeans index
    BASE_PK,                    ///< primary key of the base table
    CONTENT,                    ///< quantized value of this vector
    NUM_COLS
  };

  /** List of fields in the sub_table clustered index. */
  enum ClustIndexFields {
    CLUST_PARTITION_ID,         ///< partition of this vector in the KMeans index
    CLUST_BASE_PK,              ///< primary key of the base table
    CLUST_TRX_ID,               ///< hidden sys column trx_id
    CLUST_ROLL_PTR,             //< hidden sys column rollback ptr
    CLUST_CONTENT,              ///< quantized value of this vector
    CLUST_NUM_FIELDS
  };

  /** List of fields in the sub_table secondary index. Secondary index is built
  on base_pk. InnoDB also stores PK as part of secondary index. Since the
  sub_table's PK is composite key of partition_id and base_pk, InnoDB doesn't
  store base_pk twice. It will store base_pk and partition_id in the secondary
  index in that order. */
  enum SecIndexFields {
    SEC_BASE_PK,                ///< primary key of the base table
    SEC_PARTITION_ID,           ///< partition of this vector in the KMeans index
    SEC_NUM_FIELDS
  };

  /** Width of inline columns */
  static constexpr size_t partition_id_len = 8;
  static constexpr size_t base_pk_max_len = 3072;
  /** This is the length of base_pk we store for KMeans tree partitioner */
  static constexpr size_t partitioner_pk_len = 1;

  // Constructor

  /** Constructor
  @param[in]    index           the vector index to be persisted/updated
  @param[in]    thr             the query thread with which the persisting
                                action is associated (Not null when doing DML)
  @param[in]    info            DDL info (Not null when doing backfill) */
  KMeansTable(KMeansIndex* index, que_thr_t* thr, CreateVectorIndexInfo* info);

  /** Constructor
  @param[in]    index           the vector index to be persisted/updated
  @param[in]    base_table      base table of this index
  @remarks This constructor is used for reloading only. */
  KMeansTable(KMeansIndex* index, dict_table_t* base_table);

  /** Destructor */
  ~KMeansTable();

  virtual dberr_t persist() override;

  virtual std::pair<dberr_t, void*> load() override;

  /** Sync up with the mutation on vector data (of base table)
  @param[in]    thr             query thread that mutates the base table
  @return DB_SUCCESS or error code */
  virtual dberr_t sync_mutation(que_thr_t* thr) override;

  /** Get and serialize the vector partition id
  @param[in]    vec_index       the vector index
  @param[in]    data            the vector for which partition_id is computed
  @param[in]    len             the length of the vector data
  @param[in]    heap            the heap from where memory is allocated
  @return pointer to the serialized vector partition id
  @remarks This function makes sure the endianness of the serialization.
           This function also serves as a public interface for those who do not
           want a full bloom KMeansFile object but only the vector functions. */
  static byte* get_partition_id(KMeansIndex* vec_index,
                                             const byte* data,
                                             size_t len,
                                             mem_heap_t* heap);

  /** Get and serialize the quantized vector
  @param[in]    vec_index       the vector index
  @param[in]    data            the vector data to be serialized
  @param[in]    len             the length of the vector data
  @param[in]    heap            the heap from where memory is allocated
  @return pointer to the serialized quantized vector data and its size
  @remarks This function does not care about endianness for now, due to the
           fact that the quantized vector is stored in 8-bit format.
           This function also serves as a public interface for those who do not
           want a full bloom KMeansFile object but only the vector functions. */
  static std::pair<byte*, size_t> get_quantized_vector(KMeansIndex* vec_index,
                                                       const byte* data,
                                                       size_t len,
                                                       mem_heap_t* heap);

 protected:
  /** Insert a row into the sub_table
  @return DB_SUCCESS or error code */
  dberr_t insert_row();

  /** Update a row in the sub_table
  @return DB_SUCCESS or error code */
  dberr_t update_row();

  /** Scan the base table and backfill the sub_table
  @return DB_SUCCESS or error code */
  dberr_t backfill();

  /** Persist the partitioner to the sub_table
  @param[in]    sub_table        the sub_table handle
  @return DB_SUCCESS or error code */
  dberr_t persist_partitioner(dict_table_t* sub_table);

  /** Get the persisted partitioner
  @param[in]    sub_table         sub_table where partitioner is persisted
  @param[out]   data              pointer to the serialized partitioner data
  @param[in]    len               size of data
  @return error code or DB_SUCCESS */
  dberr_t get_partitioner(dict_table_t* sub_table, byte** data, ulint len);

  /** Get the persisted multipliers
  @param[in]    sub_table         sub_table where partitioner is persisted
  @param[out]   data              pointer to the multipliers
  @param[in]    len               size of data
  @return error code or DB_SUCCESS */
  dberr_t get_multipliers(dict_table_t* sub_table, byte** data, ulint len);

  /** Build query graph for persisting the partitioner to the sub_table
  @param[in]    sub_table        the sub_table handle */
  void create_query_graph(dict_table_t* sub_table);

  /** Insert one non_leaf row in the sub_table
  @param[in]    sub_table        the sub_table handle
  @param[in]    partition_id     the partition_id of the row to insert
  @param[in]    pk               the pk of the row to insert */
  dberr_t insert_non_leaf_row(dict_table_t* sub_table, int64_t partition_id,
                           byte pk);

  /** Read one non-leaf row from the sub_table
  @param[in]    sub_table        the sub_table handle
  @param[in]    partition_id     the partition_id of the row to read
  @param[in]    pk               the pk of the row to read
  @param[out]   data             the data of this row
  @param[out]   len              the length of the data
  @param[in]    heap             heap used to save data
  @return DB_SUCCESS or error code */
  dberr_t read_non_leaf_row(dict_table_t* sub_table, int64_t id, byte pk,
                            byte** data, ulint* len);

  /** Run a prepared row-change node
  @param[in]    node            the prepared row-change node
  @return DB_SUCCESS or error code */
  dberr_t run_node(que_node_t* node);

  /** Build an insert node structure based on the base table insert node
  @param[in]    base_ins_node   the base table insert node
  @param[in]    sub_table       the sub table to be inserted into
  @return a pointer to the created node insert structure */
  dberr_t build_ins_node(const ins_node_t* base_ins_node,
                         dict_table_t* sub_table);

  /** Build an update node structure based on the base table update node
  @param[in]    base_upd_node   the base table update node
  @param[in]    sub_table       the sub table to be updated
  @return a pointer to the created node udpate structure */
  dberr_t build_upd_node(const upd_node_t* base_upd_node,
                         dict_table_t* sub_table);

  /** Retrieve the data from the update node on base table
  @param[in]    base_upd_node   the base table update node */
  void get_upd_data(const upd_node_t* base_upd_node);

  /** Check if sub_table update is needed to accommodate the base table update
  @param[in]    base_upd_node   the base table update node
  @return whether a sub_table update is required */
  bool upd_needed(const upd_node_t* base_upd_node);

  /** Get the secondary index in the sub_table
  @param[in]    sub_table       the sub table
  @return a pointer of the secondary index */
  dict_index_t* get_secondary_index(dict_table_t* sub_table);

  /** Find the row in sub_table with a given base table PK
  @param[in]    sec_index       the (secondary) index that the cursor will open
  @param[in]    mtr             the mtr
  @return a pointer to the row data */
  dtuple_t* get_row(dict_index_t* sec_index,
                    dict_table_t* base_table,
                    mtr_t* mtr);

  /** Build the update node for the sub_table.
  @param[in]    thr             the query thread that has successfully updated
                                the base table.
  @param[in]    sub_table       sub_table to be updated.
  @return DB_SUCCESS or error code.
  @remarks The sub_table row has to exist (base.pk == sub_table.pk). */
  dberr_t build_row_upd_node(que_thr_t* thr, dict_table_t* sub_table);

  /** Validate a record in the sub_table
  @param[in]    rec             therecord to be validated.
  @param[in]    entry           value template used to match the record.
  @param[in]    index           index on which search is performed.
  @return whether the rec is validate or not */
  bool validate_rec(const rec_t* rec,
                    const dtuple_t* entry,
                    const dict_index_t* index);

  /** Update the fields of the sub_table update node
  @param[in]    clust_rec       the record in the sub_table
  @param[in]    base_index      the base table index */
  void update_upd_node_fields(const rec_t* clust_rec,
                              const dict_index_t* base_index);

  /** Serialize the vector partition id
  @param[in]    data            the vector data to be serialized
  @param[in]    len             the length of the vector data
  @return pointer to the serialized vector partition id */
  byte* get_partition_id(const byte* data, size_t len) {
    return get_partition_id(m_index, data, len, m_heap);
  }

  /** Serialize the quantized vector
  @param[in]    data            the vector data to be serialized
  @param[in]    len             the length of the vector data
  @return pointer to the serialized quantized vector data and its size */
  std::pair<byte*, size_t> get_quantized_vector(const byte* data, size_t len) {
    return get_quantized_vector(m_index, data, len, m_heap);
  }

 private:
  /** The associated vector index */
  KMeansIndex* m_index;

  /** Base table of this sub_table */
  dict_table_t* m_base_table {nullptr};

  /** The query thread that will carry the sub_table DMLs */
  que_thr_t* m_que_thr {nullptr};

  /** Struct containing information about vector index creation */
  CreateVectorIndexInfo* m_create_info{nullptr};

  /** Insert node for the sub_table */
  ins_node_t* m_ins_node {nullptr};

  /** Update node for the sub_table */
  upd_node_t* m_upd_node {nullptr};

  /** Old PK in a base table mutation, if any */
  dtuple_t* m_old_pk {nullptr};

  /** New PK in a base table mutation, if any */
  dtuple_t* m_new_pk {nullptr};

  /** New embedding (vector value) in a base table mutation, if any */
  dfield_t* m_new_embedding {nullptr};

  /** Heap from where memory is allocated. */
  mem_heap_t* m_heap {nullptr};

  /** Reserved Partition ID for NULL vector (no partition) */
  static constexpr int64_t m_null_partition_id_threshold {-1000};

  /** Reserved Partition ID for storing partitioner (non-leaf tree part) */
  static constexpr int64_t m_partitioner_id {-2000};

  /** Reserved Partition ID for storing multipliers */
  static constexpr int64_t m_multipliers_id {-3000};

  /** Serialized PK from base table are always > 1 byte. We store the following
  in base_pk field for partitioner and multiplier rows. */
  static constexpr byte m_partitioner_pk = 0x00;
  static constexpr byte m_multipliers_pk = 0x80;
};

} /* namespace ib_vector */

#endif /* _KMEANS0TABLE_H_ */
