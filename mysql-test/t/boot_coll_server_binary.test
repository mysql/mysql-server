--echo #
--echo # Bug#25054104 DATA DICTIONARY INITIALIZATION FAILS WHEN USING BINARY
--echo # CHARSETS
--echo #
--echo # Note that this test also tests dictionary initialization
--echo # using .opt file with --character_set_server=binary
--echo # --character_set_filesystem=binary --collation_server=binary
--echo # server command line options.

# Make sure the COLLATE clause recognizes 'binary' collation.
CREATE SCHEMA s1 COLLATE binary;
DROP SCHEMA s1;
