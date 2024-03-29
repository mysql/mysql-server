SET @@global.log_output = @old_log_output;
SET @@global.general_log = @old_general_log;
SET @@global.general_log_file = @old_general_log_file;
TRUNCATE TABLE mysql.general_log;
