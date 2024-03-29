# Preparation for using no_checkpoint_end.inc

let MYSQLD_DATADIR= `select @@datadir`;
replace_regex /\n/<EOL>/ /.*<EOL>Last checkpoint at[[:space:]]*([0-9]+)<EOL>.*/\1/;
let CHECKPOINT_LSN=`SHOW ENGINE INNODB STATUS`;
