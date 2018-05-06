-- Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License, version 2.0,
-- as published by the Free Software Foundation.
--
-- This program is also distributed with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have included with MySQL.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License, version 2.0, for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

delimiter |

drop function if exists mysql.mysql_cluster_privileges_are_distributed|
drop procedure if exists mysql.mysql_cluster_backup_privileges|
drop procedure if exists mysql.mysql_cluster_move_grant_tables|
drop procedure if exists mysql.mysql_cluster_restore_privileges_from_local|
drop procedure if exists mysql.mysql_cluster_restore_privileges|
drop procedure if exists mysql.mysql_cluster_restore_local_privileges|
drop procedure if exists mysql.mysql_cluster_move_privileges|

 -- Count number of privilege tables in NDB, require
 -- all the tables to be in NDB in order to return "true"
create function mysql.mysql_cluster_privileges_are_distributed()
returns bool
reads sql data
begin
 declare distributed bool default 0;

 -- Ignore warning 3090 ER_WARN_DEPRECATED_SQLMODE when
 -- resetting sql_mode to the original value
 declare continue handler for 3090 begin end;
 SET @sql_mode_orig=@@SESSION.sql_mode;
 SET SESSION sql_mode='NO_ENGINE_SUBSTITUTION';
 
 select COUNT(table_name) = 6
   into distributed
     from information_schema.tables
       where table_schema = "mysql" and
             table_name IN ("user", "db", "tables_priv",
                            "columns_priv", "procs_priv",
                            "proxies_priv") and
             table_type = 'BASE TABLE' and
             engine = 'NDBCLUSTER';

 SET SESSION sql_mode=@sql_mode_orig;
 return distributed;
end|

create procedure mysql.mysql_cluster_backup_privileges()
begin
 declare distributed_privileges bool default 0;
 declare first_backup bool default 1;
 declare first_distributed_backup bool default 1;

 -- Ignore error 1292 ER_TRUNCATED_WRONG_VALUE when
 -- copying rows into one of the backup tables. The
 -- source tables are known to hold invalid timestamp
 -- values in the Timestamp column but should be copied anyway
 declare continue handler for 1292 begin end;

 -- Ignore warning 3090 ER_WARN_DEPRECATED_SQLMODE when
 -- resetting sql_mode to the original value
 declare continue handler for 3090 begin end;
 SET @sql_mode_orig=@@SESSION.sql_mode;
 SET SESSION sql_mode='NO_ENGINE_SUBSTITUTION';

 select mysql.mysql_cluster_privileges_are_distributed()
   into distributed_privileges;
 select 0 into first_backup
   from information_schema.tables
     where table_schema = "mysql" and table_name = "user_backup";
 select 0 into first_distributed_backup
   from information_schema.tables
     where table_schema = "mysql" and table_name = "ndb_user_backup";
 if first_backup = 1 then
   create table if not exists mysql.user_backup
     like mysql.user;
   create table if not exists mysql.db_backup
     like mysql.db;
   create table if not exists mysql.tables_priv_backup
     like mysql.tables_priv;
   create table if not exists mysql.columns_priv_backup
     like mysql.columns_priv;
   create table if not exists mysql.procs_priv_backup
     like mysql.procs_priv;
   create table if not exists mysql.proxies_priv_backup
     like mysql.proxies_priv;
   if distributed_privileges = 1 then
     alter table mysql.user_backup engine = myisam;
     alter table mysql.db_backup engine = myisam;
     alter table mysql.tables_priv_backup engine = myisam;
     alter table mysql.columns_priv_backup engine = myisam;
     alter table mysql.procs_priv_backup engine = myisam;
     alter table mysql.proxies_priv_backup engine = myisam;
   end if;
 else
   truncate mysql.user_backup;
   truncate mysql.db_backup;
   truncate mysql.tables_priv_backup;
   truncate mysql.columns_priv_backup;
   truncate mysql.procs_priv_backup;
   truncate mysql.proxies_priv_backup;
 end if;
 if first_distributed_backup = 1 then
   create table if not exists mysql.ndb_user_backup
     like mysql.user;
   create table if not exists mysql.ndb_db_backup
     like mysql.db;
   create table if not exists mysql.ndb_tables_priv_backup
     like mysql.tables_priv;
   create table if not exists mysql.ndb_columns_priv_backup
     like mysql.columns_priv;
   create table if not exists mysql.ndb_procs_priv_backup
     like mysql.procs_priv;
   create table if not exists mysql.ndb_proxies_priv_backup
     like mysql.proxies_priv;

   if distributed_privileges = 0 then
     alter table mysql.ndb_user_backup algorithm = copy, engine = ndbcluster;
     alter table mysql.ndb_db_backup algorithm = copy, engine = ndbcluster;
     alter table mysql.ndb_tables_priv_backup algorithm = copy, engine = ndbcluster;
     alter table mysql.ndb_columns_priv_backup algorithm = copy, engine = ndbcluster;
     alter table mysql.ndb_procs_priv_backup algorithm = copy, engine = ndbcluster;
     alter table mysql.ndb_proxies_priv_backup algorithm = copy, engine = ndbcluster;
   end if;
 else
   truncate mysql.ndb_user_backup;
   truncate mysql.ndb_db_backup;
   truncate mysql.ndb_tables_priv_backup;
   truncate mysql.ndb_columns_priv_backup;
   truncate mysql.ndb_procs_priv_backup;
   truncate mysql.ndb_proxies_priv_backup;
 end if;
 insert into mysql.user_backup select * from mysql.user;
 insert into mysql.db_backup select * from mysql.db;
 insert into mysql.tables_priv_backup select * from mysql.tables_priv;
 insert into mysql.columns_priv_backup select * from mysql.columns_priv;
 insert into mysql.procs_priv_backup select * from mysql.procs_priv;
 insert into mysql.proxies_priv_backup select * from mysql.proxies_priv;

 insert into mysql.ndb_user_backup select * from mysql.user;
 insert into mysql.ndb_db_backup select * from mysql.db;
 insert into mysql.ndb_tables_priv_backup select * from mysql.tables_priv;
 insert into mysql.ndb_columns_priv_backup select * from mysql.columns_priv;
 insert into mysql.ndb_procs_priv_backup select * from mysql.procs_priv;
 insert into mysql.ndb_proxies_priv_backup select * from mysql.proxies_priv;
 SET SESSION sql_mode=@sql_mode_orig;
end|

create procedure mysql.mysql_cluster_restore_privileges_from_local()
begin
 declare local_backup bool default 0;

 -- Ignore warning 3090 ER_WARN_DEPRECATED_SQLMODE when
 -- resetting sql_mode to the original value
 declare continue handler for 3090 begin end;
 SET @sql_mode_orig=@@SESSION.sql_mode;
 SET SESSION sql_mode='NO_ENGINE_SUBSTITUTION';

 select 1 into local_backup
   from information_schema.tables
    where table_schema = "mysql" and table_name = "user_backup";
 if local_backup = 1 then
   create table if not exists mysql.user
     like mysql.user_backup;
   create table if not exists mysql.db
     like mysql.db_backup;
   create table if not exists mysql.tables_priv
     like mysql.tables_priv_backup;
   create table if not exists mysql.columns_priv
     like mysql.columns_priv_backup;
   create table if not exists mysql.procs_priv
     like mysql.procs_priv_backup;
   create table if not exists mysql.proxies_priv
     like mysql.proxies_priv_backup;
   delete from mysql.user;
   insert into mysql.user select * from mysql.user_backup;
   delete from mysql.db;
   insert into mysql.db select * from mysql.db_backup;
   delete from mysql.tables_priv;
   insert into mysql.tables_priv select * from mysql.tables_priv_backup;
   delete from mysql.columns_priv;
   insert into mysql.columns_priv select * from mysql.columns_priv_backup;
   delete from mysql.procs_priv;
   insert into mysql.procs_priv select * from mysql.procs_priv_backup;
   delete from mysql.proxies_priv;
   insert into mysql.proxies_priv select * from mysql.proxies_priv_backup;
 end if;
 SET SESSION sql_mode=@sql_mode_orig;
end|

create procedure mysql.mysql_cluster_restore_privileges()
begin
 declare distributed_backup bool default 0;

 -- Ignore warning 3090 ER_WARN_DEPRECATED_SQLMODE when
 -- resetting sql_mode to the original value
 declare continue handler for 3090 begin end;
 SET @sql_mode_orig=@@SESSION.sql_mode;
 SET SESSION sql_mode='NO_ENGINE_SUBSTITUTION';

 select 1 into distributed_backup
   from information_schema.tables
     where table_schema = "mysql" and table_name = "ndb_user_backup";
 if distributed_backup = 1 then
   flush tables;
   create table if not exists mysql.user
     like mysql.ndb_user_backup;
   create table if not exists mysql.db
     like mysql.ndb_db_backup;
   create table if not exists mysql.tables_priv
     like mysql.ndb_tables_priv_backup;
   create table if not exists mysql.columns_priv
     like mysql.ndb_columns_priv_backup;
   create table if not exists mysql.procs_priv
     like mysql.ndb_procs_priv_backup;
   create table if not exists mysql.proxies_priv
     like mysql.ndb_proxies_priv_backup;
   delete from mysql.user;
   insert into mysql.user
     select * from mysql.ndb_user_backup;
   delete from mysql.db;
   insert into mysql.db
     select * from mysql.ndb_db_backup;
   delete from mysql.tables_priv;
   insert into mysql.tables_priv
     select * from mysql.ndb_tables_priv_backup;
   delete from mysql.columns_priv;
   insert into mysql.columns_priv
     select * from mysql.ndb_columns_priv_backup;
   delete from mysql.procs_priv;
   insert into mysql.procs_priv
     select * from mysql.ndb_procs_priv_backup;
   delete from mysql.proxies_priv;
   insert into mysql.proxies_priv
     select * from mysql.ndb_proxies_priv_backup;
 else
   call mysql_cluster_restore_privileges_from_local();
 end if;
 SET SESSION sql_mode=@sql_mode_orig;
end|

create procedure mysql.mysql_cluster_restore_local_privileges()
begin
 declare distributed_privileges bool default 0;

 -- Ignore warning 3090 ER_WARN_DEPRECATED_SQLMODE when
 -- resetting sql_mode to the original value
 declare continue handler for 3090 begin end;
 SET @sql_mode_orig=@@SESSION.sql_mode;
 SET SESSION sql_mode='NO_ENGINE_SUBSTITUTION';

 select mysql.mysql_cluster_privileges_are_distributed()
   into distributed_privileges;
 if distributed_privileges = 1 then
  begin
    drop table mysql.user;
    drop table mysql.db;
    drop table mysql.tables_priv;
    drop table mysql.columns_priv;
    drop table mysql.procs_priv;
    drop table mysql.proxies_priv;
  end;
 end if;
 call mysql_cluster_restore_privileges_from_local();
 SET SESSION sql_mode=@sql_mode_orig;
end|

create procedure mysql.mysql_cluster_move_grant_tables()
begin
 declare distributed_privileges bool default 0;
 declare revert bool default 0;

 -- Ignore warning 3090 ER_WARN_DEPRECATED_SQLMODE when
 -- resetting sql_mode to the original value
 declare continue handler for 3090 begin end;
 SET @sql_mode_orig=@@SESSION.sql_mode;
 SET SESSION sql_mode='NO_ENGINE_SUBSTITUTION';

 select mysql.mysql_cluster_privileges_are_distributed()
   into distributed_privileges;
 if distributed_privileges = 0 then
  begin
   declare exit handler for sqlexception set revert = 1;
   alter table mysql.user algorithm = copy, engine = ndb;
   alter table mysql.db algorithm = copy, engine = ndb;
   alter table mysql.tables_priv algorithm = copy, engine = ndb;
   alter table mysql.columns_priv algorithm = copy, engine = ndb;
   alter table mysql.procs_priv algorithm = copy, engine = ndb;
   alter table mysql.proxies_priv algorithm = copy, engine = ndb;
  end;
 end if;
 if revert = 1 then
   call mysql_cluster_restore_privileges();
 end if;
 SET SESSION sql_mode=@sql_mode_orig;
end|

create procedure mysql.mysql_cluster_move_privileges()
begin
 call mysql_cluster_backup_privileges();
 call mysql_cluster_move_grant_tables();
end|

delimiter ;

