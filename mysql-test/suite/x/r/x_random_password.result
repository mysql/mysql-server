RUN SET @@GLOBAL.generated_random_password_length = 15

0 rows affected
RUN SELECT @@GLOBAL.generated_random_password_length
@@GLOBAL.generated_random_password_length
15
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
RUN SELECT user FROM mysql.user WHERE user LIKE '%usr%'
user
usr1
usr2
usr3
0 rows affected
RUN DROP USER usr1, usr2, usr3

0 rows affected
RUN SET @@GLOBAL.generated_random_password_length = DEFAULT

0 rows affected
RUN SELECT @@GLOBAL.generated_random_password_length
@@GLOBAL.generated_random_password_length
20
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
