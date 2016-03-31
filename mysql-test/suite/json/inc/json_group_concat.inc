--echo # ----------------------------------------------------------------------
--echo # Test of aggregate function GROUP_CONCAT
--echo # ----------------------------------------------------------------------
create table t2(j json);
insert into t2 values (json_array(ST_GeomFromText('LineString(0 0, 1 1, 2 2)')));
select group_concat(j order by j) from t2;
drop table t2;

# MySQL and ECMA strings have different sets of printable control
# character conventions. So make sure we get the desired test
# characters in by inserting them one by one and checking that we insert
# a single character,  which we can test JSON_QUOTE on:

create table t (c varchar(20));
insert into t values ('\\');
select char_length(c) from t;
insert into t values (X'0C');                   # formfeed \f
select sum(char_length(c)) from t;
insert into t values ('"');
select sum(char_length(c)) from t;
insert into t values ('\a');
select sum(char_length(c)) from t;
insert into t values ('\b');
select sum(char_length(c)) from t;
insert into t values ('\t');
select sum(char_length(c)) from t;
insert into t values ('\n');
select sum(char_length(c)) from t;
insert into t values ('\r');
select sum(char_length(c)) from t;
insert into t values (X'10');                   # needs \u escape
select sum(char_length(c)) from t;

select group_concat(c order by c) from t;
select char_length(group_concat(c)) from t;
select json_quote(group_concat(c order by c)) from t;
select char_length(json_quote(group_concat(c))) from t;

select convert(group_concat(c) using utf8mb4) =
       json_unquote(group_concat(c)) as should_be_equal from t;

delete from t;
alter table t add column (j json);
insert into t values (NULL, NULL);

select json_quote(c), json_quote(j) from t;
select json_unquote(c), json_unquote(j) from t;

drop table t;
