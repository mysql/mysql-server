SET @@sql_mode = "";


## I. Validate direct SP calls that generate multiple-results and uses out-params
#
# 1. Assert that result-file doesn't contain out-params inside the resultsets.
# 2. Assert that out-params set their assigned user variables

## II. Validate SQL prepared SP calls that generate multiple-results and uses out-params
#
# 1. Assert that result-file doesn't contain out-params inside the resultsets.
# 2. Assert that out-params set their assigned user variables

## III. Validate Mysqlx prepared SP calls that generate multiple-results and uses out-params
#
# 1. Assert that result-file contain out-params inside the resultsets.
# 2. Assert that out-params don't set their assigned user variables


#
# I.
#
## Setup tests: tables and stored procedures
[Execute `empty`()]

0 rows affected
[Execute one_results_set()]
c1t1	c2t1
1	ONE
2	TWO
3	THREE

0 rows affected
[Execute three_results_sets()]
c1t1	c2t1
1	ONE
2	TWO
3	THREE
c1t2
2006-07-03 00:00:00
2013-06-15 00:00:00
2012-11-24 00:00:00
1979-11-24 00:00:00
c1t1	c2t1
1	ONE
2	TWO
3	THREE

0 rows affected
[Reseting test variables]
RUN SET @res=-1
RUN SET @min_val=-1
RUN SET @max_val=-1
RUN SET @in_val=5
[Execute out_param_no_select(@in_val,@res)]

0 rows affected
Verify [User variable "res" needs to have a value of 10]
[Execute out_params_with_select(@min_val,@max_val)]
c1t1	c2t1
1	ONE
2	TWO
3	THREE

0 rows affected
Verify [User variable "min_val" needs to have a value of 1979-11-24 00:00:00]
Verify [User variable "max_val" needs to have a value of 2013-06-15 00:00:00]
[Execute update_no_select()]

2 rows affected
[Execute update_and_select()]
c1t1	c2t1
1	ONE
5	TWO
6	THREE

0 rows affected
[Execute update_and_out_param(@max_val)]

1 rows affected
Verify [User variable "max_val" needs to have a value of 6]
[Execute update_and_out_param_and_select(@max_val)]
AVG(c1t1)
6.6667

0 rows affected
Verify [User variable "max_val" needs to have a value of 12]
[Execute insert_no_select()]

3 rows affected
[Execute insert_and_select()]
c1t1	c2t1
3	ONE
5	TWO
12	THREE
11	s11
12	s12
13	s13
111	s111
112	s112
13	s113

0 rows affected
[Execute insert_and_out_param(@max_val)]

1 rows affected
Verify [User variable "max_val" needs to have a value of 9999]
[Execute insert_and_out_param_and_select(@max_val)]
MIN(c1t1)
3

0 rows affected
Verify [User variable "max_val" needs to have a value of 19999]
[Execute error()]
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
[Execute select_and_error()]
c1t1	c2t1
3	ONE
5	TWO
12	THREE
11	s11
12	s12
13	s13
111	s111
112	s112
13	s113
9999	BIG ONE
19999	BIGER ONE
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
[Reseting test variables]
RUN SET @res=-1
RUN SET @min_val=-1
RUN SET @max_val=-1
RUN SET @in_val=5
[Execute out_param_and_error(@min_val)]
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
Verify [User variable "min_val" needs to have a value of -1]
[Execute select_out_param_and_error(@min_val)]
COUNT(c1t1)
11
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
Verify [User variable "min_val" needs to have a value of -1]
[Execute with_warning()]

0 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'tiny' at row 1
Got expected warning: 1264 
[Execute select_and_warning()]
c1t2
2006-07-03 00:00:00
2013-06-15 00:00:00
2012-11-24 00:00:00
1979-11-24 00:00:00

0 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'tiny' at row 1
Got expected warning: 1264 
[Execute out_param_and_warning(@vavg)]

1 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'tiny' at row 1
Got expected warning: 1264 
RUN SELECT @vavg
@vavg
2753.636474609375
0 rows affected
[Execute select_out_param_and_warning(@min_val)]
COUNT(c1t1)
11

0 rows affected
Warnings generated:
WARNING | 1265 | Data truncated for column 'c' at row 1
Got expected warning: 1265 
Verify [User variable "min_val" needs to have a value of 3]
Mysqlx.Ok {
  msg: "bye!"
}
ok

#
# II.
DROP SCHEMA IF EXISTS xtest;
#
## Setup tests: tables and stored procedures
[Prepare and execute `empty`()]

0 rows affected
[Prepare and execute one_results_set()]
c1t1	c2t1
1	ONE
2	TWO
3	THREE

0 rows affected
[Prepare and execute three_results_sets()]
c1t1	c2t1
1	ONE
2	TWO
3	THREE
c1t2
2006-07-03 00:00:00
2013-06-15 00:00:00
2012-11-24 00:00:00
1979-11-24 00:00:00
c1t1	c2t1
1	ONE
2	TWO
3	THREE

0 rows affected
[Reseting test variables]
RUN SET @res=-1
RUN SET @min_val=-1
RUN SET @max_val=-1
RUN SET @in_val=5
[Prepare and execute out_param_no_select(@in_val,?)]

0 rows affected
Verify [User variable "res" needs to have a value of 10]
[Prepare and execute out_params_with_select(?,?)]
c1t1	c2t1
1	ONE
2	TWO
3	THREE

0 rows affected
Verify [User variable "min_val" needs to have a value of 1979-11-24 00:00:00]
Verify [User variable "max_val" needs to have a value of 2013-06-15 00:00:00]
[Prepare and execute update_no_select()]

2 rows affected
[Prepare and execute update_and_select()]
c1t1	c2t1
1	ONE
5	TWO
6	THREE

0 rows affected
[Prepare and execute update_and_out_param(?)]

1 rows affected
Verify [User variable "max_val" needs to have a value of 6]
[Prepare and execute update_and_out_param_and_select(?)]
AVG(c1t1)
6.6667

0 rows affected
Verify [User variable "max_val" needs to have a value of 12]
[Prepare and execute insert_no_select()]

3 rows affected
[Prepare and execute insert_and_select()]
c1t1	c2t1
3	ONE
5	TWO
12	THREE
11	s11
12	s12
13	s13
111	s111
112	s112
13	s113

0 rows affected
[Prepare and execute insert_and_out_param(?)]

1 rows affected
Verify [User variable "max_val" needs to have a value of 9999]
[Prepare and execute insert_and_out_param_and_select(?)]
MIN(c1t1)
3

0 rows affected
Verify [User variable "max_val" needs to have a value of 19999]
[Prepare and execute error()]
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
[Prepare and execute select_and_error()]
c1t1	c2t1
3	ONE
5	TWO
12	THREE
11	s11
12	s12
13	s13
111	s111
112	s112
13	s113
9999	BIG ONE
19999	BIGER ONE
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
[Reseting test variables]
RUN SET @res=-1
RUN SET @min_val=-1
RUN SET @max_val=-1
RUN SET @in_val=5
[Prepare and execute out_param_and_error(?)]
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
Verify [User variable "min_val" needs to have a value of -1]
[Prepare and execute select_out_param_and_error(?)]
COUNT(c1t1)
11
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
Verify [User variable "min_val" needs to have a value of -1]
[Prepare and execute with_warning()]

0 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'tiny' at row 1
Got expected warning: 1264 
[Prepare and execute select_and_warning()]
c1t2
2006-07-03 00:00:00
2013-06-15 00:00:00
2012-11-24 00:00:00
1979-11-24 00:00:00

0 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'tiny' at row 1
Got expected warning: 1264 
[Prepare and execute out_param_and_warning(?)]

1 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'tiny' at row 1
Got expected warning: 1264 
RUN SELECT @vavg
@vavg
2753.636474609375
0 rows affected
[Prepare and execute select_out_param_and_warning(@min_val)]
COUNT(c1t1)
11

0 rows affected
Warnings generated:
WARNING | 1265 | Data truncated for column 'c' at row 1
Got expected warning: 1265 
Verify [User variable "min_val" needs to have a value of 3]
Mysqlx.Ok {
  msg: "bye!"
}
ok

#
# III.
DROP SCHEMA IF EXISTS xtest;
#
## Setup tests: tables and stored procedures
[Prepare and execute `empty`()]

0 rows affected
[Prepare and execute one_results_set()]
c1t1	c2t1
1	ONE
2	TWO
3	THREE

0 rows affected
[Prepare and execute three_results_sets()]
c1t1	c2t1
1	ONE
2	TWO
3	THREE
c1t2
2006-07-03 00:00:00
2013-06-15 00:00:00
2012-11-24 00:00:00
1979-11-24 00:00:00
c1t1	c2t1
1	ONE
2	TWO
3	THREE

0 rows affected
[Reseting test variables]
RUN SET @res=-1
RUN SET @min_val=-1
RUN SET @max_val=-1
RUN SET @in_val=5
[Prepare and execute out_param_no_select(@in_val,?)]

res
10
0 rows affected
Verify [User variable "res" needs to have a value of -1]
[Prepare and execute out_params_with_select(?,?)]
c1t1	c2t1
1	ONE
2	TWO
3	THREE
min_val	max_val
1979-11-24 00:00:00	2013-06-15 00:00:00
0 rows affected
Verify [User variable "min_val" needs to have a value of -1]
Verify [User variable "max_val" needs to have a value of -1]
[Prepare and execute update_no_select()]

2 rows affected
[Prepare and execute update_and_select()]
c1t1	c2t1
1	ONE
5	TWO
6	THREE

0 rows affected
[Prepare and execute update_and_out_param(?)]

max_val
6
1 rows affected
Verify [User variable "max_val" needs to have a value of -1]
[Prepare and execute update_and_out_param_and_select(?)]
AVG(c1t1)
6.6667
max_val
12
0 rows affected
Verify [User variable "max_val" needs to have a value of -1]
[Prepare and execute insert_no_select()]

3 rows affected
[Prepare and execute insert_and_select()]
c1t1	c2t1
3	ONE
5	TWO
12	THREE
11	s11
12	s12
13	s13
111	s111
112	s112
13	s113

0 rows affected
[Prepare and execute insert_and_out_param(?)]

max_val
9999
1 rows affected
Verify [User variable "max_val" needs to have a value of -1]
[Prepare and execute insert_and_out_param_and_select(?)]
MIN(c1t1)
3
max_val
19999
0 rows affected
Verify [User variable "max_val" needs to have a value of -1]
[Prepare and execute error()]
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
[Prepare and execute select_and_error()]
c1t1	c2t1
3	ONE
5	TWO
12	THREE
11	s11
12	s12
13	s13
111	s111
112	s112
13	s113
9999	BIG ONE
19999	BIGER ONE
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
[Reseting test variables]
RUN SET @res=-1
RUN SET @min_val=-1
RUN SET @max_val=-1
RUN SET @in_val=5
[Prepare and execute out_param_and_error(?)]
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
Verify [User variable "min_val" needs to have a value of -1]
[Prepare and execute select_out_param_and_error(?)]
COUNT(c1t1)
11
Got expected error: Table 'xtest.tab_not_existing' doesn't exist (code 1146)
Verify [User variable "min_val" needs to have a value of -1]
[Prepare and execute with_warning()]

0 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'tiny' at row 1
Got expected warning: 1264 
[Prepare and execute select_and_warning()]
c1t2
2006-07-03 00:00:00
2013-06-15 00:00:00
2012-11-24 00:00:00
1979-11-24 00:00:00

0 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'tiny' at row 1
Got expected warning: 1264 
[Prepare and execute out_param_and_warning(?)]

vavg
2753.64
1 rows affected
Warnings generated:
WARNING | 1264 | Out of range value for column 'tiny' at row 1
Got expected warning: 1264 
RUN SELECT @vavg
@vavg
null
0 rows affected
[Prepare and execute select_out_param_and_warning(@min_val)]
COUNT(c1t1)
11

0 rows affected
Warnings generated:
WARNING | 1265 | Data truncated for column 'c' at row 1
Got expected warning: 1265 
Verify [User variable "min_val" needs to have a value of 3]
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP SCHEMA IF EXISTS xtest;
