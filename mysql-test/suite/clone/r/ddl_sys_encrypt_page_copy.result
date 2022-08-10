# Prepare for creating Encrypted Table
# create bootstrap file
# Create and start mysqld with keyring plugin.
# restart: --datadir=ENCRYPT_DATADIR --early-plugin-load=keyring_file=KEYRING_PLUGIN --keyring_file_data=MYSQL_TMP_DIR/mysecret_keyring
INSTALL PLUGIN clone SONAME 'CLONE_PLUGIN';
# In connection CON1
CREATE TABLESPACE tbs;
CREATE TABLE t1(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
TABLESPACE = tbs;
# In connection DEFAULT
# 1. Enable REDO encryption during PAGE COPY
SET GLOBAL DEBUG = '+d,clone_ddl_abort_wait_page_copy';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
CLONE LOCAL DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
SET DEBUG_SYNC = 'clone_notify_ddl_after_state_block SIGNAL resume_clone';
SET GLOBAL innodb_redo_log_encrypt = ON;
ERROR HY000: Internal error: Simulated Clone DDL error
# In connection DEFAULT
SET GLOBAL DEBUG = '-d,clone_ddl_abort_wait_page_copy';
SET DEBUG_SYNC = 'RESET';
# 2. Enable UNDO encryption during PAGE COPY
SET GLOBAL DEBUG = '+d,clone_ddl_abort_wait_page_copy';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
CLONE LOCAL DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
SET DEBUG_SYNC = 'clone_notify_ddl_after_state_block SIGNAL resume_clone';
SET GLOBAL innodb_undo_log_encrypt = ON;
ERROR HY000: Internal error: Simulated Clone DDL error
# In connection DEFAULT
SET GLOBAL DEBUG = '-d,clone_ddl_abort_wait_page_copy';
SET DEBUG_SYNC = 'RESET';
# 3. Enable general tablespace encryption during PAGE COPY
SET GLOBAL DEBUG = '+d,clone_ddl_abort_wait_page_copy';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
CLONE LOCAL DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
SET DEBUG_SYNC = 'clone_notify_ddl_after_state_block SIGNAL resume_clone';
ALTER TABLESPACE tbs ENCRYPTION = 'Y';
# In connection DEFAULT
ERROR HY000: Concurrent DDL is performed during clone operation. Please try again.
SET GLOBAL DEBUG = '-d,clone_ddl_abort_wait_page_copy';
SET DEBUG_SYNC = 'RESET';
# 4.Disable general tablespace encryption during PAGE COPY
SET GLOBAL DEBUG = '+d,clone_ddl_abort_wait_page_copy';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_ddl WAIT_FOR resume_clone';
SET GLOBAL clone_autotune_concurrency = OFF;
SET GLOBAL clone_max_concurrency = 8;
CLONE LOCAL DATA DIRECTORY = 'CLONE_DATADIR';
# In connection CON1
SET DEBUG_SYNC = 'now WAIT_FOR start_ddl';
SET DEBUG_SYNC = 'clone_notify_ddl_after_state_block SIGNAL resume_clone';
ALTER TABLESPACE tbs ENCRYPTION = 'N';
# In connection DEFAULT
ERROR HY000: Concurrent DDL is performed during clone operation. Please try again.
SET GLOBAL DEBUG = '-d,clone_ddl_abort_wait_page_copy';
SET DEBUG_SYNC = 'RESET';
# Cleanup
DROP TABLE t1;
DROP TABLESPACE tbs;
# Uninstall clone
UNINSTALL PLUGIN clone;
# Restart server and remove encrypted data directory
# restart:
