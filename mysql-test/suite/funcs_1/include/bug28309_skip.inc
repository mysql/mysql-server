# Skip tests which suffer from
#    Bug#28309 First insert violates unique constraint
#              - was "memory" table empty?
# if the folowing conditions are fulfilled:
# - MySQL Version is 5.0 (Bug is fixed in 5.1 and up)
# - use of embedded server
# - run on a case insensitive filesystem
#
let $value= query_get_value(SHOW VARIABLES LIKE 'lower_case_file_system', Value, 1);
if (`SELECT '$value' = 'ON' AND VERSION() LIKE '5.0%embedded%'`)
{
   skip # Test requires backport of fix for Bug#28309 First insert violates unique constraint - was "memory" table empty ?;
}
