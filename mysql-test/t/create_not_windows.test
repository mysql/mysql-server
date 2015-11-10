
 #Get deafult engine value
--let $DEFAULT_ENGINE = `select @@global.default_storage_engine`

# Non-windows specific create tests.

--source include/not_windows.inc

#
# Bug#19479:mysqldump creates invalid dump
#
--disable_warnings
drop table if exists `about:text`;
--enable_warnings
create table `about:text` ( 
_id int not null auto_increment,
`about:text` varchar(255) not null default '',
primary key (_id)
);


#Replace default engine value with static engine string 
--replace_result $DEFAULT_ENGINE ENGINE
show create table `about:text`; 
drop table `about:text`;


# End of 5.0 tests
