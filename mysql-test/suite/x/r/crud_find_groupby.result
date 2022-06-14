CREATE SCHEMA xtest DEFAULT CHARSET 'utf8mb4';

command ok

6 rows affected
Records: 6  Duplicates: 0  Warnings: 0

6 rows affected
Records: 6  Duplicates: 0  Warnings: 0

Issue #1
========
Table approach
sum_f2	sum_f1
232	4
234	6
command ok
Collection approach
doc
{"sum_f1": 4, "sum_f2": 232}
{"sum_f1": 6, "sum_f2": 234}
command ok

Issue #2
========
Table approach
Fld	sum_f2
3	234
command ok
Collection approach (doc member in grouping criteria)
Got expected error: Invalid expression in grouping criteria (code 5154)
Collection approach (alias in grouping criteria)
doc
{"Fld": 3, "sum_f2": 234}
command ok
Mysqlx.Ok {
  msg: "bye!"
}
ok
