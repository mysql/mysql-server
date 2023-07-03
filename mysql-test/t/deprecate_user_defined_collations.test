--echo #
--echo # WL#14277 Deprecate user defined collations
--echo #
--echo

# Expect 1 warning due to CREATE TABLE ... (COLLATE <user_defined_collation>).
create table t (t text collate utf8mb4_test_ci);
--echo

delimiter |;

# Expect 3 warnings due to CREATE TABLE (... COLLATE <user_defined_collation>),
# DECLARE ... COLLATE <user_defined_collation>,
# and SET <val> = ... COLLATE <user_defined_collation>
create procedure p () begin
    declare a varchar(1) charset utf8mb4 collate utf8mb4_test_ci;
    set @a = "a" collate utf8mb4_test_ci;
    create table tp (t varchar(1) collate utf8mb4_test_ci);
    insert into t values (@a);
end|
--echo

# Expect 3 warnings due to CREATE FUNCTION ... RETURNS ...
# COLLATE <user_defined_collation>,
# DECLARE ... COLLATE <user_defined_collation>,
# and SET <var> = ... COLLATE <user_defined_collation>.
create function f () returns varchar(1) charset utf8mb4 collate utf8mb4_test_ci begin
    declare a varchar(1) charset utf8mb4 collate utf8mb4_test_ci;
    set @a = "a" collate utf8mb4_test_ci;
    insert into tp values (@a);
    return @a;
end|
--echo

# Expect 2 warnings due to DECLARE ... COLLATE <user_defined_collation> and
# SET <var> = ... COLLATE <user_defined_collation>.
create trigger tr after insert on t for each row begin
	declare a varchar(1) charset utf8mb4 collate utf8mb4_test_ci;
	set @a = "a" collate utf8mb4_test_ci;
    insert into tp values (@a);
end|
--echo

delimiter ;|

# Expect 3 warnings due to procedures being parsed when called
call p();
--echo

# Expect 2 warnings
insert into t values ("a" collate utf8mb4_test_ci), ("a" collate utf8mb4_test_ci);
--echo

# Expect 3 warnings.
# The FUNCTION f() is only parsed once, which generates 3 warnings,
# even though it is called six times.
select t.*, f(), f() from t;
--echo

# Expect 9 rows in tp:
#   1 from trigger due to insert on t in p
#   2 from trigger due to insert on t in query
#   6 from trigger due to insert on t in f
select count(*) from tp;
--echo

drop table if exists tp;
drop table if exists t;
drop procedure if exists p;
drop function if exists f;