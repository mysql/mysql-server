SET @start_value = @@global.binlog_row_event_max_size;
SELECT @start_value;
@start_value
8192
include/assert.inc [Assert that the default value is correct]
SET @@global.binlog_row_event_max_size = 256;
ERROR HY000: Variable 'binlog_row_event_max_size' is a read only variable
SET @@session.binlog_row_event_max_size = 256;
ERROR HY000: Variable 'binlog_row_event_max_size' is a read only variable
SET @@persist_only.binlog_row_event_max_size = 256;
include/assert.inc [Assert that the variable has been marked as persisted]
RESET PERSIST binlog_row_event_max_size;
include/assert.inc [Assert that the variable is not marked as persisted]
SET @@persist.binlog_row_event_max_size = 256;
ERROR HY000: Variable 'binlog_row_event_max_size' is a read only variable
SET @@persist_only.binlog_row_event_max_size = MIN;
include/assert.inc [Assert that the variable is set to min]
SET @@persist_only.binlog_row_event_max_size = MIN_MINUS_ONE;
include/assert.inc [Assert that the variable is set to minimum minus one]
SET @@persist_only.binlog_row_event_max_size = MAX;
include/assert.inc [Assert that the variable is set to max]
SET @@persist_only.binlog_row_event_max_size = OVERFLOW;
ERROR 42000: Incorrect argument type to variable 'binlog_row_event_max_size'
SET @@persist_only.binlog_row_event_max_size = DEFAULT;
include/assert.inc [Assert that the variable is set to DEFAULT]
RESET PERSIST binlog_row_event_max_size;
