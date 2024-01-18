# ==== Purpose ====
#
# Read a binlog.index or relay_log.index file into a table, and strip
# away the path separators in a platform-independent way.
#
# ==== Usage ====
#
# --let $index_file= FILE_NAME
# --let $table= TABLE_NAME
# --source include/rpl/read_binlog_index_into_table.inc
#
# Parameters:
#
#   $index_file
#     The index file where the filenames are stored.
#
#   $table
#     The filenames stored in $index_file will be read into the
#     'filename' column of this table.

--let $include_filename= rpl/read_binlog_index_into_table.inc
--source include/begin_include_file.inc

--disable_query_log

# Disable escape character since otherwise to avoid that the path
# separator under windows is treated as an escape.
#
# Trim leading .\ (on windows) or ./ (on other platforms).
#
# mtr does weird things with backslash escapes so we spell backslash
# as CHAR(92) instead.

eval LOAD DATA INFILE '$index_file'
  INTO TABLE $table
  FIELDS ESCAPED BY ''
  (@binlog_filename)
  SET filename =
    REPLACE(REPLACE(@binlog_filename, './', ''),
            CONCAT('.', CHAR(92)),
            '');

--let $include_filename= rpl/read_binlog_index_into_table.inc
--source include/end_include_file.inc
