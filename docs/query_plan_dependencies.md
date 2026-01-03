# MySQL Query Plan Dependencies

This document describes what data influences query plan generation in MySQL and how to reproduce production query plans without actual production data.

## Overview

Query plans in MySQL depend on **statistics about data distribution**, not the data itself. The optimizer makes decisions based on:

1. **Table-level statistics** - row counts, data sizes
2. **Index statistics** - cardinality, records per key
3. **Column histograms** - value distribution
4. **Schema metadata** - table structure, indexes, constraints
5. **Cost model constants** - hardware-calibrated parameters

## Key Dependencies

### 1. Table Statistics (`ha_statistics`)

Stored in `handler::stats` and accessed via the storage engine:

| Field | Purpose | Impact on Plans |
|-------|---------|-----------------|
| `records` | Estimated row count | Join order, scan vs index decisions |
| `data_file_length` | Table data size in bytes | IO cost estimation |
| `mean_rec_length` | Average row length | Cost calculations |
| `block_size` | Storage block size | Page read estimates |

**Source**: `sql/handler.h:4199-4249`

### 2. Index Statistics (`KEY` class)

Per-index statistics critical for access path selection:

| Field | Purpose | Impact on Plans |
|-------|---------|-----------------|
| `rec_per_key_float[]` | Records per distinct key prefix | Index selectivity |
| `key_length` | Total key length in bytes | Covering index decisions |
| `actual_key_parts` | Number of key columns | Prefix optimization |

**Source**: `sql/key.h:113-357`

### 3. InnoDB Persistent Statistics

Stored in system tables for durability:

#### `mysql.innodb_table_stats`
```
| Column                    | Purpose                          |
|---------------------------|----------------------------------|
| n_rows                    | Estimated total rows             |
| clustered_index_size      | Primary index pages              |
| sum_of_other_index_sizes  | Secondary indexes total pages    |
```

#### `mysql.innodb_index_stats`
```
| stat_name      | Purpose                                    |
|----------------|--------------------------------------------|
| size           | Total pages in index                       |
| n_leaf_pages   | Leaf pages only                            |
| n_diff_pfxNN   | Distinct values for first NN columns       |
```

**Source**: `storage/innobase/dict/dict0stats.cc:2436-2648`

### 4. Column Histograms

Stored in `mysql.column_statistics` as JSON. Two types:

- **SINGLETON**: For columns with few distinct values
- **EQUI_HEIGHT**: For columns with many distinct values

Key histogram data:
- Number of distinct values
- NULL values fraction
- Value distribution buckets
- Sampling rate

**Usage in optimizer** (`sql/join_optimizer/estimate_selectivity.cc:60-87`):
```cpp
selectivity = histogram->get_non_null_values_fraction() /
              max(1.0, histogram->get_num_distinct_values());
```

### 5. Cost Model Constants

Fixed calibrated values in `sql/join_optimizer/cost_constants.h`:

| Constant | Value | Purpose |
|----------|-------|---------|
| `kUnitCostInMicroseconds` | 0.434 | Base cost unit |
| `kReadOneRowCost` | 0.1/0.434 | Per-row read overhead |
| `kReadOneFieldCost` | 0.02/0.434 | Per-field access cost |
| `kIndexLookupPageCost` | 0.5/0.434 | B-tree traversal per page |
| `kApplyOneFilterCost` | 0.025/0.434 | Filter evaluation per row |

## Reproducing Production Query Plans

To reproduce a production query plan without production data:

### Required Statistics Export

```sql
-- 1. Table statistics
SELECT * FROM mysql.innodb_table_stats
WHERE database_name = 'your_db';

-- 2. Index statistics
SELECT * FROM mysql.innodb_index_stats
WHERE database_name = 'your_db';

-- 3. Column histograms
SELECT * FROM information_schema.column_statistics
WHERE schema_name = 'your_db';

-- 4. Table metadata (for rec_buff_length, field counts)
SHOW CREATE TABLE your_table;
```

### Statistics That Matter Most

1. **`n_rows`** (table row count) - Primary driver of scan vs index decisions
2. **`n_diff_pfxNN`** (index cardinality) - Determines index selectivity
3. **Histogram data** - Refines selectivity for filtered columns
4. **`rec_per_key`** - Records per key value for join optimization

### What Does NOT Affect Plans

- Actual data values (only distribution statistics)
- Row contents (only aggregate metrics)
- Specific primary key values
- Individual record contents

## Optimizer Flow Summary

```
Schema Metadata (columns, types, indexes)
         ↓
Table Statistics (row count, sizes)
         ↓
Index Statistics (cardinality per prefix)
         ↓
Histograms (value distribution)
         ↓
Cost Model Constants (hardware calibration)
         ↓
Access Path Enumeration → Cost Calculation → Plan Selection
```

## Key Files

| File | Purpose |
|------|---------|
| `sql/join_optimizer/cost_model.h` | Cost calculation functions |
| `sql/join_optimizer/cost_constants.h` | Calibrated cost constants |
| `sql/join_optimizer/estimate_selectivity.cc` | Selectivity from stats |
| `sql/key.h` | Index statistics structures |
| `sql/handler.h` | Table statistics (`ha_statistics`) |
| `sql/histograms/histogram.h` | Histogram interface |
| `storage/innobase/dict/dict0stats.cc` | InnoDB stats persistence |

## Hypothesis for Testing

1. **Query plans depend only on statistics, not data**: Identical statistics should produce identical plans regardless of actual row contents.

2. **Key statistics for plan selection**:
   - Row count (`stats.records`)
   - Index cardinality (`rec_per_key_float`)
   - Histograms (when available)

3. **Minimum data for plan reproduction**:
   - Table/index definitions (DDL)
   - `mysql.innodb_table_stats` contents
   - `mysql.innodb_index_stats` contents
   - Column histogram JSON (if used)
