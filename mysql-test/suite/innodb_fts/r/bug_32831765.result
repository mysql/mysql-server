#
# Bug#32831765 SERVER HITS OOM CONDITION WHEN LOADING TWO
# INNODB TABLES WITH FTS INDEXES
#
create table t1 ( `id` int unsigned NOT NULL AUTO_INCREMENT, `col01` text,
`col02` text, `col03` text, `col04` text, `col05` text, `col06` text, `col07`
text, `col08` text, `col09` text, `col10` text, `col11` text, `col12` text,
`col13` text, `col14` text, `col15` text, `col16` text, `col17` text, `col18`
text, `col19` text, `col20` text, `col21` text, `col22` text, `col23` text,
`col24` text, `col25` text, `col26` text, `col27` text, `col28` text, `col29`
text, `col30` text, PRIMARY KEY (`id`), FULLTEXT KEY (`col01`), FULLTEXT KEY
(`col02`), FULLTEXT KEY (`col03`), FULLTEXT KEY (`col04`), FULLTEXT KEY
(`col05`), FULLTEXT KEY (`col06`), FULLTEXT KEY (`col07`), FULLTEXT KEY
(`col08`), FULLTEXT KEY (`col09`), FULLTEXT KEY (`col10`), FULLTEXT KEY
(`col11`), FULLTEXT KEY (`col12`), FULLTEXT KEY (`col13`), FULLTEXT KEY
(`col14`), FULLTEXT KEY (`col15`), FULLTEXT KEY (`col16`), FULLTEXT KEY
(`col17`), FULLTEXT KEY (`col18`), FULLTEXT KEY (`col19`), FULLTEXT KEY
(`col20`), FULLTEXT KEY (`col21`), FULLTEXT KEY (`col22`), FULLTEXT KEY
(`col23`), FULLTEXT KEY (`col24`), FULLTEXT KEY (`col25`), FULLTEXT KEY
(`col26`), FULLTEXT KEY (`col27`), FULLTEXT KEY (`col28`), FULLTEXT KEY
(`col29`), FULLTEXT KEY (`col30`)) engine=innodb;
create table t2 ( `id` int unsigned NOT NULL AUTO_INCREMENT, `col01` text,
`col02` text, `col03` text, `col04` text, `col05` text, `col06` text, `col07`
text, `col08` text, `col09` text, `col10` text, `col11` text, `col12` text,
`col13` text, `col14` text, `col15` text, `col16` text, `col17` text, `col18`
text, `col19` text, `col20` text, `col21` text, `col22` text, `col23` text,
`col24` text, `col25` text, `col26` text, `col27` text, `col28` text, `col29`
text, `col30` text, PRIMARY KEY (`id`), FULLTEXT KEY (`col01`), FULLTEXT KEY
(`col02`), FULLTEXT KEY (`col03`), FULLTEXT KEY (`col04`), FULLTEXT KEY
(`col05`), FULLTEXT KEY (`col06`), FULLTEXT KEY (`col07`), FULLTEXT KEY
(`col08`), FULLTEXT KEY (`col09`), FULLTEXT KEY (`col10`), FULLTEXT KEY
(`col11`), FULLTEXT KEY (`col12`), FULLTEXT KEY (`col13`), FULLTEXT KEY
(`col14`), FULLTEXT KEY (`col15`), FULLTEXT KEY (`col16`), FULLTEXT KEY
(`col17`), FULLTEXT KEY (`col18`), FULLTEXT KEY (`col19`), FULLTEXT KEY
(`col20`), FULLTEXT KEY (`col21`), FULLTEXT KEY (`col22`), FULLTEXT KEY
(`col23`), FULLTEXT KEY (`col24`), FULLTEXT KEY (`col25`), FULLTEXT KEY
(`col26`), FULLTEXT KEY (`col27`), FULLTEXT KEY (`col28`), FULLTEXT KEY
(`col29`), FULLTEXT KEY (`col30`)) engine=innodb;
create table t3 ( `id` int unsigned NOT NULL AUTO_INCREMENT, `col01` text,
`col02` text, `col03` text, `col04` text, `col05` text, `col06` text, `col07`
text, `col08` text, `col09` text, `col10` text, `col11` text, `col12` text,
`col13` text, `col14` text, `col15` text, `col16` text, `col17` text, `col18`
text, `col19` text, `col20` text, `col21` text, `col22` text, `col23` text,
`col24` text, `col25` text, `col26` text, `col27` text, `col28` text, `col29`
text, `col30` text, PRIMARY KEY (`id`), FULLTEXT KEY (`col01`), FULLTEXT KEY
(`col02`), FULLTEXT KEY (`col03`), FULLTEXT KEY (`col04`), FULLTEXT KEY
(`col05`), FULLTEXT KEY (`col06`), FULLTEXT KEY (`col07`), FULLTEXT KEY
(`col08`), FULLTEXT KEY (`col09`), FULLTEXT KEY (`col10`), FULLTEXT KEY
(`col11`), FULLTEXT KEY (`col12`), FULLTEXT KEY (`col13`), FULLTEXT KEY
(`col14`), FULLTEXT KEY (`col15`), FULLTEXT KEY (`col16`), FULLTEXT KEY
(`col17`), FULLTEXT KEY (`col18`), FULLTEXT KEY (`col19`), FULLTEXT KEY
(`col20`), FULLTEXT KEY (`col21`), FULLTEXT KEY (`col22`), FULLTEXT KEY
(`col23`), FULLTEXT KEY (`col24`), FULLTEXT KEY (`col25`), FULLTEXT KEY
(`col26`), FULLTEXT KEY (`col27`), FULLTEXT KEY (`col28`), FULLTEXT KEY
(`col29`), FULLTEXT KEY (`col30`)) engine=innodb;
create table t4 ( `id` int unsigned NOT NULL AUTO_INCREMENT, `col01` text,
`col02` text, `col03` text, `col04` text, `col05` text, `col06` text, `col07`
text, `col08` text, `col09` text, `col10` text, `col11` text, `col12` text,
`col13` text, `col14` text, `col15` text, `col16` text, `col17` text, `col18`
text, `col19` text, `col20` text, `col21` text, `col22` text, `col23` text,
`col24` text, `col25` text, `col26` text, `col27` text, `col28` text, `col29`
text, `col30` text, PRIMARY KEY (`id`), FULLTEXT KEY (`col01`), FULLTEXT KEY
(`col02`), FULLTEXT KEY (`col03`), FULLTEXT KEY (`col04`), FULLTEXT KEY
(`col05`), FULLTEXT KEY (`col06`), FULLTEXT KEY (`col07`), FULLTEXT KEY
(`col08`), FULLTEXT KEY (`col09`), FULLTEXT KEY (`col10`), FULLTEXT KEY
(`col11`), FULLTEXT KEY (`col12`), FULLTEXT KEY (`col13`), FULLTEXT KEY
(`col14`), FULLTEXT KEY (`col15`), FULLTEXT KEY (`col16`), FULLTEXT KEY
(`col17`), FULLTEXT KEY (`col18`), FULLTEXT KEY (`col19`), FULLTEXT KEY
(`col20`), FULLTEXT KEY (`col21`), FULLTEXT KEY (`col22`), FULLTEXT KEY
(`col23`), FULLTEXT KEY (`col24`), FULLTEXT KEY (`col25`), FULLTEXT KEY
(`col26`), FULLTEXT KEY (`col27`), FULLTEXT KEY (`col28`), FULLTEXT KEY
(`col29`), FULLTEXT KEY (`col30`)) engine=innodb;
#create procedure to inset into the table.
CREATE  PROCEDURE `proc_insert`(IN tab_name VARCHAR(40))
BEGIN
DECLARE i INT DEFAULT 1;
SET @insert_tbl =CONCAT('INSERT INTO ', tab_name, '( `col01`, `col02`,
	`col03`, `col04`, `col05`, `col06`, `col07`, `col08`, `col09`, `col10`,
	`col11`, `col12`, `col13`, `col14`, `col15`, `col16`, `col17`, `col18`,
	`col19`, `col20`, `col21`, `col22`, `col23`, `col24`, `col25`, `col26`,
	`col27`, `col28`, `col29`, `col30`)
	VALUES ( MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()),
	MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()),
	MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()),
	MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()),
	MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()),
	MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()), MD5(RAND()),
	MD5(RAND()))');
PREPARE ins_stmt FROM @insert_tbl;
while (i <= 2000) DO
EXECUTE ins_stmt;
SET i = i + 1;
END WHILE;
DEALLOCATE PREPARE ins_stmt;
END |
call proc_insert('t1');;
call proc_insert('t1');;
call proc_insert('t2');;
call proc_insert('t2');;
call proc_insert('t3');;
call proc_insert('t3');;
call proc_insert('t4');;
call proc_insert('t4');;
SET GLOBAL DEBUG="+d,fts_optimize_wq_count_check";
SET GLOBAL DEBUG="-d,fts_optimize_wq_count_check";
# clean up
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP PROCEDURE proc_insert;
