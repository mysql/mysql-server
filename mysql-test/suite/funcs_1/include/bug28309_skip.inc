# Skip tests which suffer from
#    Bug#28309 First insert violates unique constraint
#              - was "memory" table empty?
# if the folowing conditions are fulfilled:
# - MySQL Version is 5.0 (Bug is fixed in 5.1 and up)
# - run on a case insensitive filesystem
#
let $value= query_get_value(SHOW VARIABLES LIKE 'lower_case_file_system', Value, 1);
