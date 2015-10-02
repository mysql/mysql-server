eval CREATE TABLE t575 (a int PRIMARY KEY, b varchar(63))
ENGINE = InnoDB
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (100),
 PARTITION p1 VALUES LESS THAN (200),
 PARTITION pmax VALUES LESS THAN MAXVALUE);

INSERT INTO t575 (a) VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9),(10);
UPDATE t575 SET b = CONCAT("Filler_", a);
INSERT INTO t575 SELECT a + 100, CONCAT("Filler_", a + 100) FROM t575;
INSERT INTO t575 SELECT a + 200, CONCAT("Filler_", a + 200) FROM t575;

if (! $TABLE_NAME)
{
  let $TABLE_NAME=t575;
}
if (`SELECT "$TABLE_NAME" != "t575"`)
{
  eval RENAME TABLE t575 TO $TABLE_NAME;
}

--echo # Flush out the old table definition so it can be replaced
eval FLUSH TABLE $TABLE_NAME;

--echo # Replace the .frm and copy a .par file, so it is the same as a 5.7.5
--echo # table. InnoDB internals is the same. Notice char(64) instead of (63)!
--remove_file $MYSQLD_DATADIR/test/$TABLE_NAME.frm
--copy_file std_data/parts/t575.frm $MYSQLD_DATADIR/test/$TABLE_NAME.frm
--copy_file std_data/parts/t575.par $MYSQLD_DATADIR/test/$TABLE_NAME.par
