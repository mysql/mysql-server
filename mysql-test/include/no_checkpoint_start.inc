# Preparation for using no_checkpoint_end.inc

let MYSQLD_DATADIR= `select @@datadir`;
--replace_regex /.*Last checkpoint at[[:space:]]*([0-9]+).*/\1/gso
let CHECKPOINT_LSN=`SHOW ENGINE INNODB STATUS`;
