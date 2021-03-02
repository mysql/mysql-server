set global DEBUG="+d,buf_lru_free_page_delay_block_mutex_reacquisition,buf_page_free_stale_delay_lru_mutex_acquisition";
create table evict (
d mediumtext not null);
create table t_temp (
d mediumtext not null);
insert into evict values (repeat('abcdefghij', 103));
insert into t_temp select * from t_temp;
insert into t_temp select * from t_temp;
insert into t_temp select * from t_temp;
insert into evict select * from t_temp;
insert into evict select * from evict;
insert into evict select * from evict;
insert into evict select * from evict;
insert into evict select * from evict;
insert into evict select * from evict;
insert into evict select * from evict;
insert into evict select * from evict;
insert into evict select * from evict;
insert into evict select * from evict;
insert into evict select * from evict;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
DROP TABLE t, t_t;
create table t (d mediumtext not null);
insert into t select * from t_temp;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;
sum(length(d))
1054720
DROP TABLE t, t_t;
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
create table t (d mediumtext not null);
insert into t select * from t_temp;
select sum(length(d)) from evict;;
DROP TABLE t, t_t;
sum(length(d))
1054720
create temporary table t_t (d mediumtext not null);
insert into t_t select * from t_temp;
select sum(length(d)) from evict;;
sum(length(d))
1054720
DROP TABLE t_temp, evict;
set global DEBUG="-d,buf_lru_free_page_delay_block_mutex_reacquisition,buf_page_free_stale_delay_lru_mutex_acquisition";
