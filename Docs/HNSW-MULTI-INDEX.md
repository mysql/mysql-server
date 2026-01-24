# HNSW Multi-Index Per Table

Multiple HNSW indexes on the same table, one per VECTOR column. Enables multi-modal search, cascading search with Matryoshka embeddings, and specialized queries per column.

## Features

- Multiple independent HNSW indexes per table (one per VECTOR column)
- Configurable distance metrics: L2 (Euclidean), Cosine, Dot Product
- Automatic index updates on INSERT, UPDATE, and DELETE
- Soft-delete with neighbor graph reconnection
- Concurrent read access (shared_mutex)
- Persistent storage with versioned binary format
- Full backward compatibility with single-index API

## API

All functions accept an optional `column` parameter. When omitted, behavior is backward-compatible with single-index mode.

### HNSW_CREATE_INDEX

```sql
-- Single index (legacy), default metric L2
SELECT HNSW_CREATE_INDEX('table', dim, M, ef);

-- Column-specific index
SELECT HNSW_CREATE_INDEX('table', 'column', dim, M, ef);

-- With custom distance metric
SELECT HNSW_CREATE_INDEX('table', dim, M, ef, 'cosine');
SELECT HNSW_CREATE_INDEX('table', 'column', dim, M, ef, 'dot_product');
```

**Metric values:** `'l2'` (default), `'cosine'`/`'cos'`, `'dot_product'`/`'dot'`/`'ip'`/`'inner_product'`

### HNSW_DROP_INDEX

```sql
SELECT HNSW_DROP_INDEX('table');                -- legacy
SELECT HNSW_DROP_INDEX('table', 'column');      -- column-specific
```

### HNSW_SAVE_INDEX

```sql
SELECT HNSW_SAVE_INDEX('table', '/path.bin');               -- legacy
SELECT HNSW_SAVE_INDEX('table', 'column', '/path.bin');     -- column-specific
```

### HNSW_LOAD_INDEX

```sql
SELECT HNSW_LOAD_INDEX('table', '/path.bin');               -- legacy
SELECT HNSW_LOAD_INDEX('table', 'column', '/path.bin');     -- column-specific
```

### VECTOR_SEARCH

```sql
-- Legacy forms
SELECT VECTOR_SEARCH(query_vec, 'table', k);
SELECT VECTOR_SEARCH(query_vec, 'table', k, ef);

-- Column-specific forms
SELECT VECTOR_SEARCH(query_vec, 'table', 'column', k);
SELECT VECTOR_SEARCH(query_vec, 'table', 'column', k, ef);
```

When 4 arguments are passed, disambiguation is automatic:
- If arg[2] is a string literal -> interpreted as column name
- If arg[2] is an integer -> interpreted as k (legacy form)

## Parameter Reference

| Function | Args | Signature |
|----------|------|-----------|
| `HNSW_CREATE_INDEX` | 4 | (table, dim, M, ef) |
| `HNSW_CREATE_INDEX` | 5 | (table, column, dim, M, ef) OR (table, dim, M, ef, metric) |
| `HNSW_CREATE_INDEX` | 6 | (table, column, dim, M, ef, metric) |
| `HNSW_DROP_INDEX` | 1 | (table) |
| `HNSW_DROP_INDEX` | 2 | (table, column) |
| `HNSW_SAVE_INDEX` | 2 | (table, path) |
| `HNSW_SAVE_INDEX` | 3 | (table, column, path) |
| `HNSW_LOAD_INDEX` | 2 | (table, path) |
| `HNSW_LOAD_INDEX` | 3 | (table, column, path) |
| `VECTOR_SEARCH` | 3 | (query, table, k) |
| `VECTOR_SEARCH` | 4 | (query, table, k, ef) OR (query, table, column, k) |
| `VECTOR_SEARCH` | 5 | (query, table, column, k, ef) |

## Distance Metrics

| Metric | Description | Use Case |
|--------|-------------|----------|
| `l2` | Euclidean distance (sqrt of sum of squared differences) | General purpose, unnormalized vectors |
| `cosine` | 1 - cosine_similarity | Text embeddings, normalized vectors |
| `dot_product` | Negative inner product (maximizes similarity) | Maximum inner product search (MIPS) |

## Registry Architecture

The `HnswIndexRegistry` uses composite keys `table:column` internally:

```cpp
// Key construction
static std::string make_key(table_name, column_name) {
    if (column_name.empty()) return table_name;      // legacy
    return table_name + ":" + column_name;            // multi-index
}
```

Available registry methods:

| Method | Description |
|--------|-------------|
| `register_index(table, column, dim, M, ef, metric)` | Create index for column |
| `get_index(table, column)` | Get index pointer |
| `drop_index(table, column)` | Remove index |
| `has_index(table, column)` | Check existence |
| `list_indexes()` | All registered keys |
| `get_columns_for_table(table)` | Column names with indexes on this table |
| `parse_metric(string)` | Parse metric string to enum |
| `metric_to_string(enum)` | Convert enum to string |

## DML Hook Behavior

### INSERT (write_row)
On each INSERT, the InnoDB `write_row` hook iterates ALL VECTOR columns in the table:
1. For each VECTOR column, checks if a column-specific index exists (`table:column`)
2. If not, falls back to legacy index (`table` alone) -- used only for the first VECTOR column
3. If no index matches, skips that column
4. Extracts the primary key once (shared across all inserts)
5. Inserts the vector data into the matched index

### UPDATE (update_row)
On each UPDATE, after the row is successfully updated in InnoDB:
1. Same column iteration logic as INSERT
2. Reads the NEW vector data from the updated row
3. Calls `HnswIndex::update(pk, new_vector)` which atomically removes the old entry and re-inserts

### DELETE (delete_row)
On each DELETE, after the row is successfully deleted from InnoDB:
1. Checks if table has any VECTOR columns
2. Extracts the primary key of the deleted row
3. For each VECTOR column with a registered index, calls `HnswIndex::remove(pk)`
4. The remove operation soft-deletes the node and reconnects its neighbors

## HNSW Algorithm Details

### Soft-Delete with Reconnection
When a node is removed:
1. All neighbors of the deleted node have it removed from their neighbor lists
2. If a neighbor's connection count drops below `M/2`, the algorithm attempts to connect it to other former neighbors of the deleted node (maintaining graph connectivity)
3. The node slot is added to a free list for reuse
4. If the entry point is deleted, a new valid entry point is found

### Concurrency Model
- `search()` and `contains()` use `std::shared_lock` (multiple concurrent readers)
- `insert()`, `remove()`, and `update()` use `std::unique_lock` (exclusive writer)
- The registry uses a separate `std::mutex` for thread-safe index management

### ID Mapping
- External IDs (row primary keys) are mapped to internal node indices via `std::unordered_map`
- A free list tracks deleted node slots for O(1) reuse
- The persistence format (v2) stores external IDs in neighbor lists, resolving to internal indices on load

## Backward Compatibility

- All existing single-index queries work unchanged
- Legacy registry key (table name only) is still supported
- `write_row` hook: legacy index is applied only to the first VECTOR column (same behavior as before)
- No changes needed to existing client code
- Binary format v1 files can still be loaded (automatic version detection)

## Validation

- `dim` must be between 1 and 16,383
- `M` must be between 1 and 128
- `ef_construction` should be >= M (recommended: 100-500)
- Vector dimensionality at insert time must match the configured `dim`
- Duplicate external IDs are rejected by `insert()`

## Memory Considerations

Each HNSW index lives fully in memory. Multiple indexes multiply RAM usage:

| Vectors | Dim | M | Approx RAM per index |
|---------|-----|---|---------------------|
| 10,000 | 128 | 16 | ~25 MB |
| 10,000 | 512 | 16 | ~85 MB |
| 100,000 | 512 | 16 | ~850 MB |

For a table with 2 indexes (128d + 512d) and 10K rows: ~110 MB total.

Soft-deleted nodes retain their slot (no vector data) until reused by a new insert.

## Examples

### Multi-Modal Table with Custom Metrics

```sql
CREATE TABLE documents (
    id         BIGINT UNSIGNED PRIMARY KEY,
    doc_type   VARCHAR(50),
    data       JSON,
    emb_title  VECTOR(128),
    emb_body   VECTOR(512),
    emb_image  VECTOR(768)
);

-- Create indexes with appropriate metrics
SELECT HNSW_CREATE_INDEX('documents', 'emb_title', 128, 16, 200, 'cosine');
SELECT HNSW_CREATE_INDEX('documents', 'emb_body', 512, 16, 200, 'cosine');
SELECT HNSW_CREATE_INDEX('documents', 'emb_image', 768, 16, 200, 'l2');

-- Insert data (all three indexes are automatically populated)
INSERT INTO documents VALUES (1, 'article', '{}',
    TO_VECTOR('[0.1, ...]'),
    TO_VECTOR('[0.2, ...]'),
    TO_VECTOR('[0.3, ...]'));

-- Search by title (cosine similarity)
SELECT VECTOR_SEARCH(TO_VECTOR('[0.1,...]'), 'documents', 'emb_title', 10);

-- Search by content
SELECT VECTOR_SEARCH(TO_VECTOR('[0.2,...]'), 'documents', 'emb_body', 5);

-- Search by image (L2 distance)
SELECT VECTOR_SEARCH(TO_VECTOR('[0.3,...]'), 'documents', 'emb_image', 5);

-- Update a vector (index is automatically updated)
UPDATE documents SET emb_title = TO_VECTOR('[0.15, ...]') WHERE id = 1;

-- Delete a row (all indexes are automatically cleaned up)
DELETE FROM documents WHERE id = 1;

-- Persist each index separately
SELECT HNSW_SAVE_INDEX('documents', 'emb_title', '/data/docs_title.bin');
SELECT HNSW_SAVE_INDEX('documents', 'emb_body', '/data/docs_body.bin');
SELECT HNSW_SAVE_INDEX('documents', 'emb_image', '/data/docs_image.bin');
```

### Maximum Inner Product Search (MIPS)

```sql
CREATE TABLE products (
    id      BIGINT UNSIGNED PRIMARY KEY,
    name    VARCHAR(255),
    features VECTOR(256)
);

-- Use dot_product for MIPS (when vectors are not normalized)
SELECT HNSW_CREATE_INDEX('products', 'features', 256, 16, 200, 'dot_product');

-- Results sorted by highest inner product (lowest negative distance)
SELECT VECTOR_SEARCH(TO_VECTOR('[...]'), 'products', 'features', 10);
```

### Cascading Search (Matryoshka Embeddings)

For models producing Matryoshka embeddings (valid at any prefix dimension):

```sql
CREATE TABLE articles (
    id        BIGINT UNSIGNED PRIMARY KEY,
    content   TEXT,
    emb_fast  VECTOR(128),    -- First 128 dims (coarse)
    emb_full  VECTOR(512)     -- First 512 dims (precise)
);

SELECT HNSW_CREATE_INDEX('articles', 'emb_fast', 128, 16, 200, 'cosine');
SELECT HNSW_CREATE_INDEX('articles', 'emb_full', 512, 16, 200, 'cosine');

-- Step 1: Fast coarse search (128d, cheap)
SELECT VECTOR_SEARCH(TO_VECTOR('[short_vec]'), 'articles', 'emb_fast', 50);

-- Step 2: Re-rank candidates with precise vectors (512d)
SELECT id, COSINE_DISTANCE(emb_full, TO_VECTOR('[full_vec]')) AS dist
FROM articles
WHERE id IN (...)  -- candidates from step 1
ORDER BY dist
LIMIT 10;
```

This reduces search latency by ~4x for large datasets while maintaining result quality.

## Persistence Format

### Version 2 (current)
```
[4 bytes] Magic: "HNSW"
[4 bytes] Version: 2
[struct]  hnsw_config_t (includes metric field)
[8 bytes] active_elements
[8 bytes] deleted_count
[4 bytes] max_level
[8 bytes] entry_point
[8 bytes] node_count (active only)
For each node:
  [8 bytes] external_id
  [4 bytes] max_level
  [4 bytes] vector_size
  [N*4 bytes] vector_data (floats)
  [4 bytes] level_count
  For each level:
    [4 bytes] neighbor_count
    [N*8 bytes] neighbor_external_ids
```

Neighbors are stored as external IDs for portability (resolved to internal indices on load).

### Version 1 (legacy, read-only support)
Same as v2 but without version field, deleted_count, or metric. Neighbors stored as internal indices.

## Source Files

| File | Role |
|------|------|
| `storage/innobase/vector/vec0hnsw.h` | HnswIndex class, config, node, result types |
| `storage/innobase/vector/vec0hnsw.cc` | HNSW algorithm: insert, remove, update, search, persistence |
| `storage/innobase/include/vec0hnsw_registry.h` | Registry class declaration |
| `storage/innobase/vector/vec0hnsw_registry.cc` | Registry implementation with metric parsing |
| `sql/item_hnsw_func.cc` | CREATE/DROP/SAVE/LOAD SQL functions |
| `sql/item_vector_func.cc` | VECTOR_SEARCH SQL function |
| `sql/item_create.cc` | Function registration (param counts) |
| `storage/innobase/handler/ha_innodb.cc` | write_row, update_row, delete_row hooks |
