use test;
drop procedure if exists signal_proc;
drop function if exists signal_func;
create procedure signal_proc()
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo;
SIGNAL foo SET MESSAGE_TEXT = "This is an error message";
RESIGNAL foo;
RESIGNAL foo SET MESSAGE_TEXT = "This is an error message";
end $$
create function signal_func() returns int
begin
DECLARE foo CONDITION FOR SQLSTATE '12345';
SIGNAL foo;
SIGNAL foo SET MESSAGE_TEXT = "This is an error message";
RESIGNAL foo;
RESIGNAL foo SET MESSAGE_TEXT = "This is an error message";
return 0;
end $$
show procedure code signal_proc;
Pos	Instruction
0	stmt "SIGNAL foo"
1	stmt "SIGNAL foo SET MESSAGE_TEXT = "This i..."
2	stmt "RESIGNAL foo"
3	stmt "RESIGNAL foo SET MESSAGE_TEXT = "This..."
drop procedure signal_proc;
show function code signal_func;
Pos	Instruction
0	stmt "SIGNAL foo"
1	stmt "SIGNAL foo SET MESSAGE_TEXT = "This i..."
2	stmt "RESIGNAL foo"
3	stmt "RESIGNAL foo SET MESSAGE_TEXT = "This..."
4	freturn 3 0
drop function signal_func;
