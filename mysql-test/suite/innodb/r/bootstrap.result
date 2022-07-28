# Testcase for the following bugs
# Bug#16691130 - ASSERT WHEN INNODB_LOG_GROUP_HOME_DIR DOES NOT EXIST
# Bug#16418661 - CHANGING NAME IN FOR INNODB_DATA_FILE_PATH SHOULD NOT SUCCEED WITH LOG FILES
# Write tmp/log_file/my.cnf
# Start mysqld without the possibility to create undo tablespaces
# because a directory exists named 'undo_002'.
Pattern "\[ERROR\] .*MY-\d+.* File .*ibdata1: 'open' returned OS error 71. Cannot continue operation" found
auto.cnf
undo_002
# Start mysqld to create tablespaces according to my.cnf
# Backup the datadir
# 1. Start mysqld with non existent innodb_log_group_home_dir
Pattern "\[ERROR\] .* Failed to list redo log files in the redo log directory .path.to.non-existent" found
Pattern "\[ERROR\] .* Aborting" found
# 2. Start mysqld without ibdata1
Pattern "\[ERROR\] .* File .*ibdata1: 'open' returned OS error 71. Cannot continue operation" found
# 3. Start mysqld without ibdata2
Pattern "\[ERROR\] .* Tablespace size stored in header is \d+ pages, but" found
Pattern "\[ERROR\] .* Aborting" found
# 4. Start mysqld without ibdata1 & ibdata2
Pattern "\[ERROR\] .* File .*ibdata1: 'open' returned OS error 71. Cannot continue operation" found
# 5. Start mysqld without ibdata*, #ib_redo* and undo_00*
Pattern "\[ERROR\] .* File .*ibdata1: 'open' returned OS error 71. Cannot continue operation" found
# 6. Start mysqld without undo_002
Pattern "\[Note\] .* Creating UNDO Tablespace .*undo_002" found
# 7. Start mysqld without undo_001, undo_002
Pattern "\[Note\] .* Creating UNDO Tablespace .*undo_001" found
Pattern "\[Note\] .* Creating UNDO Tablespace .*undo_002" found
# 8. Start mysqld without #ib_redo*
Pattern "\[Note\] .* Apply batch completed" found
# 9. Add new datafile at the end
Pattern "\[Note\] .* File.*ibdata3. size is now 20 MB" found
# 10. Show that --innodb-undo-directory cannot be an ancestor of the datadir
Pattern "\[ERROR\] .* The innodb_undo_directory is not allowed to be an ancestor of the datadir" found
# 11. Try to initialize InnoDB in directory which has log files
Pattern "--initialize specified but the redo log directory .* has redo log files inside" found
# 12. Rename log file after shutdown and start MySQL (should succeed)
Pattern "[ERROR].*innodb-unknown-parameter-12.*" found
# Cleanup
# restart
