#
# Bug #29717909	MEMORY LIFETIME OF VARIABLES BETWEEN CHECK AND UPDATE INCORRECTLY MANAGED
#
select @@innodb_ft_server_stopword_table;
@@innodb_ft_server_stopword_table
NULL
create table user_stopword_1(value varchar(30)) engine = innodb;
create table user_stopword_2(value varchar(30)) engine = innodb;
set @blah = 'test/user_stopword_1';
SET GLOBAL innodb_ft_server_stopword_table= @blah;
select @@innodb_ft_server_stopword_table;
@@innodb_ft_server_stopword_table
test/user_stopword_1
set @blah = 'test/user_stopword_2';
SET GLOBAL innodb_ft_server_stopword_table= @blah;
select @@innodb_ft_server_stopword_table;
@@innodb_ft_server_stopword_table
test/user_stopword_2
SET GLOBAL innodb_ft_server_stopword_table= NULL;
select @@innodb_ft_server_stopword_table;
@@innodb_ft_server_stopword_table
NULL
SET GLOBAL innodb_ft_server_stopword_table= default;
select @@innodb_ft_server_stopword_table;
@@innodb_ft_server_stopword_table
NULL
drop table user_stopword_1, user_stopword_2;
select @@innodb_buffer_pool_filename;
@@innodb_buffer_pool_filename
ib_buffer_pool
set @blah='hello';
set global innodb_buffer_pool_filename = @blah;
select @@innodb_buffer_pool_filename;
@@innodb_buffer_pool_filename
hello
set global innodb_buffer_pool_filename="bye";
select @@innodb_buffer_pool_filename;
@@innodb_buffer_pool_filename
bye
set global innodb_buffer_pool_filename=default;
select @@innodb_buffer_pool_filename;
@@innodb_buffer_pool_filename
ib_buffer_pool
CREATE TABLE t1 ( id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
opening_line TEXT(500), author VARCHAR(200), title VARCHAR(200), FULLTEXT idx
(opening_line)) ENGINE=InnoDB;
CREATE TABLE t2 ( id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
opening_line TEXT(500), author VARCHAR(200), title VARCHAR(200), FULLTEXT idx
(opening_line)) ENGINE=InnoDB;
select @@innodb_ft_aux_table;
@@innodb_ft_aux_table
NULL
set @blah = 'test/t1';
SET GLOBAL innodb_ft_aux_table = @blah;
select @@innodb_ft_aux_table;
@@innodb_ft_aux_table
test/t1
set @blah = 'test/t2';
SET GLOBAL innodb_ft_aux_table = @blah;
SET GLOBAL innodb_ft_aux_table = NULL;
select @@innodb_ft_aux_table;
@@innodb_ft_aux_table
NULL
SET GLOBAL innodb_ft_aux_table =default;
select @@innodb_ft_aux_table;
@@innodb_ft_aux_table
NULL
drop table t1,t2;
set @blah = 'label1:/home/guest/redo_log_archive_1';
SET global innodb_redo_log_archive_dirs = @blah;
select @@innodb_redo_log_archive_dirs;
@@innodb_redo_log_archive_dirs
label1:/home/guest/redo_log_archive_1
SET global innodb_redo_log_archive_dirs = default;
select @@innodb_redo_log_archive_dirs;
@@innodb_redo_log_archive_dirs
NULL
SET global innodb_redo_log_archive_dirs = NULL;
select @@innodb_redo_log_archive_dirs;
@@innodb_redo_log_archive_dirs
NULL
SET @@global.innodb_redo_log_archive_dirs = '';
