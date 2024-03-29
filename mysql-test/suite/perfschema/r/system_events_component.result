## Test the Performance Schema System service
##
## Verify that the SOURCE column in all wait, stage, statement,
## transaction and metadata lock tables is set to NULL after a plugin
## or component is unloaded.
##
## 0. Setup
## 1. Install an example component to generate wait events
## 2. Generate stage, statement and transaction events
## 3. Unload the component / component
## 4. Verify that the SOURCE column is null in relevant tables
## 5. Re-install the test plugin
## 6. Verify that the SOURCE column is re-enabled
## 7. Cleanup

#
# 0) Setup
#
Connection default
USE performance_schema;

# Disable all events from the control thread.
UPDATE performance_schema.threads SET instrumented = 'NO' WHERE processlist_id = CONNECTION_ID();

# Disable all events.
UPDATE performance_schema.setup_instruments SET enabled = 'NO', timed = 'NO';

# Clear the tables that have a SOURCE column.
TRUNCATE events_waits_current;
TRUNCATE events_waits_history;
TRUNCATE events_waits_history_long;
TRUNCATE events_stages_current;
TRUNCATE events_stages_history;
TRUNCATE events_stages_history_long;
TRUNCATE events_statements_current;
TRUNCATE events_statements_history;
TRUNCATE events_statements_history_long;
TRUNCATE events_transactions_current;
TRUNCATE events_transactions_history;
TRUNCATE events_transactions_history_long;
# metadata_locks is not truncatable

#
# 1) Install a component that generates its own wait events.
#
Connection con0
USE test;
INSTALL COMPONENT 'file://component_pfs_example';

Connection default

# Enable events that record the source file and line.
UPDATE performance_schema.setup_instruments SET enabled = 'yes', timed = 'yes'
  WHERE name LIKE 'stage/%' OR name LIKE 'statement/%' OR name = 'transaction' OR
name LIKE '%metadata/sql%' OR name LIKE 'wait/synch/mutex/sql/THD::%';

Connection con0

#
# 2) Generate wait, stage, statement, transaction and metadata lock events. Keep the transaction open for now.
#
CREATE TABLE t1 (s1 INT);
START TRANSACTION;
INSERT INTO t1 VALUES (1), (2), (3);

Connection default

# Verify that the component registered and generated the wait events.
## TODO Resolve timing issues with events_waits_history. ##
# SELECT (COUNT(*) > 0) AS "Expect 1" FROM performance_schema.events_waits_history WHERE event_name LIKE "%pfs_example%";
# SELECT (COUNT(*) > 0) AS "Expect 1" FROM performance_schema.events_waits_history_long WHERE event_name LIKE "%pfs_example%";

# Verify that each table is non-empty and that the SOURCE columns are populated.

SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_waits_current AS t1 WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_waits_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_waits_history_long WHERE source <> "";
Expect 1
1
# May or may not contain rows, but shares same code path as _history and _history_long
# SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_stages_current WHERE source <> "";

SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_stages_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_stages_history_long WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_statements_current WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_statements_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_statements_history_long WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_transactions_current WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_transactions_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_transactions_history_long WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM metadata_locks WHERE object_schema = "test" AND source <> "";
Expect 1
1

#
# 3) Uninstall the test component to force a reset of the SOURCE columns.
#
UNINSTALL COMPONENT 'file://component_pfs_example';

#
# 4) Verify that the SOURCE column was reset for all events.
#
SELECT (COUNT(*) = 0) AS "Expect 1" FROM events_waits_current WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) = 0) AS "Expect 1" FROM events_waits_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) = 0) AS "Expect 1" FROM events_waits_history_long WHERE source <> "";
Expect 1
1
# May or may not contain rows, but shares same code path as _history and _history_long
# SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_stages_current WHERE source = "";

# Some stage events occur after the component is unloaded, so confirm that at least some of the SOURCE columns were reset.
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_stages_history WHERE source = "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_stages_history_long WHERE source = "";
Expect 1
1

SELECT (COUNT(*) = 0) AS "Expect 1" FROM events_statements_current WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) = 0) AS "Expect 1" FROM events_statements_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) = 0) AS "Expect 1" FROM events_statements_history_long WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) = 0) AS "Expect 1" FROM events_transactions_current WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) = 0) AS "Expect 1" FROM events_transactions_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) = 0) AS "Expect 1" FROM events_transactions_history_long WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) = 0) AS "Expect 1" FROM metadata_locks WHERE object_schema = "test" AND source <> "";
Expect 1
1

Connection con0

# Commit the open transaction from step 2.
COMMIT;

Connection default

# Clear tables with SOURCE columns.
TRUNCATE events_waits_current;
TRUNCATE events_waits_history;
TRUNCATE events_waits_history_long;
TRUNCATE events_stages_current;
TRUNCATE events_stages_history;
TRUNCATE events_stages_history_long;
TRUNCATE events_statements_current;
TRUNCATE events_statements_history;
TRUNCATE events_statements_history_long;
TRUNCATE events_transactions_current;
TRUNCATE events_transactions_history;
TRUNCATE events_transactions_history_long;
# metadata_locks is not truncatable

Connection con0

#
# 5) Verify that the SOURCE columns are re-enabled after the reset.
#

# Re-install the component.
INSTALL COMPONENT 'file://component_pfs_example';

# Generate new wait, stage, statement and transaction events. Leave the transaction open.
START TRANSACTION;
INSERT INTO t1 VALUES (4), (5), (6);

Connection default

#
# 6) Verify that each table is non-empty and that the SOURCE columns are populated.
#
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_waits_current AS t1 WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_waits_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_waits_history_long WHERE source <> "";
Expect 1
1
# May or may not contain rows, but shares same code path as _history and _history_long
# SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_stages_current WHERE source <> "";

SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_stages_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_stages_history_long WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_statements_current WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_statements_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_statements_history_long WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_transactions_current WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_transactions_history WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM events_transactions_history_long WHERE source <> "";
Expect 1
1
SELECT (COUNT(*) > 0) AS "Expect 1" FROM metadata_locks WHERE object_schema = "test" AND source <> "";
Expect 1
1

Connection con0

#
# 7) Clean up
#

# Commit the open transaction from step 5.
COMMIT;

Connection default
DROP TABLE test.t1;
UNINSTALL COMPONENT 'file://component_pfs_example';
UPDATE performance_schema.setup_instruments SET enabled = 'YES', timed = 'YES';
