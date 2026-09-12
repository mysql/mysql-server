// Copyright 2026 Google LLC

/**
  @file
  Implementation of the Cloud SQL Vector Index Scan Iterator that iterates over
  the results of the ANN search.
*/

#include "sql/iterators/cloudsql_vector_iterators.h"

#include <assert.h>

#include <string>
#include <vector>

#include "my_base.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/debug_sync.h"
#include "sql/handler.h"
#include "sql/iterators/row_iterator.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_executor.h"
#include "sql/sql_plugin.h"
#include "sql/sql_tmp_table.h"
#include "sql/table.h"
#include "sql/vector_opts.h"
#include "sql/log.h"
#include "storage/innobase/include/vector0types.h"

using std::string;

VectorIndexScanIterator::VectorIndexScanIterator(
    THD *thd, TABLE *table, ha_rows *examined_rows, float *query,
    uint query_size, VectorSearchOptions search_options,
    VectorSearchResults *results, VectorSearchQueryStatus *query_status)
    : TableRowIterator(thd, table),
      m_record(table->record[0]),
      m_examined_rows(examined_rows),
      m_query_vector(query),
      m_query_vector_size(query_size),
      m_results(results),
      m_search_options(search_options),
      m_query_status(query_status) {}

VectorIndexScanIterator::~VectorIndexScanIterator() {}

// Allocate the VectorSearchResults object and initialize counter.
bool VectorIndexScanIterator::DoInit() {
  m_results_counter = 0;
  m_search_options.stream_id = 0;
  total_results_streamed = 0;
  return false;
}

// Reset the iterator counter to 0 and populate a new result set into m_results.
int VectorIndexScanIterator::ReInitWithMoreResults() {
  // Reset counters so that m_results can be overwritten.
  m_results_counter = 0;
  m_results->reset();

  assert(m_search_options.stream_id == thd()->query_id);
  TABLE *tab = table();
  if (!(tab && tab->file && tab->s)) {
    return HandleError(HA_ERR_ANN_FAILED);
  }

  // Re-do the ANN search with the stream_id.
  std::string qualifiedTableName = table()->s->db.str;
  qualifiedTableName += ".";
  qualifiedTableName += table()->s->table_name.str;
  std::vector<float> query_vec = std::vector<float>(
      m_query_vector, m_query_vector + (m_query_vector_size));

  int ret = table()->file->ha_cloudsql_vector_ann_search(
      std::move(query_vec), m_search_options, m_results);

  DBUG_EXECUTE_IF(
    "return_ann_exhausted_error", {
    ret = HA_ERR_ANN_EXHAUSTED;
  });
  DBUG_EXECUTE_IF(
    "return_ann_failed", {
    ret = HA_ERR_ANN_FAILED;
  });

  // Increment total streamed.
  total_results_streamed += m_results->count();

  if (ret != 0 && ret != HA_ERR_ANN_EXHAUSTED) {
    return HandleError(ret);
  }
  return ret;
}

void VectorIndexScanIterator::SetStreamId() {
  m_search_options.stream_id = thd()->query_id;
}


bool VectorIndexScanIterator::Cleanup() {
  assert(m_search_options.stream_id == thd()->query_id);
  return table()->file->ha_cloudsql_vector_ann_cleanup(
    m_search_options.stream_id);
}

// On the first call, fetch the results from the vector index info m_results.
// Additionally we read a record from m_results (ANN search results)
// into m_record which is mapped to table->record[0] in every call.
int VectorIndexScanIterator::DoRead() {
  if (m_query_status && !(*m_query_status == RESULTS_POPULATED)) {
    std::string qualifiedTableName = table()->s->db.str;
    qualifiedTableName += ".";
    qualifiedTableName += table()->s->table_name.str;

    // Convert the query vector array to a std::vector<float>
    // TODO: b/381546831 try to avoid this copy
    std::vector<float> query_vec = std::vector<float>(
        m_query_vector, m_query_vector + (m_query_vector_size));

    // TODO: juliaofferman please chk if the query vector is needed after this
    //       call, if not then we can avoid one memcpy
    int ret = table()->file->ha_cloudsql_vector_ann_search(
        std::move(query_vec), m_search_options, m_results);
    total_results_streamed = m_results->count();

    /* Return if we failed to fetch the results. Once we switch to the new
    * handler interface, we can return HandleError(ret). */
    if (ret != 0) {
      return HandleError(ret);
    }
    if (m_query_status) *m_query_status = RESULTS_POPULATED;
  }
  if (m_results_counter >= m_results->count()) {
    return HandleError(HA_ERR_END_OF_FILE);
  }

  // Index into the results vector & copy the record into m_record
  std::memcpy(
      m_record,
      m_results->records() + (m_results_counter * m_results->record_size()),
      m_results->record_size());
  m_results_counter++;
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

bool VectorIndexJoinIterator::DoInit() {
  m_state = NEEDS_OUTER_ROW;
  if (m_source_outer->Init()) {
    return true;
  }
  VectorIndexScanIterator* outer =
      down_cast<VectorIndexScanIterator*>(m_source_outer->real_iterator());
  outer->SetStreamId();
  return false;
}

int VectorIndexJoinIterator::DoRead() {
  if (m_state == END_OF_ROWS) {
    return -1;
  }
  VectorIndexScanIterator* outer =
      down_cast<VectorIndexScanIterator*>(m_source_outer->real_iterator());
  for (;;) {  // Termination condition within loop.
    if (m_state == NEEDS_OUTER_ROW) {
      int err = m_source_outer->Read();
      if (err == 1) {
        outer->Cleanup();
        return 1;  // Error.
      }
      if (err == -1) {
        if (static_cast<uint>(outer->GetTotalResultsStreamed())
            < thd()->variables.cloudsql_vector_iterative_filtering_max_neighbors
          && num_results_found < outer->GetNumNeighbors()
          && num_search_calls < maximum_search_calls) {
          int ret = outer->ReInitWithMoreResults();
          num_search_calls++;

          // Since this is not the first ANN search, if HA_ERR_ANN_EXHAUSTED
          // is returned, we can return the results we have and not return an
          // error.
          if (ret == HA_ERR_ANN_EXHAUSTED) {
            outer->Cleanup();
            m_state = END_OF_ROWS;
            return -1;
          }
          if (ret != 0) {
            outer->Cleanup();
            return ret;
          }
          continue;
        } else {
          outer->Cleanup();
          m_state = END_OF_ROWS;
          return -1;
        }
      }

      // Init() could read the NULL row flags (e.g., when building a hash
      // table), so unset them before instead of after.
      m_source_inner->SetNullRowFlag(false);

      if (m_source_inner->Init()) {
        outer->Cleanup();
        return 1;
      }
      m_state = READING_FIRST_INNER_ROW;
    }
    assert(m_state == READING_INNER_ROWS || m_state == READING_FIRST_INNER_ROW);

    int err = m_source_inner->Read();
    if (err == 1) {
      outer->Cleanup();
      return 1;  // Error.
    }
    if (thd()->killed) {  // Aborted by user.
      thd()->send_kill_message();
      outer->Cleanup();
      return 1;
    }
    if (err == -1) {
      // Out of inner rows for this outer row. If we are an outer join
      // and never found any inner rows, return a null-complemented row.
      // If not, skip that and go straight to reading a new outer row.
        m_state = NEEDS_OUTER_ROW;
        continue;
    }

    // An inner row has been found.
    num_results_found++;

      m_state = READING_INNER_ROWS;
    return 0;
  }
}
