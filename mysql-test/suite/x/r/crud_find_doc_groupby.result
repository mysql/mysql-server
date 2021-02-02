CREATE SCHEMA xtest DEFAULT CHARSET 'utf8mb4';

command ok

5 rows affected
Records: 5  Duplicates: 0  Warnings: 0
group by name
doc
{"name": "foo"}
{"name": "bar"}
{"name": "baz"}
{"name": "buz"}
command ok
group by name and count
doc
{"name": "foo", "count": 2}
{"name": "bar", "count": 1}
{"name": "baz", "count": 1}
{"name": "buz", "count": 1}
command ok
group by name, age and count
doc
{"age": 1, "name": "foo", "count": 1}
{"age": 2, "name": "bar", "count": 1}
{"age": 3, "name": "baz", "count": 1}
{"age": 7, "name": "foo", "count": 1}
{"age": 17, "name": "buz", "count": 1}
command ok
no projection - error expected
Got expected error: Invalid empty projection list for grouping (code 5114)
group by name, count name and criteria on count
Got expected error: Invalid expression in grouping criteria (code 5154)
group by name, criteria on max(age)
doc
{"my_age": "17", "my_name": "buz"}
{"my_age": "7", "my_name": "foo"}
command ok
Mysqlx.Ok {
  msg: "bye!"
}
ok
