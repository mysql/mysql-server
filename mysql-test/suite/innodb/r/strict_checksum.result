#
# Bug #20031570 INNODB_CHECKSUM_ALGORITHM VARIABLE LEADS TO CRASHING
#
call mtr.add_suppression("Plugin \'InnoDB\'");
set global innodb_checksum_algorithm=crc32;
create table t1(f1 int not null primary key)engine=innodb;
SET GLOBAL innodb_buffer_pool_dump_at_shutdown=OFF;
# Restart the server with --log-error
# restart: --log-error=SEARCH_FILE --no-console
set global innodb_checksum_algorithm=strict_innodb;
set global innodb_limit_optimistic_insert_debug = 2;
select count(*) from t1;
count(*)
0
Pattern "\[Warning\] .*MY-\d+.* innodb_checksum_algorithm is set to "strict_innodb" but the page \[page id: space=\d+, page number=\d+\] contains a valid checksum .*. Accepting the page as valid. Change innodb_checksum_algorithm to .* to silently accept such pages or rewrite all pages so that they contain .* checksum." found
insert into t1 values(4),(5),(6);
SET GLOBAL innodb_buffer_pool_dump_at_shutdown=OFF;
# Restart the server with --log-error
# restart: --log-error=SEARCH_FILE --no-console
set global innodb_checksum_algorithm=strict_none;
set global innodb_limit_optimistic_insert_debug = 2;
select count(*) from t1;
count(*)
3
insert into t1 values(7),(8),(9);
Pattern "\[Warning\] .*MY-\d+.* innodb_checksum_algorithm is set to "strict_none" but the page \[page id: space=\d+, page number=\d+\] contains a valid checksum .*. Accepting the page as valid. Change innodb_checksum_algorithm to .* to silently accept such pages or rewrite all pages so that they contain .* checksum." found
SET GLOBAL innodb_buffer_pool_dump_at_shutdown=OFF;
# Restart the server with --log-error
# restart: --log-error=SEARCH_FILE --no-console
set global innodb_checksum_algorithm=strict_crc32;
select count(*) from t1;
count(*)
6
Pattern "\[Warning\] .*MY-\d+.* innodb_checksum_algorithm is set to "strict_crc32" but the page \[page id: space=\d+, page number=\d+\] contains a valid checksum .*. Accepting the page as valid. Change innodb_checksum_algorithm to .* to silently accept such pages or rewrite all pages so that they contain .* checksum." found
drop table t1;
# restart:
