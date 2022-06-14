SET @@tmpdir= 'no/such/directory';
ERROR HY000: Variable 'tmpdir' is a read only variable
Occurrences of 'No such file or directory' in the input file: 1
# restart: --tmpdir=tmpdir
CALL mtr.add_suppression("Cannot create temporary merge file");
SET @@innodb_tmpdir= NULL;
CREATE TABLE test.t(a text);
ALTER TABLE test.t ADD fulltext(a);
ERROR HY000: Can't create/write to file 'tmpdir/tmpfile' (OS errno 2 - No such file or directory)
DROP TABLE test.t;
# restart:
