
--echo #
--echo # Checking dd string memory use with PFS
--echo #

--echo # Create and drop a basic table to populate the
--echo # shared cache with DD table objects.
CREATE TABLE t1(i int);
DROP TABLE t1;

--echo #
--echo # Truncating memory events table to avoid noise
TRUNCATE TABLE performance_schema.memory_summary_global_by_event_name;

--echo #
--echo # Saving baseline
CREATE TABLE baseline AS
  SELECT * FROM performance_schema.memory_summary_global_by_event_name
    WHERE EVENT_NAME = 'memory/sql/dd::String_type';

# Uncomment to see raw numbers
#SELECT * FROM baseline;

--echo #
--echo # Creating basic table
CREATE TABLE t1(i int);

--echo #
--echo # Collect PFS info after create
CREATE TABLE after_create AS
  SELECT * FROM performance_schema.memory_summary_global_by_event_name
    WHERE EVENT_NAME = 'memory/sql/dd::String_type';

# Uncomment to see raw numbers
#SELECT * FROM after_create;

--echo #
--echo # Create diffs table for simpler analysis
CREATE TABLE diffs (EVENT_NAME VARCHAR(512), COUNT_ALLOC_DIFF BIGINT,
                    COUNT_FREE_DIFF BIGINT,
                    SUM_NUMBER_OF_BYTES_ALLOC_DIFF BIGINT,
                    SUM_NUMBER_OF_BYTES_FREE_DIFF BIGINT) AS
  SELECT baseline.EVENT_NAME,
    after_create.COUNT_ALLOC - baseline.COUNT_ALLOC AS COUNT_ALLOC_DIFF,
    after_create.COUNT_FREE - baseline.COUNT_FREE AS COUNT_FREE_DIFF,
    after_create.SUM_NUMBER_OF_BYTES_ALLOC -
      baseline.SUM_NUMBER_OF_BYTES_ALLOC AS SUM_NUMBER_OF_BYTES_ALLOC_DIFF,
    after_create.SUM_NUMBER_OF_BYTES_FREE -
      baseline.SUM_NUMBER_OF_BYTES_FREE AS SUM_NUMBER_OF_BYTES_FREE_DIFF
   FROM after_create, baseline
   WHERE after_create.EVENT_NAME = baseline.EVENT_NAME;

# Uncomment this to get the raw numbers
# SELECT
#  *,
#  COUNT_ALLOC_DIFF - COUNT_FREE_DIFF AS ALLOC_IN_USE,
#  SUM_NUMBER_OF_BYTES_ALLOC_DIFF - SUM_NUMBER_OF_BYTES_FREE_DIFF
#    AS BYTES_IN_USE
#FROM diffs;

--echo #
--echo # Create percents table for simpler analysis
CREATE TABLE percents (EVENT_NAME VARCHAR(512), ALLOC_PCT DECIMAL(10,3),
                       FREE_PCT DECIMAL(10,3), BYTES_ALLOC_PCT DECIMAL(10,3),
                       BYTES_FREE_PCT DECIMAL(10,3));

INSERT INTO percents
  SELECT
    baseline.EVENT_NAME,
    100 * ROUND(diffs.COUNT_ALLOC_DIFF / baseline.COUNT_ALLOC, 5),
    100 * ROUND(diffs.COUNT_FREE_DIFF / baseline.COUNT_FREE, 5),
    100 * ROUND(diffs.SUM_NUMBER_OF_BYTES_ALLOC_DIFF / baseline.SUM_NUMBER_OF_BYTES_ALLOC, 5),
    100 * ROUND(diffs.SUM_NUMBER_OF_BYTES_FREE_DIFF / baseline.SUM_NUMBER_OF_BYTES_FREE, 5)
  FROM diffs, baseline
  WHERE diffs.EVENT_NAME = baseline.EVENT_NAME;

# Uncomment to see raw numbers
#SELECT * FROM percents;

--echo #
--echo # Checking increase in the number of allocations (threshold 19%)
--echo #
SELECT
  baseline.EVENT_NAME AS 'PSI MEMORY EVENT NAME',
  baseline.COUNT_ALLOC AS 'BASE ALLOC',
  after_create.COUNT_ALLOC AS 'POST ALLOC',
  diffs.COUNT_ALLOC_DIFF AS DELTA,
  percents.ALLOC_PCT AS PCT_CHANGE,
  'Exceeds threshold of +19%' AS REASON
FROM baseline JOIN (after_create, diffs, percents) ON
  (baseline.EVENT_NAME = after_create.EVENT_NAME AND
   baseline.EVENT_NAME = diffs.EVENT_NAME AND
   baseline.EVENT_NAME = percents.EVENT_NAME)
WHERE percents.ALLOC_PCT > 19;

--echo #
--echo # Checking increase in the number of bytes allocated (threshold 25%)
--echo #
SELECT
  baseline.EVENT_NAME AS 'PSI MEMORY EVENT NAME',
  baseline.SUM_NUMBER_OF_BYTES_ALLOC AS 'BASE ALLOC BYTES',
  after_create.SUM_NUMBER_OF_BYTES_ALLOC AS 'POST ALLOC BYTES',
  diffs.SUM_NUMBER_OF_BYTES_ALLOC_DIFF AS DELTA,
  percents.BYTES_ALLOC_PCT AS PCT_CHANGE,
  'Exceeds threshold of +29%' AS REASON
FROM baseline JOIN (after_create, diffs, percents) ON
  (baseline.EVENT_NAME = after_create.EVENT_NAME AND
   baseline.EVENT_NAME = diffs.EVENT_NAME AND
   baseline.EVENT_NAME = percents.EVENT_NAME)
WHERE percents.BYTES_ALLOC_PCT > 29;

--echo #
--echo # Cleaning up
DROP TABLE percents;
DROP TABLE diffs;
DROP TABLE after_create;
DROP TABLE t1;
DROP TABLE baseline;
