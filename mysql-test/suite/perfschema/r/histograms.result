update performance_schema.threads
set INSTRUMENTED='NO'
  where PROCESSLIST_ID = connection_id();
TRUNCATE TABLE performance_schema.events_statements_summary_by_digest;
TRUNCATE TABLE performance_schema.events_statements_histogram_by_digest;
TRUNCATE TABLE performance_schema.events_statements_histogram_global;
select count(*) from performance_schema.events_statements_summary_by_digest;
count(*)
0
select count(*) from performance_schema.events_statements_histogram_by_digest;
count(*)
0
select count(*) from performance_schema.events_statements_histogram_global;
count(*)
450
select * from performance_schema.events_statements_histogram_global
where COUNT_BUCKET > 0;
BUCKET_NUMBER	BUCKET_TIMER_LOW	BUCKET_TIMER_HIGH	COUNT_BUCKET	COUNT_BUCKET_AND_LOWER	BUCKET_QUANTILE
select BUCKET_NUMBER, BUCKET_TIMER_LOW, BUCKET_TIMER_HIGH
from performance_schema.events_statements_histogram_global
where BUCKET_NUMBER <= 10
order by BUCKET_NUMBER;
BUCKET_NUMBER	BUCKET_TIMER_LOW	BUCKET_TIMER_HIGH
0	0	10000000
1	10000000	10471285
2	10471285	10964781
3	10964781	11481536
4	11481536	12022644
5	12022644	12589254
6	12589254	13182567
7	13182567	13803842
8	13803842	14454397
9	14454397	15135612
10	15135612	15848931
select BUCKET_NUMBER, BUCKET_TIMER_LOW, BUCKET_TIMER_HIGH
from performance_schema.events_statements_histogram_global
where BUCKET_NUMBER >= 440
order by BUCKET_NUMBER;
BUCKET_NUMBER	BUCKET_TIMER_LOW	BUCKET_TIMER_HIGH
440	6025595860743779	6309573444802144
441	6309573444802144	6606934480076182
442	6606934480076182	6918309709189598
443	6918309709189598	7244359600750145
444	7244359600750145	7585775750292094
445	7585775750292094	7943282347243084
446	7943282347243084	8317637711026992
447	8317637711026992	8709635899561102
448	8709635899561102	9120108393559408
449	9120108393559408	18446744073709551615
select min(BUCKET_TIMER_HIGH/BUCKET_TIMER_LOW),
max(BUCKET_TIMER_HIGH/BUCKET_TIMER_LOW)
from performance_schema.events_statements_histogram_global
where (BUCKET_NUMBER > 0) and (BUCKET_NUMBER < 449);
min(BUCKET_TIMER_HIGH/BUCKET_TIMER_LOW)	max(BUCKET_TIMER_HIGH/BUCKET_TIMER_LOW)
1.0471	1.0471
select "A 1st statement" as marker_one;
marker_one
A 1st statement
select count(*)
from performance_schema.events_statements_histogram_by_digest;
count(*)
450
select count(*)
from performance_schema.events_statements_histogram_by_digest
where COUNT_BUCKET > 0;
count(*)
1
select DIGEST
from performance_schema.events_statements_summary_by_digest
where DIGEST_TEXT like "%marker_one%"
  into @digest_one;
select count(*)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
count(*)
450
select BUCKET_NUMBER, BUCKET_TIMER_LOW, BUCKET_TIMER_HIGH
from performance_schema.events_statements_histogram_by_digest
where (BUCKET_NUMBER <= 10)
and (DIGEST = @digest_one)
order by BUCKET_NUMBER;
BUCKET_NUMBER	BUCKET_TIMER_LOW	BUCKET_TIMER_HIGH
0	0	10000000
1	10000000	10471285
2	10471285	10964781
3	10964781	11481536
4	11481536	12022644
5	12022644	12589254
6	12589254	13182567
7	13182567	13803842
8	13803842	14454397
9	14454397	15135612
10	15135612	15848931
select BUCKET_NUMBER, BUCKET_TIMER_LOW, BUCKET_TIMER_HIGH
from performance_schema.events_statements_histogram_by_digest
where (BUCKET_NUMBER >= 440)
and (DIGEST = @digest_one)
order by BUCKET_NUMBER;
BUCKET_NUMBER	BUCKET_TIMER_LOW	BUCKET_TIMER_HIGH
440	6025595860743779	6309573444802144
441	6309573444802144	6606934480076182
442	6606934480076182	6918309709189598
443	6918309709189598	7244359600750145
444	7244359600750145	7585775750292094
445	7585775750292094	7943282347243084
446	7943282347243084	8317637711026992
447	8317637711026992	8709635899561102
448	8709635899561102	9120108393559408
449	9120108393559408	18446744073709551615
select min(BUCKET_TIMER_HIGH/BUCKET_TIMER_LOW),
max(BUCKET_TIMER_HIGH/BUCKET_TIMER_LOW)
from performance_schema.events_statements_histogram_by_digest
where (BUCKET_NUMBER > 0) and (BUCKET_NUMBER < 449)
and (DIGEST = @digest_one);
min(BUCKET_TIMER_HIGH/BUCKET_TIMER_LOW)	max(BUCKET_TIMER_HIGH/BUCKET_TIMER_LOW)
1.0471	1.0471
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
1
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
1
select "A 1st statement, again" as marker_one;
marker_one
A 1st statement, again
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
2
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
2
select "A 1st statement, again and again" as marker_one;
marker_one
A 1st statement, again and again
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
3
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
3
select "A 2nd statement" as marker_two;
marker_two
A 2nd statement
select DIGEST
from performance_schema.events_statements_summary_by_digest
where DIGEST_TEXT like "%marker_two%"
  into @digest_two;
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
3
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_two;
sum(COUNT_BUCKET)
1
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
4
select "A 2nd statement, again" as marker_two;
marker_two
A 2nd statement, again
select sum(COUNT_BUCKET), max(COUNT_BUCKET_AND_LOWER)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)	max(COUNT_BUCKET_AND_LOWER)
3	3
select sum(COUNT_BUCKET), max(COUNT_BUCKET_AND_LOWER)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_two;
sum(COUNT_BUCKET)	max(COUNT_BUCKET_AND_LOWER)
2	2
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
5
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where (DIGEST = @digest_one)
into @sum_digest_one;
select min(BUCKET_TIMER_LOW)
from performance_schema.events_statements_histogram_by_digest
where (DIGEST = @digest_one)
and (COUNT_BUCKET > 0)
into @min_digest_one;
select BUCKET_TIMER_HIGH
from performance_schema.events_statements_histogram_by_digest
where (DIGEST = @digest_one)
and (COUNT_BUCKET_AND_LOWER = @sum_digest_one)
order by BUCKET_NUMBER
limit 1
into @max_digest_one;
select DIGEST_TEXT,
"Check quantile relative values" as check_title,
(0 <= QUANTILE_95) as check_1,
(QUANTILE_95 <= QUANTILE_99) as check_2,
(QUANTILE_99 <= QUANTILE_999) as check_3
from performance_schema.events_statements_summary_by_digest;
DIGEST_TEXT	check_title	check_1	check_2	check_3
SELECT ? AS `marker_one`	Check quantile relative values	1	1	1
SELECT ? AS `marker_two`	Check quantile relative values	1	1	1
select
DIGEST_TEXT,
"Check quantile values within bounds" as check_title,
(0 <= @min_digest_one) as check_1,
(@min_digest_one <= QUANTILE_95) as check_2,
(QUANTILE_999 <= @max_digest_one) as check_3
from performance_schema.events_statements_summary_by_digest
where DIGEST = @digest_one;
DIGEST_TEXT	check_title	check_1	check_2	check_3
SELECT ? AS `marker_one`	Check quantile values within bounds	1	1	1
truncate table performance_schema.events_statements_histogram_by_digest;
select count(*)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
count(*)
450
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
0
select count(*)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_two;
count(*)
450
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_two;
sum(COUNT_BUCKET)
0
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
5
select DIGEST_TEXT, COUNT_STAR
from performance_schema.events_statements_summary_by_digest;
DIGEST_TEXT	COUNT_STAR
SELECT ? AS `marker_one`	3
SELECT ? AS `marker_two`	2
select "A 2nd statement, again" as marker_two;
marker_two
A 2nd statement, again
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
0
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_two;
sum(COUNT_BUCKET)
1
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
6
truncate table performance_schema.events_statements_histogram_global;
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
0
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_two;
sum(COUNT_BUCKET)
1
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
0
select DIGEST_TEXT, COUNT_STAR
from performance_schema.events_statements_summary_by_digest;
DIGEST_TEXT	COUNT_STAR
SELECT ? AS `marker_one`	3
SELECT ? AS `marker_two`	3
select "A 1st statement, again" as marker_one;
marker_one
A 1st statement, again
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
1
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_two;
sum(COUNT_BUCKET)
1
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
1
truncate table performance_schema.events_statements_summary_by_digest;
select count(*)
from performance_schema.events_statements_histogram_by_digest;
count(*)
0
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
1
select "A 1st statement, another time" as marker_one;
marker_one
A 1st statement, another time
select count(*)
from performance_schema.events_statements_histogram_by_digest;
count(*)
450
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
1
select count(*)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_two;
count(*)
0
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
2
truncate table performance_schema.events_statements_summary_global_by_event_name;
select count(*)
from performance_schema.events_statements_histogram_by_digest;
count(*)
450
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_one;
sum(COUNT_BUCKET)
1
select count(*)
from performance_schema.events_statements_histogram_by_digest
where DIGEST = @digest_two;
count(*)
0
select sum(COUNT_BUCKET)
from performance_schema.events_statements_histogram_global;
sum(COUNT_BUCKET)
0
