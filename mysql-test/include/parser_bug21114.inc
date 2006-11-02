#
# Bug#21114 (Foreign key creation fails to table with name format)
#
# Trying to trick the parser into thinking $FCT(...) is a function call,
# which is not in the CREATE TABLE and FOREIGN KEY ... REFERENCES syntax
#
# Usage :
#
# let $engine_type=InnoDb;
# let $verbose=1;
# let $FCT= <value_1>;
# -- source parser_stress_func.inc
# let $FCT= <value_2>;
# -- source parser_stress_func.inc
# let $verbose=0;
# let $FCT= <value_3>;
# -- source parser_stress_func.inc
# let $FCT= <value_4>;
# -- source parser_stress_func.inc

-- disable_warnings
eval drop table if exists $FCT;
drop table if exists bug21114_child;
-- enable_warnings

--disable_query_log
--disable_result_log

eval CREATE TABLE $FCT(
  col1 int not null,
  col2 int not null,
  col3 varchar(10),
  CONSTRAINT pk PRIMARY KEY (col1, col2)
) ENGINE $engine_type;

eval CREATE TABLE bug21114_child(
  pk int not null,
  fk_col1 int not null,
  fk_col2 int not null,
  fk_col3 int not null,
  fk_col4 int not null,
  CONSTRAINT fk_fct FOREIGN KEY (fk_col1, fk_col2)
    REFERENCES $FCT(col1, col2),
  CONSTRAINT fk_fct_space FOREIGN KEY (fk_col3, fk_col4)
    REFERENCES $FCT (col1, col2)
) ENGINE $engine_type;

--enable_query_log
--enable_result_log

if ($verbose)
{
  eval SHOW CREATE TABLE $FCT;
  SHOW CREATE TABLE bug21114_child;
}

DROP TABLE bug21114_child;
eval DROP TABLE $FCT;

