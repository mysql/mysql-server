# MySQL Server Fork - Custom Modifications

This document describes custom modifications made to this MySQL server fork.

## Overview

This fork includes modifications to enable importing InnoDB table statistics from a different database (e.g., production) to achieve accurate query plans in a development/test environment without requiring actual data.

## Custom Variable: `innodb_stats_force_refresh`

### Purpose

When enabled, this variable forces InnoDB to reload persistent statistics from `mysql.innodb_table_stats` and `mysql.innodb_index_stats` tables, bypassing the in-memory cache.

### Usage

```sql
-- After manually updating statistics in persistent storage:
SET GLOBAL innodb_stats_force_refresh = ON;
SET GLOBAL innodb_stats_on_metadata = ON;

-- Access the table (e.g., via EXPLAIN) to trigger stats reload
EXPLAIN SELECT * FROM your_table WHERE ...;

-- Disable after reload
SET GLOBAL innodb_stats_force_refresh = OFF;
SET GLOBAL innodb_stats_on_metadata = OFF;
```

### How It Works

1. Normally, InnoDB caches statistics in memory after first load
2. Even if you manually UPDATE `mysql.innodb_table_stats`, the in-memory cache is used
3. When `innodb_stats_force_refresh = ON`, InnoDB bypasses the cache and re-reads from persistent storage
4. This allows manually imported statistics to take effect

## Modified Files

### 1. `storage/innobase/srv/srv0srv.cc`
- Added: `bool srv_stats_force_refresh = false;`

### 2. `storage/innobase/include/srv0srv.h`
- Added: `extern bool srv_stats_force_refresh;`

### 3. `storage/innobase/handler/ha_innodb.cc`
- Added: `MYSQL_SYSVAR_BOOL` definition for `stats_force_refresh`
- Added: Entry in `innobase_system_variables[]` array

### 4. `storage/innobase/dict/dict0stats.cc`
- Added: `#include "srv0srv.h"`
- Modified: `DICT_STATS_FETCH_ONLY_IF_NOT_IN_MEMORY` case to check `srv_stats_force_refresh`
  ```cpp
  if (table->stat_initialized && !srv_stats_force_refresh) {
    return (DB_SUCCESS);
  }
  ```

### 5. `storage/innobase/dict/dict0dict.cc`
- Added: `#include "srv0srv.h"`
- Modified: `dict_table_close()` to bypass ref_count check when force refresh is enabled
  ```cpp
  if (strchr(table->name.m_name, '/') != nullptr &&
      (table->get_ref_count() == 0 || srv_stats_force_refresh) &&
      dict_stats_is_persistent_enabled(table)) {
    dict_stats_deinit(table);
  }
  ```

## Workflow for Importing Statistics

1. **Create tables with same schema** (in dev/test environment):
   ```sql
   CREATE TABLE your_table (...) ENGINE=InnoDB STATS_PERSISTENT=1;
   ```

2. **Copy statistics from production**:
   ```sql
   -- Export from production
   SELECT * FROM mysql.innodb_table_stats WHERE ...;
   SELECT * FROM mysql.innodb_index_stats WHERE ...;
   
   -- Import to dev/test (use REPLACE to handle existing rows)
   REPLACE INTO mysql.innodb_table_stats VALUES (...);
   REPLACE INTO mysql.innodb_index_stats VALUES (...);
   ```

3. **Force InnoDB to reload**:
   ```sql
   SET GLOBAL innodb_stats_force_refresh = ON;
   SET GLOBAL innodb_stats_on_metadata = ON;
   
   -- Trigger reload by accessing the table
   EXPLAIN SELECT * FROM your_table;
   
   SET GLOBAL innodb_stats_force_refresh = OFF;
   SET GLOBAL innodb_stats_on_metadata = OFF;
   ```

4. **Verify**:
   ```sql
   EXPLAIN SELECT * FROM your_table WHERE ...;
   -- Row estimates should now match production
   ```

## Known Limitations

### 1. `information_schema.tables.TABLE_ROWS`

The `TABLE_ROWS` column in `information_schema.tables` may not reflect imported statistics. This is because MySQL 8.0+ maintains a separate Data Dictionary (DD) statistics cache that is not updated by manual changes to `mysql.innodb_table_stats`.

**Workaround**: Use `EXPLAIN` to verify the optimizer is using imported statistics, as EXPLAIN directly queries InnoDB statistics.

### 2. Index Statistics Required

When importing statistics, you MUST update BOTH:
- `mysql.innodb_table_stats` (table-level statistics)
- `mysql.innodb_index_stats` (index-level statistics)

If index statistics are missing, InnoDB falls back to calculating transient statistics from actual data.

### 3. `STATS_AUTO_RECALC` Behavior

If `STATS_AUTO_RECALC=1` (default) and index stats are missing, InnoDB may recalculate statistics from actual data, overwriting imported values. Consider using `STATS_AUTO_RECALC=0` for tables with imported stats.

## Merge Considerations

When updating this fork from upstream MySQL, watch for conflicts in:
- `storage/innobase/srv/srv0srv.cc` - variable definition
- `storage/innobase/include/srv0srv.h` - variable declaration
- `storage/innobase/handler/ha_innodb.cc` - SYSVAR definition and registration
- `storage/innobase/dict/dict0stats.cc` - stats update logic
- `storage/innobase/dict/dict0dict.cc` - table close logic

The changes are minimal and localized, so conflicts should be straightforward to resolve.

## Testing

See test files:
- `mysql-test/t/innodb_stats_import_issue.test` - Full workflow test
- `mysql-test/t/query_plan_statistics_dependency.test` - Demonstrates stats vs data dependency

Run tests:
```bash
cd build/mysql-test
./mtr innodb_stats_import_issue
./mtr query_plan_statistics_dependency
```

## References

- `Docs/query_plan_dependencies.md` - Documentation on query plan dependencies
- InnoDB persistent statistics: https://dev.mysql.com/doc/refman/8.0/en/innodb-persistent-stats.html
