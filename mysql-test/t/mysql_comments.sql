##============================================================================
## Notes
##============================================================================

# Test case for Bug#11230

# The point of this test is to make sure that '#', '-- ' and '/* ... */'
# comments, as well as empty lines, are sent from the client to the server.
# This is to ensure better error reporting, and to keep comments in the code
# for stored procedures / functions / triggers (Bug#11230).
# As a result, be careful when editing comments in this script, they do
# matter.
#
# Also, note that this is a script for **mysql**, not mysqltest.
# This is critical, as the mysqltest client interprets comments differently.

##============================================================================
## Setup
##============================================================================

## See mysql_comments.test for initial cleanup

# Test tables
#
# t1 is reused throughout the file, and dropped at the end.
#
drop table if exists t1;
create table t1 (
  id   char(16) not null default '',
  data int not null
);

##============================================================================
## Comments outside statements
##============================================================================

# Ignored 1a
-- Ignored 1b
/*
   Ignored 1c
*/

select 1;

##============================================================================
## Comments inside statements
##============================================================================

select # comment 1a
# comment 2a
-- comment 2b
/*
   comment 2c
*/
2
; # not strictly inside, but on same line
# ignored

##============================================================================
## Comments inside functions
##============================================================================

drop function if exists foofct ;

create function foofct (x char(20))
returns char(20)
/* not inside the body yet */
return
-- comment 1a
# comment 1b
/* comment 1c */
x; # after body, on same line

select foofct("call 1");

show create function foofct;
drop function foofct;

delimiter |

create function foofct(x char(20))
returns char(20)
begin
  -- comment 1a
  # comment 1b
  /*
     comment 1c
  */

  -- empty line below

  -- empty line above
  return x;
end|

delimiter ;

select foofct("call 2");

show create function foofct;
drop function foofct;

##============================================================================
## Comments inside stored procedures
##============================================================================

# Empty statement
drop procedure if exists empty;
create procedure empty()
begin
end;

call empty();
show create procedure empty;
drop procedure empty;

drop procedure if exists foosp;

## These comments are before the create, and will be lost
# Comment 1a
-- Comment 1b
/*
   Comment 1c
 */
create procedure foosp()
/* Comment not quiet in the body yet */
  insert into test.t1
## These comments are part of the procedure body, and should be kept.
# Comment 2a
-- Comment 2b
/* Comment 2c */
  -- empty line below

  -- empty line above
  values ("foo", 42); # comment 3, still part of the body
## After the ';', therefore not part of the body
# comment 4a
-- Comment 4b
/*
   Comment 4c
 */

call foosp();
select * from t1;
delete from t1;
show create procedure foosp;
drop procedure foosp;

drop procedure if exists nicesp;

delimiter |

create procedure nicesp(a int)
begin
  -- declare some variables here
  declare b int;
  declare c float;

  -- do more stuff here
  -- commented nicely and so on

  -- famous last words ...
end|

delimiter ;

show create procedure nicesp;
drop procedure nicesp;

##============================================================================
## Comments inside triggers
##============================================================================

drop trigger if exists t1_empty;

create trigger t1_empty after delete on t1
for each row
begin
end;

show create trigger t1_empty;

drop trigger if exists t1_bi;

delimiter |

create trigger t1_bi before insert on t1
for each row
begin
# comment 1a
-- comment 1b
/*
   comment 1c
*/
  -- declare some variables here
  declare b int;
  declare c float;

  -- do more stuff here
  -- commented nicely and so on

  -- famous last words ...
  set NEW.data := 12;
end|

delimiter ;

show create trigger t1_bi;

# also make sure the trigger still works
insert into t1(id) value ("trig");
select * from t1;

##============================================================================
## Cleanup
##============================================================================

drop table t1;
