#
# WL#5968: Implement START TRANSACTION READ (WRITE|ONLY);
#
#
# Test9:  The --transaction-read-only option.
# Saving the initial value of transaction_read_only variable
SET @transaction_read_only_save = @@transaction_read_only;
SET @@global.transaction_read_only = ON;
# Also for new connections. Switching to con1
SELECT @@transaction_read_only;
@@transaction_read_only
1
SET SESSION TRANSACTION READ WRITE;
SELECT @@transaction_read_only;
@@transaction_read_only
0
# Connection default
SELECT @@transaction_read_only;
@@transaction_read_only
0
#
# Test 10: SET TRANSACTION / START TRANSACTION + implicit commit.
SET SESSION TRANSACTION READ WRITE;
SET TRANSACTION READ ONLY;
# Since DDL does implicit commit before starting, SET TRANSACTION
# will have no effect because the "next" transaction will already
# be over before the DDL statement starts.
CREATE TABLE t1 (a INT);
START TRANSACTION READ ONLY;
# The same happens with START TRANSACTION
DROP TABLE t1;
# Restoring the original value of transaction_read_only
SET @@global.transaction_read_only = @transaction_read_only_save;
