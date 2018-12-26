#
# Test for the optimizer cost model
#

# Validate that the optimizer cost configuration tables exists and
# has the expected content.
#
# Table: server_cost
#
# Mask out the content of the last_update column
--replace_column 3 #
SELECT * FROM mysql.server_cost;

# Check that it has the expected index defined
# The index should be:
#
#   PRIMARY (cost_name)
#
# We do not care about the cardinality of this index since it can vary
--replace_column 7 #
SHOW INDEX FROM mysql.server_cost;

#
# Table: engine_cost
#
# Mask out the content of the last_update column
--replace_column 5 #
SELECT * FROM mysql.engine_cost;

# Check that it has the expected index defined
# The index should be a unique index on:
#
#   PRIMARY (cost_name, engine_name, device_type)
#
# We do not care about the cardinality of this index since it can vary
--replace_column 7 #
SHOW INDEX FROM mysql.engine_cost;

#
# Test of updating cost constants in server_cost
#
# 1. Updating an existing cost constant
#
# Change the value
UPDATE mysql.server_cost
SET cost_value=0.1
WHERE cost_name="row_evaluate_cost";

--replace_column 3 #
SELECT *
FROM mysql.server_cost
WHERE cost_name="row_evaluate_cost";

# Reset it to its default value
UPDATE mysql.server_cost
SET cost_value=DEFAULT
WHERE cost_name="row_evaluate_cost";

--replace_column 3 #
SELECT *
FROM mysql.server_cost
WHERE cost_name="row_evaluate_cost";

#
# 2. Insert a new cost constant
#
INSERT INTO mysql.server_cost
VALUES ("lunch_cost", DEFAULT, CURRENT_TIMESTAMP, "Lunch is important", DEFAULT);

--replace_column 3 #
SELECT * FROM mysql.server_cost;

DELETE FROM mysql.server_cost
WHERE cost_name="lunch_cost";

#
# 3. Try to insert an already existing cost constant.
#
--error ER_DUP_ENTRY
INSERT INTO mysql.server_cost
VALUES ("row_evaluate_cost", DEFAULT, CURRENT_TIMESTAMP, "Faster CPU", DEFAULT);

#
# 4. Insert an entry with the same name as an existing cost constant
#    but in upper case.
#
--error ER_DUP_ENTRY
INSERT INTO mysql.server_cost
VALUES ("ROW_EVALUATE_COST", DEFAULT, CURRENT_TIMESTAMP, "Faster CPU", DEFAULT);

--replace_column 3 #
SELECT * FROM mysql.server_cost;

#
# Test of updating cost constants in engine_cost
#
# 1. Updating an existing cost constant
#
# Change the value
UPDATE mysql.engine_cost
SET cost_value=0.1
WHERE cost_name="io_block_read_cost";

--replace_column 5 #
SELECT *
FROM mysql.engine_cost
WHERE cost_name="io_block_read_cost";

# Reset it to its default value
UPDATE mysql.engine_cost
SET cost_value=DEFAULT
WHERE cost_name="io_block_read_cost";

--replace_column 5 #
SELECT *
FROM mysql.engine_cost
WHERE cost_name="io_block_read_cost";

#
# 2. Insert some new cost constant
#
INSERT INTO mysql.engine_cost
VALUES ("InnoDB", 2, "lunch_cost1", DEFAULT, CURRENT_TIMESTAMP, "Lunch 1", DEFAULT),
       ("InnoDB", 2, "lunch_cost2", DEFAULT, CURRENT_TIMESTAMP, "Lunch 2", DEFAULT);

--replace_column 5 #
SELECT * FROM mysql.engine_cost;

DELETE FROM mysql.engine_cost
WHERE cost_name LIKE "lunch_cost%";

#
# 3. Try to insert an already existing cost constant
#    (this should fail due to the primary key)
#
INSERT INTO mysql.engine_cost
VALUES ("default", 0, "lunch_cost", DEFAULT, CURRENT_TIMESTAMP, "Lunch", DEFAULT);

--error ER_DUP_ENTRY
INSERT INTO mysql.engine_cost
VALUES ("default", 0, "lunch_cost", DEFAULT, CURRENT_TIMESTAMP, "Lunch", DEFAULT);

--replace_column 5 #
SELECT * FROM mysql.engine_cost;

DELETE FROM mysql.engine_cost
WHERE cost_name="lunch_cost";

#
# 5. Insert an entry with the same name as an existing cost constant
#    but in upper case.
#    (the collation on the table should prevent this from being accepted)
#

--error ER_DUP_ENTRY
INSERT INTO mysql.engine_cost
VALUES ("default", 0, "IO_BLOCK_READ_COST", DEFAULT, CURRENT_TIMESTAMP,
        "Lunch", DEFAULT);

--replace_column 5 #
SELECT * FROM mysql.engine_cost;

#
# Test that the last_update column in the two tables are automatically 
# updated with a new time stamp value when updating the content of
# a row
#

CREATE TABLE server_cost_tmp (
  cost_name VARCHAR(64) NOT NULL,
  last_update TIMESTAMP
);

CREATE TABLE engine_cost_tmp (
  cost_name VARCHAR(64) NOT NULL,
  last_update TIMESTAMP
);

# Copy the last_update time stamp values form the cost tables
INSERT INTO server_cost_tmp
SELECT cost_name, last_update FROM mysql.server_cost;
INSERT INTO engine_cost_tmp
SELECT cost_name, last_update FROM mysql.engine_cost;

# Need to do a sleep to ensure that we get a new distinct time stamp
--sleep 1

#
# 1. Test server_cost table
#
# Update on entry in the server_cost table
UPDATE mysql.server_cost 
SET cost_value=0.1
WHERE cost_name="row_evaluate_cost";

# Validate that this has gotten a new time stamp and all others have the same
# This query should return "row_evaluate_cost"
SELECT mysql.server_cost.cost_name
FROM mysql.server_cost JOIN server_cost_tmp 
  ON mysql.server_cost.cost_name = server_cost_tmp.cost_name
WHERE mysql.server_cost.last_update > server_cost_tmp.last_update;

# Reset the cost value
UPDATE mysql.server_cost 
SET cost_value=DEFAULT
WHERE cost_name="row_evaluate_cost";

#
# 2. Test engine_cost table
#
# Update on entry in the engine_cost table
UPDATE mysql.engine_cost 
SET cost_value=2.0
WHERE cost_name="io_block_read_cost";

# Validate that this has gotten a new time stamp and all others have the same
# This query should return "io_block_read_cost"
SELECT mysql.engine_cost.cost_name
FROM mysql.engine_cost JOIN engine_cost_tmp 
  ON mysql.engine_cost.cost_name = engine_cost_tmp.cost_name
WHERE mysql.engine_cost.last_update > engine_cost_tmp.last_update;

# Reset the cost value
UPDATE mysql.engine_cost 
SET cost_value=DEFAULT
WHERE cost_name="io_block_read_cost";

DROP TABLE server_cost_tmp, engine_cost_tmp;

--echo
--echo WL#10128:  Add defaults column to optimizer cost tables
--echo

--echo Verify that default values are defined for all cost constants
SELECT COUNT(*) FROM mysql.server_cost WHERE default_value IS NULL;
SELECT COUNT(*) FROM mysql.engine_cost WHERE default_value IS NULL;

--echo Verify that default_value columns can not be updated
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
UPDATE mysql.server_cost SET default_value = 1.0;
--error ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN
UPDATE mysql.engine_cost SET default_value = 1.0;