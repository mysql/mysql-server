# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

# Tests for the performance schema

# =======================================
# HELPER include/connection_load.inc
# =======================================

call dump_all();

connect (con1a, localhost, user1, , );
select "user1 in con1a" as status;
call dump_all();

connect (con1b, localhost, user1, , );
select "user1 in con1b" as status;
call dump_all();

connect (con1c, localhost, user1, , );
select "user1 in con1c" as status;
call dump_all();

connect (con2a, localhost, user2, , );
select "user2 in con2a" as status;
call dump_all();

connect (con2b, localhost, user2, , );
select "user2 in con2b" as status;
call dump_all();

connect (con2c, localhost, user2, , );
select "user2 in con2c" as status;
call dump_all();

connect (con3a, localhost, user3, , );
select "user3 in con3a" as status;
call dump_all();

connect (con3b, localhost, user3, , );
select "user3 in con3b" as status;
call dump_all();

connect (con3c, localhost, user3, , );
select "user3 in con3c" as status;
call dump_all();

--connection default

--disconnect con1a

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 8 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con1a disconnected" as status;
call dump_all();

--disconnect con2a

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 7 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con2a disconnected" as status;
call dump_all();

--disconnect con3a

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 6 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con3a disconnected" as status;
call dump_all();

truncate table performance_schema.accounts;
call dump_all();
truncate table performance_schema.users;
call dump_all();
truncate table performance_schema.hosts;
call dump_all();

connect (con4a, localhost, user4, , );
select "user4 in con4a" as status;
call dump_all();

connect (con4b, localhost, user4, , );
select "user4 in con4b" as status;
call dump_all();

connect (con4c, localhost, user4, , );
select "user4 in con4c" as status;
call dump_all();

--connection default

--disconnect con1b

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 8 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con1b disconnected" as status;
call dump_all();

--disconnect con2b

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 7 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con2b disconnected" as status;
call dump_all();

--disconnect con3b

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 6 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con3b disconnected" as status;
call dump_all();

--disconnect con1c

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 5 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con1c disconnected" as status;
call dump_all();

--disconnect con2c

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 4 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con2c disconnected" as status;
call dump_all();

--disconnect con3c

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 3 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con3c disconnected" as status;
call dump_all();

truncate table performance_schema.accounts;
call dump_all();
truncate table performance_schema.users;
call dump_all();
truncate table performance_schema.hosts;
call dump_all();

connect (con5a, localhost, user5, , );
select "user5 in con5a" as status;
call dump_all();

connect (con5b, localhost, user5, , );
select "user5 in con5b" as status;
call dump_all();

connect (con5c, localhost, user5, , );
select "user5 in con5c" as status;
call dump_all();

--connection default

--disconnect con4a
--disconnect con4b
--disconnect con4c
--disconnect con5a
--disconnect con5b
--disconnect con5c

# Wait for the disconnect to complete
let $wait_condition=
  select count(*) = 0 from performance_schema.threads
  where `TYPE`='FOREGROUND' and PROCESSLIST_USER like 'user%';
--source include/wait_condition.inc

select "con 5a, 5b, 5c, 6a, 6b, 6c disconnected" as status;
call dump_all();

truncate table performance_schema.hosts;
call dump_all();
truncate table performance_schema.users;
call dump_all();
truncate table performance_schema.accounts;
call dump_all();

