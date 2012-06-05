
show variables like "table_definition_cache";
show variables like "table_open_cache";
show variables like "max_connections";
# Results vary by platform:
# show variables like "open_files_limit";
show variables like "%performance_schema%";
show status like "%performance_schema%";

# Each test script should provide a different test.cnf file,
# with different settings.
# This output will show the sizes computed automatically.
# Note that this output is very dependent on the platform.

show engine performance_schema status;

