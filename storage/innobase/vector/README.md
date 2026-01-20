# MySQL Vector Store - HNSW Index Extension

## Overview

This extension adds native vector similarity search capabilities to MySQL using the HNSW (Hierarchical Navigable Small World) algorithm for approximate nearest neighbor (ANN) queries.

## Features

| Feature | Description |
|---------|-------------|
| **Vector Distance Functions** | `COSINE_DISTANCE`, `L2_DISTANCE`, `DOT_PRODUCT` |
| **Vector Search** | `VECTOR_SEARCH(query, table)` for ANN queries |
| **Index Management** | Create, drop, save, and load HNSW indexes |
| **Persistence** | Binary serialization for index persistence |

---

## SQL Functions

### Vector Operations

```sql
-- Convert array to binary vector
SELECT TO_VECTOR('[1.0, 2.0, 3.0]');

-- Calculate cosine distance
SELECT COSINE_DISTANCE(vec1, vec2);

-- Calculate L2 (Euclidean) distance
SELECT L2_DISTANCE(vec1, vec2);
```

### Index Management

```sql
-- Create an HNSW index
SELECT HNSW_CREATE_INDEX('my_table', 128, 16, 200);
-- Parameters: table_name, dimensions, M, ef_construction

-- Drop an index
SELECT HNSW_DROP_INDEX('my_table');

-- Save index to disk
SELECT HNSW_SAVE_INDEX('my_table', '/path/to/index.hnsw');

-- Load index from disk
SELECT HNSW_LOAD_INDEX('my_table', '/path/to/index.hnsw');
```

### Vector Search

```sql
-- Search for nearest neighbors
SELECT VECTOR_SEARCH(query_vector, 'my_table');
-- Returns: JSON array of {id, distance} pairs
```

---

## HNSW Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `M` | 16 | Max connections per node per layer |
| `ef_construction` | 200 | Search expansion factor during build |
| `ef_search` | 50 | Search expansion factor during query |
| `dimensions` | Auto | Vector dimensionality (set on first insert) |

---

## Architecture

```
┌─────────────────────────────────────────────────┐
│                  SQL Layer                       │
│  VECTOR_SEARCH │ HNSW_CREATE_INDEX │ etc.       │
├─────────────────────────────────────────────────┤
│              HnswIndexRegistry                   │
│         (table → index mapping)                  │
├─────────────────────────────────────────────────┤
│                  HnswIndex                       │
│   insert() │ search() │ save/load_to_file()     │
└─────────────────────────────────────────────────┘
```

---

## Files

| File | Purpose |
|------|---------|
| `storage/innobase/vector/vec0hnsw.cc` | HNSW algorithm implementation |
| `storage/innobase/vector/vec0hnsw_registry.cc` | Global index registry |
| `sql/item_vector_func.cc` | VECTOR_SEARCH implementation |
| `sql/item_hnsw_func.cc` | UDF implementations |

---

## Building

```bash
cd mysql-server/build
cmake --build . --target vector_unittest -j4
./bin/vector_unittest --gtest_filter=HnswIndexTest.*
```

## Branch

`vector-search-hnsw-phase2` on `MauricioPerera/mysql-server`
