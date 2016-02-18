#
# Basic test for parser_max_mem_size
#
# There are separate tests for 32bit and 64bit systems.
#
# There are separate tests for command line options
#   --parser-max-mem-size
#   --maximum-parser-max-mem-size
#

--source include/not_embedded.inc
--source include/load_sysvars.inc

--echo # Display the DEFAULT value of parser_max_mem_size
--echo # The DEFAULT is determined by --maximum-parser-max-mem-size

SELECT @@session.parser_max_mem_size;
SELECT @@global.parser_max_mem_size;

SET @@session.parser_max_mem_size = DEFAULT;
SELECT @@session.parser_max_mem_size;

--echo # Change the value of parser_max_mem_size to a valid
--echo # value for SESSION Scope

SET @@session.parser_max_mem_size = 500 * 1000 * 100;
SELECT @@session.parser_max_mem_size;

--echo # Change the value of the session parser_max_mem_size to a
--echo # value greater than --maximum-parser-max-mem-size
SET session parser_max_mem_size = 1000 * 1000 * 100;
SELECT @@session.parser_max_mem_size;

--echo # Change the value of the global parser_max_mem_size to a
--echo # value greater than --maximum-parser-max-mem-size
SET @@global.parser_max_mem_size = 1000 * 1000 * 100;
SELECT @@global.parser_max_mem_size;

--echo # Change the value of parser_max_mem_size to an invalid value

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.parser_max_mem_size= 'NOT_CHAR_TYPE';

SET @@session.parser_max_mem_size = -10;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.parser_max_mem_size = 0.5;

--echo # Check if TRUE and FALSE values can be used on variable

SET @@session.parser_max_mem_size = TRUE;
SELECT @@session.parser_max_mem_size;

SET @@session.parser_max_mem_size = FALSE;
SELECT @@session.parser_max_mem_size;

--echo # Restore initial value

SET @@session.parser_max_mem_size = DEFAULT;
SELECT @@session.parser_max_mem_size;

--echo # Creating a new user
CREATE USER example;
connect(conn_example, localhost, example,,);

--echo # example user sets 45000000
SET @@session.parser_max_mem_size = 45000000;

--connection default

--echo # default sets global 40000000
SET @@global.parser_max_mem_size = 40000000;

-- connection conn_example

--echo # example user sets 45000000
SET @@session.parser_max_mem_size = 45000000;
SELECT @@session.parser_max_mem_size;

--echo # example user sets DEFAULT, which is now 40000000
SET @@session.parser_max_mem_size = DEFAULT;
SELECT @@session.parser_max_mem_size;

--echo # disconnect
disconnect conn_example;
--source include/wait_until_disconnected.inc
-- connection default

--echo # default sets sesion 45000000
SET @@session.parser_max_mem_size = 45000000;
SELECT @@session.parser_max_mem_size;

DROP USER example;

SET @@global.parser_max_mem_size = DEFAULT;

--echo # END OF parser_max_mem_size TESTS
