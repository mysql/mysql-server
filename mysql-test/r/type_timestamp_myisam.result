SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t1 (
`id` int(11) NOT NULL auto_increment,
`username` varchar(80) NOT NULL default '',
`posted_on` timestamp NOT NULL default '0000-00-00 00:00:00',
PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
show fields from t1;
Field	Type	Null	Key	Default	Extra
id	int	NO	PRI	NULL	auto_increment
username	varchar(80)	NO			
posted_on	timestamp	NO		0000-00-00 00:00:00	
select is_nullable from INFORMATION_SCHEMA.COLUMNS where TABLE_NAME='t1' and COLUMN_NAME='posted_on';
IS_NULLABLE
NO
drop table t1;
