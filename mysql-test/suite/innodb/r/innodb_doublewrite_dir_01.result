SELECT @@datadir;
@@datadir
DATADIR
SELECT @@innodb_doublewrite_dir;
@@innodb_doublewrite_dir
TMP_DIR/#DBWL-DIR
SELECT @@innodb_doublewrite_files;
@@innodb_doublewrite_files
1
SELECT @@innodb_doublewrite_pages;
@@innodb_doublewrite_pages
10
SELECT @@innodb_doublewrite_batch_size;
@@innodb_doublewrite_batch_size
10
CREATE TABLE tab(c1 int, c2 TEXT);
INSERT INTO tab VALUES(1,'Test wl#5655');
SELECT * FROM tab;
c1	c2
1	Test wl#5655
DROP TABLE tab;
