--------------
select 1+1,1-1,1+1*2,8/5,8%5,mod(8,5),mod(8,5)|0,-(1+1)*-2,sign(-5)
--------------

1+1	1-1	1+1*2	8/5	8%5	mod(8,5)	mod(8,5)|0	-(1+1)*-2	sign(-5)
2	0	3	1.60	3	3	3	4	-1
--------------
select floor(5.5),floor(-5.5),ceiling(5.5),ceiling(-5.5),round(5.5),round(-5.5)
--------------

floor(5.5)	floor(-5.5)	ceiling(5.5)	ceiling(-5.5)	round(5.5)	round(-5.5)
5	-6	6	-5	6	-6
--------------
select abs(-10),log(exp(10)),ln(exp(10)),log2(65535),log(2,65535),exp(log(sqrt(10))*2),pow(10,log10(10)),rand(999999),rand()
--------------

abs(-10)	log(exp(10))	ln(exp(10))	log2(65535)	log(2,65535)	exp(log(sqrt(10))*2)	pow(10,log10(10))	rand(999999)	rand()
10	10.000000	10.000000	2.000000	2.000000	10.000000	10.000000	0.1844	0.7637
--------------
select least(6,1.0,2.0),greatest(3,4,5,0)
--------------

least(6,1.0,2.0)	greatest(3,4,5,0)
1.0	5
--------------
select 1 | (1+1),5 & 3,bit_count(7)
--------------

1 | (1+1)	5 & 3	bit_count(7)
3	1	3
--------------
select 0=0,1>0,1>=1,1<0,1<=0,strcmp("abc","abcd"),strcmp("b","a"),strcmp("a","a")
--------------

0=0	1>0	1>=1	1<0	1<=0	strcmp("abc","abcd")	strcmp("b","a")	strcmp("a","a")
1	1	1	0	0	-1	1	0
--------------
select "a"<"b","a"<="b","b">="a","b">"a","a"="A","a"<>"b"
--------------

"a"<"b"	"a"<="b"	"b">="a"	"b">"a"	"a"="A"	"a"<>"b"
1	1	1	1	1	1
--------------
select "abc" like "a%", "abc" not like "%d%", "ab" like "a\%", "a%" like "a\%","abcd" like "a%b_%d"
--------------

"abc" like "a%"	"abc" not like "%d%"	"ab" like "a\%"	"a%" like "a\%"	"abcd" like "a%b_%d"
1	1	0	1	1
--------------
select "Det här är svenska" regexp "h[[:alpha:]]+r", "aba" regexp "^(a|b)*$"
--------------

"Det här är svenska" regexp "h[[:alpha:]]+r"	"aba" regexp "^(a|b)*$"
1	1
--------------
select !0,NOT 0=1,!(0=0),1 AND 1,1 && 0,0 OR 1,1 || NULL, 1=1 or 1=1 and 1=0
--------------

!0	NOT 0=1	!(0=0)	1 AND 1	1 && 0	0 OR 1	1 || NULL	1=1 or 1=1 and 1=0
1	1	0	1	0	1	1	1
--------------
select IF(0,"ERROR","this"),IF(1,"is","ERROR"),IF(NULL,"ERROR","a"),IF(1,2,3)|0,IF(1,2.0,3.0)+0
--------------

IF(0,"ERROR","this")	IF(1,"is","ERROR")	IF(NULL,"ERROR","a")	IF(1,2,3)|0	IF(1,2.0,3.0)+0
this	is	a	2	2.0
--------------
select 2 between 1 and 3, "monty" between "max" and "my",2=2 and "monty" between "max" and "my" and 3=3
--------------

2 between 1 and 3	"monty" between "max" and "my"	2=2 and "monty" between "max" and "my" and 3=3
1	1	1
--------------
select 2 in (3,2,5,9,5,1),"monty" in ("david","monty","allan"), 1.2 in (1.4,1.2,1.0)
--------------

2 in (3,2,5,9,5,1)	"monty" in ("david","monty","allan")	1.2 in (1.4,1.2,1.0)
1	1	1
--------------
select 'hello',"'hello'",'""hello""','''h''e''l''l''o''',"hel""lo",'hel\'lo'
--------------

hello	'hello'	""hello""	'h'e'l'l'o'	hel"lo	hel'lo
hello	'hello'	""hello""	'h'e'l'l'o'	hel"lo	hel'lo
--------------
select concat("monty"," was here ","again"),length("hello"),ascii("hello")
--------------

concat("monty"," was here ","again")	length("hello")	ascii("hello")
monty was here again	5	104
--------------
select locate("he","hello"),locate("he","hello",2),locate("lo","hello",2)
--------------

locate("he","hello")	locate("he","hello",2)	locate("lo","hello",2)
1	0	4
--------------
select left("hello",2),right("hello",2),substring("hello",2,2),mid("hello",1,5)
--------------

left("hello",2)	right("hello",2)	substring("hello",2,2)	mid("hello",1,5)
he	lo	el	hello
--------------
select concat("",left(right(concat("what ",concat("is ","happening")),9),4),"",substring("monty",5,1))
--------------

concat("",left(right(concat("what ",concat("is ","happening")),9),4),"",substring("monty",5,1))
happy
--------------
select concat("!",ltrim("  left  "),"!",rtrim("  right  "),"!")
--------------

concat("!",ltrim("  left  "),"!",rtrim("  right  "),"!")
!left  !  right!
--------------
select insert("txs",2,1,"hi"),insert("is ",4,0,"a"),insert("txxxxt",2,4,"es")
--------------

insert("txs",2,1,"hi")	insert("is ",4,0,"a")	insert("txxxxt",2,4,"es")
this	is a	test
--------------
select replace("aaaa","a","b"),replace("aaaa","aa","b"),replace("aaaa","a","bb"),replace("aaaa","","b"),replace("bbbb","a","c")
--------------

replace("aaaa","a","b")	replace("aaaa","aa","b")	replace("aaaa","a","bb")	replace("aaaa","","b")	replace("bbbb","a","c")
bbbb	bb	bbbbbbbb	aaaa	bbbb
--------------
select replace(concat(lcase(concat("THIS"," ","IS"," ","A"," ")),ucase("false")," ","test"),"FALSE","REAL")
--------------

replace(concat(lcase(concat("THIS"," ","IS"," ","A"," ")),ucase("false")," ","test"),"FALSE","REAL")
this is a REAL test
--------------
select soundex(""),soundex("he"),soundex("hello all folks")
--------------

soundex("")	soundex("he")	soundex("hello all folks")
	H000	H4142
--------------
select password("test")
--------------

password("test")
378b243e220ca493
--------------
select 0x41,0x41+0,0x41 | 0x7fffffffffffffff | 0,0xffffffffffffffff | 0
--------------

0x41	0x41+0	0x41 | 0x7fffffffffffffff | 0	0xffffffffffffffff | 0
A	65	9223372036854775807	-1
--------------
select interval(55,10,20,30,40,50,60,70,80,90,100),interval(3,1,1+1,1+1+1+1),field("IBM","NCA","ICL","SUN","IBM","DIGITAL"),field("A","B","C"),elt(2,"ONE","TWO","THREE"),interval(0,1,2,3,4),elt(1,1,2,3)|0,elt(1,1.1,1.2,1.3)+0
--------------

interval(55,10,20,30,40,50,60,70,80,90,100)	interval(3,1,1+1,1+1+1+1)	field("IBM","NCA","ICL","SUN","IBM","DIGITAL")	field("A","B","C")	elt(2,"ONE","TWO","THREE")	interval(0,1,2,3,4)	elt(1,1,2,3)|0	elt(1,1.1,1.2,1.3)+0
5	2	4	0	TWO	0	1	1.1
--------------
select format(1.5555,0),format(123.5555,1),format(1234.5555,2),format(12345.5555,3),format(123456.5555,4),format(1234567.5555,5),format("12345.2399",2)
--------------

format(1.5555,0)	format(123.5555,1)	format(1234.5555,2)	format(12345.5555,3)	format(123456.5555,4)	format(1234567.5555,5)	format("12345.2399",2)
2	123.6	1,234.56	12,345.556	123,456.5555	1,234,567.55550	12,345.24
--------------
select database(),user()
--------------

database()	user()
	monty
--------------
select null,isnull(null),isnull(1/0),isnull(1/0 = null),ifnull(null,1),ifnull(null,"TRUE"),ifnull("TRUE","ERROR"),1/0 is null,1 is not null
--------------

NULL	isnull(null)	isnull(1/0)	isnull(1/0 = null)	ifnull(null,1)	ifnull(null,"TRUE")	ifnull("TRUE","ERROR")	1/0 is null	1 is not null
NULL	1	1	1	1	TRUE	TRUE	1	1
--------------
select 1 | NULL,1 & NULL,1+NULL,1-NULL
--------------

1 | NULL	1 & NULL	1+NULL	1-NULL
NULL	NULL	NULL	NULL
--------------
select NULL=NULL,NULL<>NULL,NULL IS NULL, NULL IS NOT NULL,IFNULL(NULL,1.1)+0,IFNULL(NULL,1) | 0
--------------

NULL=NULL	NULL<>NULL	NULL IS NULL	NULL IS NOT NULL	IFNULL(NULL,1.1)+0	IFNULL(NULL,1) | 0
NULL	NULL	1	0	1.1	1
--------------
select strcmp("a",NULL),(1<NULL)+0.0,NULL regexp "a",null like "a%","a%" like null
--------------

strcmp("a",NULL)	(1<NULL)+0.0	NULL regexp "a"	null like "a%"	"a%" like null
NULL	NULL	NULL	NULL	NULL
--------------
select concat("a",NULL),replace(NULL,"a","b"),replace("string","i",NULL),replace("string",NULL,"i"),insert("abc",1,1,NULL),left(NULL,1)
--------------

concat("a",NULL)	replace(NULL,"a","b")	replace("string","i",NULL)	replace("string",NULL,"i")	insert("abc",1,1,NULL)	left(NULL,1)
NULL	NULL	NULL	NULL	NULL	NULL
--------------
select field(NULL,"a","b","c")
--------------

field(NULL,"a","b","c")
0
--------------
select 2 between null and 1,2 between 3 AND NULL,NULL between 1 and 2,2 between NULL and 3, 2 between 1 AND null,2 between null and 1,2 between 3 AND NULL
--------------

2 between null and 1	2 between 3 AND NULL	NULL between 1 and 2	2 between NULL and 3	2 between 1 AND null	2 between null and 1	2 between 3 AND NULL
0	0	NULL	NULL	NULL	0	0
--------------
select insert("aa",100,1,"b"),insert("aa",1,3,"b"),left("aa",-1),substring("a",1,2)
--------------

insert("aa",100,1,"b")	insert("aa",1,3,"b")	left("aa",-1)	substring("a",1,2)
aa	b		a
--------------
select elt(2,1),field(NULL,"a","b","c")
--------------

elt(2,1)	field(NULL,"a","b","c")
NULL	0
--------------
select locate("a","b",2),locate("","a",1),ltrim("a"),rtrim("a")
--------------

locate("a","b",2)	locate("","a",1)	ltrim("a")	rtrim("a")
0	1	a	a
--------------
select concat("1","2")|0,concat("1",".5")+0.0
--------------

concat("1","2")|0	concat("1",".5")+0.0
12	1.5
--------------
select from_days(to_days("960101")),to_days(960201)-to_days("19960101"),to_days(curdate()+1)-to_days(curdate()),weekday("1997-01-01")
--------------

from_days(to_days("960101"))	to_days(960201)-to_days("19960101")	to_days(curdate()+1)-to_days(curdate())	weekday("1997-01-01")
1996-01-01	31	1	2
--------------
select period_add("9602",-12),period_diff(199505,"9404")
--------------

period_add("9602",-12)	period_diff(199505,"9404")
199502	13
--------------
select now()-now(),weekday(curdate())-weekday(now()),unix_timestamp()-unix_timestamp(now())
--------------

now()-now()	weekday(curdate())-weekday(now())	unix_timestamp()-unix_timestamp(now())
0	0	0
--------------
select now(),now()+0,curdate(),weekday(curdate()),weekday(now()),unix_timestamp(),unix_timestamp(now())
--------------

now()	now()+0	curdate()	weekday(curdate())	weekday(now())	unix_timestamp()	unix_timestamp(now())
1998-08-17 04:24:33	19980817042433	1998-08-17	0	0	903317073	903317073
