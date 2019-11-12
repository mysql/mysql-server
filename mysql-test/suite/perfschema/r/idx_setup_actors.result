insert into performance_schema.setup_actors values ("host1","user1","%",'YES','YES');
insert into performance_schema.setup_actors values ("host2","user1","%",'YES','YES');
insert into performance_schema.setup_actors values ("host3","user1","%",'YES','YES');
############ Explain for Query ####################################
explain select HOST, USER, `ROLE`
  from performance_schema.setup_actors
where HOST = 'host1';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	setup_actors	NULL	ref	PRIMARY	PRIMARY	255	const	#	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `performance_schema`.`setup_actors`.`HOST` AS `HOST`,`performance_schema`.`setup_actors`.`USER` AS `USER`,`performance_schema`.`setup_actors`.`ROLE` AS `ROLE` from `performance_schema`.`setup_actors` where (`performance_schema`.`setup_actors`.`HOST` = 'host1')
############ Explain for Query ####################################
explain select *
from performance_schema.setup_actors
where HOST = 'host1' and USER = 'user1';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	setup_actors	NULL	ref	PRIMARY	PRIMARY	383	const,const	#	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `performance_schema`.`setup_actors`.`HOST` AS `HOST`,`performance_schema`.`setup_actors`.`USER` AS `USER`,`performance_schema`.`setup_actors`.`ROLE` AS `ROLE`,`performance_schema`.`setup_actors`.`ENABLED` AS `ENABLED`,`performance_schema`.`setup_actors`.`HISTORY` AS `HISTORY` from `performance_schema`.`setup_actors` where ((`performance_schema`.`setup_actors`.`USER` = 'user1') and (`performance_schema`.`setup_actors`.`HOST` = 'host1'))
############ Explain for Query ####################################
explain select *
from performance_schema.setup_actors
where HOST = 'host1' and USER = 'user1'
        and `ROLE` = 'norole';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
Warnings:
Note	1003	/* select#1 */ select NULL AS `HOST`,NULL AS `USER`,NULL AS `ROLE`,NULL AS `ENABLED`,NULL AS `HISTORY` from `performance_schema`.`setup_actors` where (multiple equal('host1', NULL) and multiple equal('user1', NULL) and multiple equal('norole', NULL))
############ Explain for Query ####################################
explain select HOST, USER, `ROLE`
  from performance_schema.setup_actors
where HOST = 'nohost' and USER = 'user1'
        and `ROLE` = '%';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
Warnings:
Note	1003	/* select#1 */ select NULL AS `HOST`,NULL AS `USER`,NULL AS `ROLE` from `performance_schema`.`setup_actors` where (multiple equal('nohost', NULL) and multiple equal('user1', NULL) and multiple equal('%', NULL))
############ Explain for Query ####################################
explain select HOST, USER, `ROLE`
  from performance_schema.setup_actors
where HOST = 'host1' and USER = 'nouser'
        and `ROLE` = '%';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
Warnings:
Note	1003	/* select#1 */ select NULL AS `HOST`,NULL AS `USER`,NULL AS `ROLE` from `performance_schema`.`setup_actors` where (multiple equal('host1', NULL) and multiple equal('nouser', NULL) and multiple equal('%', NULL))
############ Explain for Query ####################################
explain select HOST, USER, `ROLE`
  from performance_schema.setup_actors
where HOST = 'host1' and USER = 'user1'
        and `ROLE` = '%';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	setup_actors	NULL	const	PRIMARY	PRIMARY	511	const,const,const	#	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select 'host1' AS `HOST`,'user1' AS `USER`,'%' AS `ROLE` from `performance_schema`.`setup_actors` where true
############# Explain End #########################################
flush status;
select ENABLED, HISTORY
from performance_schema.setup_actors
where HOST = 'TBALE' and USER = 'user1'
        and `ROLE` = '%';
ENABLED	HISTORY
show session status like "handler_read%";
Variable_name	Value
Handler_read_first	0
Handler_read_key	1
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
select ENABLED, HISTORY
from performance_schema.setup_actors
where HOST = 'host1' and USER = 'user1'
        and `ROLE` = '%';
ENABLED	HISTORY
YES	YES
show session status like "handler_read%";
Variable_name	Value
Handler_read_first	0
Handler_read_key	2
Handler_read_last	0
Handler_read_next	0
Handler_read_prev	0
Handler_read_rnd	0
Handler_read_rnd_next	0
delete from performance_schema.setup_actors where user = 'user1';
