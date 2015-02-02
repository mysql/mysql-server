create user test_nopw;
grant select on test.* to test_nopw;
create user test_newpw identified by "newpw";
grant select on test.* to test_newpw;
select user(), current_user(), database();
user()	current_user()	database()
root@localhost	root@localhost	test
select concat('<', user(), '>'), concat('<', current_user(), '>'), database();
concat('<', user(), '>')	concat('<', current_user(), '>')	database()
<root@localhost>	<root@localhost>	test
select concat('<', user(), '>'), concat('<', current_user(), '>'), database();
concat('<', user(), '>')	concat('<', current_user(), '>')	database()
<test_nopw@localhost>	<test_nopw@%>	NULL
select concat('<', user(), '>'), concat('<', current_user(), '>'), database();
concat('<', user(), '>')	concat('<', current_user(), '>')	database()
<test_newpw@localhost>	<test_newpw@%>	NULL
select concat('<', user(), '>'), concat('<', current_user(), '>'), database();
concat('<', user(), '>')	concat('<', current_user(), '>')	database()
<root@localhost>	<root@localhost>	NULL
select concat('<', user(), '>'), concat('<', current_user(), '>'), database();
concat('<', user(), '>')	concat('<', current_user(), '>')	database()
<test_nopw@localhost>	<test_nopw@%>	test
select concat('<', user(), '>'), concat('<', current_user(), '>'), database();
concat('<', user(), '>')	concat('<', current_user(), '>')	database()
<test_newpw@localhost>	<test_newpw@%>	test
select concat('<', user(), '>'), concat('<', current_user(), '>'), database();
concat('<', user(), '>')	concat('<', current_user(), '>')	database()
<root@localhost>	<root@localhost>	test
drop user test_nopw;
drop user test_newpw;
Bug#20023
SELECT @@session.sql_big_selects;
@@session.sql_big_selects
1
SELECT @@global.max_join_size;
@@global.max_join_size
HA_POS_ERROR
change_user
SELECT @@session.sql_big_selects;
@@session.sql_big_selects
1
SELECT @@global.max_join_size;
@@global.max_join_size
HA_POS_ERROR
SET @@global.max_join_size = 10000;
SET @@session.max_join_size = default;
change_user
SELECT @@session.sql_big_selects;
@@session.sql_big_selects
0
SET @@global.max_join_size = 18446744073709551615;
SET @@session.max_join_size = default;
change_user
SELECT @@session.sql_big_selects;
@@session.sql_big_selects
1
SET INSERT_ID=12;
SELECT @@INSERT_ID;
@@INSERT_ID
12
SET TIMESTAMP=200;
SELECT @@TIMESTAMP;
@@TIMESTAMP
200.000000
change_user
SELECT @@INSERT_ID;
@@INSERT_ID
0
SELECT @@TIMESTAMP=200;
@@TIMESTAMP=200
0
Bug#31418
SELECT IS_FREE_LOCK('bug31418');
IS_FREE_LOCK('bug31418')
1
SELECT IS_USED_LOCK('bug31418');
IS_USED_LOCK('bug31418')
NULL
SELECT GET_LOCK('bug31418', 1);
GET_LOCK('bug31418', 1)
1
SELECT IS_USED_LOCK('bug31418') = CONNECTION_ID();
IS_USED_LOCK('bug31418') = CONNECTION_ID()
1
change_user
SELECT IS_FREE_LOCK('bug31418');
IS_FREE_LOCK('bug31418')
1
SELECT IS_USED_LOCK('bug31418');
IS_USED_LOCK('bug31418')
NULL
FLUSH STATUS;
Value of com_select did not change
