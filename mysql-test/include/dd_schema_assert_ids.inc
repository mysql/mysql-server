--echo ########################################################################
--echo # Verify the hard coded ids of the mysql schema, the mysql tablespace,
--echo # the dd_properties table, and the innodb_dynamic_metadata table.
--echo ########################################################################

--source include/have_debug.inc
SET debug = '+d,skip_dd_table_access_check';

--echo ########################################################################
--echo # The id of the mysql schema must be fixed.
--echo ########################################################################
let $assert_cond = "[SELECT id FROM mysql.schemata WHERE name LIKE \'mysql\']"
                    = 1;
--let $assert_text = The id of the 'mysql' schema is 1.
--source include/assert.inc

--echo ########################################################################
--echo # The ids of the mysql tablespace must be fixed.
--echo ########################################################################
let $assert_cond = "[SELECT id FROM mysql.tablespaces WHERE name LIKE
                     \'mysql\']"
                    = 1;
--let $assert_text = The id of the 'mysql' tablespace is 1.
--source include/assert.inc

let $assert_cond = "[SELECT SUBSTRING_INDEX(SUBSTRING(se_private_data,
                              LOCATE(\'id\', se_private_data), 30), \';\', 1)
                       FROM mysql.tablespaces WHERE name LIKE \'mysql\']"
                    = "id=4294967294";

--let $assert_text = The SE private id of the 'mysql' tablespace is 4294967294.
--source include/assert.inc

--echo ########################################################################
--echo # The ids of the dd_properties table must be fixed.
--echo ########################################################################
let $assert_cond = "[SELECT id FROM mysql.tables
                       WHERE name LIKE \'dd_properties\' AND schema_id = 1]"
                    = 1;
--let $assert_text = The id of 'mysql.dd_properties' is 1.
--source include/assert.inc

let $assert_cond = "[SELECT se_private_id FROM mysql.tables
                       WHERE name LIKE \'dd_properties\' AND schema_id = 1]"
                    = 1;
--let $assert_text = The SE private id of 'mysql.dd_properties' is 1.
--source include/assert.inc

let $assert_cond = "[SELECT id FROM mysql.indexes
                       WHERE table_id =
                         (SELECT id FROM mysql.tables
                            WHERE name LIKE \'dd_properties\'
                            AND schema_id = 1)]"
                    = 1;
--let $assert_text = The clustering index id of 'mysql.dd_properties' is 1.
--source include/assert.inc

--echo ########################################################################
--echo # The ids of the innodb_dynamic_metadata table must be fixed.
--echo ########################################################################
let $assert_cond = "[SELECT id FROM mysql.tables
                       WHERE name LIKE \'innodb_dynamic_metadata\'
                       AND schema_id = 1]"
                    = 2;
--let $assert_text = The id of 'mysql.innodb_dynamic_metadata' is 2.
--source include/assert.inc

let $assert_cond = "[SELECT se_private_id FROM mysql.tables
                       WHERE name LIKE \'innodb_dynamic_metadata\'
                       AND schema_id = 1]"
                    = 2;
--let $assert_text = The SE private id of 'mysql.innodb_dynamic_metadata' is 2.
--source include/assert.inc

let $assert_cond = "[SELECT id FROM mysql.indexes
                       WHERE table_id =
                         (SELECT id FROM mysql.tables
                            WHERE name LIKE \'innodb_dynamic_metadata\'
                            AND schema_id = 1)]"
                    = 2;
--let $assert_text = The index id of 'mysql.innodb_dynamic_metadata' is 2.
--source include/assert.inc

SET debug = '-d,skip_dd_table_access_check';
