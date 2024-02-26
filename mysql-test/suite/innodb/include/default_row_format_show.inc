if (!$TEMPORARY) {
--source suite/innodb/include/show_i_s_tables.inc
}

if ($TEMPORARY) {
SHOW CREATE TABLE test.t1;
}
