--echo #
--echo # Check consistency of delete-ref-check
--echo #

--echo # Create procedure to transactionally move a child->parent
--echo # ref between several child rows.
--echo # At all times there is a reference, but the reference
--echo # is from rows with different pks at different times.
--echo # The transaction to move the ref to a different child row
--echo # might some time get a Lock wait timeout error if the other
--echo # procedure's transaction has acquired the SimpleRead lock
--echo # for the same parent row at the same time.
--echo # We simply ignore those errors and move on.
--echo # At no time should it be possible to delete the parent row.
--echo #

delimiter %;

create procedure updateref(iterations int)
ur:begin
  declare continue handler for 1205 SET @dummy__ = 0;
  set @x = 1;
  repeat
    start transaction;
      delete from child;
      insert into child values (@x, 42);
    commit;
    set @x=@x+1;
  until @x = iterations
  end repeat;
end%

delimiter ;%

--echo #
--echo # Create procedure to continually attempt to delete
--echo # a parent row. This should always fail due to child
--echo # references. The error return codes would vary between
--echo # 1205(Lock wait time out) and 1451(child row exists)
--echo # depending on the time at which the delete is trying
--echo # acquire the child row lock w.r.to the other procedure.
--echo #

drop procedure if exists removeparent;

delimiter %;

create procedure removeparent(iterations int)
rp:begin
  declare continue handler for 1205, 1451 SET @err = @err+1;
  set @x = 0;
  set @err = 0;
  repeat
    delete from parent where id=42;
    set @x=@x+1;
    if @x != @err then
      leave rp;
    end if;
    until @x = iterations
  end repeat;
end%

delimiter ;%

--echo # Setting up initial values
insert into parent values (42);
insert into child values (0,42);

--echo # Sanity check, delete must fail
--error ER_ROW_IS_REFERENCED_2
delete from parent where id=42;

connection con1;
--echo # Start procedure moving child reference around
--send call updateref(100);

connection con2;
--echo # Start procedure attempting to delete parent
--send call removeparent(100);

--echo # Wait for parent delete to finish,
--echo # FK error is masked by the SP
--reap

--echo # Now let the child ref transfers complete.
connection con1;
--reap

--echo # Verify that the parent row is intact.
select * from parent;

--echo # Verify that we can add a new child row ref.
insert into child values (100,42);

--echo # Drop the procedures and delete the rows from tables
drop procedure updateref;
drop procedure removeparent;
delete from child;
delete from parent;
