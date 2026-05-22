# Common test logic for partition reorganize statistics.
# This file is sourced by both persistent and transient stats tests.
#
# Expected variables set by caller:
#   $use_persistent_stats - 1 for persistent stats, 0 for transient stats
#
# Persistent stats verification: mysql.innodb_table_stats (n_rows column)
# Transient stats verification: INFORMATION_SCHEMA.PARTITIONS (TABLE_ROWS column)

--echo #
--echo # Test 1: REORGANIZE PARTITION updates statistics correctly
--echo #

CREATE TABLE sales_data (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  region VARCHAR(50),
  status VARCHAR(20),
  PRIMARY KEY (id, sale_date),
  KEY idx_status (status)
) ENGINE=InnoDB
PARTITION BY RANGE (YEAR(sale_date)) (
  PARTITION p2020 VALUES LESS THAN (2021),
  PARTITION p2021 VALUES LESS THAN (2022),
  PARTITION p2022 VALUES LESS THAN (2023),
  PARTITION p2023 VALUES LESS THAN (2024)
);

--echo # Insert test data
INSERT INTO sales_data (sale_date, amount, region, status) VALUES
('2020-01-15', 1000.00, 'North', 'completed'),
('2020-06-20', 1500.00, 'South', 'completed'),
('2020-12-10', 2000.00, 'East', 'pending'),
('2021-03-05', 2500.00, 'West', 'completed'),
('2021-08-15', 3000.00, 'North', 'pending'),
('2021-11-25', 3500.00, 'South', 'completed'),
('2022-02-14', 4000.00, 'East', 'pending'),
('2022-07-04', 4500.00, 'West', 'completed'),
('2022-10-31', 5000.00, 'North', 'pending'),
('2023-01-20', 5500.00, 'South', 'completed'),
('2023-05-12', 6000.00, 'East', 'pending'),
('2023-09-08', 6500.00, 'West', 'completed');

--echo # Ensure statistics are populated
ANALYZE TABLE sales_data;

--echo # Check statistics before REORGANIZE PARTITION
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_data%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_data' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # Verify actual row counts match statistics
SELECT 
  'p2020' as partition_name,
  COUNT(*) as actual_rows
FROM sales_data PARTITION (p2020)
UNION ALL
SELECT 'p2021', COUNT(*) FROM sales_data PARTITION (p2021)
UNION ALL
SELECT 'p2022', COUNT(*) FROM sales_data PARTITION (p2022)
UNION ALL
SELECT 'p2023', COUNT(*) FROM sales_data PARTITION (p2023)
ORDER BY partition_name;

--echo # Perform REORGANIZE PARTITION (split p2022 and p2023 into smaller partitions)
ALTER TABLE sales_data REORGANIZE PARTITION p2022, p2023 INTO (
  PARTITION p2022 VALUES LESS THAN (2023),
  PARTITION p2023 VALUES LESS THAN (2024),
  PARTITION p2024 VALUES LESS THAN (2025)
);

--echo # Check statistics AFTER REORGANIZE PARTITION (should show correct row counts)
--echo # Without the fix, the reorganized partitions (p2022, p2023) and the new
--echo # partition (p2024) would report n_rows=0 here, because their statistics
--echo # are never recalculated after the data is copied. With the fix, p2022 and
--echo # p2023 correctly show n_rows=3 and p2024 shows n_rows=0 (it is empty).
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_data%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_data' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # Verify actual row counts in ALL partitions (including untouched ones)
SELECT 
  'p2020' as partition_name,
  COUNT(*) as actual_rows
FROM sales_data PARTITION (p2020)
UNION ALL
SELECT 'p2021', COUNT(*) FROM sales_data PARTITION (p2021)
UNION ALL
SELECT 'p2022', COUNT(*) FROM sales_data PARTITION (p2022)
UNION ALL
SELECT 'p2023', COUNT(*) FROM sales_data PARTITION (p2023)
UNION ALL
SELECT 'p2024', COUNT(*) FROM sales_data PARTITION (p2024)
ORDER BY partition_name;

--echo # Verify optimizer uses index statistics for query planning
EXPLAIN SELECT * FROM sales_data WHERE status = 'pending';

DROP TABLE sales_data;

--echo #
--echo # Test 2: Subpartitioned tables with HASH subpartitions
--echo #
--echo # Test with subpartitioned table
CREATE TABLE sales_subpart (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  region VARCHAR(50),
  PRIMARY KEY (id, sale_date, region)
) ENGINE=InnoDB
PARTITION BY RANGE (YEAR(sale_date))
SUBPARTITION BY HASH(id)
SUBPARTITIONS 2 (
  PARTITION p2020 VALUES LESS THAN (2021),
  PARTITION p2021 VALUES LESS THAN (2022)
);

--echo # Insert data into subpartitioned table
INSERT INTO sales_subpart (sale_date, amount, region) VALUES
('2020-01-15', 1000.00, 'North'),
('2020-06-20', 1500.00, 'South'),
('2020-12-10', 2000.00, 'East'),
('2021-03-05', 2500.00, 'West'),
('2021-08-15', 3000.00, 'North'),
('2021-11-25', 3500.00, 'South');

--echo # Ensure statistics are populated
ANALYZE TABLE sales_subpart;

--echo # Check statistics before REORGANIZE on subpartitioned table
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_subpart%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, SUBPARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_subpart' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME, SUBPARTITION_NAME;
}

--echo # REORGANIZE subpartitioned table (add new partition)
ALTER TABLE sales_subpart REORGANIZE PARTITION p2021 INTO (
  PARTITION p2021 VALUES LESS THAN (2022),
  PARTITION p2022 VALUES LESS THAN (2023)
);

--echo # Check statistics AFTER REORGANIZE on subpartitioned table
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_subpart%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, SUBPARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_subpart' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME, SUBPARTITION_NAME;
}

--echo #
--echo # Test 3: REORGANIZE with MERGE (multiple partitions into one)
--echo #
CREATE TABLE sales_merge (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  PRIMARY KEY (id, sale_date)
) ENGINE=InnoDB
PARTITION BY RANGE (YEAR(sale_date)) (
  PARTITION p2020 VALUES LESS THAN (2021),
  PARTITION p2021 VALUES LESS THAN (2022),
  PARTITION p2022 VALUES LESS THAN (2023)
);

INSERT INTO sales_merge (sale_date, amount) VALUES
('2020-01-15', 1000.00),
('2020-06-20', 1500.00),
('2021-03-05', 2500.00),
('2021-08-15', 3000.00),
('2022-02-14', 4000.00),
('2022-07-04', 4500.00);

--echo # Ensure statistics are populated
ANALYZE TABLE sales_merge;

--echo # Check stats before REORGANIZE (merge)
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_merge%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_merge' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # Merge p2020 and p2021 into p2020_2021
ALTER TABLE sales_merge REORGANIZE PARTITION p2020, p2021 INTO (
  PARTITION p2020_2021 VALUES LESS THAN (2022)
);

--echo # Verify merged partition has correct statistics
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_merge%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_merge' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # Verify actual row count in merged partition
SELECT COUNT(*) as actual_rows FROM sales_merge PARTITION (p2020_2021);

DROP TABLE sales_merge;
DROP TABLE sales_subpart;

--echo #
--echo # Test 4: COALESCE PARTITION (HASH partitions) - PART_REORGED_DROPPED
--echo #
CREATE TABLE sales_coalesce (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  PRIMARY KEY (id, sale_date)
) ENGINE=InnoDB
PARTITION BY HASH(YEAR(sale_date))
PARTITIONS 4;

INSERT INTO sales_coalesce (sale_date, amount) VALUES
('2020-01-15', 1000.00),
('2020-06-20', 1500.00),
('2021-03-05', 2500.00),
('2021-08-15', 3000.00),
('2022-02-14', 4000.00),
('2022-07-04', 4500.00),
('2023-01-20', 5500.00),
('2023-05-12', 6000.00);

--echo # Ensure statistics are populated
ANALYZE TABLE sales_coalesce;

--echo # Check stats before COALESCE
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_coalesce%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_coalesce' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # COALESCE PARTITION (reduce from 4 to 2 partitions)
ALTER TABLE sales_coalesce COALESCE PARTITION 2;

--echo # Check stats after COALESCE (should show correct row counts)
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_coalesce%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_coalesce' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # Verify actual row counts
SELECT 
  'p0' as partition_name,
  COUNT(*) as actual_rows
FROM sales_coalesce PARTITION (p0)
UNION ALL
SELECT 'p1', COUNT(*) FROM sales_coalesce PARTITION (p1)
ORDER BY partition_name;

DROP TABLE sales_coalesce;

--echo #
--echo # Test 5: REORGANIZE with subpartitions
--echo #
CREATE TABLE sales_reorganize_sub (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  region VARCHAR(50),
  PRIMARY KEY (id, sale_date, region)
) ENGINE=InnoDB
PARTITION BY RANGE(YEAR(sale_date))
SUBPARTITION BY HASH(id)
SUBPARTITIONS 2
(
  PARTITION p0 VALUES LESS THAN (2021),
  PARTITION p1 VALUES LESS THAN (2022),
  PARTITION p2 VALUES LESS THAN (2023),
  PARTITION p3 VALUES LESS THAN MAXVALUE
);

INSERT INTO sales_reorganize_sub (sale_date, amount, region) VALUES
('2020-01-15', 1000.00, 'North'),
('2020-06-20', 1500.00, 'South'),
('2021-03-05', 2500.00, 'West'),
('2021-08-15', 3000.00, 'East'),
('2022-02-14', 4000.00, 'North'),
('2022-07-04', 4500.00, 'South');

--echo # Ensure statistics are populated
ANALYZE TABLE sales_reorganize_sub;

--echo # Check stats before REORGANIZE
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_reorganize_sub%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, SUBPARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_reorganize_sub' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME, SUBPARTITION_NAME;
}

--echo # REORGANIZE subpartitioned table (merge p1 and p2)
ALTER TABLE sales_reorganize_sub REORGANIZE PARTITION p1, p2 INTO (
  PARTITION p1_2 VALUES LESS THAN (2023)
);

--echo # Check stats after REORGANIZE on subpartitioned table
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_reorganize_sub%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, SUBPARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_reorganize_sub' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME, SUBPARTITION_NAME;
}

DROP TABLE sales_reorganize_sub;

--echo #
--echo # Test 6: ADD PARTITION (simple add without reorganize)
--echo #
CREATE TABLE sales_add (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  PRIMARY KEY (id, sale_date)
) ENGINE=InnoDB
PARTITION BY RANGE (YEAR(sale_date)) (
  PARTITION p2020 VALUES LESS THAN (2021),
  PARTITION p2021 VALUES LESS THAN (2022)
);

INSERT INTO sales_add (sale_date, amount) VALUES
('2020-01-15', 1000.00),
('2020-06-20', 1500.00),
('2021-03-05', 2500.00),
('2021-08-15', 3000.00);

--echo # Ensure statistics are populated
ANALYZE TABLE sales_add;

--echo # Check stats before ADD PARTITION
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_add%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_add' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # ADD PARTITION (not reorganize)
ALTER TABLE sales_add ADD PARTITION (
  PARTITION p2022 VALUES LESS THAN (2023)
);

INSERT INTO sales_add (sale_date, amount) VALUES
('2022-02-14', 4000.00),
('2022-07-04', 4500.00);

--echo # Ensure statistics are populated after insert
ANALYZE TABLE sales_add;

--echo # Check stats after ADD PARTITION
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_add%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_add' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

DROP TABLE sales_add;

--echo #
--echo # Test 7: REBUILD PARTITION
--echo #
CREATE TABLE sales_rebuild (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  PRIMARY KEY (id, sale_date)
) ENGINE=InnoDB
PARTITION BY RANGE (YEAR(sale_date)) (
  PARTITION p2020 VALUES LESS THAN (2021),
  PARTITION p2021 VALUES LESS THAN (2022),
  PARTITION p2022 VALUES LESS THAN (2023)
);

INSERT INTO sales_rebuild (sale_date, amount) VALUES
('2020-01-15', 1000.00),
('2020-06-20', 1500.00),
('2021-03-05', 2500.00),
('2021-08-15', 3000.00),
('2022-02-14', 4000.00);

--echo # Ensure statistics are populated
ANALYZE TABLE sales_rebuild;

--echo # Check stats before REBUILD
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_rebuild%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_rebuild' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # REBUILD specific partition
ALTER TABLE sales_rebuild REBUILD PARTITION p2021;

--echo # Check stats after REBUILD
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_rebuild%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_rebuild' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

DROP TABLE sales_rebuild;

--echo #
--echo # Test 8: AUTO_INCREMENT with COALESCE PARTITION
--echo #
CREATE TABLE sales_autoinc (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  PRIMARY KEY (id)
) ENGINE=InnoDB
PARTITION BY HASH(id)
PARTITIONS 4;

INSERT INTO sales_autoinc (sale_date, amount) VALUES
('2020-01-15', 1000.00),
('2020-06-20', 1500.00),
('2021-03-05', 2500.00),
('2021-08-15', 3000.00),
('2022-02-14', 4000.00),
('2022-07-04', 4500.00);

--echo # Ensure statistics are populated
ANALYZE TABLE sales_autoinc;

--echo # Check stats before COALESCE
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_autoinc%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_autoinc' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # COALESCE with AUTO_INCREMENT
ALTER TABLE sales_autoinc COALESCE PARTITION 2;

--echo # Check stats after COALESCE with AUTO_INCREMENT
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_autoinc%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_autoinc' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

DROP TABLE sales_autoinc;

--echo #
--echo # Test 9: REORGANIZE with empty partitions
--echo #
CREATE TABLE sales_empty (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  PRIMARY KEY (id, sale_date)
) ENGINE=InnoDB
PARTITION BY RANGE (YEAR(sale_date)) (
  PARTITION p2020 VALUES LESS THAN (2021),
  PARTITION p2021 VALUES LESS THAN (2022),
  PARTITION p2022 VALUES LESS THAN (2023)
);

--echo # Insert data only in p2020, leave others empty
INSERT INTO sales_empty (sale_date, amount) VALUES
('2020-01-15', 1000.00),
('2020-06-20', 1500.00);

--echo # Ensure statistics are populated
ANALYZE TABLE sales_empty;

--echo # Check stats before REORGANIZE
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_empty%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_empty' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # REORGANIZE including empty partitions
ALTER TABLE sales_empty REORGANIZE PARTITION p2021, p2022 INTO (
  PARTITION p2021 VALUES LESS THAN (2022),
  PARTITION p2022 VALUES LESS THAN (2023),
  PARTITION p2023 VALUES LESS THAN (2024)
);

--echo # Check stats after REORGANIZE with empty partitions
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_empty%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_empty' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

DROP TABLE sales_empty;

--echo #
--echo # Test 10: Multiple operations with PART_NORMAL partitions
--echo #
CREATE TABLE sales_mixed (
  id INT AUTO_INCREMENT,
  sale_date DATE NOT NULL,
  amount DECIMAL(10,2),
  PRIMARY KEY (id, sale_date)
) ENGINE=InnoDB
PARTITION BY RANGE (YEAR(sale_date)) (
  PARTITION p2020 VALUES LESS THAN (2021),
  PARTITION p2021 VALUES LESS THAN (2022),
  PARTITION p2022 VALUES LESS THAN (2023),
  PARTITION p2023 VALUES LESS THAN (2024)
);

INSERT INTO sales_mixed (sale_date, amount) VALUES
('2020-01-15', 1000.00),
('2020-06-20', 1500.00),
('2021-03-05', 2500.00),
('2021-08-15', 3000.00),
('2022-02-14', 4000.00),
('2023-01-20', 5500.00);

--echo # Ensure statistics are populated
ANALYZE TABLE sales_mixed;

--echo # Check stats before REORGANIZE
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_mixed%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_mixed' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # REORGANIZE only p2022 and p2023, leaving p2020 and p2021 as PART_NORMAL
ALTER TABLE sales_mixed REORGANIZE PARTITION p2022, p2023 INTO (
  PARTITION p2022 VALUES LESS THAN (2023),
  PARTITION p2023 VALUES LESS THAN (2024),
  PARTITION p2024 VALUES LESS THAN (2025)
);

--echo # Check stats - PART_NORMAL partitions should retain their stats
if ($use_persistent_stats)
{
  --replace_column 3 #
  SELECT * FROM mysql.innodb_table_stats WHERE table_name LIKE '%sales_mixed%';
}
if (!$use_persistent_stats)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS
WHERE TABLE_NAME = 'sales_mixed' AND TABLE_SCHEMA = 'test'
ORDER BY PARTITION_NAME;
}

--echo # Verify actual row counts in all partitions
SELECT 
  'p2020' as partition_name,
  COUNT(*) as actual_rows
FROM sales_mixed PARTITION (p2020)
UNION ALL
SELECT 'p2021', COUNT(*) FROM sales_mixed PARTITION (p2021)
UNION ALL
SELECT 'p2022', COUNT(*) FROM sales_mixed PARTITION (p2022)
UNION ALL
SELECT 'p2023', COUNT(*) FROM sales_mixed PARTITION (p2023)
UNION ALL
SELECT 'p2024', COUNT(*) FROM sales_mixed PARTITION (p2024)
ORDER BY partition_name;

DROP TABLE sales_mixed;
