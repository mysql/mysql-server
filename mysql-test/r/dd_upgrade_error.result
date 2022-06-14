# This test scripts covers upgrade of tables, views, tablespaces,
# events, stored routines and triggers from 5.7 to 8.0.
# Set different paths for --datadir
# Stop DB server which was created by MTR default
# Copy the remote tablespace & DB zip files from suite location to working location.
# Check that the file exists in the working folder.
# Unzip the zip file.
# Create a bootstrap file in temp location
MYSQL_TMP_DIR/bootstrap.log
# Copy mismatch .frms to test_mismatch dir
# Copy pre 5.0 decimal files to test dir
# Starting the DB server will fail since the data dir contains
# tables with non native partitioning.
Pattern "Unknown storage engine 'partition'" found
Pattern "Table upgrade required for `test`.`41_decimal`. Please dump/reload table to fix it!" found
Pattern "Error in creating TABLE_SHARE from p4_hash.frm file." found
Pattern "Error in fixing SE data for test_mismatch.t1_col_name" found
Pattern "Error in fixing SE data for test_mismatch.t2_col_type" found
Pattern "Error in fixing SE data for test_mismatch.t3_col_num" found
Pattern "Error in fixing SE data for test_mismatch.t4_index_name" found
Pattern "Error in fixing SE data for test_mismatch.t5_index_cols" found
Pattern "Error in fixing SE data for test_mismatch.t6_indexes_num" found
Pattern "Error in fixing SE data for test_mismatch.t7_indexes_unique" found
Pattern "Error in fixing SE data for test_mismatch.t8_col_type_nulls" found
Pattern "Error in fixing SE data for test_mismatch.t9_index_prefix_len" found
Pattern "Table mismatch_frms/t1_col_name is not found in InnoDB dictionary" found
# ------------------------------------------------------------------
# Check upgrade of tablespaces with a missing tablespace file.
# ------------------------------------------------------------------
# Upgrade fails for the case where the general tablespace file is removed.
Pattern "Error in fixing SE data for tablespace.t3" found
# Upgrade fails for the case where the file per tablespace is removed.
Pattern "Got error 197 from SE while migrating tablespaces" found
# Remove copied files
# ------------------------------------------------------------------
# Check upgrade of tables and stored programs using long enum elements.
# ------------------------------------------------------------------
# Set different path for --datadir
# DB server which was started above is not running, no need for shutdown
# Copy the remote tablespace & DB zip files from suite location to working location.
# Check that the file exists in the working folder.
# Unzip the zip file.
#
# BUG#26743291 : VIEW WITH EXPLICIT COLUMN NAME > 64 IS MARKED
#                INVALID WHEN UPGRADED TO 8.0.
# Starting the DB server will fail since the data dir contains
# a table and a stored program with a too long enum literals.
Pattern "Error in Creating DD entry for test.t_long_enum" found
Pattern "Error in creating stored program 'test.p_long_enum'" found
Pattern "Error in processing \(possibly deprecated\) expression or function 'des_encrypt\(`b`\)' for generated column test.t_gcol_dep.c" found
Pattern "Error in processing generated columns for table test.t_gcol_dep" found
Pattern "No database selected" not found
Pattern "Incorrect column name 'a123456789012345678901234567890123456789012345678901234567890123456789'" found
# Remove copied files
# ------------------------------------------------------------------
# Check upgrade from 5.7 when events/triggers/views/routines contain GROUP BY DESC.
# ------------------------------------------------------------------
# Set different path for --datadir
# DB server which was started above is not running, no need for shutdown
# Copy the remote tablespace & DB zip files from suite location to working location.
# Check that the file exists in the working folder.
# Unzip the zip file.
#
# Upgrade tests for WL#8693
#
# Starting the DB server will fail since the data dir contains
# events/triggers/views/routines contain GROUP BY DESC
Pattern "Trigger 'trigger_groupby_desc' has an error in its body" found
Pattern "Error in parsing Triggers from t1.TRG file" found
Pattern "Error in parsing Event 'test'.'event_groupby_desc' during upgrade" found
Pattern "Error in parsing Routine 'test'.'function_groupby_desc' during upgrade" found
Pattern "Error in parsing Routine 'test'.'procedure_groupby_desc' during upgrade" found
Pattern "Error in parsing View 'test'.'view_groupby_desc' during upgrade" found
Pattern "Failed to Populate DD tables." found
# Remove copied files
# ------------------------------------------------------------------
# Bug#28760011: UPGRADE TO 8.0.12 FAILS
# Check upgrade from 5.7 when triggers are listed in wrong order in the TRG file.
# ------------------------------------------------------------------
# Set different path for log file and datadir, and unzip the zip file.
# DB server which was started above is not running, no need for shutdown, just start
# the server. This will not fail after the bugfix since the trigger order is now corrected.
# Stop the server.
# Remove the datadir, but keep the error log in case.

# ------------------------------------------------------------------
# Bug#31335554 8.0 CRASHES ON UPGRADING DATADIR WITH ORPHAN EVENTS
# ------------------------------------------------------------------

# Testcase 1: Upgrading with orphan event should fail with an error.

# Copy the 5.7 data zip file to working directory.
# Check that the zip file exists in the working directory.
# Unzip 5.7 data directory.
# Set data directory to the 5.7 data directory.
# Set log directory.
# Start server to trigger upgrade.
# Upgrade will fail due to presence of orphan event.

# Should find this error in the log.
Pattern "The schema "the_cake_is_a_lie" referenced by event "e1" does not exist\. Please clean up any orphan events before upgrading\." found

# Cleanup.

# Testcase 2: Upgrading with orphan routine should fail with an error.

# Copy the 5.7 data zip file to working directory.
# Check that the zip file exists in the working directory.
# Unzip 5.7 data directory.
# Set data directory to the 5.7 data directory.
# Set log directory.
# Start server to trigger upgrade.
# Upgrade will fail due to presence of orphan routine.

# Should find this error in the log.
Pattern "The schema "xyz" referenced by routine "p1" does not exist\. Please clean up any orphan routines before upgrading\." found

# Cleanup.

# -----------------------------------------------------------------------
# Bug#33148961 FAILURE TO UPGRADE FROM 5.7, INVALID UTF8 CHARACTER STRING
# -----------------------------------------------------------------------

# Upgrade 5.7 data directory containing a table, index, column, and
# stored routine, each having an invalid character string in their
# comments. Also contains a stored routine with invalid character string
# in the body definition.

# Upgrade should fail with these errors in the log.
Pattern "Comment for index 'test\.t3\.idx1' contains an invalid utf8mb3 character string: '\\xBF\\xE1'\." found
Pattern "Comment for field 'test\.t2\.a' contains an invalid utf8mb3 character string: '\\xBF\\xE1'\." found
Pattern "Comment for table 'test\.t1' contains an invalid utf8mb3 character string: '\\xBF\\xE1'\." found
Pattern "Definition of stored routine 'test\.p1' contains an invalid utf8mb3 character string: '\\xF0\\x9F\\x90'\." found

# Cleanup.

# Upgrade 5.7 data directory containing a view with invalid body
# definition string.

# Upgrade should fail with these errors in the log.
Pattern "Definition of view 'test\.v1' contains an invalid utf8mb3 character string: '\\xF0\\x9F\\x90'\." found

# Cleanup.

# ------------------------------------------------------------------
# End-of-test cleanup.
# ------------------------------------------------------------------
# Restart the server with default options.
# restart
