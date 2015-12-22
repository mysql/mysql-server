select @@global.innodb_ft_sort_pll_degree;
@@global.innodb_ft_sort_pll_degree
2
select @@session.innodb_ft_sort_pll_degree;
ERROR HY000: Variable 'innodb_ft_sort_pll_degree' is a GLOBAL variable
show global variables like 'innodb_ft_sort_pll_degree';
Variable_name	Value
innodb_ft_sort_pll_degree	2
show session variables like 'innodb_ft_sort_pll_degree';
Variable_name	Value
innodb_ft_sort_pll_degree	2
select * from performance_schema.global_variables where variable_name='innodb_ft_sort_pll_degree';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_sort_pll_degree	2
select * from performance_schema.session_variables where variable_name='innodb_ft_sort_pll_degree';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_sort_pll_degree	2
set global innodb_ft_sort_pll_degree=1;
ERROR HY000: Variable 'innodb_ft_sort_pll_degree' is a read only variable
set session innodb_ft_sort_pll_degree=1;
ERROR HY000: Variable 'innodb_ft_sort_pll_degree' is a read only variable
