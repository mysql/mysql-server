Bug #54466 client 5.5 built from source lacks "pager" support
a
1
#
# Bug#21074643: SERVER SETS OPEN_FILES_LIMIT UNCONDITIONALLY
#
# Filtering out the warning since platforms Solaris and osx set the
# open_files_limit to too low a value after setrlimit fails to set
# open_files_limit to 10000000
CALL mtr.add_suppression("innodb_open_files should not be greater than the open_files_limit.");
CALL mtr.add_suppression("Changed limits: table_open_cache: *");
CALL mtr.add_suppression("Could not increase number of max_open_files to more than *");
SELECT @@open_files_limit = 10000000;
@@open_files_limit = 10000000
0

End of tests
