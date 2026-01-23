# HNSW Multi-Index Per Table

Multiple HNSW indexes on the same table, one per VECTOR column. Enables multi-modal search, cascading search with Matryoshka embeddings, and specialized queries per column.

## API

All functions accept an optional `column` parameter. When omitted, behavior is backward-compatible with single-index mode.

### HNSW_CREATE_INDEX

```sql
-- Single index (legacy)
SELECT HNSW_CREATE_INDEX('table', dim, M, ef);

-- Column-specific index
SELECT HNSW_CREATE_INDEX('table', 'column', dim, M, ef);
```

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
- If arg[2] is a string literal → interpreted as column name
- If arg[2] is an integer → interpreted as k (legacy form)

## Parameter Reference

| Function | Args | Signature |
|----------|------|-----------|
| `HNSW_CREATE_INDEX` | 4 | (table, dim, M, ef) |
| `HNSW_CREATE_INDEX` | 5 | (table, column, dim, M, ef) |
| `HNSW_DROP_INDEX` | 1 | (table) |
| `HNSW_DROP_INDEX` | 2 | (table, column) |
| `HNSW_SAVE_INDEX` | 2 | (table, path) |
| `HNSW_SAVE_INDEX` | 3 | (table, column, path) |
| `HNSW_LOAD_INDEX` | 2 | (table, path) |
| `HNSW_LOAD_INDEX` | 3 | (table, column, path) |
| `VECTOR_SEARCH` | 3 | (query, table, k) |
| `VECTOR_SEARCH` | 4 | (query, table, k, ef) OR (query, table, column, k) |
| `VECTOR_SEARCH` | 5 | (query, table, column, k, ef) |

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
| `register_index(table, column, dim, M, ef)` | Create index for column |
| `get_index(table, column)` | Get index pointer |
| `drop_index(table, column)` | Remove index |
| `has_index(table, column)` | Check existence |
| `list_indexes()` | All registered keys |
| `get_columns_for_table(table)` | Column names with indexes on this table |

## write_row Hook Behavior

On each INSERT, the InnoDB `write_row` hook iterates ALL VECTOR columns in the table:

1. For each VECTOR column, checks if a column-specific index exists (`table:column`)
2. If not, falls back to legacy index (`table` alone) — used only for the first VECTOR column
3. If no index matches, skips that column
4. Extracts the primary key once (shared across all inserts)
5. Inserts the vector data into the matched index

This means a table with 3 VECTOR columns and 3 registered indexes will perform 3 index insertions per row write.

## Backward Compatibility

- All existing single-index queries work unchanged
- Legacy registry key (table name only) is still supported
- `write_row` hook: legacy index is applied only to the first VECTOR column (same behavior as before)
- No changes needed to existing client code

## Memory Considerations

Each HNSW index lives fully in memory. Multiple indexes multiply RAM usage:

| Vectors | Dim | M | Approx RAM per index |
|---------|-----|---|---------------------|
| 10,000 | 128 | 16 | ~25 MB |
| 10,000 | 512 | 16 | ~85 MB |
| 100,000 | 512 | 16 | ~850 MB |

For a table with 2 indexes (128d + 512d) and 10K rows: ~110 MB total.

## Examples

### Multi-Modal Table

```sql
CREATE TABLE documents (
    id         BIGINT UNSIGNED PRIMARY KEY,
    doc_type   VARCHAR(50),
    data       JSON,
    emb_title  VECTOR(128),
    emb_body   VECTOR(512),
    emb_image  VECTOR(768)
);

-- Create one index per column
SELECT HNSW_CREATE_INDEX('documents', 'emb_title', 128, 16, 200);
SELECT HNSW_CREATE_INDEX('documents', 'emb_body', 512, 16, 200);
SELECT HNSW_CREATE_INDEX('documents', 'emb_image', 768, 16, 200);

-- Search by title
SELECT VECTOR_SEARCH(TO_VECTOR('[0.1,...]'), 'documents', 'emb_title', 10);

-- Search by content
SELECT VECTOR_SEARCH(TO_VECTOR('[0.2,...]'), 'documents', 'emb_body', 5);

-- Search by image
SELECT VECTOR_SEARCH(TO_VECTOR('[0.3,...]'), 'documents', 'emb_image', 5);

-- Persist each index separately
SELECT HNSW_SAVE_INDEX('documents', 'emb_title', '/data/docs_title.bin');
SELECT HNSW_SAVE_INDEX('documents', 'emb_body', '/data/docs_body.bin');
SELECT HNSW_SAVE_INDEX('documents', 'emb_image', '/data/docs_image.bin');
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

SELECT HNSW_CREATE_INDEX('articles', 'emb_fast', 128, 16, 200);
SELECT HNSW_CREATE_INDEX('articles', 'emb_full', 512, 16, 200);

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

## Source Files

| File | Role |
|------|------|
| `storage/innobase/include/vec0hnsw_registry.h` | Registry class declaration |
| `storage/innobase/vector/vec0hnsw_registry.cc` | Registry implementation |
| `sql/item_hnsw_func.cc` | CREATE/DROP/SAVE/LOAD SQL functions |
| `sql/item_vector_func.cc` | VECTOR_SEARCH SQL function |
| `sql/item_create.cc` | Function registration (param counts) |
| `storage/innobase/handler/ha_innodb.cc` | write_row auto-insert hook |
