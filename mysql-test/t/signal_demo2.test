
#
# Demonstrate how RESIGNAL can be used to 'catch' and 're-throw' an error
#

--disable_warnings
drop database if exists demo;
--enable_warnings

create database demo;

use demo;

delimiter $$;

create procedure proc_top_a(p1 integer)
begin
  ## DECLARE CONTINUE HANDLER for SQLEXCEPTION, NOT FOUND
  begin
  end;

  select "Starting ...";
  call proc_middle_a(p1);
  select "The end";
end
$$

create procedure proc_middle_a(p1 integer)
begin
  DECLARE l integer;
  # without RESIGNAL:
  # Should be: DECLARE EXIT HANDLER for SQLEXCEPTION, NOT FOUND
  DECLARE EXIT HANDLER for 1 /* not sure how to handle exceptions */
  begin
    select "Oops ... now what ?";
  end;

  select "In prod_middle()";

  create temporary table t1(a integer, b integer);
  select GET_LOCK("user_mutex", 10) into l;

  insert into t1 set a = p1, b = p1;

  call proc_bottom_a(p1);

  select RELEASE_LOCK("user_mutex") into l;
  drop temporary table t1;
end
$$

create procedure proc_bottom_a(p1 integer)
begin
  select "In proc_bottom()";

  if (p1 = 1) then
    begin
      select "Doing something that works ...";
      select * from t1;
    end;
  end if;

  if (p1 = 2) then
    begin
      select "Doing something that fail (simulate an error) ...";
      drop table no_such_table;
    end;
  end if;

  if (p1 = 3) then
    begin
      select "Doing something that *SHOULD* works ...";
      select * from t1;
    end;
  end if;

end
$$

delimiter ;$$

#
# Code without RESIGNAL:
# errors are apparent to the caller,
# but there is no cleanup code,
# so that the environment (get_lock(), temporary table) is polluted ...
#
call proc_top_a(1);

# Expected
--error ER_BAD_TABLE_ERROR
call proc_top_a(2);

# Dirty state
--error ER_TABLE_EXISTS_ERROR
call proc_top_a(3);

# Dirty state
--error ER_TABLE_EXISTS_ERROR
call proc_top_a(1);

drop temporary table if exists t1;

delimiter $$;

create procedure proc_top_b(p1 integer)
begin
  select "Starting ...";
  call proc_middle_b(p1);
  select "The end";
end
$$

create procedure proc_middle_b(p1 integer)
begin
  DECLARE l integer;
  DECLARE EXIT HANDLER for SQLEXCEPTION, NOT FOUND
  begin
    begin
      DECLARE CONTINUE HANDLER for SQLEXCEPTION, NOT FOUND
      begin
        /* Ignore errors from the cleanup code */
      end;

      select "Doing cleanup !";
      select RELEASE_LOCK("user_mutex") into l;
      drop temporary table t1;
    end;

    RESIGNAL;
  end;

  select "In prod_middle()";

  create temporary table t1(a integer, b integer);
  select GET_LOCK("user_mutex", 10) into l;

  insert into t1 set a = p1, b = p1;

  call proc_bottom_b(p1);

  select RELEASE_LOCK("user_mutex") into l;
  drop temporary table t1;
end
$$

create procedure proc_bottom_b(p1 integer)
begin
  select "In proc_bottom()";

  if (p1 = 1) then
    begin
      select "Doing something that works ...";
      select * from t1;
    end;
  end if;

  if (p1 = 2) then
    begin
      select "Doing something that fail (simulate an error) ...";
      drop table no_such_table;
    end;
  end if;

  if (p1 = 3) then
    begin
      select "Doing something that *SHOULD* works ...";
      select * from t1;
    end;
  end if;

end
$$

delimiter ;$$

#
# Code with RESIGNAL:
# errors are apparent to the caller,
# the but cleanup code did get a chance to act ...
#

call proc_top_b(1);

--error ER_BAD_TABLE_ERROR
call proc_top_b(2);

call proc_top_b(3);

call proc_top_b(1);

drop database demo;

