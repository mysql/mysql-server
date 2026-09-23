// Copyright 2026 Google LLC

#ifndef SQL_ITERATORS_CLOUDSQL_VECTOR_ITERATORS_H_
#define SQL_ITERATORS_CLOUDSQL_VECTOR_ITERATORS_H_

/**
  @file
  Cloud SQL Vector Index Scan Iterator that performs ANN search and iterates
  over the results.
 */

#include <assert.h>
#include <sys/types.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "include/my_base.h"
#include "include/my_inttypes.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/iterators/row_iterator.h"
#include "sql/key.h"
#include "sql/key_spec.h"
#include "sql/sql_class.h"

class Filesort_info;
class Item;
class JOIN;
class Sort_result;
class THD;
struct IO_CACHE;
struct TABLE;

enum VectorSearchQueryStatus {
  RESULTS_POPULATED = 0,
  INDEX_UNUSABLE = 1,
  INLINED_INDEX_RESULTS_NOT_POPULATED = 2,
  INDEX_NOT_INLINED = 3,
  INDEX_NOT_FOUND = 4,
  LIMIT_NOT_FOUND = 5,
  LIMIT_TOO_LARGE = 6,
  ANN_MORE_EXPENSIVE_THAN_KNN = 7,
  MULTIPLE_ANN_ONE_TABLE = 8
};

/** A helper class to communicate search results from InnoDB back to MySQL.
We rely on simple allocation to not tie the memory allocation to either of
MySQL or InnoDB. The attempt here is to hide the MySQL level details from
InnoDB and only expose as much as upstream is already doing as part of a
regular SELECT processing */
class VectorSearchResults {
 public:
  // Constructor that allocates memory for records and distances.
  VectorSearchResults(THD *thd, const TABLE *table, uint32_t number_records)
        : m_table(table) {
    assert(!m_table->s->is_missing_primary_key());
    const KEY &key = table->key_info[table->s->primary_key];
    // Estimate the record size of ANN search results by adding up the size of
    // all the columns up to the last column which is part of the primary key.
    const uchar *prefix_end = m_table->record[0];
    for (auto kp = key.key_part, end = kp + key.user_defined_key_parts;
         kp < end; ++kp) {
      const Field *f = table->field[kp->fieldnr - 1];
      prefix_end = std::max(prefix_end, f->field_ptr() + f->pack_length());
    }
    m_record_size = prefix_end - m_table->record[0];
    m_records = new (thd->mem_root) uchar[number_records * m_record_size];
    m_distances = new (thd->mem_root) float[number_records];
    m_capacity = number_records;
  }

  const TABLE *table() const { return m_table; }
  uint32_t count() const { return m_count; }
  uint32_t capacity() const { return m_capacity; }
  uint32_t record_size() const { return m_record_size; }
  bool has_capacity() const { return m_count < m_capacity; }
  const float *distances() const { return m_distances; }
  const uchar *records() const { return m_records; }

  /* A consumer of the records should call this to get hold of a record buffer.
  add_record() must be called after the record is copied. */
  uchar *get_next_record() {
    assert(!m_add_pending);
    if (m_count >= m_capacity) {
      return nullptr;
    }
    m_add_pending = true;
    return m_records + (m_count * m_record_size);
  }

  /* Adds a record to which primary key has been copied. Update the distance
  value.
  @param[in]  record    record that must have been obtained through
                        get_next_record.
  @param[in]  distance  distance measure */
  void add_record(uchar *record, float distance) {
    assert(m_add_pending);
    assert(record >= m_records);

    [[maybe_unused]] auto offset = record - m_records;
    assert(offset % m_record_size == 0);
    assert(offset / m_record_size == m_count);

    m_distances[m_count++] = distance;
    m_add_pending = false;
  }

  // Reset counters to 0 so that we can reuse the same object for a new search.
  void reset() {
    // We want to overwrite the existing results, so just reset the counter.
    m_count = 0;
  }
  /* Allocates memory to hold records and distances. It must be called
  before first call to get_next_record() */

 private:
  /* Pointer to the open table we are working with. */
  const TABLE *m_table{nullptr};
  /* Currently used records. */
  uint32_t m_count{0};
  /* Total capacity. */
  uint32_t m_capacity{0};
  /* Record size is such that it includes all the columns up to the last
  column which is part of the primary key. */
  uint32_t m_record_size{0};
  /* To keep track that caller goes through records serially. */
  bool m_add_pending{false};
  /* Memory to hold records and distances. */
  uchar *m_records{nullptr};
  float *m_distances{nullptr};
};

/**
  Initialize vector search results based on the query vector and iterate over
  each of them.
*/
class VectorIndexScanIterator final : public TableRowIterator {
 public:
  /**
    @param thd     session context
    @param table   base table that the vector index belongs toxw
    @param examined_rows if not nullptr, is incremented for each successful
                   Read().
    @param query   vector search query
    @param search_options  vector search options
    @param results  vector search results
    @param vector_index_inlined  whether the vector index is inlined
  */
  VectorIndexScanIterator(THD *thd, TABLE *table, ha_rows *examined_rows,
                          float *query, uint query_size,
                          VectorSearchOptions search_options,
                          VectorSearchResults *results,
                          VectorSearchQueryStatus *query_status);
  ~VectorIndexScanIterator() override;

  bool DoInit() override;
  int DoRead() override;

  int ReInitWithMoreResults();
  void SetStreamId();
  bool Cleanup();

  int GetTotalResultsStreamed() { return total_results_streamed; }
  int GetNumNeighbors() { return m_search_options.num_neighbors; }

 private:
  // m_record is the record that is currently being read/scanned.
  // (copied from TableScanIterator)
  uchar *m_record;
  ha_rows *const m_examined_rows;

  ha_rows m_results_counter{0};

  // Vector search query
  float *m_query_vector;
  uint m_query_vector_size;

  // Vector search results
  VectorSearchResults *m_results;

  // Vector search options
  VectorSearchOptions m_search_options;

  // Whether we need to call kmeans/access search results
  VectorSearchQueryStatus *m_query_status;

  // Used to track if we have requested more results than
  // cloudsql_vector_iterative_filtering_max_neighbors
  int total_results_streamed;
};

class VectorIndexJoinIterator final : public RowIterator {
 public:
  VectorIndexJoinIterator(THD *thd,
                      unique_ptr_destroy_only<RowIterator> source_outer,
                      unique_ptr_destroy_only<RowIterator> source_inner)
        : RowIterator(thd),
          m_source_outer(std::move(source_outer)),
          m_source_inner(std::move(source_inner)) {
      assert(m_source_outer != nullptr);
      assert(m_source_inner != nullptr);
    maximum_search_calls = thd->variables
        .cloudsql_vector_iterative_filtering_max_neighbors * 10;
    }

  bool DoInit() override;

  int DoRead() override;

  /**
    * Unsetting the null row flag must be propagated to the inner iterators
    * because they may cache or rely on this state during their operations.
  */
  void SetNullRowFlag(bool is_null_row) override {
    m_source_outer->SetNullRowFlag(is_null_row);
    m_source_inner->SetNullRowFlag(is_null_row);
  }

  void EndPSIBatchModeIfStarted() override {
    m_source_outer->EndPSIBatchModeIfStarted();
    m_source_inner->EndPSIBatchModeIfStarted();
  }

  void UnlockRow() override {
    // Since we don't know which condition that caused the row to be rejected,
    // we can't know whether we could also unlock the outer row
    // (it may still be used as parts of other joined rows).
    if (m_state == READING_FIRST_INNER_ROW || m_state == READING_INNER_ROWS) {
      m_source_inner->UnlockRow();
    }
  }

 private:
  enum {
    NEEDS_OUTER_ROW,
    READING_FIRST_INNER_ROW,
    READING_INNER_ROWS,
    END_OF_ROWS
  } m_state;
  int num_results_found{0};
  unique_ptr_destroy_only<RowIterator> const m_source_outer;
  unique_ptr_destroy_only<RowIterator> const m_source_inner;

  // A ceiling on number of storage layer calls to avoid any bugs causing an
  // infinite loop.
  int maximum_search_calls;

  // Used to track the number of ANN search calls made to the storage layer.
  int num_search_calls{0};
};
#endif  // SQL_ITERATORS_CLOUDSQL_VECTOR_ITERATORS_H_
