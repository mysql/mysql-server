drop table if exists t1,t2;
SET sort_buffer_size=32804;
CREATE TABLE `t1` (
`id` int(11) NOT NULL default '0',
`id2` int(11) NOT NULL default '0',
`id3` int(11) NOT NULL default '0');
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
create table t2 select id2 from t1 order by id3;
select count(*) from t2;
count(*)
4000
drop table t1,t2;
SET sort_buffer_size=DEFAULT;
