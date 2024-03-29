# Display the DEFAULT value of histogram_generation_max_mem_size
SET @@session.histogram_generation_max_mem_size = DEFAULT;
SELECT @@session.histogram_generation_max_mem_size;
@@session.histogram_generation_max_mem_size
20000000
# Change the value of histogram_generation_max_mem_size to a valid
# value for (20 megabytes)
SET @@session.histogram_generation_max_mem_size = 20000000;
SELECT @@session.histogram_generation_max_mem_size;
@@session.histogram_generation_max_mem_size
20000000
# Change the value of histogram_generation_max_mem_size to a value
# outside of the valid range.
SET @@session.histogram_generation_max_mem_size = 0;
Warnings:
Warning	1292	Truncated incorrect histogram_generation_max_mem_size value: '0'
SELECT @@session.histogram_generation_max_mem_size;
@@session.histogram_generation_max_mem_size
1000000
# Change the value of histogram_generation_max_mem_size to an invalid value
SET @@session.histogram_generation_max_mem_size= 'NOT_CHAR_TYPE';
ERROR 42000: Incorrect argument type to variable 'histogram_generation_max_mem_size'
SET @@session.histogram_generation_max_mem_size = -10;
Warnings:
Warning	1292	Truncated incorrect histogram_generation_max_mem_size value: '-10'
SET @@session.histogram_generation_max_mem_size = 0.5;
ERROR 42000: Incorrect argument type to variable 'histogram_generation_max_mem_size'
# Check if the value in session Table matches value in variable
SET @@session.histogram_generation_max_mem_size = DEFAULT;
SELECT * FROM performance_schema.session_variables
WHERE VARIABLE_NAME='histogram_generation_max_mem_size';
VARIABLE_NAME	VARIABLE_VALUE
histogram_generation_max_mem_size	20000000
# Check if TRUE and FALSE values can be used on variable
SET @@session.histogram_generation_max_mem_size = TRUE;
Warnings:
Warning	1292	Truncated incorrect histogram_generation_max_mem_size value: '1'
SELECT @@session.histogram_generation_max_mem_size;
@@session.histogram_generation_max_mem_size
1000000
SET @@session.histogram_generation_max_mem_size = FALSE;
Warnings:
Warning	1292	Truncated incorrect histogram_generation_max_mem_size value: '0'
SELECT @@session.histogram_generation_max_mem_size;
@@session.histogram_generation_max_mem_size
1000000
# Restore initial value
SET @@session.histogram_generation_max_mem_size = DEFAULT;
SELECT @@session.histogram_generation_max_mem_size;
@@session.histogram_generation_max_mem_size
20000000
# Create a user with non-root privileges, and verifty that the variable
# cannot be modified.
CREATE USER non_root@localhost IDENTIFIED BY '';
SET @@session.histogram_generation_max_mem_size = 450000000;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
DROP USER non_root@localhost;
# END OF histogram_generation_max_mem_size TESTS
