drop table if exists t1;
show variables like "ft\_%";
Variable_name	Value
ft_boolean_syntax	+ -><()~*:""&|
ft_max_word_len	84
ft_min_word_len	4
ft_query_expansion_limit	20
ft_stopword_file	(built-in)
create table t1 (b text not null, fulltext(b)) engine = innodb;
insert t1 values ('aaaaaa bbbbbb cccccc');
insert t1 values ('bbbbbb cccccc');
insert t1 values ('aaaaaa cccccc');
select * from t1 where match b against ('+aaaaaa bbbbbb' in boolean mode);
b
aaaaaa bbbbbb cccccc
aaaaaa cccccc
set ft_boolean_syntax=' +-><()~*:""&|';
ERROR HY000: Variable 'ft_boolean_syntax' is a GLOBAL variable and should be set with SET GLOBAL
set global ft_boolean_syntax=' +-><()~*:""&|';
select * from t1 where match b against ('+aaaaaa bbbbbb' in boolean mode);
b
aaaaaa bbbbbb cccccc
aaaaaa cccccc
set global ft_boolean_syntax='@ -><()~*:""&|';
select * from t1 where match b against ('+aaaaaa bbbbbb' in boolean mode);
b
aaaaaa bbbbbb cccccc
aaaaaa cccccc
select * from t1 where match b against ('+aaaaaa @bbbbbb' in boolean mode);
ERROR 42000: syntax error, unexpected '@', expecting $end
set global ft_boolean_syntax='@ -><()~*:""@|';
ERROR 42000: Variable 'ft_boolean_syntax' can't be set to the value of '@ -><()~*:""@|'
set global ft_boolean_syntax='+ -><()~*:""@!|';
ERROR 42000: Variable 'ft_boolean_syntax' can't be set to the value of '+ -><()~*:""@!|'
drop table t1;
