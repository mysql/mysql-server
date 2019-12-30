--source include/force_myisam_default.inc
--source include/have_myisam.inc

SET sql_mode = 'NO_ENGINE_SUBSTITUTION';

CREATE TABLE t1 (
`id` int(11) NOT NULL auto_increment,
`username` varchar(80) NOT NULL default '',
`posted_on` timestamp NOT NULL default '0000-00-00 00:00:00',
PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1;

show fields from t1;
select is_nullable from INFORMATION_SCHEMA.COLUMNS where TABLE_NAME='t1' and COLUMN_NAME='posted_on';
drop table t1;
