# Test of functions
# 
# mysql -v < this_file

#
# numerical functions
#
select 1+1,1-1,1+1*2,8/5,8%5,mod(8,5),mod(8,5)|0,-(1+1)*-2,sign(-5) ;
select floor(5.5),floor(-5.5),ceiling(5.5),ceiling(-5.5),round(5.5),round(-5.5);
select abs(-10),log(exp(10)),ln(exp(10)),log2(65535),log(2,65535),exp(log(sqrt(10))*2),pow(10,log10(10)),rand(999999),rand();
select least(6,1.0,2.0),greatest(3,4,5,0) ;
select 1 | (1+1),5 & 3,bit_count(7) ;
#
# test functions
#
select 0=0,1>0,1>=1,1<0,1<=0,strcmp("abc","abcd"),strcmp("b","a"),strcmp("a","a") ;
select "a"<"b","a"<="b","b">="a","b">"a","a"="A","a"<>"b";
select "abc" like "a%", "abc" not like "%d%", "ab" like "a\%", "a%" like "a\%","abcd" like "a%b_%d";
select "Det här är svenska" regexp "h[[:alpha:]]+r", "aba" regexp "^(a|b)*$";
select !0,NOT 0=1,!(0=0),1 AND 1,1 && 0,0 OR 1,1 || NULL, 1=1 or 1=1 and 1=0;
select IF(0,"ERROR","this"),IF(1,"is","ERROR"),IF(NULL,"ERROR","a"),IF(1,2,3)|0,IF(1,2.0,3.0)+0 ;
select 2 between 1 and 3, "monty" between "max" and "my",2=2 and "monty" between "max" and "my" and 3=3;
select 2 in (3,2,5,9,5,1),"monty" in ("david","monty","allan"), 1.2 in (1.4,1.2,1.0);

#
# string functions
#
select 'hello',"'hello'",'""hello""','''h''e''l''l''o''',"hel""lo",'hel\'lo';
select concat("monty"," was here ","again"),length("hello"),ascii("hello");
select locate("he","hello"),locate("he","hello",2),locate("lo","hello",2) ;
select left("hello",2),right("hello",2),substring("hello",2,2),mid("hello",1,5) ;
select concat("",left(right(concat("what ",concat("is ","happening")),9),4),"",substring("monty",5,1)) ;
select concat("!",ltrim("  left  "),"!",rtrim("  right  "),"!");
select insert("txs",2,1,"hi"),insert("is ",4,0,"a"),insert("txxxxt",2,4,"es");
select replace("aaaa","a","b"),replace("aaaa","aa","b"),replace("aaaa","a","bb"),replace("aaaa","","b"),replace("bbbb","a","c");
select replace(concat(lcase(concat("THIS"," ","IS"," ","A"," ")),ucase("false")," ","test"),"FALSE","REAL") ;
select soundex(""),soundex("he"),soundex("hello all folks");
select password("test");
#
# varbinary as string and number
#
select 0x41,0x41+0,0x41 | 0x7fffffffffffffff | 0,0xffffffffffffffff | 0 ;

#
# misc functions
#
select interval(55,10,20,30,40,50,60,70,80,90,100),interval(3,1,1+1,1+1+1+1),field("IBM","NCA","ICL","SUN","IBM","DIGITAL"),field("A","B","C"),elt(2,"ONE","TWO","THREE"),interval(0,1,2,3,4),elt(1,1,2,3)|0,elt(1,1.1,1.2,1.3)+0;
select format(1.5555,0),format(123.5555,1),format(1234.5555,2),format(12345.5555,3),format(123456.5555,4),format(1234567.5555,5),format("12345.2399",2);

#
# system functions
#
select database(),user();

#
# Null tests
#
select null,isnull(null),isnull(1/0),isnull(1/0 = null),ifnull(null,1),ifnull(null,"TRUE"),ifnull("TRUE","ERROR"),1/0 is null,1 is not null;
select 1 | NULL,1 & NULL,1+NULL,1-NULL;
select NULL=NULL,NULL<>NULL,NULL IS NULL, NULL IS NOT NULL,IFNULL(NULL,1.1)+0,IFNULL(NULL,1) | 0;
select strcmp("a",NULL),(1<NULL)+0.0,NULL regexp "a",null like "a%","a%" like null;
select concat("a",NULL),replace(NULL,"a","b"),replace("string","i",NULL),replace("string",NULL,"i"),insert("abc",1,1,NULL),left(NULL,1);
select field(NULL,"a","b","c");
select 2 between null and 1,2 between 3 AND NULL,NULL between 1 and 2,2 between NULL and 3, 2 between 1 AND null,2 between null and 1,2 between 3 AND NULL;
#
# Wrong or 'funny' use of functions.
#
select insert("aa",100,1,"b"),insert("aa",1,3,"b"),left("aa",-1),substring("a",1,2);
select elt(2,1),field(NULL,"a","b","c");
select locate("a","b",2),locate("","a",1),ltrim("a"),rtrim("a");
select concat("1","2")|0,concat("1",".5")+0.0;

#
# time functions
# The last line should return new values for each test run
#
select from_days(to_days("960101")),to_days(960201)-to_days("19960101"),to_days(curdate()+1)-to_days(curdate()),weekday("1997-01-01") ;
select period_add("9602",-12),period_diff(199505,"9404") ;
select now()-now(),weekday(curdate())-weekday(now()),unix_timestamp()-unix_timestamp(now());
select now(),now()+0,curdate(),weekday(curdate()),weekday(now()),unix_timestamp(),unix_timestamp(now());
