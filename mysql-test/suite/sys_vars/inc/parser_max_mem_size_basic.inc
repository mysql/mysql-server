#
# Basic test for parser_max_mem_size
#

--source include/not_embedded.inc
--source include/load_sysvars.inc

--echo # Display the DEFAULT value of parser_max_mem_size

SET @@session.parser_max_mem_size = DEFAULT;
SELECT @@session.parser_max_mem_size;

--echo # Change the value of parser_max_mem_size to a valid
--echo # value for SESSION Scope

SET @@session.parser_max_mem_size = 500 * 1000;
SELECT @@session.parser_max_mem_size;

--echo # Change the value of the parser_max_mem_size to a 
--echo # value greater than --maximum-parser-max-mem-size
SET session parser_max_mem_size = 1000 * 1000;
SELECT @@session.parser_max_mem_size;

--echo # Change the value of parser_max_mem_size to an invalid value

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.parser_max_mem_size= 'NOT_CHAR_TYPE';

SET @@session.parser_max_mem_size = -10;

--Error ER_WRONG_TYPE_FOR_VAR
SET @@session.parser_max_mem_size = 0.5;

--echo # Check if the value in session Table matches value in variable

--disable_warnings
## SET @@session.parser_max_mem_size = DEFAULT;
## SELECT * FROM INFORMATION_SCHEMA.SESSION_VARIABLES
## WHERE VARIABLE_NAME='parser_max_mem_size';
--enable_warnings

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

--echo # example user sets 450000
SET @@session.parser_max_mem_size = 450000;

--connection default

--echo # default sets global 400000
SET @@global.parser_max_mem_size = 400000;

-- connection conn_example

--echo # example user sets 450000
SET @@session.parser_max_mem_size = 450000;
SELECT @@session.parser_max_mem_size;

--echo # example user sets DEFAULT, which is now 400000
SET @@session.parser_max_mem_size = DEFAULT;
SELECT @@session.parser_max_mem_size;

-- connection default

--echo # default sets sesion 450000
SET @@session.parser_max_mem_size = 450000;
SELECT @@session.parser_max_mem_size;

--echo # disconnect
disconnect conn_example;
DROP USER example;

SET @@global.parser_max_mem_size = DEFAULT;

--echo # END OF parser_max_mem_size TESTS
