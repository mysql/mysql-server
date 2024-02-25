set @v1a="Hello";
set @v2a="World";
set @v3a=65;
set @v4a=NULL;
select @v1a, @v2a, @v3a, @v4a, @v_unknown;
@v1a	@v2a	@v3a	@v4a	@v_unknown
Hello	World	65	NULL	NULL
select VARIABLE_NAME, VARIABLE_VALUE
from performance_schema.user_variables_by_thread
order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_VALUE
v1a	Hello
v2a	World
v3a	65
v4a	NULL
set @v1b="Salut";
set @v2b="La compagnie";
set @v3b=12;
set @v4b=NULL;
select @v1b, @v2b, @v3b, @v4b, @v_unknown;
@v1b	@v2b	@v3b	@v4b	@v_unknown
Salut	La compagnie	12	NULL	NULL
select VARIABLE_NAME, VARIABLE_VALUE
from performance_schema.user_variables_by_thread
order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_VALUE
v1a	Hello
v1b	Salut
v2a	World
v2b	La compagnie
v3a	65
v3b	12
v4a	NULL
v4b	NULL
select VARIABLE_NAME, VARIABLE_VALUE
from performance_schema.user_variables_by_thread
order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_VALUE
v1a	Hello
v2a	World
v3a	65
v4a	NULL
set @blob_under_16_bits=repeat("a", 65000);
set @blob_under_24_bits=repeat("a", 1000000);
select length(@blob_under_16_bits);
length(@blob_under_16_bits)
65000
select length(@blob_under_24_bits);
length(@blob_under_24_bits)
1000000
select variable_name, length(variable_value)
from performance_schema.user_variables_by_thread
where VARIABLE_NAME like "blob%"
  order by VARIABLE_NAME;
variable_name	length(variable_value)
blob_under_16_bits	65000
blob_under_24_bits	1000000
set @a=10;
CREATE procedure test_user_val
(IN input INT,OUT output INT)
BEGIN
SELECT input + input
INTO output;
END|
CALL test_user_val(@a , @total);
select VARIABLE_NAME,VARIABLE_VALUE from performance_schema.user_variables_by_thread 
where variable_name in ('a','total') order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_VALUE
a	10
total	20
create table t1 (c1 int);
CREATE TRIGGER test_uvar_trigger
AFTER INSERT ON t1
FOR EACH ROW BEGIN
set @a=@a+@a;
END|
insert into t1 values(1);
select VARIABLE_NAME,VARIABLE_VALUE from performance_schema.user_variables_by_thread 
where variable_name ='a' order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_VALUE
a	20
insert into t1 values(1);
select VARIABLE_NAME,VARIABLE_VALUE from performance_schema.user_variables_by_thread 
where variable_name ='a' order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_VALUE
a	40
drop table t1;
drop procedure test_user_val;
