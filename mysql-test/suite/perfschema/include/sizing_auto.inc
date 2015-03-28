
show variables like "table_definition_cache";
show variables like "table_open_cache";
show variables like "max_connections";
# open_files_limit depends on OS configuration (ulimit -n)
#show variables like "open_files_limit";
show variables where
    `Variable_name` != "performance_schema_max_statement_classes" and
    `Variable_name` like "performance_schema%";
show status like "%performance_schema%";

# Each test script should provide a different test.cnf file,
# with different settings.
# This output will show the sizes computed automatically.
# Note that this output is very dependent on the platform.
# The output of SHOW ENGINE PERFORMANCE_SCHEMA STATUS
# is very dependent on the platform,
# so it is not printed here to ensure stability of the .results files.
# To troubleshoot the performance schema memory consumption at different
# configuration settings, comment the following line.
# Debug only:

# show engine performance_schema status;

