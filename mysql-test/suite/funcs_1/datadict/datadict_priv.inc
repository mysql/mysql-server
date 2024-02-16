############## suite/funcs_1/datadict/datadict_priv.inc ################
#                                                                      #
# DDL and DML operations on information_schema tables                  #
#                                                                      #
# Creation:                                                            #
# 2007-08    hhunger Implement this test as part of                    #
#                    WL#3982 Test information_schema.processlist       #
#                                                                      #
# Last update:                                                         #
# 2007-08-14 mleich  Some cleanup                                      #
#                                                                      #
########################################################################

# These variables have to be set before sourcing this file.
#
# information_schema table to be tested
# let $table= processlist;
#
# columns of the information_schema table e.g. to use in a select.
# let $columns= ID, USER, HOST, DB, COMMAND, TIME, STATE, INFO;
#
# Where clause for an update.
# let $update_where= WHERE id=1 ;
#
# Column to be used in the SET of an update.
# let $set_column= user='any_user' ;
#
# Where clause of a delete.
# let $delete_where= WHERE id=1 ;
#
# Column to be dropped.
# let $drop_column= user;
#
# Column to be indexed
# let $index_col= user;


# data access

eval CREATE TEMPORARY TABLE test.t_$table AS SELECT * FROM $table;

eval UPDATE test.t_$table SET user='horst' $update_where ;

--error ER_DBACCESS_DENIED_ERROR
eval INSERT INTO $table SELECT * FROM test.t_$table;

# bug#30208: CREATE TABLE ...LIKE does not accept dbname.tablename:unknown database
eval DROP TABLE test.t_$table;

--error ER_VIEW_NONUPD_CHECK
eval CREATE VIEW test.v_$table ($columns) AS SELECT * FROM $table WITH CHECK OPTION;

eval CREATE VIEW test.v_$table ($columns) AS SELECT * FROM $table;

--error ER_DBACCESS_DENIED_ERROR
eval UPDATE test.v_$TABLE SET TIME=NOW() WHERE id = 1;

eval DROP VIEW test.v_$table;

--error ER_DBACCESS_DENIED_ERROR
eval UPDATE $table SET $set_column $update_where;

--error ER_DBACCESS_DENIED_ERROR
eval DELETE FROM $table $delete_where;

# change privileges

--error ER_DBACCESS_DENIED_ERROR
eval REVOKE ALL ON $table FROM current_user;

--error ER_DBACCESS_DENIED_ERROR
eval GRANT INSERT,UPDATE ON $table TO current_user;

--let $user = 0
--source include/show_grants.inc

#----------------------------------------------------------------------
# table access

--error ER_DBACCESS_DENIED_ERROR
eval CREATE INDEX i_$table ON $table ($index_col);

--error ER_DBACCESS_DENIED_ERROR
eval DROP TABLE $table;

--error ER_DBACCESS_DENIED_ERROR
eval ALTER TABLE $table DROP COLUMN $drop_column;

--error ER_DBACCESS_DENIED_ERROR
eval ALTER TABLE $table ADD COLUMN (my_column INT);

--error ER_DBACCESS_DENIED_ERROR
eval RENAME TABLE $table TO new_$table;

--error ER_DBACCESS_DENIED_ERROR
eval RENAME TABLE $table TO files;

--error ER_DBACCESS_DENIED_ERROR
eval CREATE TABLE new_$table AS SELECT * FROM $table;

#----------------------------------------------------------------------
# database access

--error ER_DBACCESS_DENIED_ERROR
DROP DATABASE information_schema;

--error ER_PARSE_ERROR
RENAME DATABASE information_schema TO info_schema;
