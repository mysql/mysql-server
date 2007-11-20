################################################################################
# t/partition_supported_sql_funcs_main.inc                                     #
#                                                                              #
# Purpose:                                                                     #
#  Tests which SQL functions are allowed in partinioning clauses.              #
#                                                                              #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: HH                                                          #
# Original Date: 2006-11-22                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
#                                                                              #
# This test uses a test frame (partition_supported_sql_funcs.inc) for every    #
# SQL function allowed in the partitioning parts of CREATE and ALTE TABLE.     #
# The variales represent the                                                   #
# - SQL function isself with a column (sqlfunc) and a literal (valsqlsunc),    #
# - the type of the column (coltype),                                          #
# - a file with test values of the coltype (infile) and                        # 
# - single test values (val1 to val4).                                         #
# The test frame includes CREATE/ALTER TABLE and some access statements.       #
# Column types are int, float(7,4), char(1), date and time depending on the    #
# SQL function. The test frame uses the include file                           # 
# "part_supported_sql_funcs_delete.inc" testing the deletion of           #
# partitions.                                                                  #
# The CREATE and ALTER TABLE statement do not cover the complete partitions    #
# functions, but will ashure that the SQL functions are basically working.     #
################################################################################


let $sqlfunc = abs(col1);
let $valsqlfunc = abs(15);
let $coltype = int;
let $infile = part_supported_sql_funcs_int_int.inc;
let $val1 =  5 ;
let $val2 = 13 ;
let $val3 = 17 ;
let $val4 = 15 ;
--source suite/parts/inc/partition_supported_sql_funcs.inc


let $sqlfunc = ceiling(col1);
let $valsqlfunc = ceiling(15);
let $coltype = float(7,4);
let $infile = part_supported_sql_funcs_int_float.inc;
let $val1 =  5.1230;
let $val2 = 13.345;
let $val3 = 17.987;
let $val4 = 15.654 ;
# DISABLED due to bug 30577
#--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = floor(col1);
let $valsqlfunc = floor(15.123);
let $coltype = float(7,4);
let $infile = part_supported_sql_funcs_int_float.inc;
let $val1 =  5.1230;
let $val2 = 13.345;
let $val3 = 17.987;
let $val4 = 15.654 ;
# DISABLED due to bug 30577
#--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = mod(col1,10);
let $valsqlfunc = mod(15,10);
let $coltype = int;
let $infile = part_supported_sql_funcs_int_int.inc;
let $val1 =  5;
let $val2 = 19;
let $val3 = 17;
let $val4 = 15 ;
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = day(col1);
let $valsqlfunc = day('2006-12-21');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-02-03';
let $val2 =  '2006-01-17';
let $val3 =  '2006-01-25';
let $val4 =  '2006-02-05';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = dayofmonth(col1);
let $valsqlfunc = dayofmonth('2006-12-24');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-02-03';
let $val2 =  '2006-01-17';
let $val3 =  '2006-01-25';
let $val4 =  '2006-02-05';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = dayofweek(col1);
let $valsqlfunc = dayofweek('2006-12-24');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-01-03';
let $val2 =  '2006-02-17';
let $val3 =  '2006-01-25';
let $val4 =  '2006-02-05';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = dayofyear(col1);
let $valsqlfunc = dayofyear('2006-12-25');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-01-03';
let $val2 =  '2006-01-17';
let $val3 =  '2006-02-25';
let $val4 =  '2006-02-05';
--source suite/parts/inc/partition_supported_sql_funcs.inc
let $coltype = char(30);
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = extract(month from col1);
let $valsqlfunc = extract(year from '1998-11-23');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-01-03';
let $val2 =  '2006-02-17';
let $val3 =  '2006-01-25';
let $val4 =  '2006-02-05';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = hour(col1);
let $valsqlfunc = hour('18:30');
let $coltype = time;
let $infile = part_supported_sql_funcs_int_time.inc;
let $val1 =  '09:09';
let $val2 =  '14:30';
let $val3 =  '21:59';
let $val4 =  '10:30';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = microsecond(col1);
let $valsqlfunc = microsecond('10:30:10.000010');
let $coltype = time;
let $infile = part_supported_sql_funcs_int_time.inc;
let $val1 =  '09:09:15.000002';
let $val2 =  '04:30:01.000018';
let $val3 =  '00:59:22.000024';
let $val4 =  '05:30:34.000037';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = minute(col1);
let $valsqlfunc = minute('18:30');
let $coltype = time;
let $val1 =  '09:09:15';
let $val2 =  '14:30:45';
let $val3 =  '21:59:22';
let $val4 =  '10:24:23';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = second(col1);
let $valsqlfunc = second('18:30:14');
let $coltype = time;
let $infile = part_supported_sql_funcs_int_time.inc;
let $val1 =  '09:09:09';
let $val2 =  '14:30:20';
let $val3 =  '21:59:22';
let $val4 =  '10:22:33';
--source suite/parts/inc/partition_supported_sql_funcs.inc
let $coltype = char(30);
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = month(col1);
let $valsqlfunc = month('2006-10-14');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-01-03';
let $val2 =  '2006-12-17';
let $val3 =  '2006-05-25';
let $val4 =  '2006-11-06';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = quarter(col1);
let $valsqlfunc = quarter('2006-10-14');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-01-03';
let $val2 =  '2006-12-17';
let $val3 =  '2006-09-25';
let $val4 =  '2006-07-30';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = time_to_sec(col1)-(time_to_sec(col1)-20);
let $valsqlfunc = time_to_sec('18:30:14')-(time_to_sec('17:59:59'));
let $coltype = time;
let $infile = part_supported_sql_funcs_int_time.inc;
let $val1 =  '09:09:15';
let $val2 =  '14:30:45';
let $val3 =  '21:59:22';
let $val4 =  '10:33:11';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = to_days(col1)-to_days('2006-01-01');
let $valsqlfunc = to_days('2006-02-02')-to_days('2006-01-01');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-02-03';
let $val2 =  '2006-01-17';
let $val3 =  '2006-01-25';
let $val4 =  '2006-02-06';
--source suite/parts/inc/partition_supported_sql_funcs.inc

# DATEDIFF() is implemented as (TO_DAYS(d1) - TO_DAYS(d2))
let $sqlfunc = datediff(col1, '2006-01-01');
let $valsqlfunc = datediff('2006-02-02', '2006-01-01');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-02-03';
let $val2 =  '2006-01-17';
let $val3 =  '2006-01-25';
let $val4 =  '2006-02-06';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = weekday(col1);
let $valsqlfunc = weekday('2006-10-14');
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-12-03';
let $val2 =  '2006-11-17';
let $val3 =  '2006-05-25';
let $val4 =  '2006-02-06';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = year(col1)-1990;
let $valsqlfunc = year('2005-10-14')-1990;
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '1996-01-03';
let $val2 =  '2000-02-17';
let $val3 =  '2004-05-25';
let $val4 =  '2002-02-15';
--source suite/parts/inc/partition_supported_sql_funcs.inc

let $sqlfunc = yearweek(col1)-200600;
let $valsqlfunc = yearweek('2006-10-14')-200600;
let $coltype = date;
let $infile = part_supported_sql_funcs_int_date.inc;
let $val1 =  '2006-01-03';
let $val2 =  '2006-08-17';
let $val3 =  '2006-03-25';
let $val4 =  '2006-11-15';
--source suite/parts/inc/partition_supported_sql_funcs.inc
