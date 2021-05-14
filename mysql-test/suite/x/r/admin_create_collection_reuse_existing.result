CREATE SCHEMA xtest DEFAULT CHARSET 'UTF8MB4';

command ok
Got expected error (code 0)
Got expected error: Table 'coll' already exists (code 1050)
Got expected error: Table 'coll' already exists (code 1050)

command ok
Got expected error (code 0)
RUN CREATE TABLE xtest.fake_coll (field INT)

0 rows affected
Got expected error: Table 'fake_coll' already exists (code 1050)
Got expected error: Table 'fake_coll' already exists (code 1050)
Got expected error: Table 'xtest.fake_coll' exists but is not a collection (code 5156)
Mysqlx.Ok {
  msg: "bye!"
}
ok
