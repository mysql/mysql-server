# enkel sökning
#
use ok;
#set option sql_big_tables=1;

select period from ok_system;
select * from ok_system;
select ok_system.* from ok_system;

# sökning med ej nyckel
#
select station.ok_namn from station where föreningsnr = 58 and ok_namn like "%LJUGARN%";
select ok_namn from station where ok_namn like "%leden" ;

# sökning med ej nyckel samt sortering och limit
#
select station.ok_namn from station where föreningsnr = 57+1 order by ok_namn;
select stationsnr from station where föreningsnr = 58 order by ok_namn;
select ok_namn from station order by ok_namn desc limit 10;
select ok_namn from station order by ok_namn desc limit 5;
select ok_namn from station order by ok_namn desc limit 5,5;

# Sökning med nyckel = constant på icke unik nyckel
# Här läses databasen direkt med read-next på ok_namn
#
select station.ok_namn from station where ok_namn = 'JÄRLÅSMACKEN AB';
select station.ok_namn from station where ok_namn LIKE 'JÄRLÅSMACKEN A_';
select station.ok_namn from station where ok_namn LIKE 'JÄRL_SMACKEN A_';
select station.ok_namn from station where ok_namn LIKE 'JÄRLÅSMACKEN AB%';
select station.ok_namn from station where ok_namn LIKE 'J%AB';
select station.ok_namn from station where ok_namn LIKE 'JÄRLÅSMACKEN AB_';
select station.ok_namn from station where ok_namn LIKE 'b_s%';

#
# Test av USE INDEX and IGNORE INDEX
#
explain select station.ok_namn from station where ok_namn = 'JÄRLÅSMACKEN AB';
explain select ok_namn from station ignore index (ok_namn) where ok_namn = 'JÄRLÅSMACKEN AB';
explain select ok_namn from station use index (stationsnr) where ok_namn = 'JÄRLÅSMACKEN AB';
explain select ok_namn from station use index (ok_namn) where ok_namn = 'JÄRLÅSMACKEN AB';
explain select ok_namn from station use index (stationsnr,ok_namn) where ok_namn = 'JÄRLÅSMACKEN AB';

# Följande skall ge fel
explain select ok_namn from station ignore index (ok_namn,not_used);
explain select ok_namn from station use index (not_used);


# Test av sortering på på använd nyckel (ingen sortering behövs)
#
select station.ok_namn from station where ok_namn >= 'JÄRLÅSMACKEN AB' and ok_namn <= 'JÄRLÅSMACKEN AB' order by ok_namn;
select stationsnr,ok_namn from station where ok_namn="" or ok_namn = "." order by ok_namn ;

# sökning med nyckel = constant med flera träffar
# Här läses databasen direkt med read-next på ok_namn ifall få träffar
#
select stationsnr,ok_namn from station where föreningsnr = 37 and ok_namn = 'ÖSTERSUND';


# sökning med eller-nivåer på nyckel
# Ifall man kan begränsa nyckel till ett antal intervall sökes endast de
# möjliga intervallen igenom.
#
select stationsnr from station where stationsnr=250501 or stationsnr="250502"; 
select stationsnr from station where stationsnr=250501 or stationsnr=250502 or stationsnr >= 250505 and stationsnr <= 250601 or stationsnr between 250501 and 250502;


# sökning med nyckel LIKE constant
# Ifall LIKE begreppet börjar på en bokstav används nyckeln
#
select stationsnr,ok_namn from station where föreningsnr = 37 and ok_namn like 'f%';
select ok_namn from station where ok_namn like "L%" and ok_namn = "ok";
select ok_namn from station where (ok_namn like "L%" and ok_namn = "Lund");
select stationsnr,ok_namn from station where stationsnr like "25050%";
select stationsnr,ok_namn from station where stationsnr like "25050_";

# Sökning med distinct
# ifall distinct endast är på en riktig tabell görs en automatisk group över
# alla fält. I andra fall skapas alltid en temporär databas.
# Ifall endast sorteringsfält från huvudregistret, sorteras detta först innan
# den distinkta tabellen skapas.
#
select distinct föreningsnr from station ;
select distinct föreningsnr from station order by föreningsnr;
select distinct föreningsnr from station order by föreningsnr desc;
select distinct station.ok_namn,period from station,ok_system where föreningsnr = 58 and ok_namn like "O%";
select distinct ok_namn from station where föreningsnr = 34 order by ok_namn;
select distinct ok_namn from station limit 10;
select distinct ok_namn from station having ok_namn like "A%" limit 10;
select distinct substring(ok_namn,1,3) from station where ok_namn like "A%";
select distinct substring(ok_namn,1,3) as a from station having a like "A%" order by a limit 10 ;
select distinct substring(ok_namn,1,3) from station where ok_namn like "A%" limit 10;
select distinct substring(ok_namn,1,3) as a from station having a like "A%" limit 10;
SET OPTION SQL_BIG_TABLES=1;
select distinct concat(ok_namn," ",ok_namn) as namn from station,station_period where station.stationsnr=station_period.stationsnr order by namn limit 10;
SET OPTION SQL_BIG_TABLES=0;
select distinct concat(ok_namn," ",ok_namn) from station,station_period where station.stationsnr=station_period.stationsnr order by ok_namn limit 10;
select distinct co from station limit 10;

#
# Force use of remove_dupp
#
select distinct ok_namn,count(*) from station group by föreningsnr,ok_namn;
SET OPTION SQL_BIG_TABLES=1; # Force use of MyISAM
select distinct ok_namn,count(*) from station group by föreningsnr,ok_namn;
SET OPTION SQL_BIG_TABLES=0;
select distinct ok_namn,repeat("a",length(ok_namn)),count(*) from station group by föreningsnr,ok_namn;

# En order by som borde använda merge i filesort!
select distinct föreningsnr,rtrim(space(512+föreningsnr)) from station_period order by 1,2;

# Sökning med distinct och order och flera register
# Ifall result-posten är liten skapas en unik nyckel i temporärbasen som
# täcker hela posten. I annat fall görs en jämförelse alla-mot-alla för att
# eliminera dubletterna :(
# Detta specifica exempel tar tyvärr station_period som huvudtabell. Fixas
# genom att
# sätta nyckel på föreningsnr.

select distinct ok_namn from station,station_period where station.föreningsnr = 34 and station.stationsnr=station_period.stationsnr order by ok_namn;

#
# Here the last ok_namn is optimized away from the order by
#
explain select station_period.stationsnr,ok_namn from station,station_period where station.föreningsnr = 34 and station.stationsnr=station_period.stationsnr order by station_period.stationsnr,ok_namn;

# Sökning med konstantregister (systemregister samt nyckel = konstant)
# Alla konstantregister läses i början och härefter läses resten normalt.
#
select period from ok_system;
select period from ok_system where period=1900;
select ok_namn,period from ok_system,station where stationsnr = 011401 order by period;


# Sökning med konstantregister och flera nyckeldelar (posterna läses 1 gång vid
# start av sökningen)
#
select ok_namn,period from station,station_period where station.stationsnr = 011401 and station.stationsnr=station_period.stationsnr and station_period.period=9501;
explain select ok_namn,period from station,station_period where station.stationsnr = 011401 and station_period.stationsnr=station.stationsnr and 9501 = station_period.period;

# Sökning med ett konstantregister och flera poster i andra register
#
select ok_namn,period from station,ok_system where föreningsnr*10 = 37*10;

# Sökning med register referens och sökning på ej nyckel
# (Här är station_period huvudregister)
#
select ok_namn,period,priv_inköp,ftg_inköp from station,station_period where station.stationsnr=station_period.stationsnr and period >= 9401 and period <= 9402 and station.föreningsnr = 34 order by ok_namn,period, priv_inköp;


# Sökning med intervall på ett register med full nyckel på referensregister
# (Här är station huvudregister och endast stationerna i stationsnummer
# intervallet kontrolleras)
#
select station.stationsnr,ok_namn,period,priv_inköp,ftg_inköp from station,station_period where station.stationsnr>= 250501 and station.stationsnr <= 250505 and station.stationsnr=station_period.stationsnr and period = 9401 and station.föreningsnr = 34;

#
# Test of stright join to force a full join
#
select STRAIGHT_JOIN station.föreningsnr,föreningsnamn from forening,station where station.föreningsnr=forening.föreningsnr group by station.föreningsnr;

select SQL_SMALL_RESULT station.föreningsnr,föreningsnamn from forening,station where station.föreningsnr=forening.föreningsnr group by station.föreningsnr;

#Full join (samt alias)
select * from ok_system,ok_system ok_system2;
select station.stationsnr,station2.stationsnr from station,station station2 where station.stationsnr >= 250501 and station.stationsnr <= 250505 and station2.stationsnr >= 250501 and station2.stationsnr <= 250505;

#
# test of left join
#
select station.föreningsnr,föreningsnamn from station left join forening using (föreningsnr) where forening.föreningsnr is null;
explain select station.föreningsnr,föreningsnamn from station left join forening using (föreningsnr) where forening.föreningsnr is null;
explain select station.föreningsnr,föreningsnamn from forening left join station using (föreningsnr) where station.föreningsnr is null;

#
# Join med formler
#

select distinct station.föreningsnr,forening.föreningsnr from station,forening where station.föreningsnr=forening.föreningsnr+1;
explain select distinct station.föreningsnr,forening.föreningsnr from station,forening where station.föreningsnr=forening.föreningsnr+1;

# Sökning med 'eller' med samma referens-begrepp
# Först görs en intervall sökning i det första registret och sedan refereras
# det andra med nyckel med en 'test ifall nyckelanvänding' för varje post
#
select station.stationsnr,station.föreningsnr,ok_namn,period from station_period,station where station.stationsnr = 250501 and station.stationsnr=station_period.stationsnr and period = 9401 or station.stationsnr = 250502 and station.stationsnr=station_period.stationsnr and period = 9402;
select station.stationsnr,station.föreningsnr,ok_namn,period from station_period,station where (station.stationsnr = 250501 or station.stationsnr = 250502) and station.stationsnr=station_period.stationsnr and period>=9401 and period<=9402;
select station.stationsnr,station.föreningsnr,ok_namn,period from station_period,station where (station_period.stationsnr = 250501 or station_period.stationsnr = 250502) and station.stationsnr=station_period.stationsnr and period>=9401 and period<=9402;

# Test of many paren levels
#
select period from ok_system where (((period > 0) or period < 10000 or (period = 1900)) and (period=1900 and period <= 1901) or (period=1903 and (period=1903)) and period>=1902) or ((period=1904 or period=1905) or (period=1906 or period>1907)) or (period=1908 and period = 1909) ;
select period from ok_system where ((period > 0 and period < 1) or (((period > 0 and period < 100) and (period > 10)) or (period > 10)) or (period > 0 and (period > 5 or period > 6)));

select a.stationsnr from station as a,station b where ((a.stationsnr = 250501 and a.stationsnr=b.stationsnr) or a.stationsnr=250502 or a.stationsnr=250503 or (a.stationsnr=250505 and a.stationsnr<=b.stationsnr and b.stationsnr>=a.stationsnr)) and a.stationsnr=b.stationsnr;

select stationsnr from station where stationsnr in (250502,98005,98006,250503,250605,250606) and stationsnr >=250502 and stationsnr not in (250605,250606);

select stationsnr from station where stationsnr between 250502 and 250504;

select ok_namn from station where (((ok_namn like "_%L%" ) or (ok_namn like "%ok%")) and ( ok_namn like "L%" or ok_namn like "G%")) and ok_namn like "L%" ;


# Group on one table
# optimizer: sort table by group and send rows.
#
select count(*) from ok_system ;
select föreningsnr,count(*),sum(stationsnr) from station group by föreningsnr ;
select föreningsnr,count(*) from station group by föreningsnr order by föreningsnr desc limit 5;
select count(*),min(stationsnamn),max(stationsnamn),sum(stationsnr),avg(stationsnr),std(stationsnr) from station where föreningsnr = 34 and stationsnamn<>"";
select föreningsnr,count(*),min(stationsnamn),max(stationsnamn),sum(stationsnr),avg(stationsnr),std(stationsnr) from station group by föreningsnr limit 3;
select föreningsnr,stationsnr,count(priv_inköp),sum(priv_inköp),min(priv_inköp),max(priv_inköp),avg(priv_inköp) from station_period where föreningsnr = 34 group by föreningsnr,stationsnr ;
select /*! SQL_SMALL_RESULT */ föreningsnr,stationsnr,count(priv_inköp),sum(priv_inköp),min(priv_inköp),max(priv_inköp),avg(priv_inköp) from station_period where föreningsnr = 34 group by föreningsnr,stationsnr ;
select föreningsnr,count(priv_inköp),sum(priv_inköp),min(priv_inköp),max(priv_inköp),avg(priv_inköp) from station_period group by föreningsnr ;
select distinct mod(föreningsnr,10) from forening group by föreningsnr;
select distinct 1 from forening group by föreningsnr;
select count(distinct stationsnr) from station;
select föreningsnr,count(distinct stationsnr) from station group by föreningsnr;
select föreningsnr,count(*) from station group by föreningsnr;
select föreningsnr,count(distinct concat(stationsnr,repeat(65,1000))) from station group by föreningsnr;
select föreningsnr,count(distinct concat(stationsnr,repeat(65,200))) from station group by föreningsnr;
select föreningsnr,count(distinct floor(stationsnr/100)) from station group by föreningsnr; 
select föreningsnr,count(distinct concat(repeat(65,1000),floor(stationsnr/100))) from station group by föreningsnr;

#
# group with where on a key field
#
select sum(stationsnr) from station where ok_namn="." group by stationsnr limit 10;
select stationsnr,count(*) from station_drvm where stationsnr=098001 group by stationsnr;
select stationsnr,count(*) from station_drvm where stationsnr=098004 and ftg>10 group by stationsnr;
select count(*) from station_drvm where stationsnr=98001 and ftg=1;
select stationsnr,count(*) from station_drvm where stationsnr=98001 and ftg=1 group by stationsnr;
select stationsnr,count(*) from station_drvm where stationsnr>=98004 and stationsnr <=98005 group by stationsnr;
select station.stationsnr,count(*) from station,station_drvm where station.stationsnr=98004 and station_drvm.stationsnr=station.stationsnr group by station_drvm.stationsnr;

# Group with extra not group fields.
#
select föreningsnr|0,föreningsnamn from forening group by 1 ;
select station.föreningsnr,föreningsnamn,count(*) from station,forening where station.föreningsnr=forening.föreningsnr group by station.föreningsnr order by föreningsnamn;

#
# Calculation with group functions
#

select sum(Period)/count(*) from ok_system ;
select föreningsnr,count(priv_inköp) as "count",sum(priv_inköp) as "sum" ,sum(priv_inköp)/count(priv_inköp)-avg(priv_inköp) as "diff",(0+count(priv_inköp))*föreningsnr as func from station_period group by föreningsnr ;
select föreningsnr,sum(priv_inköp)/count(priv_inköp) as avg from station_period group by föreningsnr having avg > 70000000 order by avg;

# Group with order on not first table
# optimizer: sort table by group and write group records to tmp table.
#            sort tmp_table and send rows.
#
select föreningsnr,count(*) from station group by föreningsnr order by 2 desc ;
select föreningsnr,count(*) from station where föreningsnr > 40 group by föreningsnr order by 2 desc ;
select station.stationsnamn,station.stationsnr,count(priv_inköp),sum(priv_inköp),min(priv_inköp),max(priv_inköp),avg(priv_inköp) from station_period,station where station_period.föreningsnr = 34 and station.stationsnr = station_period.stationsnr group by stationsnr,station.stationsnamn ;


# group by with many tables
# optimizer: create tmp table with group-by uniq index.
#           write with update to tmp table.
#           sort tmp table according to order (or group if no order)
#	    send rows
#
select station_period.föreningsnr,ok_namn,sum(priv_inköp) from station_period,station where station.stationsnr = station_period.stationsnr and station_period.föreningsnr = 58 group by föreningsnr,ok_namn ;
select station.föreningsnr,count(*),min(ok_namn),max(ok_namn),sum(priv_inköp),avg(priv_inköp) from station,station_period where station_period.föreningsnr >= 30 and station_period.föreningsnr <= 58 and station_period.stationsnr = station.stationsnr and 1+1=2 group by station.föreningsnr;


# group with many tables and long group on many tables. group on formula
#optimizer: create tmp table with neaded fields
#           sort tmp table by group and calculate sums to new table
#	    if different order by than group, sort tmp table
#	    send rows

select station_period.föreningsnr+0,station_period.stationsnr,ok_namn,sum(priv_inköp) from station_period,station where station.stationsnr = station_period.stationsnr and station_period.föreningsnr = 34 group by 1,station_period.stationsnr,ok_namn,ok_namn,ok_namn,ok_namn,ok_namn order by stationsnr;


# WHERE const folding
# optimize: If there is a "field = const" part in the where, change all
#           instances of field in the and level to const.
#	    All instances of const = const are checked once and removed.

#Where -> station_period.stationsnr = 98005 and station.stationsnr = 98005
select sum(priv_inköp) from station_period,station where station.stationsnr = station_period.stationsnr and station_period.föreningsnr = 58 and station_period.stationsnr = 98004 and station.stationsnr = 98005 or station.stationsnr = station_period.stationsnr and station_period.stationsnr = 98005 and station.stationsnr = 98005 ;

select station.stationsnr,sum(priv_inköp) from station_period,station where station.stationsnr = station_period.stationsnr and station_period.föreningsnr = 58 and station_period.stationsnr = 98004 and station.stationsnr = 98005 or station.stationsnr = station_period.stationsnr and station_period.stationsnr = 98005 and station.stationsnr = 98005 or station_period.stationsnr = station.stationsnr and station.stationsnr = 98004 group by station.stationsnr ;

explain select ok_namn from station where 1>2 or 2>3;
explain select ok_namn from station where stationsnr=stationsnr;

#
# HAVING
#
select föreningsnr,stationsnr from station HAVING stationsnr=250501 or stationsnr=250502; 
select föreningsnr,stationsnr from station WHERE stationsnr>=250501 HAVING stationsnr<=250502;
select föreningsnr,count(*) as count,sum(stationsnr) as sum from station group by föreningsnr having count > 40 and sum/count >= 120000 ;
select föreningsnr from station group by föreningsnr having count(*) > 40 and sum(stationsnr)/count(*) >= 120000 ;
select station.föreningsnr,föreningsnamn,count(*) from station,forening where station.föreningsnr=forening.föreningsnr group by föreningsnamn having station.föreningsnr >= 40;

#
# MIN(), MAX() and COUNT() optimizing
#
select count(*) from station;
select count(*) from station where stationsnr < 098024;
select min(stationsnr) from station where stationsnr>= 098024;
select max(stationsnr) from station where stationsnr>= 098024;
select count(*) from station_drvm where stationsnr=098024;
select count(*) from station_drvm where kundnr=78987 and stationsnr=098024;
explain select min(stationsnr),max(stationsnr),count(*) from station;
select min(stationsnr),max(stationsnr),count(*) from station;
select min(kortnr),max(kortnr) from station_drvm where kundnr=78987 and stationsnr=098024;
select count(*),min(kortnr),max(kortnr) from station_drvm where kundnr=78987 and stationsnr=098024;
select kundnr,count(*) from station_drvm where stationsnr=098024 group by kundnr limit 20;
select max(kundnr) from station_drvm where stationsnr=98024;
#
# Test of alias
#
select system.period from ok_system = system ;
select system.period from ok_system as system ;
select system.period as "Nuvarande period" from ok_system as system;
select period as ok_period from ok_system;
select period as ok_period from ok_system group by ok_period;
select 1+1 as summa from ok_system group by summa;
select period as "Nuvarande period" from ok_system group by "Nuvarande period";

#
# Some simple show commands
#
show databases;
show tables;
show tables from kf96 like "s%";
show columns from station ;
show columns from station from ok like 's%' ;
show keys from station ;
show variables;
show variables like "p%";
show table status from ok like "station%";


#############################################################################
#############################################################################
#####################  END `ok' TESTS  ######################################
#############################################################################
#############################################################################


#
# numerical functions
#
select 1+1,1-1,1+1*2,8/5,8%5,mod(8,5),mod(8,5)|0,-(1+1)*-2,sign(-5) ;
select floor(5.5),floor(-5.5),ceiling(5.5),ceiling(-5.5),round(5.5),round(-5.5);
select round(5.64,1),round(5.64,2),round(5.64,-1),round(5.64,-2);
select truncate(52.64,1),truncate(52.64,2),truncate(52.64,-1),truncate(52.64,-2);
select abs(-10),log(exp(10)),exp(log(sqrt(10))*2),pow(10,log10(10)),rand(999999),rand(),power(2,4);
select pi(),sin(pi()/2),cos(pi()/2),tan(pi()),cot(1),asin(1),acos(0),atan(1);
select 1 | (1+1),5 & 3,bit_count(7) ;
select 1 << 32,1 << 63, 1 << 64, 4 >> 2, 4 >> 63, 1<< 63 >> 60;
select 10,10.0,10.,.1e+2,100.0e-1;
select 6e-05, -6e-05, --6e-05, -6e-05+1.000000;
select 0,256,00000000000000065536,2147483647,-2147483648,2147483648,+4294967296;
select 922337203685477580,92233720368547758000;
select -922337203685477580,-92233720368547758000;
select 9223372036854775807,-009223372036854775808;
select +9999999999999999999,-9999999999999999999;
select degrees(pi()),radians(360);

#
# test functions
#
select 0=0,1>0,1>=1,1<0,1<=0,1!=0,strcmp("abc","abcd"),strcmp("b","a"),strcmp("a","a") ;
select "a"<"b","a"<="b","b">="a","b">"a","a"="A","a"<>"b";
select "a "="A", "A "="a", "a  " <= "A b"; 
select "abc" like "a%", "abc" not like "%d%", "a%" like "a\%","abc%" like "a%\%","abcd" like "a%b_%d", "a" like "%%a","abcde" like "a%_e","abc" like "abc%";
select "a" like "%%b","a" like "%%ab","ab" like "a\%", "ab" like "_", "ab" like "ab_", "abc" like "%_d", "abc" like "abc%d";
select '?' like '|%', '?' like '|%' ESCAPE '|', '%' like '|%', '%' like '|%' ESCAPE '|', '%' like '%';
select 'abc' like '%c','abcabc' like '%c',  "ab" like "", "ab" like "a", "ab" like "ab";
select "Det här är svenska" regexp "h[[:alpha:]]+r", "aba" regexp "^(a|b)*$";
select "aba" regexp concat("^","a");
select !0,NOT 0=1,!(0=0),1 AND 1,1 && 0,0 OR 1,1 || NULL, 1=1 or 1=1 and 1=0;
select IF(0,"ERROR","this"),IF(1,"is","ERROR"),IF(NULL,"ERROR","a"),IF(1,2,3)|0,IF(1,2.0,3.0)+0 ;
select 2 between 1 and 3, "monty" between "max" and "my",2=2 and "monty" between "max" and "my" and 3=3;
select 'b' between 'a' and 'c', 'B' between 'a' and 'c';
select 2 in (3,2,5,9,5,1),"monty" in ("david","monty","allan"), 1.2 in (1.4,1.2,1.0);
select -1.49 or -1.49,0.6 or 0.6;
select least(1,2,3) | greatest(16,32,8), least(5,4)*1,greatest(-1.0,1.0)*1,least(3,2,1)*1.0,greatest(1,1.1,1.0),least("10",9),greatest("A","B","0");
select decode(encode(repeat("a",100000),"monty"),"monty")=repeat("a",100000);
select decode(encode("abcdef","monty"),"monty")="abcdef";

select CASE "b" when "a" then 1 when "b" then 2 END;
select CASE "c" when "a" then 1 when "b" then 2 END;
select CASE "c" when "a" then 1 when "b" then 2 ELSE 3 END;
select CASE BINARY "b" when "a" then 1 when "B" then 2 WHEN "b" then "ok" END;
select CASE "b" when "a" then 1 when binary "B" then 2 WHEN "b" then "ok" END;
select CASE concat("a","b") when concat("ab","") then "a" when "b" then "b" end;
select CASE when 1=0 then "true" else "false" END;
select CASE 1 when 1 then "one" WHEN 2 then "two" ELSE "more" END;
select CASE 2.0 when 1 then "one" WHEN 2.0 then "two" ELSE "more" END;
select (CASE "two" when "one" then "1" WHEN "two" then "2" END) | 0;
select (CASE "two" when "one" then 1.00 WHEN "two" then 2.00 END) +0.0;
select case 1/0 when "a" then "true" else "false" END;
select case 1/0 when "a" then "true" END;
select (case 1/0 when "a" then "true" END) | 0;
select (case 1/0 when "a" then "true" END) + 0.0;
select case when 1>0 then "TRUE" else "FALSE" END;
select case when 1<0 then "TRUE" else "FALSE" END;

#
# string functions
#
select 'hello',"'hello'",'""hello""','''h''e''l''l''o''',"hel""lo",'hel\'lo';
select 'hello' 'monty';
select length("\n\t\r\b\0\_\%\\");
select concat("monty"," was here ","again"),length("hello"),char(ascii('h'));
select locate("he","hello"),locate("he","hello",2),locate("lo","hello",2) ;
select instr("hello","he");
select position("ll" in "hello"),position("a" in "hello");
select left("hello",2),right("hello",2),substring("hello",2,2),mid("hello",1,5) ;
select concat("",left(right(concat("what ",concat("is ","happening")),9),4),"",substring("monty",5,1)) ;
select substring_index("www.tcx.se",".",-2),substring_index("www.tcx.se",".",1);
select substring_index("www.tcx.se","tcx",1),substring_index("www.tcx.se","tcx",-1);
select substring_index(".tcx.se",".",-2),substring_index(".tcx.se",".tcx",-1);

select concat(":",ltrim("  left  "),":",rtrim("  right  "),":");
select concat(":",trim(LEADING FROM " left"),":",trim(TRAILING FROM " right "),":");
select concat(":",trim(" m "),":",trim(BOTH FROM " y "),":",trim("*" FROM "*s*"),":");
select concat(":",trim(BOTH "ab" FROM "ababmyabab"),":",trim(BOTH "*" FROM "***sql"),":");
select concat(":",trim(LEADING ".*" FROM ".*my"),":",trim(TRAILING ".*" FROM "sql.*.*"),":");

select insert("txs",2,1,"hi"),insert("is ",4,0,"a"),insert("txxxxt",2,4,"es");
select replace("aaaa","a","b"),replace("aaaa","aa","b"),replace("aaaa","a","bb"),replace("aaaa","","b"),replace("bbbb","a","c");
select replace(concat(lcase(concat("THIS"," ","IS"," ","A"," ")),ucase("false")," ","test"),"FALSE","REAL") ;
select soundex(""),soundex("he"),soundex("hello all folks");
select password("test"),length(encrypt("test")),encrypt("test","aa");
select md5("hello");
select repeat("monty",5),concat("*",space(5),"*");
select reverse("abc"),reverse("abcd");
select rpad("a",4,"1"),rpad("a",4,"12"),rpad("abcd",3,"12");
select lpad("a",4,"1"),lpad("a",4,"12"),lpad("abcd",3,"12");
select rpad(741653838,17,'0'),lpad(741653838,17,'0');
select LEAST(NULL,'HARRY','HARRIOT',NULL,'HAROLD'),GREATEST(NULL,'HARRY','HARRIOT',NULL,'HAROLD');

#
# varbinary as string and number
#
select 0x41,0x41+0,0x41 | 0x7fffffffffffffff | 0,0xffffffffffffffff | 0 ;
select 0x31+1,concat(0x31)+1,-0xf;

#
# misc functions
#
select interval(55,10,20,30,40,50,60,70,80,90,100),interval(3,1,1+1,1+1+1+1),field("IBM","NCA","ICL","SUN","IBM","DIGITAL"),field("A","B","C"),elt(2,"ONE","TWO","THREE"),interval(0,1,2,3,4),elt(1,1,2,3)|0,elt(1,1.1,1.2,1.3)+0;
select find_in_set("b","a,b,c"),find_in_set("c","a,b,c"),find_in_set("dd","a,bbb,dd"),find_in_set("bbb","a,bbb,dd");
select find_in_set("d","a,b,c"),find_in_set("dd","a,bbb,d"),find_in_set("bb","a,bbb,dd");
select make_set(0,'a','b','c'),make_set(-1,'a','b','c'),make_set(1,'a','b','c'),make_set(2,'a','b','c'),make_set(1+2,concat('a','b'),'c');
select make_set(NULL,'a','b','c'),make_set(1|4,'a',NULL,'c'),make_set(1+2,'a',NULL,'c');
select export_set(9,"Y","N","-",5),export_set(9,"Y","N"),export_set(9,"Y","N","");

select format(1.5555,0),format(123.5555,1),format(1234.5555,2),format(12345.5555,3),format(123456.5555,4),format(1234567.5555,5),format("12345.2399",2);

select inet_ntoa(inet_aton("255.255.255.255.255.255.255.255"));
select inet_aton("255.255.255.255.255"),inet_aton("255.255.1.255"),inet_aton("0.1.255");
select inet_ntoa(1099511627775),inet_ntoa(4294902271),inet_ntoa(511);

#
# system functions
#
select database(),user();

#
# Null tests
#
select null,\N,isnull(null),isnull(1/0),isnull(1/0 = null),ifnull(null,1),ifnull(null,"TRUE"),ifnull("TRUE","ERROR"),1/0 is null,1 is not null;
select 1 | NULL,1 & NULL,1+NULL,1-NULL;
select NULL=NULL,NULL<>NULL,IFNULL(NULL,1.1)+0,IFNULL(NULL,1) | 0;
select strcmp("a",NULL),(1<NULL)+0.0,NULL regexp "a",null like "a%","a%" like null;
select concat("a",NULL),replace(NULL,"a","b"),replace("string","i",NULL),replace("string",NULL,"i"),insert("abc",1,1,NULL),left(NULL,1);
select repeat("a",0),repeat("ab",5+5),repeat("ab",-1),reverse(NULL);
select field(NULL,"a","b","c");
select 2 between null and 1,2 between 3 AND NULL,NULL between 1 and 2,2 between NULL and 3, 2 between 1 AND null;
SELECT NULL AND NULL, 1 AND NULL, NULL AND 1, NULL OR NULL, 0 OR NULL, NULL OR 0;
SELECT (NULL OR NULL) IS NULL;
select NULL AND 0, 0 and NULL;
select inet_ntoa(null),inet_aton(null),inet_aton("122.256"),inet_aton("122.226."),inet_aton("");
#
# Wrong or 'funny' use of functions.
#

select insert("aa",100,1,"b"),insert("aa",1,3,"b"),left("aa",-1),substring("a",1,2);
select elt(2,1),field(NULL,"a","b","c"),reverse("");
select find_in_set("","a,b,c"),find_in_set("","a,b,c,"),find_in_set("",",a,b,c");
select find_in_set("abc","abc"),find_in_set("ab","abc"),find_in_set("abcd","abc");
select locate("a","b",2),locate("","a",1);
select ltrim("a"),rtrim("a"),trim(BOTH "" from "a"),trim(BOTH " " from "a");
select concat("1","2")|0,concat("1",".5")+0.0;
select substring_index("www.tcx.se","",3);
select length(repeat("a",100000000)),length(repeat("a",1000*64));
select position("0" in "baaa" in (1)),position("0" in "1" in (1,2,3)),position("sql" in ("mysql"));
select position(("1" in (1,2,3)) in "01");
select 5 between 0 and 10 between 0 and 1,(5 between 0 and 10) between 0 and 1;
select 1 and 2 between 2 and 10, 2 between 2 and 10 and 1;
select 1 and 0 or 2, 2 or 1 and 0;
select length(repeat("a",65500)),length(concat(repeat("a",32000),repeat("a",32000))),length(replace("aaaaa","a",concat(repeat("a",10000)))),length(insert(repeat("a",40000),1,30000,repeat("b",50000)));
select length(repeat("a",1000000)),length(concat(repeat("a",32000),repeat("a",32000),repeat("a",32000))),length(replace("aaaaa","a",concat(repeat("a",32000)))),length(insert(repeat("a",48000),1,1000,repeat("a",48000)));
select 1+2/*hello*/+3;
select 1 /* long
multi line comment */;
/* empty query */;
select 1 /*!32301 +1 */;
select 1 /*!52301 +1 */;

#
# time functions
#
select from_days(to_days("960101")),to_days(960201)-to_days("19960101"),to_days(date_add(curdate(), interval 1 day))-to_days(curdate()),weekday("1997-11-29");
select period_add("9602",-12),period_diff(199505,"9404") ;
select now()-now(),weekday(curdate())-weekday(now()),unix_timestamp()-unix_timestamp(now());
select from_unixtime(unix_timestamp("1994-03-02 10:11:12")),from_unixtime(unix_timestamp("1994-03-02 10:11:12"),"%Y-%m-%d %h:%i:%s"),from_unixtime(unix_timestamp("1994-03-02 10:11:12"))+0;
select sec_to_time(9001),sec_to_time(9001)+0,time_to_sec("15:12:22");
select now()-curdate()*1000000-curtime();
select strcmp(current_timestamp(),concat(current_date()," ",current_time()));
select date_format("1997-01-02 03:04:05", "%M %W %D %Y %y %m %d %h %i %s %w");
select date_format("1997-01-02", concat("%M %W %D ","%Y %y %m %d %h %i %s %w"));
select dayofmonth("1997-01-02"),dayofmonth(19970323);
select month("1997-01-02"),year("98-02-03"),dayofyear("1997-12-31");
select DAYOFYEAR("1997-03-03"), WEEK("1998-03-03"), QUARTER(980303);
select HOUR("1997-03-03 23:03:22"), MINUTE("23:03:22"), SECOND(230322);
select week(19980101),week(19970101),week(19980101,1),week(19970101,1);
select week(19981231),week(19971231),week(19981231,1),week(19971231,1);
select week(19950101),week(19950101,1);
select yearweek('1981-12-31',1),yearweek('1982-01-01',1),yearweek('1982-12-31',1),yearweek('1983-01-01',1);
select date_format('1998-12-31','%x-%v'),date_format('1999-01-01','%x-%v');
select date_format('1999-12-31','%x-%v'),date_format('2000-01-01','%x-%v');
select yearweek('1987-01-01',1),yearweek('1987-01-01');

select dayname("1962-03-03"),dayname("1962-03-03")+0;
select monthname("1972-03-04"),monthname("1972-03-04")+0;
select time_format(19980131000000,'%H|%I|%k|%l|%i|%p|%r|%S|%T');
select time_format(19980131010203,'%H|%I|%k|%l|%i|%p|%r|%S|%T');
select time_format(19980131131415,'%H|%I|%k|%l|%i|%p|%r|%S|%T');
select time_format(19980131010015,'%H|%I|%k|%l|%i|%p|%r|%S|%T');
select date_format(concat('19980131',131415),'%H|%I|%k|%l|%i|%p|%r|%S|%T| %M|%W|%D|%Y|%y|%a|%b|%j|%m|%d|%h|%s|%w');
select date_format(19980021000000,'%H|%I|%k|%l|%i|%p|%r|%S|%T| %M|%W|%D|%Y|%y|%a|%b|%j|%m|%d|%h|%s|%w');
select date_add("1997-12-31 23:59:59",INTERVAL 1 SECOND);
select date_add("1997-12-31 23:59:59",INTERVAL 1 MINUTE);
select date_add("1997-12-31 23:59:59",INTERVAL 1 HOUR);
select date_add("1997-12-31 23:59:59",INTERVAL 1 DAY);
select date_add("1997-12-31 23:59:59",INTERVAL 1 MONTH);
select date_add("1997-12-31 23:59:59",INTERVAL 1 YEAR);
select date_add("1997-12-31 23:59:59",INTERVAL "1:1" MINUTE_SECOND);
select date_add("1997-12-31 23:59:59",INTERVAL "1:1" HOUR_MINUTE);
select date_add("1997-12-31 23:59:59",INTERVAL "1:1" DAY_HOUR);
select date_add("1997-12-31 23:59:59",INTERVAL "1 1" YEAR_MONTH);
select date_add("1997-12-31 23:59:59",INTERVAL "1:1:1" HOUR_SECOND);
select date_add("1997-12-31 23:59:59",INTERVAL "1 1:1" DAY_MINUTE);
select date_add("1997-12-31 23:59:59",INTERVAL "1 1:1:1" DAY_SECOND);

select date_sub("1998-01-01 00:00:00",INTERVAL 1 SECOND);
select date_sub("1998-01-01 00:00:00",INTERVAL 1 MINUTE);
select date_sub("1998-01-01 00:00:00",INTERVAL 1 HOUR);
select date_sub("1998-01-01 00:00:00",INTERVAL 1 DAY);
select date_sub("1998-01-01 00:00:00",INTERVAL 1 MONTH);
select date_sub("1998-01-01 00:00:00",INTERVAL 1 YEAR);
select date_sub("1998-01-01 00:00:00",INTERVAL "1:1" MINUTE_SECOND);
select date_sub("1998-01-01 00:00:00",INTERVAL "1:1" HOUR_MINUTE);
select date_sub("1998-01-01 00:00:00",INTERVAL "1:1" DAY_HOUR);
select date_sub("1998-01-01 00:00:00",INTERVAL "1 1" YEAR_MONTH);
select date_sub("1998-01-01 00:00:00",INTERVAL "1:1:1" HOUR_SECOND);
select date_sub("1998-01-01 00:00:00",INTERVAL "1 1:1" DAY_MINUTE);
select date_sub("1998-01-01 00:00:00",INTERVAL "1 1:1:1" DAY_SECOND);

select date_add("1997-12-31 23:59:59",INTERVAL 100000 SECOND);
select date_add("1997-12-31 23:59:59",INTERVAL -100000 MINUTE);
select date_add("1997-12-31 23:59:59",INTERVAL 100000 HOUR);
select date_add("1997-12-31 23:59:59",INTERVAL -100000 DAY);
select date_add("1997-12-31 23:59:59",INTERVAL 100000 MONTH);
select date_add("1997-12-31 23:59:59",INTERVAL -100000 YEAR);
select date_add("1997-12-31 23:59:59",INTERVAL "10000:1" MINUTE_SECOND);
select date_add("1997-12-31 23:59:59",INTERVAL "-10000:1" HOUR_MINUTE);
select date_add("1997-12-31 23:59:59",INTERVAL "10000:1" DAY_HOUR);
select date_add("1997-12-31 23:59:59",INTERVAL "-100 1" YEAR_MONTH);
select date_add("1997-12-31 23:59:59",INTERVAL "10000:99:99" HOUR_SECOND);
select date_add("1997-12-31 23:59:59",INTERVAL " -10000 99:99" DAY_MINUTE);
select date_add("1997-12-31 23:59:59",INTERVAL "10000 99:99:99" DAY_SECOND);
select "1997-12-31 23:59:59" + INTERVAL 1 SECOND;
select INTERVAL 1 DAY + "1997-12-31";
select "1998-01-01 00:00:00" - INTERVAL 1 SECOND;

select date_sub("1998-01-02",INTERVAL 31 DAY);
select date_add("1997-12-31",INTERVAL 1 SECOND);
select date_add("1997-12-31",INTERVAL 1 DAY);
select date_add(NULL,INTERVAL 100000 SECOND);
select date_add("1997-12-31 23:59:59",INTERVAL NULL SECOND);
select date_add("1997-12-31 23:59:59",INTERVAL NULL MINUTE_SECOND);
select date_add("9999-12-31 23:59:59",INTERVAL 1 SECOND);
select date_sub("0000-00-00 00:00:00",INTERVAL 1 SECOND);
select date_add('1998-01-30',Interval 1 month);
select date_add('1998-01-30',Interval '2:1' year_month);
select date_add('1996-02-29',Interval '1' year);
select extract(YEAR FROM "1999-01-02 10:11:12");
select extract(YEAR_MONTH FROM "1999-01-02");
select extract(DAY FROM "1999-01-02");
select extract(DAY_HOUR FROM "1999-01-02 10:11:12");
select extract(DAY_MINUTE FROM "02 10:11:12");
select extract(DAY_SECOND FROM "225 10:11:12");
select extract(HOUR FROM "1999-01-02 10:11:12");
select extract(HOUR_MINUTE FROM "10:11:12");
select extract(HOUR_SECOND FROM "10:11:12");
select extract(MINUTE FROM "10:11:12");
select extract(MINUTE_SECOND FROM "10:11:12");
select extract(SECOND FROM "1999-01-02 10:11:12");

#
# test variables
#
set @`test`=1,@TEST=3,@select=2;
select @test,@`select`,@TEST,@not_used;
set @test_int=10,@test_double=1e-10,@test_string="abcdeghi",@test_string2="abcdefghij",@select=NULL;
select @test_int,@test_double,@test_string,@test_string2,@select;
set @test_int="hello",@test_double="hello",@test_string="hello",@test_string2="hello";
select @test_int,@test_double,@test_string,@test_string2;
set @test_int="hellohello",@test_double="hellohello",@test_string="hellohello",@test_string2="hellohello";
select @test_int,@test_double,@test_string,@test_string2;
set @test_int=null,@test_double=null,@test_string=null,@test_string2=null;
select @test_int,@test_double,@test_string,@test_string2;
select @t1:=(@t2:=1)+@t3:=4,@t1,@t2,@t3;

#
# Some functions that has failed sometimes
#
select concat(encrypt('haha','R1'),encrypt('haha','R2'));

#
# Functions and groups
#
select concat(station_period.föreningsnr,station_period.stationsnr) as concat,station_period.stationsnr+1,left(ok_namn,4),sum(priv_inköp/100),sum(length(concat("1","2"))) from station_period,station where station.stationsnr = station_period.stationsnr and station_period.föreningsnr = 34 group by concat,2,3 order by 2;

#
# Some new procedures
#
select stationsnr from station_drvm where stationsnr>=98001 and stationsnr <= 98002 group by stationsnr procedure split_sum(5,dm_1_kvant10,1,stationsnr,kundnr,kortnr) ;
select stationsnr from station_drvm where stationsnr>=98001 and stationsnr <= 98002 group by stationsnr procedure split_count(5,dm_1_kvant10,1,stationsnr,kundnr,kortnr) ;

#
# explain
#
explain select period from ok_system;
explain select period from ok_system where period=1900;
explain select ok_namn from station where ok_namn like "L%" and ok_namn = "ok";
explain select distinct ok_namn from station,station_period where station.stationsnr=station_period.stationsnr order by ok_namn;
explain select 1;
explain select count(*) from ok_system;

select forening.föreningsnr,max(ok_namn) from station,forening where forening.föreningsnr=station.föreningsnr group by 1 into outfile "/tmp/select-test.out";

#
# ODBC compatibility
# 

select {fn length("hello")}, { date "1997-10-20" };

#
# AVG() crashed for Perra when sorting.
#

use kf96;
#set option sql_big_tables=1;

select övergrupp as Övergrupp,AVG(100 * omdöme * antal_läst_mersmak / antal_läst_med_mersmak / antal_intervjuade) as my_index from mersmak_tidnings_info,mersmak_artikel_info where mersmak_tidnings_info.år=mersmak_artikel_info.år and mersmak_tidnings_info.nummer=mersmak_artikel_info.nr and mersmak_tidnings_info.namn=mersmak_artikel_info.artikelnamn and mersmak_tidnings_info.typ='ar' and mersmak_tidnings_info.bildnr=1 group by Övergrupp order by my_index desc ;

select concat("O",substring('000',1,3-length(concat(111))),"K");

#
# Test in the "test" database
#

use test;
#set option sql_big_tables=1;

drop table if exists t1,t2,t3,t4,t5;

CREATE TABLE t1 (
  auto int(5) unsigned DEFAULT 0 NOT NULL auto_increment,
  string char(10) default "hello",
  tiny tinyint(4) DEFAULT '0' NOT NULL ,
  short smallint(6) DEFAULT '1' NOT NULL ,
  medium mediumint(8) DEFAULT '0' NOT NULL,
  long_int int(11) DEFAULT '0' NOT NULL,
  longlong bigint(13) DEFAULT '0' NOT NULL,
  real_float float(13,1) DEFAULT 0.0 NOT NULL,
  real_double double(16,4),
  utiny tinyint(3) unsigned DEFAULT '0' NOT NULL,
  ushort smallint(5) unsigned zerofill DEFAULT '00000' NOT NULL,
  umedium mediumint(8) unsigned DEFAULT '0' NOT NULL,
  ulong int(11) unsigned DEFAULT '0' NOT NULL,
  ulonglong bigint(13) unsigned DEFAULT '0' NOT NULL,
  time_stamp timestamp,
  date_field date,	
  time_field time,	
  date_time datetime,
  blob_col blob,
  tinyblob_col tinyblob,
  mediumblob_col mediumblob  not null,
  longblob_col longblob  not null,
  options enum('one','two','tree') not null,
  flags set('one','two','tree') not null,
  PRIMARY KEY (auto),
  KEY (utiny),
  KEY (tiny),
  KEY (short),
  KEY any_name (medium),
  KEY (longlong),
  KEY (real_float),
  KEY (ushort),
  KEY (umedium),
  KEY (ulong),
  KEY (ulonglong,ulong),
  KEY (options,flags)
);

show fields from t1;
show keys from t1;

CREATE UNIQUE INDEX test on t1 ( auto ) ;
CREATE INDEX test2 on t1 ( ulonglong,ulong) ;
CREATE INDEX test3 on t1 ( medium ) ;
DROP INDEX test ON t1;

insert into t1 values (10, 1,1,1,1,1,1,1,1,1,1,1,1,1,NULL,0,0,0,1,1,1,1,'one','one');
insert into t1 values (NULL,2,2,2,2,2,2,2,2,2,2,2,2,2,NULL,NULL,NULL,NULL,NULL,NULL,2,2,'two','two,one');
insert into t1 values (0,1/3,3,3,3,3,3,3,3,3,3,3,3,3,NULL,'19970303','10:10:10','19970303 101010','','','','3',3,3);
insert into t1 values (0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,NULL,19970807,080706,19970403090807,-1,-1,-1,'-1',-1,-1);
insert into t1 values (0,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,NULL,0,0,0,-4294967295,-4294967295,-4294967295,'-4294967295',0,"one,two,tree");
insert into t1 values (0,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,NULL,0,0,0,4294967295,4294967295,4294967295,'4294967295',0,0);
insert into t1 (tiny) values (1);

select auto,string,tiny,short,medium,long_int,longlong,real_float,real_double,utiny,ushort,umedium,ulong,ulonglong,mod(floor(time_stamp/1000000),1000000)-mod(curdate(),1000000),date_field,time_field,date_time,blob_col,tinyblob_col,mediumblob_col,longblob_col from t1;


ALTER TABLE t1
add new_field char(10) default "new" not null,
change blob_col new_blob_col varchar(20),
change date_field date_field char(10),
alter column string set default "new default",
alter short drop default,
DROP INDEX utiny,
DROP INDEX ushort,
DROP PRIMARY KEY,
DROP FOREIGN KEY any_name,
ADD INDEX (auto);

LOCK TABLES t1 WRITE;
ALTER TABLE t1 
RENAME as t2,
DROP longblob_col;
UNLOCK TABLES;

ALTER TABLE t2 rename as t3;
LOCK TABLES t3 WRITE ;
ALTER TABLE t3 rename as t1;
UNLOCK TABLES;

select auto,new_field,new_blob_col,date_field from t1 ;

#
# check with old syntax
#
CREATE TABLE t2 (
  auto int(5) unsigned NOT NULL DEFAULT 0 auto_increment,
  string char(20),
  mediumblob_col mediumblob not null,
  new_field char(2),
  PRIMARY KEY (auto)
);

INSERT INTO t2 (string,mediumblob_col,new_field) SELECT string,mediumblob_col,new_field from t1 where auto > 10;

select * from t2;

# test enums
select distinct flags from t1;
select flags from t1 where find_in_set("two",flags)>0;
select flags from t1 where find_in_set("unknown",flags)>0;
select options,flags from t1 where options="ONE" and flags="ONE";
select options,flags from t1 where options="one" and flags="one";

drop table t2;

#
# Check CREATE ... SELECT
#

create table t2 select * from t1;
update t2 set string="changed" where auto=16;
show columns from t1;
show columns from t2;
select * from t1,t2 where t1.auto=t2.auto and ((t1.string<>t2.string and (t1.string is not null or t2.string is not null)) or (t1.tiny<>t2.tiny and (t1.tiny is not null or t2.tiny is not null)) or (t1.short<>t2.short and (t1.short is not null or t2.short is not null)) or (t1.medium<>t2.medium and (t1.medium is not null or t2.medium is not null)) or (t1.long_int<>t2.long_int and (t1.long_int is not null or t2.long_int is not null)) or (t1.longlong<>t2.longlong and (t1.longlong is not null or t2.longlong is not null)) or (t1.real_float<>t2.real_float and (t1.real_float is not null or t2.real_float is not null)) or (t1.real_double<>t2.real_double and (t1.real_double is not null or t2.real_double is not null)) or (t1.utiny<>t2.utiny and (t1.utiny is not null or t2.utiny is not null)) or (t1.ushort<>t2.ushort and (t1.ushort is not null or t2.ushort is not null)) or (t1.umedium<>t2.umedium and (t1.umedium is not null or t2.umedium is not null)) or (t1.ulong<>t2.ulong and (t1.ulong is not null or t2.ulong is not null)) or (t1.ulonglong<>t2.ulonglong and (t1.ulonglong is not null or t2.ulonglong is not null)) or (t1.time_stamp<>t2.time_stamp and (t1.time_stamp is not null or t2.time_stamp is not null)) or (t1.date_field<>t2.date_field and (t1.date_field is not null or t2.date_field is not null)) or (t1.time_field<>t2.time_field and (t1.time_field is not null or t2.time_field is not null)) or (t1.date_time<>t2.date_time and (t1.date_time is not null or t2.date_time is not null)) or (t1.new_blob_col<>t2.new_blob_col and (t1.new_blob_col is not null or t2.new_blob_col is not null)) or (t1.tinyblob_col<>t2.tinyblob_col and (t1.tinyblob_col is not null or t2.tinyblob_col is not null)) or (t1.mediumblob_col<>t2.mediumblob_col and (t1.mediumblob_col is not null or t2.mediumblob_col is not null)) or (t1.options<>t2.options and (t1.options is not null or t2.options is not null)) or (t1.flags<>t2.flags and (t1.flags is not null or t2.flags is not null)) or (t1.new_field<>t2.new_field and (t1.new_field is not null or t2.new_field is not null)));
select * from t1,t2 where t1.auto=t2.auto and not (t1.string<=>t2.string and t1.tiny<=>t2.tiny and t1.short<=>t2.short and t1.medium<=>t2.medium and t1.long_int<=>t2.long_int and t1.longlong<=>t2.longlong and t1.real_float<=>t2.real_float and t1.real_double<=>t2.real_double and t1.utiny<=>t2.utiny and t1.ushort<=>t2.ushort and t1.umedium<=>t2.umedium and t1.ulong<=>t2.ulong and t1.ulonglong<=>t2.ulonglong and t1.time_stamp<=>t2.time_stamp and t1.date_field<=>t2.date_field and t1.time_field<=>t2.time_field and t1.date_time<=>t2.date_time and t1.new_blob_col<=>t2.new_blob_col and t1.tinyblob_col<=>t2.tinyblob_col and t1.mediumblob_col<=>t2.mediumblob_col and t1.options<=>t2.options and t1.flags<=>t2.flags and t1.new_field<=>t2.new_field);

drop table t2;

create table t2 (primary key (auto)) select auto+1 as auto,1 as t1, "a" as t2, repeat("a",256) as t3, binary repeat("b",256) as t4 from t1;
show columns from t2;
select * from t2;
drop table t1,t2;

create table t1 (c int);
insert into t1 values(1),(2);
create table t2 select * from t1;
create table t3 select * from t1, t2; # Should give an error
create table t3 select t1.c AS c1, t2.c AS c2,1 as "const" from t1, t2;
show columns from t3;
drop table t1,t2,t3;

create table t1 ( myfield INT NOT NULL, UNIQUE INDEX (myfield), unique (myfield), index(myfield));
drop table t1;

create table t1 ( id integer unsigned not null primary key );
create table t2 ( id integer unsigned not null primary key );
insert into t1 values (1), (2);
insert into t2 values (1);
select  t1.id as id_A,  t2.id as id_B from t1 left join t2 using ( id ); 
create table t3 (id_A integer unsigned not null, id_B integer unsigned null  );
insert into t3 select t1.id as id_A,  t2.id as id_B from t1 left join t2 using ( id );
select * from t3;
drop table t3;
create table t3 select t1.id as id_A,  t2.id as id_B from t1 left join t2 using ( id );
select * from t3;
drop table t1,t2,t3;

#
# Check floating point handling

create table t1 (f1 float(24),f2 float(52));
show columns from t1;
insert into t1 values(10,10),(1e+5,1e+5),(1234567890,1234567890),(1e+10,1e+10),(1e+15,1e+15),(1e+20,1e+20),(1e+50,1e+50),(1e+150,1e+150);
insert into t1 values(-10,-10),(1e-5,1e-5),(1e-10,1e-10),(1e-15,1e-15),(1e-20,1e-20),(1e-50,1e-50),(1e-150,1e-150);
select * from t1;
drop table t1;

#
# FLOAT/DOUBLE/DECIMAL handling
#

create table t1 (f float, f2 float(24), f3 float(6,2), d double, d2 float(53), d3 double(10,3), de decimal, de2 decimal(6), de3 decimal(5,2), n numeric, n2 numeric(8), n3 numeric(5,6));
show columns from t1;
drop table t1;
create table t1 (f float(54));	# Should give an error
drop table if exists t1;

create table t1 (a  decimal(7,3) not null, key (a));
insert into t1 values ("0"),("-0.00"),("-0.01"),("-0.002"),("1");
select a from t1 order by a;
select min(a) from t1;
drop table t1;

#
# BIGINT handling
#

create table t1 (a bigint unsigned);
insert into t1 values (18446744073709551615), (0xFFFFFFFFFFFFFFFF);
select * from t1;
drop table t1;

#
# Check on condition on different length keys.

CREATE TABLE t1 (
  a char(5) NOT NULL,
  b char(4) NOT NULL,
  KEY (a),
  KEY (b)
);

INSERT INTO t1 VALUES ('A','B'),('b','A'),('C','c'),('D','E'),('a','a');

select * from t1,t1 as t2;
explain select t1.*,t2.* from t1,t1 as t2 where t1.A=t2.B;
select t1.*,t2.* from t1,t1 as t2 where t1.A=t2.B;
select t1.*,t2.* from t1,t1 as t2 where t1.A=t2.B order by t1.a;
select * from t1 where a='a';
drop table t1;

#
# Check null keys

create table t1 (a int, b int not null,unique key (a,b),index(b)) type=myisam;
insert ignore into t1 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(null,7),(9,9),(8,8),(7,7),(null,9),(null,9),(6,6);
explain select * from t1 where a is null;
explain select * from t1 where a is null and b = 2;
explain select * from t1 where a is null and b = 7;
explain select * from t1 where a=2 and b = 2;
explain select * from t1 where a<=>b limit 2;
explain select * from t1 where (a is null or a > 0 and a < 3) and b < 5 limit 3;
explain select * from t1 where (a is null or a = 7) and b=7;
explain select * from t1 where (a is null and b>a) or a is null and b=7 limit 2;
explain select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
explain select * from t1 where a > 1 and a < 3 limit 1;
explain select * from t1 where a > 8 and a < 9;
select * from t1 where a is null;
select * from t1 where a is null and b = 7;
select * from t1 where a<=>b limit 2;
select * from t1 where (a is null or a > 0 and a < 3) and b < 5 limit 3;
select * from t1 where (a is null or a > 0 and a < 3) and b > 7 limit 3;
select * from t1 where (a is null or a = 7) and b=7;
select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
alter table t1 modify b blob not null, add c int not null, drop key a, add unique key (a,b(20),c), drop key b, add key (b(10));
explain select * from t1 where a is null and b = 2;
explain select * from t1 where a is null and b = 2 and c=0;
explain select * from t1 where a is null and b = 7 and c=0;
explain select * from t1 where a=2 and b = 2;
explain select * from t1 where a<=>b limit 2;
explain select * from t1 where (a is null or a > 0 and a < 3) and b < 5 and c=0 limit 3;
explain select * from t1 where (a is null or a = 7) and b=7 and c=0;
explain select * from t1 where (a is null and b>a) or a is null and b=7 limit 2;
explain select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
explain select * from t1 where a > 1 and a < 3 limit 1;
explain select * from t1 where a is null and b=7 or a > 1 and a < 3 limit 1;
explain select * from t1 where a > 8 and a < 9;
explain select * from t1 where b like "6%";
select * from t1 where a is null;
select * from t1 where a is null and b = 7 and c=0;
select * from t1 where a<=>b limit 2;
select * from t1 where (a is null or a > 0 and a < 3) and b < 5 limit 3;
select * from t1 where (a is null or a > 0 and a < 3) and b > 7 limit 3;
select * from t1 where (a is null or a = 7) and b=7 and c=0;
select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
select * from t1 where b like "6%";
drop table t1;

#
# Check some special create statements.
#

create table t1 (b char(0));
insert into t1 values (""),(null);
select * from t1;
drop table if exists t1;

create table t1 (b char(0) not null);
create table if not exists t1 (b char(0) not null);
insert into t1 values (""),(null);
select * from t1;
drop table if exists t1;

#
# the following should give errors
#

create table t2 type=heap select * from t1;
create table t2 select auto+1 from t1;
drop table if exists t1,t2;
create table t1 (b char(0) not null, index(b));
create table t1 (a int not null auto_increment,primary key (a)) type=heap;
create table t1 (a int not null,b text) type=heap;
create table t1 (a int ,primary key(a)) type=heap;
create table t1 (a int,b text, index(a)) type=isam;
create table t1 (a int,b text, index(b)) type=isam;
drop table if exists t1;

#
# Test of some show commands
#
create table t1 (a int not null primary key, b int not null,c int not null, key(b,c));
insert into t1 values (1,2,2),(2,2,3),(3,2,4),(4,2,4);
create table t2 type=isam select * from t1;
optimize table t1;
check table t1,t2;
repair table t1,t2;
check table t2,t1;
lock tables t1 read;
check table t2,t1;
show keys from t1;
drop table t1,t2;

create table t1 (a int not null primary key, b int not null,c int not null, key(b,c));
insert into t1 values (1,2,2),(2,2,3),(3,2,4),(4,2,4);
check table t1 type=fast;
check table t1 type=fast;
check table t1 type=changed;
insert into t1 values (5,5,5);
check table t1 type=changed;
check table t1 type=extended;
show keys from t1;
insert into t1 values (5,5,5);
optimize table t1;
optimize table t1;
drop table t1;

#
# simple test of all group functions
#

create table t1 (grp int, a bigint unsigned, c char(10) not null);
insert into t1 values (1,1,"a");
insert into t1 values (2,2,"b");
insert into t1 values (2,3,"c");
insert into t1 values (3,4,"E");
insert into t1 values (3,5,"C");
insert into t1 values (3,6,"D");

# Test of MySQL field extension with and without matching records.
select a,c,sum(a) from t1 group by a;
select a,c,sum(a) from t1 where a > 10 group by a;
select sum(a) from t1 where a > 10;
select a from t1 order by rand(10);
select distinct a from t1 order by rand(10);
select count(distinct a),count(distinct grp) from t1;
insert into t1 values (null,null,'');
select count(distinct a),count(distinct grp) from t1;

select sum(a),count(a),avg(a),std(a),bit_or(a),bit_and(a),min(a),max(a),min(c),max(c) from t1;
select grp, sum(a),count(a),avg(a),std(a),bit_or(a),bit_and(a),min(a),max(a),min(c),max(c) from t1 group by grp;
select grp, sum(a)+count(a)+avg(a)+std(a)+bit_or(a)+bit_and(a)+min(a)+max(a)+min(c)+max(c) as sum from t1 group by grp;

create table t2 (grp int, a bigint unsigned, c char(10));
insert into t2 select grp,max(a)+max(grp),max(c) from t1 group by grp;
replace into t2 select grp, a, c from t1 limit 2,1;
select * from t2;

drop table t2;

#
# test of left outer join
#

create table t2 (id int, a bigint unsigned not null, c char(10), d int, primary key (a));
insert into t2 values (1,1,"a",1);
insert into t2 values (3,4,"A",4);
insert into t2 values (3,5,"B",5);
insert into t2 values (3,6,"C",6);
insert into t2 values (4,7,"D",7);

select t1.*,t2.* from t1 JOIN t2 where t1.a=t2.a;
select t1.*,t2.* from t1 left join t2 on (t1.a=t2.a) order by t1.grp,t1.a,t2.c;
select t1.*,t2.* from { oj t2 left outer join t1 on (t1.a=t2.a) };
select t1.*,t2.* from t1 as t0,{ oj t2 left outer join t1 on (t1.a=t2.a) } WHERE t0.a=2;
select t1.*,t2.* from t1 left join t2 using (a);
select t1.*,t2.* from t1 left join t2 using (a,c);
select t1.*,t2.* from t1 left join t2 using (c);
select t1.*,t2.* from t1 natural left outer join t2;

select t1.*,t2.* from t1 left join t2 on (t1.a=t2.a) where t2.id=3;
select t1.*,t2.* from t1 left join t2 on (t1.a=t2.a) where t2.id is null;

explain select t1.*,t2.* from t1,t2 where t1.a=t2.a and isnull(t2.a)=1;
explain select t1.*,t2.* from t1 left join t2 on t1.a=t2.a where isnull(t2.a)=1;

select t1.*,t2.*,t3.a from t1 left join t2 on (t1.a=t2.a) left join t1 as t3 on (t2.a=t3.a);

# The next query should rearange the left joins to get this to work
explain select t1.*,t2.*,t3.a from t1 left join t2 on (t3.a=t2.a) left join t1 as t3 on (t1.a=t3.a);
select t1.*,t2.*,t3.a from t1 left join t2 on (t3.a=t2.a) left join t1 as t3 on (t1.a=t3.a);

# The next query should give an error in MySQL
select t1.*,t2.*,t3.a from t1 left join t2 on (t3.a=t2.a) left join t1 as t3 on (t2.a=t3.a);

# Test of inner join
select t1.*,t2.* from t1 inner join t2 using (a);
select t1.*,t2.* from t1 inner join t2 on (t1.a=t2.a);
select t1.*,t2.* from t1 natural join t2;

drop table t1,t2;

#
# Problem with std()
#
CREATE TABLE t1 (id int(11),value1 float(10,2));
INSERT INTO t1 VALUES (1,0.00),(1,1.00), (1,2.00), (2,10.00), (2,11.00), (2,12.00); 
CREATE TABLE t2 (id int(11),name char(20)); 
INSERT INTO t2 VALUES (1,'Set One'),(2,'Set Two'); 
select id, avg(value1), std(value1) from t1 group by id;
select name, avg(value1), std(value1) from t1, t2 where t1.id = t2.id group by t1.id;
drop table t1,t2;

#
# Test of bug in left join & avg

create table t1 (id int not null);
create table t2 (id int not null,rating int null);
insert into t1 values(1),(2),(3);
insert into t2 values(1, 3),(2, NULL),(2, NULL),(3, 2),(3, NULL);
select t1.id, avg(rating) from t1 left join t2 on ( t1.id = t2.id ) group by t1.id;
drop table t1,t2;

# test of count

create table t1 (a smallint(6) primary key, c char(10), b text);
INSERT INTO t1 VALUES (1,'1','1');
INSERT INTO t1 VALUES (2,'2','2');
INSERT INTO t1 VALUES (4,'4','4');

select count(*) from t1;
select count(*) from t1 where a = 1;
select count(*) from t1 where a = 100;
select count(*) from t1 where a >= 10;
select count(a) from t1 where a = 1;
select count(a) from t1 where a = 100;
select count(a) from t1 where a >= 10;
select count(b) from t1 where b >= 2;
select count(b) from t1 where b >= 10;
select count(c) from t1 where c = 10;
drop table t1;

#
# Test of left join bug
#

CREATE TABLE t1 (
 usr_id INT unsigned NOT NULL,
 uniq_id INT unsigned NOT NULL AUTO_INCREMENT,
        start_num INT unsigned NOT NULL DEFAULT 1,
        increment INT unsigned NOT NULL DEFAULT 1,
 PRIMARY KEY (uniq_id),
 INDEX usr_uniq_idx (usr_id, uniq_id),
 INDEX uniq_usr_idx (uniq_id, usr_id)
);
CREATE TABLE t2 (
 id INT unsigned NOT NULL DEFAULT 0,
 usr2_id INT unsigned NOT NULL DEFAULT 0,
 max INT unsigned NOT NULL DEFAULT 0,
 c_amount INT unsigned NOT NULL DEFAULT 0,
 d_max INT unsigned NOT NULL DEFAULT 0,
 d_num INT unsigned NOT NULL DEFAULT 0,
 orig_time INT unsigned NOT NULL DEFAULT 0,
 c_time INT unsigned NOT NULL DEFAULT 0,
 active ENUM ("no","yes") NOT NULL,
 PRIMARY KEY (id,usr2_id),
 INDEX id_idx (id),
 INDEX usr2_idx (usr2_id)
);
INSERT INTO t1 VALUES (3,NULL,0,50),(3,NULL,0,200),(3,NULL,0,25),(3,NULL,0,84676),(3,NULL,0,235),(3,NULL,0,10),(3,NULL,0,3098),(3,NULL,0,2947),(3,NULL,0,8987),(3,NULL,0,8347654),(3,NULL,0,20398),(3,NULL,0,8976),(3,NULL,0,500),(3,NULL,0,198);

#1st select shows that one record is returned with null entries for the right
#table, when selecting on an id that does not exist in the right table t2
SELECT t1.usr_id,t1.uniq_id,t1.increment,
t2.usr2_id,t2.c_amount,t2.max
FROM t1
LEFT JOIN t2 ON t2.id = t1.uniq_id
WHERE t1.uniq_id = 4
ORDER BY t2.c_amount;

# The same with RIGHT JOIN
SELECT t1.usr_id,t1.uniq_id,t1.increment,
t2.usr2_id,t2.c_amount,t2.max
FROM t2
RIGHT JOIN t1 ON t2.id = t1.uniq_id
WHERE t1.uniq_id = 4
ORDER BY t2.c_amount;

INSERT INTO t2 VALUES (2,3,3000,6000,0,0,746584,837484,'yes');
INSERT INTO t2 VALUES (2,3,3000,6000,0,0,746584,837484,'yes');
INSERT INTO t2 VALUES (7,3,1000,2000,0,0,746294,937484,'yes');

#3rd select should show that one record is returned with null entries for the
# right table, when selecting on an id that does not exist in the right table
# t2 but this select returns an empty set!!!!
SELECT t1.usr_id,t1.uniq_id,t1.increment,t2.usr2_id,t2.c_amount,t2.max FROM t1 LEFT JOIN t2 ON t2.id = t1.uniq_id WHERE t1.uniq_id = 4 ORDER BY t2.c_amount;
SELECT t1.usr_id,t1.uniq_id,t1.increment,t2.usr2_id,t2.c_amount,t2.max FROM t1 LEFT JOIN t2 ON t2.id = t1.uniq_id WHERE t1.uniq_id = 4 GROUP BY t2.c_amount;
# Removing the ORDER BY works:
SELECT t1.usr_id,t1.uniq_id,t1.increment,t2.usr2_id,t2.c_amount,t2.max FROM t1 LEFT JOIN t2 ON t2.id = t1.uniq_id WHERE t1.uniq_id = 4;

drop table t1,t2;

#
# Test syntax of not supported functions
#

create table t1 (
	a int not null references t2,
	b int not null references t2 (c),
	primary key (a,b),
	foreign key (a) references t3 match full,
	foreign key (a) references t3 match partial,
	foreign key (a,b) references t3 (c,d) on delete no action
	  on update no action,
	foreign key (a,b) references t3 (c,d) on update cascade,
	foreign key (a,b) references t3 (c,d) on delete set default,
	foreign key (a,b) references t3 (c,d) on update set null);

create index a on t1 (a);
create unique index b on t1 (a,b);

grant all privileges on t1 to monty,david with grant option;
show grants for monty;
show grants for david;
revoke all privileges on t1 from david;
show grants for david;
drop table t1;

#
# test sort,min and max on binary fields
#

create table t1 (name char(20) not null, primary key (name));
create table t2 (name char(20) binary not null, primary key (name));
insert into t1 values ("å");
insert into t1 values ("ä");
insert into t1 values ("ö");
insert into t2 select * from t1;

select * from t1 order by name;
select concat("*",name,"*") from t1 order by 1;
select min(name),min(concat("*",name,"*")),max(name),max(concat("*",name,"*")) from t1;
select * from t2 order by name;
select concat("*",name,"*") from t2 order by 1;
select min(name),min(concat("*",name,"*")),max(name),max(concat("*",name,"*")) from t2;
select name from t1 where name between 'Ä' and 'Ö';
select name from t2 where name between 'ä' and 'ö';
select name from t2 where name between 'Ä' and 'Ö';

drop table t1,t2;

#
# test of full join with blob
#

create table t1 (nr int(5) not null auto_increment,b blob,str char(10), primary key (nr));
insert into t1 values (null,"a","A");
insert into t1 values (null,"bbb","BBB");
insert into t1 values (null,"ccc","CCC");
select last_insert_id();
select * from t1,t1 as t2;

drop table t1;

#
# Test of changing TEXT column
#
create table t1 (a text);
insert into t1 values ('where');
update t1 set a='Where'; 
select * from t1;
drop table t1;

#
# Some special cases with empty tables
#

create table t1 (nr int(5) not null auto_increment,b blob,str char(10), primary key (nr));
select count(*) from t1;
select * from t1;
select * from t1 limit 0;
drop table t1;

#
# Test keywords as fields
#
create table t1 (time time, date date, timestamp timestamp);
insert into t1 values ("12:22:22","97:02:03","1997-01-02");
select * from t1;
select t1.time+0,t1.date+0,t1.timestamp+0,concat(date," ",time) from t1;
drop table t1;

#
# test of blob, text, char and char binary
#
create table t1 (t text,c char(10),b blob, d char(10) binary);
insert into t1 values (NULL,NULL,NULL,NULL);
insert into t1 values ("","","","");
insert into t1 values ("hello","hello","hello","hello");
insert into t1 values ("HELLO","HELLO","HELLO","HELLO");
insert into t1 values ("HELLO MY","HELLO MY","HELLO MY","HELLO MY");
insert into t1 values ("a","a","a","a");
insert into t1 values (1,1,1,1);
insert into t1 values (NULL,NULL,NULL,NULL);
update t1 set c="",b=null where c="1";

lock tables t1 READ;
show fields from t1;
lock tables t1 WRITE;
show fields from t1;
unlock tables;

select t from t1 where t like "hello";
select c from t1 where c like "hello";
select b from t1 where b like "hello";
select d from t1 where d like "hello";
select c from t1 having c like "hello";
select d from t1 having d like "hello";
select t from t1 where t like "%HELLO%";
select c from t1 where c like "%HELLO%";
select b from t1 where b like "%HELLO%";
select d from t1 where d like "%HELLO%";
select c from t1 having c like "%HELLO%";
select d from t1 having d like "%HELLO%";
select t from t1 order by t;
select c from t1 order by c;
select b from t1 order by b;
select d from t1 order by d;
select distinct t from t1;
select distinct b from t1;
select distinct t from t1 order by t;
select distinct b from t1 order by b;
select t from t1 group by t;
select b from t1 group by b;
set option sql_big_tables=1;
select distinct t from t1;
select distinct b from t1;
select distinct t from t1 order by t;
select distinct b from t1 order by b;
select distinct c from t1;
select distinct d from t1;
select distinct c from t1 order by c;
select distinct d from t1 order by d;
select c from t1 group by c;
select d from t1 group by d;
set option sql_big_tables=0;
select distinct * from t1;
select t,count(*) from t1 group by t;
select b,count(*) from t1 group by b;
select c,count(*) from t1 group by c;
select d,count(*) from t1 group by d;
drop table t1;

#
# testing different DATETIME ranges
#
create table t1 (t datetime);
insert into t1 values(101),(691231),(700101),(991231),(10000101),(99991231),(101000000),(691231000000),(700101000000),(991231235959),(10000101000000),(99991231235959);
select * from t1;
delete from t1 where t > 0;
optimize table t1;
insert into t1 values("000101"),("691231"),("700101"),("991231"),("00000101"),("00010101"),("99991231"),("00101000000"),("691231000000"),("700101000000"),("991231235959"),("10000101000000"),("99991231235959");
select * from t1;
drop table t1;

#
# testing different TIME formats
#
create table t1 (t time);
insert into t1 values("10:22:33"),("12:34:56.78"),(10),(1234),(123456.78),(1234559.99),("1"),("1:23"),("1:23:45"), ("10.22"), ("-10  1:22:33.45"),("20 10:22:33"),("1999-02-03 20:33:34");
# Test wrong values
insert into t1 values("10.22.22"),(1234567),(123456789),(123456789.10),("10 22:22"),("12.45a");
select * from t1;
drop table t1;

create table t1 (date date);  
insert into t1 values ("2000-08-10"),("2000-08-11");
select date_add(date,INTERVAL 1 DAY),date_add(date,INTERVAL 1 SECOND) from t1;
drop table t1;

create table t1 (t time);
insert into t1 values ('09:00:00'),('13:00:00'),('19:38:34'), ('13:00:00'),('09:00:00'),('09:00:00'),('13:00:00'),('13:00:00'),('13:00:00'),('09:00:00');
;
select t, time_to_sec(t),sec_to_time(time_to_sec(t)) from t1;
select sec_to_time(time_to_sec(t)) from t1;
drop table t1;

#
# test of into outfile|dumpfile
#

create table t1 (`a` blob);
insert into t1 values("hello world"),("Hello mars"),(NULL);
select * into outfile "/tmp/select-test.1" from t1;
select load_file("/tmp/select-test.1");
select * into dumpfile "/tmp/select-test.2" from t1 limit 1;
select load_file("/tmp/select-test.2");
select * into dumpfile "/tmp/select-test.3" from t1 where a is null;
select load_file("/tmp/select-test.3");

# the following should give errors

select * into outfile "/tmp/select-test.1" from t1;
select * into dumpfile "/tmp/select-test.1" from t1;
select * into dumpfile "/tmp/select-test.99" from t1;
select load_file("/tmp/select-test.not-exist");
drop table t1;

#
# This failed for lia Perminov
#

create table t1 (id int primary key);
create table t2 (id int);
insert into t1 values (75);
insert into t1 values (79);
insert into t1 values (78);
insert into t1 values (77);
replace into t1 values (76);
replace into t1 values (76);
insert into t1 values (104);
insert into t1 values (103);
insert into t1 values (102);
insert into t1 values (101);
insert into t1 values (105);
insert into t1 values (106);
insert into t1 values (107);

insert into t2 values (107);
insert into t2 values (75);

select t1.id, t2.id from t1, t2 where t2.id = t1.id;
select t1.id, count(t2.id) from t1,t2 where t2.id = t1.id group by t1.id;
select t1.id, count(t2.id) from t1,t2 where t2.id = t1.id group by t2.id;

drop table t1,t2;

#
# Bug with distinct and INSERT INTO
# Bug with group by and not used fields
#

CREATE TABLE t1 (id int,facility char(20));
CREATE TABLE t2 (facility char(20));
INSERT INTO t1 VALUES (NULL,NULL);
INSERT INTO t1 VALUES (-1,'');
INSERT INTO t1 VALUES (0,'');
INSERT INTO t1 VALUES (1,'/L');
INSERT INTO t1 VALUES (2,'A01');
INSERT INTO t1 VALUES (3,'ANC');
INSERT INTO t1 VALUES (4,'F01');
INSERT INTO t1 VALUES (5,'FBX');
INSERT INTO t1 VALUES (6,'MT');
INSERT INTO t1 VALUES (7,'P');
INSERT INTO t1 VALUES (8,'RV');
INSERT INTO t1 VALUES (9,'SRV');
INSERT INTO t1 VALUES (10,'VMT');
INSERT INTO t2 SELECT DISTINCT FACILITY FROM t1;

select id from t1 group by id;
select * from t1 order by id;
select id-5,facility from t1 order by "id-5";
select max(id),concat(facility) from t1 group by id ;
select id+0 as a,max(id),concat(facility) as b from t1 group by a order by b desc;
select id >= 0 and id <= 5 as grp,count(*) from t1 group by grp;

SELECT DISTINCT FACILITY FROM t1;
SELECT FACILITY FROM t2;
SELECT count(*) from t1,t2 where t1.facility=t2.facility;
select count(facility) from t1;
select count(*) from t1;
select count(*) from t1 where facility IS NULL;
select count(*) from t1 where facility = NULL;
select count(*) from t1 where facility IS NOT NULL;
select count(*) from t1 where id IS NULL;
select count(*) from t1 where id IS NOT NULL;

drop table t1,t2;

#
# Bug with order by
#

CREATE TABLE t1 (
  id int(6) DEFAULT '0' NOT NULL,
  idservice int(5),
  clee char(20) NOT NULL,
  flag char(1),
  KEY id (id),
  PRIMARY KEY (clee)
);


INSERT INTO t1 VALUES (2,4,'6067169d','Y');
INSERT INTO t1 VALUES (2,5,'606716d1','Y');
INSERT INTO t1 VALUES (2,1,'606717c1','Y');
INSERT INTO t1 VALUES (3,1,'6067178d','Y');
INSERT INTO t1 VALUES (2,6,'60671515','Y');
INSERT INTO t1 VALUES (2,7,'60671569','Y');
INSERT INTO t1 VALUES (2,3,'dd','Y');

CREATE TABLE t2 (
  id int(6) DEFAULT '0' NOT NULL auto_increment,
  description varchar(40) NOT NULL,
  idform varchar(40),
  ordre int(6) unsigned DEFAULT '0' NOT NULL,
  image varchar(60),
  PRIMARY KEY (id),
  KEY id (id,ordre)
);

#
# Dumping data for table 't2'
#

INSERT INTO t2 VALUES (1,'Emettre un appel d''offres','en_construction.html',10,'emettre.gif');
INSERT INTO t2 VALUES (2,'Emettre des soumissions','en_construction.html',20,'emettre.gif');
INSERT INTO t2 VALUES (7,'Liste des t2','t2_liste_form.phtml',51060,'link.gif');
INSERT INTO t2 VALUES (8,'Consulter les soumissions','consulter_soumissions.phtml',200,'link.gif');
INSERT INTO t2 VALUES (9,'Ajouter un type de materiel','typeMateriel_ajoute_form.phtml',51000,'link.gif');
INSERT INTO t2 VALUES (10,'Lister/modifier un type de materiel','typeMateriel_liste_form.phtml',51010,'link.gif');
INSERT INTO t2 VALUES (3,'Créer une fiche de client','clients_ajoute_form.phtml',40000,'link.gif');
INSERT INTO t2 VALUES (4,'Modifier des clients','en_construction.html',40010,'link.gif');
INSERT INTO t2 VALUES (5,'Effacer des clients','en_construction.html',40020,'link.gif');
INSERT INTO t2 VALUES (6,'Ajouter un service','t2_ajoute_form.phtml',51050,'link.gif');


select t1.id,t1.idservice,t2.ordre,t2.description  from t1, t2 where t1.id = 2   and t1.idservice = t2.id  order by t2.ordre;
 
drop table t1,t2;

create table t1 (first char(10),last char(10));
insert into t1 values ("Michael","Widenius");
insert into t1 values ("Allan","Larsson");
insert into t1 values ("David","Axmark");
select concat(first," ",last) as name from t1 order by name;
select concat(last," ",first) as name from t1 order by name;
drop table t1;

#
# bug in distinct + order by
#

create table t1 (i int);
insert into t1 values(1),(2),(1),(2),(1),(2),(3);
select distinct i from t1;
select distinct i from t1 order by rand(5);
select distinct i from t1 order by i desc;
select distinct i from t1 order by 1-i;
select distinct i from t1 order by mod(i,2);
drop table t1;

#
# test of dummy table names
#

create table 1ea10 (1a20 int,1e int);
insert into 1ea10 values(1,1);
select 1ea10.1a20,1e+ 1e+10 from 1ea10;
drop table 1ea10;
create table t1 (t1.index int);
drop table t1;
drop database if exists test$1;
create database test$1;
create table test$1.$test1 (a$1 int, $b int, c$ int);
insert into test$1.$test1 values (1,2,3);
select a$1, $b, c$ from test$1.$test1;
create table test$1.test2$ (a int);
drop table test$1.test2$;
drop database  test$1;

#
# This failed for Elizabeth Mattijsen
#

CREATE TABLE t1 (
  ID CHAR(32) NOT NULL,
  name CHAR(32) NOT NULL,
  value CHAR(255),
  INDEX indexIDname (ID(8),name(8))
) ;

INSERT INTO t1 VALUES
('keyword','indexdir','/export/home/local/www/database/indexes/keyword');
INSERT INTO t1 VALUES ('keyword','urlprefix','text/ /text');
INSERT INTO t1 VALUES ('keyword','urlmap','/text/ /');
INSERT INTO t1 VALUES ('keyword','attr','personal employee company');
INSERT INTO t1 VALUES
('emailgids','indexdir','/export/home/local/www/database/indexes/emailgids');
INSERT INTO t1 VALUES ('emailgids','urlprefix','text/ /text');
INSERT INTO t1 VALUES ('emailgids','urlmap','/text/ /');
INSERT INTO t1 VALUES ('emailgids','attr','personal employee company');

SELECT value FROM t1 WHERE ID='emailgids' AND name='attr';

drop table t1;

#
# problem with date conversions
#

CREATE TABLE t1 (name char(6),cdate date);
INSERT INTO t1 VALUES ('name1','1998-01-01');
INSERT INTO t1 VALUES ('name2','1998-01-01');
INSERT INTO t1 VALUES ('name1','1998-01-02');
INSERT INTO t1 VALUES ('name2','1998-01-02');
CREATE TABLE t2 (cdate date, note char(6));
INSERT INTO t2 VALUES ('1998-01-01','note01');
INSERT INTO t2 VALUES ('1998-01-02','note02');
select name,t1.cdate,note from t1,t2 where t1.cdate=t2.cdate and t1.cdate='1998-01-01';
drop table t1,t2;

#
# Date and BETWEEN
#

CREATE TABLE t1 ( datum DATE );
INSERT INTO t1 VALUES ( "2000-1-1" );
INSERT INTO t1 VALUES ( "2000-1-2" );
INSERT INTO t1 VALUES ( "2000-1-3" );
INSERT INTO t1 VALUES ( "2000-1-4" );
INSERT INTO t1 VALUES ( "2000-1-5" );
SELECT * FROM t1 WHERE datum BETWEEN "2000-1-2" AND "2000-1-4";
DROP TABLE t1;

#
# test of primary key conversions
#

create table t1 (t1 char(3) primary key);
insert into t1 values("ABC");
insert into t1 values("ABA");
insert into t1 values("AB%");
select * from t1 where t1="ABC";
select * from t1 where t1="ABCD";
select * from t1 where t1 like "a_\%";
describe select * from t1 where t1="ABC";
describe select * from t1 where t1="ABCD";
drop table t1;

#
# Test of like
#

create table t1 (a varchar(10), key(a));
insert into t1 values ("a"),("abc"),("abcd"),("hello"),("test");
select * from t1 where a like "abc%"; 
select * from t1 where a like "test%"; 
select * from t1 where a like "te_t"; 
drop table t1;

#
# test of max(date) and having
#

CREATE TABLE t1 (
  user_id char(10),
  summa int(11),
  rdate date
);
INSERT INTO t1 VALUES ('aaa',100,'1998-01-01');
INSERT INTO t1 VALUES ('aaa',200,'1998-01-03');
INSERT INTO t1 VALUES ('bbb',50,'1998-01-02');
INSERT INTO t1 VALUES ('bbb',200,'1998-01-04');
select max(rdate) as s from t1 where rdate < '1998-01-03' having s> "1998-01-01";
select max(rdate) as s from t1 having s="1998-01-04";
select max(rdate+0) as s from t1 having s="19980104";
drop table t1;

#
# Test of group (Failed for Lars Hoss <lh@pbm.de>)
#

CREATE TABLE t1 (
  spID int(10) unsigned,
  userID int(10) unsigned,
  score smallint(5) unsigned,
  lsg char(40),
  date date
);

INSERT INTO t1 VALUES (1,1,1,'','0000-00-00');
INSERT INTO t1 VALUES (2,2,2,'','0000-00-00');
INSERT INTO t1 VALUES (2,1,1,'','0000-00-00');
INSERT INTO t1 VALUES (3,3,3,'','0000-00-00');

CREATE TABLE t2 (
  userID int(10) unsigned DEFAULT '0' NOT NULL auto_increment,
  niName char(15),
  passwd char(8),
  mail char(50),
  isAukt enum('N','Y') DEFAULT 'N',
  vName char(30),
  nName char(40),
  adr char(60),
  plz char(5),
  ort char(35),
  land char(20),
  PRIMARY KEY (userID)
);

INSERT INTO t2 VALUES (1,'name','pass','mail','Y','v','n','adr','1','1','1');
INSERT INTO t2 VALUES (2,'name','pass','mail','Y','v','n','adr','1','1','1');
INSERT INTO t2 VALUES (3,'name','pass','mail','Y','v','n','adr','1','1','1');

SELECT t2.userid, MIN(t1.score) FROM t1, t2 WHERE t1.userID=t2.userID GROUP BY t2.userid;
SELECT t2.userid, MIN(t1.score) FROM t1, t2 WHERE t1.userID=t2.userID AND t1.spID=2  GROUP BY t2.userid;
SELECT t2.userid, MIN(t1.score+0.0) FROM t1, t2 WHERE t1.userID=t2.userID AND t1.spID=2  GROUP BY t2.userid;

drop table test.t1,test.t2;

#
# Test of LEFT JOIN with const tables (failed for frankie@etsetb.upc.es)
#

CREATE TABLE t1 (
  cod_asig int(11) DEFAULT '0' NOT NULL,
  desc_larga_cat varchar(80) DEFAULT '' NOT NULL,
  desc_larga_cas varchar(80) DEFAULT '' NOT NULL,
  desc_corta_cat varchar(40) DEFAULT '' NOT NULL,
  desc_corta_cas varchar(40) DEFAULT '' NOT NULL,
  cred_total double(3,1) DEFAULT '0.0' NOT NULL,
  pre_requisit int(11),
  co_requisit int(11),
  preco_requisit int(11),
  PRIMARY KEY (cod_asig)
);

INSERT INTO t1 VALUES (10360,'asdfggfg','Introduccion a los  Ordenadores I','asdfggfg','Introduccio Ordinadors I',6.0,NULL,NULL,NULL);
INSERT INTO t1 VALUES (10361,'Components i Circuits Electronics I','Componentes y Circuitos Electronicos I','Components i Circuits Electronics I','Comp. i Circ. Electr. I',6.0,NULL,NULL,NULL);
INSERT INTO t1 VALUES (10362,'Laboratori d`Ordinadors','Laboratorio de Ordenadores','Laboratori d`Ordinadors','Laboratori Ordinadors',4.5,NULL,NULL,NULL);
INSERT INTO t1 VALUES (10363,'Tecniques de Comunicacio Oral i Escrita','Tecnicas de Comunicacion Oral y Escrita','Tecniques de Comunicacio Oral i Escrita','Tec. Com. Oral i Escrita',4.5,NULL,NULL,NULL);
INSERT INTO t1 VALUES (11403,'Projecte Fi de Carrera','Proyecto Fin de Carrera','Projecte Fi de Carrera','PFC',9.0,NULL,NULL,NULL);
INSERT INTO t1 VALUES (11404,'+lgebra lineal','Algebra lineal','+lgebra lineal','+lgebra lineal',15.0,NULL,NULL,NULL);
INSERT INTO t1 VALUES (11405,'+lgebra lineal','Algebra lineal','+lgebra lineal','+lgebra lineal',18.0,NULL,NULL,NULL);
INSERT INTO t1 VALUES (11406,'Calcul Infinitesimal','Cßlculo Infinitesimal','Calcul Infinitesimal','Calcul Infinitesimal',15.0,NULL,NULL,NULL);

CREATE TABLE t2 (
  idAssignatura int(11) DEFAULT '0' NOT NULL,
  Grup int(11) DEFAULT '0' NOT NULL,
  Places smallint(6) DEFAULT '0' NOT NULL,
  PlacesOcupades int(11) DEFAULT '0',
  PRIMARY KEY (idAssignatura,Grup)
);


INSERT INTO t2 VALUES (10360,12,333,0);
INSERT INTO t2 VALUES (10361,30,2,0);
INSERT INTO t2 VALUES (10361,40,3,0);
INSERT INTO t2 VALUES (10360,45,10,0);
INSERT INTO t2 VALUES (10362,10,12,0);
INSERT INTO t2 VALUES (10360,55,2,0);
INSERT INTO t2 VALUES (10360,70,0,0);
INSERT INTO t2 VALUES (10360,565656,0,0);
INSERT INTO t2 VALUES (10360,32767,7,0);
INSERT INTO t2 VALUES (10360,33,8,0);
INSERT INTO t2 VALUES (10360,7887,85,0);
INSERT INTO t2 VALUES (11405,88,8,0);
INSERT INTO t2 VALUES (10360,0,55,0);
INSERT INTO t2 VALUES (10360,99,0,0);
INSERT INTO t2 VALUES (11411,30,10,0);
INSERT INTO t2 VALUES (11404,0,0,0);
INSERT INTO t2 VALUES (10362,11,111,0);
INSERT INTO t2 VALUES (10363,33,333,0);
INSERT INTO t2 VALUES (11412,55,0,0);
INSERT INTO t2 VALUES (50003,66,6,0);
INSERT INTO t2 VALUES (11403,5,0,0);
INSERT INTO t2 VALUES (11406,11,11,0);
INSERT INTO t2 VALUES (11410,11410,131,0);
INSERT INTO t2 VALUES (11416,11416,32767,0);
INSERT INTO t2 VALUES (11409,0,0,0);

CREATE TABLE t3 (
  id int(11) DEFAULT '0' NOT NULL auto_increment,
  dni_pasaporte char(16) DEFAULT '' NOT NULL,
  idPla int(11) DEFAULT '0' NOT NULL,
  cod_asig int(11) DEFAULT '0' NOT NULL,
  any smallint(6) DEFAULT '0' NOT NULL,
  quatrimestre smallint(6) DEFAULT '0' NOT NULL,
  estat char(1) DEFAULT 'M' NOT NULL,
  PRIMARY KEY (id),
  UNIQUE dni_pasaporte (dni_pasaporte,idPla),
  UNIQUE dni_pasaporte_2 (dni_pasaporte,idPla,cod_asig,any,quatrimestre)
);

INSERT INTO t3 VALUES (1,'11111111',1,10362,98,1,'M');

CREATE TABLE t4 (
  id int(11) DEFAULT '0' NOT NULL auto_increment,
  papa int(11) DEFAULT '0' NOT NULL,
  fill int(11) DEFAULT '0' NOT NULL,
  idPla int(11) DEFAULT '0' NOT NULL,
  PRIMARY KEY (id),
  KEY papa (idPla,papa),
  UNIQUE papa_2 (idPla,papa,fill)
);

INSERT INTO t4 VALUES (1,-1,10360,1);
INSERT INTO t4 VALUES (2,-1,10361,1);
INSERT INTO t4 VALUES (3,-1,10362,1);

SELECT DISTINCT fill,desc_larga_cat,cred_total,Grup,Places,PlacesOcupades FROM t4 LEFT JOIN t3 ON t3.cod_asig=fill AND estat='S'   AND dni_pasaporte='11111111'   AND t3.idPla=1 , t2,t1 WHERE fill=t1.cod_asig   AND Places>PlacesOcupades   AND fill=idAssignatura   AND t4.idPla=1   AND papa=-1;

SELECT DISTINCT fill,t3.idPla FROM t4 LEFT JOIN t3 ON t3.cod_asig=t4.fill AND t3.estat='S' AND t3.dni_pasaporte='1234' AND t3.idPla=1 ;

INSERT INTO t3 VALUES (3,'1234',1,10360,98,1,'S');
SELECT DISTINCT fill,t3.idPla FROM t4 LEFT JOIN t3 ON t3.cod_asig=t4.fill AND t3.estat='S' AND t3.dni_pasaporte='1234' AND t3.idPla=1 ;

drop table t1,t2,t3,test.t4;

#
# Bug in GROUP BY, by Nikki Chumakov <nikki@saddam.cityline.ru>

CREATE TABLE t1 (
  PID int(10) unsigned DEFAULT '0' NOT NULL auto_increment,
  payDate date DEFAULT '0000-00-00' NOT NULL,
  recDate datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
  URID int(10) unsigned DEFAULT '0' NOT NULL,
  CRID int(10) unsigned DEFAULT '0' NOT NULL,
  amount int(10) unsigned DEFAULT '0' NOT NULL,
  operator int(10) unsigned,
  method enum('unknown','cash','dealer','check','card','lazy','delayed','test') DEFAULT 'unknown' NOT NULL,
  DIID int(10) unsigned,
  reason char(1) binary DEFAULT '' NOT NULL,
  code_id int(10) unsigned,
  qty mediumint(8) unsigned DEFAULT '0' NOT NULL,
  PRIMARY KEY (PID),
  KEY URID (URID),
  KEY reason (reason),
  KEY method (method),
  KEY payDate (payDate)
);

INSERT INTO t1 VALUES (1,'1970-01-01','1997-10-17 00:00:00',2529,1,21000,11886,'check',0,'F',16200,6);

SELECT COUNT(P.URID),SUM(P.amount),P.method, MIN(PP.recdate+0) > 19980501000000   AS IsNew FROM t1 AS P JOIN t1 as PP WHERE P.URID = PP.URID GROUP BY method,IsNew;

drop table t1;

# Another SUM() problem with 3.23.2

create table t1 (
        num float(5,2),
        user char(20)
);
insert into t1 values (10.3,'nem'),(20.53,'monty'),(30.23,'sinisa');
insert into t1 values (30.13,'nem'),(20.98,'monty'),(10.45,'sinisa');
insert into t1 values (5.2,'nem'),(8.64,'monty'),(11.12,'sinisa');
select sum(num) from t1;
select sum(num) from t1 group by user;
drop table t1;

#
# Problem with GROUP BY + ORDER BY when no match
# Tested with locking
#

CREATE TABLE t1 (
  cid mediumint(9) DEFAULT '0' NOT NULL auto_increment,
  firstname varchar(32) DEFAULT '' NOT NULL,
  surname varchar(32) DEFAULT '' NOT NULL,
  PRIMARY KEY (cid)
);
INSERT INTO t1 VALUES (1,'That','Guy');
INSERT INTO t1 VALUES (2,'Another','Gent');

CREATE TABLE t2 (
  call_id mediumint(8) DEFAULT '0' NOT NULL auto_increment,
  contact_id mediumint(8) DEFAULT '0' NOT NULL,
  PRIMARY KEY (call_id),
  KEY contact_id (contact_id)
);

lock tables t1 read,t2 write;

INSERT INTO t2 VALUES (10,2);
INSERT INTO t2 VALUES (18,2);
INSERT INTO t2 VALUES (62,2);
INSERT INTO t2 VALUES (91,2);
INSERT INTO t2 VALUES (92,2);

SELECT cid, CONCAT(firstname, ' ', surname), COUNT(call_id) FROM t1 LEFT JOIN t2 ON cid=contact_id WHERE firstname like '%foo%' GROUP BY cid;
SELECT HIGH_PRIORITY cid, CONCAT(firstname, ' ', surname), COUNT(call_id) FROM t1 LEFT JOIN t2 ON cid=contact_id WHERE firstname like '%foo%' GROUP BY cid ORDER BY surname, firstname;

drop table t1,t2;
unlock tables;

#
# Test of locking and delete of files
#

CREATE TABLE t1 (a int);
CREATE TABLE t2 (a int);
lock tables t1 write,t1 as b write, t2 write, t2 as c read;
drop table t1;
drop table t2;

CREATE TABLE t1 (a int);
CREATE TABLE t2 (a int);
lock tables t1 write,t1 as b write, t2 write, t2 as c read;
drop table t2;
drop table t1;
unlock tables;

#
# Order by on first index part
#

create table t1 (id int not null,col1 int not null,col2 int not null,index(col1));
insert into t1 values(1,2,2),(2,2,1),(3,1,2),(4,1,1),(5,1,4),(6,2,3),(7,3,1),(8,2,4);
select * from t1 order by col1,col2;
select col1 from t1 order by id;
select col1 as id from t1 order by t1.id;
select concat(col1) as id from t1 order by t1.id;
drop table t1;

#
# Problem with many key parts and many or
#

CREATE TABLE t1 (
  price int(5) DEFAULT '0' NOT NULL,
  area varchar(40) DEFAULT '' NOT NULL,
  type varchar(40) DEFAULT '' NOT NULL,
  transityes enum('Y','N') DEFAULT 'Y' NOT NULL,
  shopsyes enum('Y','N') DEFAULT 'Y' NOT NULL,
  schoolsyes enum('Y','N') DEFAULT 'Y' NOT NULL,
  petsyes enum('Y','N') DEFAULT 'Y' NOT NULL,
  KEY price (price,area,type,transityes,shopsyes,schoolsyes,petsyes)
);

INSERT INTO t1 VALUES (900,'Vancouver','Shared/Roomate','N','N','N','N');
INSERT INTO t1 VALUES (900,'Vancouver','Shared/Roomate','N','N','N','N');
INSERT INTO t1 VALUES (900,'Vancouver','Shared/Roomate','','','','');
INSERT INTO t1 VALUES (900,'Vancouver','Shared/Roomate','Y','Y','Y','Y');
INSERT INTO t1 VALUES (900,'Vancouver','Shared/Roomate','Y','Y','Y','Y');
INSERT INTO t1 VALUES (900,'Vancouver','Shared/Roomate','Y','Y','Y','Y');
INSERT INTO t1 VALUES (900,'Vancouver','Shared/Roomate','Y','Y','Y','Y');
INSERT INTO t1 VALUES (900,'Vancouver','Shared/Roomate','Y','Y','Y','Y');

 SELECT * FROM t1 WHERE area='Vancouver' and transityes='y' and schoolsyes='y' and ( ((type='1 Bedroom' or type='Studio/Bach') and (price<=500)) or ((type='2 Bedroom') and (price<=550)) or ((type='Shared/Roomate') and (price<=300)) or ((type='Room and Board') and (price<=500)) ) and price <= 400;

drop table t1;

#
# Problem with distinct without results
#
CREATE TABLE t1 (UserId int(11) DEFAULT '0' NOT NULL);
INSERT INTO t1 VALUES (20);
INSERT INTO t1 VALUES (27);

SELECT UserId FROM t1 WHERE Userid=22;
SELECT UserId FROM t1 WHERE UserId=22 group by Userid;
SELECT DISTINCT UserId FROM t1 WHERE UserId=22 group by Userid;
SELECT DISTINCT UserId FROM t1 WHERE UserId=22;
drop table t1;

#
# Check for problems with delete
#

CREATE TABLE t1 (a tinyint(3), b tinyint(5));
INSERT INTO t1 VALUES (1,1);
INSERT LOW_PRIORITY INTO t1 VALUES (1,2);
INSERT INTO t1 VALUES (1,3);
DELETE from t1 where a=1 limit 1;
DELETE LOW_PRIORITY from t1 where a=1;

INSERT INTO t1 VALUES (1,1);
DELETE from t1;
LOCK TABLE t1 write;
INSERT INTO t1 VALUES (1,2);
DELETE from t1;
UNLOCK TABLES;
INSERT INTO t1 VALUES (1,2);
SET AUTOCOMMIT=0;
DELETE from t1;
SET AUTOCOMMIT=1;
drop table t1;

create table t1 (s1 char(64),s2 char(64));

insert into t1 values('aaa','aaa');
insert into t1 values('aaa|qqq','qqq');
insert into t1 values('gheis','^[^a-dXYZ]+$');
insert into t1 values('aab','^aa?b');
insert into t1 values('Baaan','^Ba*n');
insert into t1 values('aaa','qqq|aaa');
insert into t1 values('qqq','qqq|aaa');

insert into t1 values('bbb','qqq|aaa');
insert into t1 values('bbb','qqq');
insert into t1 values('aaa','aba');

insert into t1 values(null,'abc');
insert into t1 values('def',null);
insert into t1 values(null,null);
insert into t1 values('ghi','ghi[');

select HIGH_PRIORITY s1 regexp s2 from t1;

drop table t1;

#
# Test retreiving row with last insert_id value.
#

create table t1 (a int not null auto_increment,b int not null,primary key (a,b));
insert into t1 SET A=NULL,B=1;
insert into t1 SET a=null,b=2;
select * from t1 where a is null and b=2;
select * from t1 where a is null;
explain select * from t1 where b is null;
drop table t1;

#
# Test of IS NULL on AUTO_INCREMENT with LEFT JOIN
#

CREATE TABLE t1 (
  id smallint(5) unsigned DEFAULT '0' NOT NULL auto_increment,
  name char(60) DEFAULT '' NOT NULL,
  PRIMARY KEY (id)
);
INSERT INTO t1 VALUES (1,'Antonio Paz');
INSERT INTO t1 VALUES (2,'Lilliana Angelovska');
INSERT INTO t1 VALUES (3,'Thimble Smith');

CREATE TABLE t2 (
  id smallint(5) unsigned DEFAULT '0' NOT NULL auto_increment,
  owner smallint(5) unsigned DEFAULT '0' NOT NULL,
  name char(60),
  PRIMARY KEY (id)
);
INSERT INTO t2 VALUES (1,1,'El Gato');
INSERT INTO t2 VALUES (2,1,'Perrito');
INSERT INTO t2 VALUES (3,3,'Happy');

select t1.name, t2.name, t2.id from t1 left join t2 on (t1.id = t2.owner);
select t1.name, t2.name, t2.id from t1 left join t2 on (t1.id = t2.owner) where t2.id is null;
explain select t1.name, t2.name, t2.id from t1 left join t2 on (t1.id = t2.owner) where t2.id is null;
explain select t1.name, t2.name, t2.id from t1 left join t2 on (t1.id = t2.owner) where t2.name is null;
select count(*) from t1 left join t2 on (t1.id = t2.owner);

select t1.name, t2.name, t2.id from t2 right join t1 on (t1.id = t2.owner);
select t1.name, t2.name, t2.id from t2 right join t1 on (t1.id = t2.owner) where t2.id is null;
explain select t1.name, t2.name, t2.id from t2 right join t1 on (t1.id = t2.owner) where t2.id is null;
explain select t1.name, t2.name, t2.id from t2 right join t1 on (t1.id = t2.owner) where t2.name is null;
select count(*) from t2 right join t1 on (t1.id = t2.owner);

select t1.name, t2.name, t2.id,t3.id from t2 right join t1 on (t1.id = t2.owner) left join t1 as t3 on t3.id=t2.owner;
select t1.name, t2.name, t2.id,t3.id from t1 right join t2 on (t1.id = t2.owner) right join t1 as t3 on t3.id=t2.owner;
select t1.name, t2.name, t2.id, t2.owner, t3.id from t1 left join t2 on (t1.id = t2.owner) right join t1 as t3 on t3.id=t2.owner;

drop table t1,t2;

create table t1 (id int not null, str char(10), index(str));
insert into t1 values (1, null), (2, null), (3, "foo"), (4, "bar");
select * from t1 where str is not null;
select * from t1 where str is null;
drop table t1;

#
# Test wrong LEFT JOIN query
#

CREATE TABLE t1 (
  t1_id bigint(21) DEFAULT '0' NOT NULL auto_increment,
  PRIMARY KEY (t1_id)
);
CREATE TABLE t2 (
  t2_id bigint(21) DEFAULT '0' NOT NULL auto_increment,
  PRIMARY KEY (t2_id)
);
CREATE TABLE t3 (
  t3_id bigint(21) DEFAULT '0' NOT NULL auto_increment,
  PRIMARY KEY (t3_id)
);
CREATE TABLE t4 (
  seq_0_id bigint(21) DEFAULT '0' NOT NULL,
  seq_1_id bigint(21) DEFAULT '0' NOT NULL,
  KEY seq_0_id (seq_0_id),
  KEY seq_1_id (seq_1_id)
);
CREATE TABLE t5 (
  seq_0_id bigint(21) DEFAULT '0' NOT NULL,
  seq_1_id bigint(21) DEFAULT '0' NOT NULL,
  KEY seq_1_id (seq_1_id),
  KEY seq_0_id (seq_0_id)
);

insert into t1 values (1);
insert into t2 values (1);
insert into t3 values (1);
insert into t4 values (1,1);
insert into t5 values (1,1);

explain select * from t3 left join t4 on t4.seq_1_id = t2.t2_id left join t1 on t1.t1_id = t4.seq_0_id left join t5 on t5.seq_0_id = t1.t1_id left join t2 on t2.t2_id = t5.seq_1_id where t3.t3_id = 23;

drop table t1,t2,t3,t4,t5;

# Test bug with NATURAL join:

CREATE TABLE t1 (id1 INT NOT NULL PRIMARY KEY, dat1 CHAR(1), id2 INT);   
INSERT INTO t1 VALUES (1,'a',1);
INSERT INTO t1 VALUES (2,'b',1);
INSERT INTO t1 VALUES (3,'c',2);

CREATE TABLE t2 (id2 INT NOT NULL PRIMARY KEY, dat2 CHAR(1));   
INSERT INTO t2 VALUES (1,'x');
INSERT INTO t2 VALUES (2,'y');
INSERT INTO t2 VALUES (3,'z');

SELECT t2.id2 FROM t2 LEFT OUTER JOIN t1 ON t1.id2 = t2.id2 WHERE id1 IS NULL;
SELECT t2.id2 FROM t2 NATURAL LEFT OUTER JOIN t1 WHERE id1 IS NULL;

drop table t1,t2;

#
# Test of order by on field()
#

CREATE TABLE t1 (id int auto_increment primary key,aika varchar(40),aikakentta  timestamp);
insert into t1 (aika) values ('Keskiviikko');
insert into t1 (aika) values ('Tiistai');
insert into t1 (aika) values ('Maanantai');
insert into t1 (aika) values ('Sunnuntai');

SELECT FIELD(SUBSTRING(t1.aika,1,2),'Ma','Ti','Ke','To','Pe','La','Su') AS test FROM t1 ORDER by test;
drop table t1;

#
# Test of ORDER BY on IF
#

CREATE TABLE t1
(
  a          int unsigned       NOT NULL,
  b          int unsigned       NOT NULL,
  c          int unsigned       NOT NULL,
  UNIQUE(a),
  INDEX(b),
  INDEX(c)
);

CREATE TABLE t2
(
  c          int unsigned       NOT NULL,
  i          int unsigned       NOT NULL,
  INDEX(c)
);

CREATE TABLE t3
(
  c          int unsigned       NOT NULL,
  v          varchar(64),
  INDEX(c)
);

INSERT INTO t1 VALUES (1,1,1);
INSERT INTO t1 VALUES (2,1,2);
INSERT INTO t1 VALUES (3,2,1);
INSERT INTO t1 VALUES (4,2,2);
INSERT INTO t2 VALUES (1,50);
INSERT INTO t2 VALUES (2,25);
INSERT INTO t3 VALUES (1,'123 Park Place');
INSERT INTO t3 VALUES (2,'453 Boardwalk');

SELECT    a,b,if(b = 1,i,if(b = 2,v,''))
FROM      t1
LEFT JOIN t2 USING(c)
LEFT JOIN t3 ON t3.c = t1.c;

SELECT    a,b,if(b = 1,i,if(b = 2,v,''))
FROM      t1
LEFT JOIN t2 USING(c)
LEFT JOIN t3 ON t3.c = t1.c
ORDER BY a;

drop table t1,t2,t3;

#
# test of IN (NULL)
#

CREATE TABLE t1 (field char(1));
INSERT INTO t1 VALUES ('A'),(NULL);
SELECT * from t1 WHERE field IN (NULL);
SELECT * from t1 WHERE field NOT IN (NULL);
SELECT * from t1 where field = field;
SELECT * from t1 where field <=> field;
DELETE FROM t1 WHERE field NOT IN (NULL);
SELECT * FROM t1;
drop table t1;

#
# Test insert of now() and curtime()
#

CREATE TABLE t1 (a timestamp, b date, c time, d datetime);
insert into t1 (b,c,d) values(now(),curtime(),now());
select date_format(a,"%Y-%m-%d")=b,right(a,6)=c+0,a=d+0 from t1;
drop table t1;

#
# Test of binary and normal strings
#

create table t1 (a char(10) not null, b char(10) binary not null,index (a));
insert into t1 values ("hello ","hello "),("hello2 ","hello2 ");
select * from t1 where a="hello ";
select * from t1 where b="hello ";
select * from t1 where b="hello";
drop table t1;

#
# Test some warnings
#

create table t1 (a int);
insert into t1 values (1);
insert into t1 values ("hej");
insert into t1 values ("hej"),("då");
set SQL_WARNINGS=1;
insert into t1 values ("hej");
insert into t1 values ("hej"),("då");
drop table t1;
set SQL_WARNINGS=0;

#
# Test of join with blobs and min
#

CREATE TABLE t1 (
  t1_id bigint(21) DEFAULT '0' NOT NULL auto_increment,
  _field_72 varchar(128) DEFAULT '' NOT NULL,
  _field_95 varchar(32),
  _field_115 tinyint(4) DEFAULT '0' NOT NULL,
  _field_122 tinyint(4) DEFAULT '0' NOT NULL,
  _field_126 tinyint(4),
  _field_134 tinyint(4),
  PRIMARY KEY (t1_id),
  UNIQUE _field_72 (_field_72),
  KEY _field_115 (_field_115),
  KEY _field_122 (_field_122)
);


INSERT INTO t1 VALUES (1,'admin','21232f297a57a5a743894a0e4a801fc3',0,1,NULL,NULL);
INSERT INTO t1 VALUES (2,'hroberts','7415275a8c95952901e42b13a6b78566',0,1,NULL,NULL);
INSERT INTO t1 VALUES (3,'guest','d41d8cd98f00b204e9800998ecf8427e',1,0,NULL,NULL);


CREATE TABLE t2 (
  seq_0_id bigint(21) DEFAULT '0' NOT NULL,
  seq_1_id bigint(21) DEFAULT '0' NOT NULL,
  PRIMARY KEY (seq_0_id,seq_1_id)
);


INSERT INTO t2 VALUES (1,1);
INSERT INTO t2 VALUES (2,1);
INSERT INTO t2 VALUES (2,2);

CREATE TABLE t3 (
  t3_id bigint(21) DEFAULT '0' NOT NULL auto_increment,
  _field_131 varchar(128),
  _field_133 tinyint(4) DEFAULT '0' NOT NULL,
  _field_135 datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
  _field_137 tinyint(4),
  _field_139 datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
  _field_140 blob,
  _field_142 tinyint(4) DEFAULT '0' NOT NULL,
  _field_145 tinyint(4) DEFAULT '0' NOT NULL,
  _field_148 tinyint(4) DEFAULT '0' NOT NULL,
  PRIMARY KEY (t3_id),
  KEY _field_133 (_field_133),
  KEY _field_135 (_field_135),
  KEY _field_139 (_field_139),
  KEY _field_142 (_field_142),
  KEY _field_145 (_field_145),
  KEY _field_148 (_field_148)
);


INSERT INTO t3 VALUES (1,'test job 1',0,'0000-00-00 00:00:00',0,'1999-02-25 22:43:32','test\r\njob\r\n1',0,0,0);
INSERT INTO t3 VALUES (2,'test job 2',0,'0000-00-00 00:00:00',0,'1999-02-26 21:08:04','',0,0,0);


CREATE TABLE t4 (
  seq_0_id bigint(21) DEFAULT '0' NOT NULL,
  seq_1_id bigint(21) DEFAULT '0' NOT NULL,
  PRIMARY KEY (seq_0_id,seq_1_id)
);


INSERT INTO t4 VALUES (1,1);
INSERT INTO t4 VALUES (2,1);

CREATE TABLE t5 (
  t5_id bigint(21) DEFAULT '0' NOT NULL auto_increment,
  _field_149 tinyint(4),
  _field_156 varchar(128) DEFAULT '' NOT NULL,
  _field_157 varchar(128) DEFAULT '' NOT NULL,
  _field_158 varchar(128) DEFAULT '' NOT NULL,
  _field_159 varchar(128) DEFAULT '' NOT NULL,
  _field_160 varchar(128) DEFAULT '' NOT NULL,
  _field_161 varchar(128) DEFAULT '' NOT NULL,
  PRIMARY KEY (t5_id),
  KEY _field_156 (_field_156),
  KEY _field_157 (_field_157),
  KEY _field_158 (_field_158),
  KEY _field_159 (_field_159),
  KEY _field_160 (_field_160),
  KEY _field_161 (_field_161)
);


INSERT INTO t5 VALUES (1,0,'tomato','','','','','');
INSERT INTO t5 VALUES (2,0,'cilantro','','','','','');

CREATE TABLE t6 (
  seq_0_id bigint(21) DEFAULT '0' NOT NULL,
  seq_1_id bigint(21) DEFAULT '0' NOT NULL,
  PRIMARY KEY (seq_0_id,seq_1_id)
);

INSERT INTO t6 VALUES (1,1);
INSERT INTO t6 VALUES (1,2);
INSERT INTO t6 VALUES (2,2);

CREATE TABLE t7 (
  t7_id bigint(21) DEFAULT '0' NOT NULL auto_increment,
  _field_143 tinyint(4),
  _field_165 varchar(32),
  _field_166 smallint(6) DEFAULT '0' NOT NULL,
  PRIMARY KEY (t7_id),
  KEY _field_166 (_field_166)
);


INSERT INTO t7 VALUES (1,0,'High',1);
INSERT INTO t7 VALUES (2,0,'Medium',2);
INSERT INTO t7 VALUES (3,0,'Low',3);

select t3._field_140,min(t3._field_131), min(t3._field_135), min(t3._field_139), min(t3._field_137), min(link_alias_142._field_165), min(link_alias_133._field_72), min(t3._field_145), min(link_alias_148._field_156), min(t3._field_140), t3.t3_id from t3 left join t4 on t4.seq_0_id = t3.t3_id left join t7 link_alias_142 on t4.seq_1_id = link_alias_142.t7_id left join t6 on t6.seq_0_id = t3.t3_id left join t1 link_alias_133 on t6.seq_1_id = link_alias_133.t1_id left join t2 on t2.seq_0_id = t3.t3_id left join t5 link_alias_148 on t2.seq_1_id = link_alias_148.t5_id where t3.t3_id in (1) group by t3.t3_id order by link_alias_142._field_166, _field_139, link_alias_133._field_72, _field_135, link_alias_148._field_156;

drop table t1,t2,t3,t4,t5,t6,t7;

#
# Test of timestamp and blobs

CREATE TABLE t1 (value TEXT NOT NULL, id VARCHAR(32) NOT NULL, stamp timestamp, PRIMARY KEY (id));
INSERT INTO t1 VALUES ("my value", "myKey","1999-04-02 00:00:00");
SELECT stamp FROM t1 WHERE id="myKey";
UPDATE t1 SET value="my value" WHERE id="myKey";
SELECT stamp FROM t1 WHERE id="myKey";
drop table t1;

create table t1 (a timestamp);
insert into t1 values (now());
select date_format(a,"%Y %y"),year(a),year(now()) from t1;
drop table t1;

create table t1 (ix timestamp);
insert into t1 values (19991101000000),(19990102030405),(19990630232922),(19990601000000),(19990930232922),(19990531232922),(19990501000000),(19991101000000),(19990501000000);
select * from t1; 
drop table t1;

CREATE TABLE t1 (date date, date_time datetime, time_stamp timestamp);
INSERT INTO t1 VALUES ("1998-12-31","1998-12-31 23:59:59",19981231235959);
INSERT INTO t1 VALUES ("1999-01-01","1999-01-01 00:00:00",19990101000000);
INSERT INTO t1 VALUES ("1999-09-09","1999-09-09 23:59:59",19990909235959);
INSERT INTO t1 VALUES ("2000-01-01","2000-01-01 00:00:00",20000101000000);
INSERT INTO t1 VALUES ("2000-02-28","2000-02-28 00:00:00",20000228000000);
INSERT INTO t1 VALUES ("2000-02-29","2000-02-29 00:00:00",20000229000000);
INSERT INTO t1 VALUES ("2000-03-01","2000-03-01 00:00:00",20000301000000);
INSERT INTO t1 VALUES ("2000-12-31","2000-12-31 23:59:59",20001231235959);
INSERT INTO t1 VALUES ("2001-01-01","2001-01-01 00:00:00",20010101000000);
INSERT INTO t1 VALUES ("2004-12-31","2004-12-31 23:59:59",20041231235959);
INSERT INTO t1 VALUES ("2005-01-01","2005-01-01 00:00:00",20050101000000);
INSERT INTO t1 VALUES ("2030-01-01","2030-01-01 00:00:00",20300101000000);
INSERT INTO t1 VALUES ("2050-01-01","2050-01-01 00:00:00",20500101000000);
SELECT * FROM t1;
drop table t1;

#
# test of DELAYED insert and timestamps
# (Can't be tested with purify :( )
#

#create table t1 (a char(10), tmsp timestamp);
#insert into t1 set a = 1;
#insert delayed into t1 set a = 2;
#insert into t1 set a = 3, tmsp=NULL;
#insert delayed into t1 set a = 4;
#insert delayed into t1 set a = 5, tmsp = 19711006010203;
#insert delayed into t1 (a, tmsp) values (6, 19711006010203);
#insert delayed into t1 (a, tmsp) values (7, NULL);
#insert into t1 set a = 8,tmsp=19711006010203;
#select * from t1 where tmsp=0;
#select * from t1 where tmsp=19711006010203;
#drop table t1;

#
# Test of date and not null
#

CREATE TABLE t1 (a datetime not null);
insert into t1 values (0);
select * from t1 where a is null;
drop table t1;

create table t1 (id int not null, str char(10), unique(str));
insert into t1 values (1, null),(2, null),(3, "foo"),(4, "bar");
select * from t1 where str is null;
select * from t1 where str="foo";
explain select * from t1 where str is null;
explain select * from t1 where str="foo";
explain select * from t1 ignore key (str) where str="foo";
explain select * from t1 use key (str,str) where str="foo";

#The following should give errors
explain select * from t1 use key (str,str,foo) where str="foo";
explain select * from t1 ignore key (str,str,foo) where str="foo";
drop table t1;

#
# Test of bug in COUNT(i)*(i+0)
#

CREATE TABLE t1 (d DATETIME, i INT);
INSERT INTO t1 VALUES (NOW(), 1);
SELECT COUNT(i), i, COUNT(i)*i FROM t1 GROUP BY i;
SELECT COUNT(i), (i+0), COUNT(i)*(i+0) FROM t1 GROUP BY i; 
DROP TABLE t1;

#
# Test if time type
#
create table t1 (t time);
insert t1 values (30),(1230),("1230"),("12:30"),("12:30:35"),("1 12:30:31.32");
select * from t1;
drop table t1;

#
# Problem med concat
#

create table t1 (Zeit time, Tag tinyint not null, Monat tinyint not null, Jahr smallint not null, index(Tag), index(Monat), index(Jahr) );
insert into t1 values ("09:26:00",16,9,1998);
insert into t1 values ("09:26:00",16,9,1998);
SELECT CONCAT(Jahr,'-',Monat,'-',Tag,' ',Zeit) AS Date,
   UNIX_TIMESTAMP(CONCAT(Jahr,'-',Monat,'-',Tag,' ',Zeit)) AS Unix
FROM t1;
drop table t1;

create table t1 ( domain char(50) );
insert into t1 VALUES ("hello.de" ), ("test.de" );
select domain from t1 where concat('@', trim(leading '.' from concat('.', domain))) = '@hello.de';
select domain from t1 where concat('@', trim(leading '.' from concat('.', domain))) = '@test.de';
drop table t1;

#
# Problem med range optimizer
#

CREATE TABLE t1 (
  event_date date DEFAULT '0000-00-00' NOT NULL,
  type int(11) DEFAULT '0' NOT NULL,
  event_id int(11) DEFAULT '0' NOT NULL,
  PRIMARY KEY (event_date,type,event_id)
);

INSERT INTO t1 VALUES ('1999-07-10',100100,24),('1999-07-11',100100,25),('1999-07-13',100600,0),('1999-07-13',100600,4),('1999-07-13',100600,26),('1999-07-14',100600,10),('1999-07-15',100600,16),('1999-07-15',100800,45),('1999-07-15',101000,47),('1999-07-16',100800,46),('1999-07-20',100600,5),('1999-07-20',100600,27),('1999-07-21',100600,11),('1999-07-22',100600,17),('1999-07-23',100100,39),('1999-07-24',100100,39),('1999-07-24',100500,40),('1999-07-25',100100,39),('1999-07-27',100600,1),('1999-07-27',100600,6),('1999-07-27',100600,28),('1999-07-28',100600,12),('1999-07-29',100500,41),('1999-07-29',100600,18),('1999-07-30',100500,41),('1999-07-31',100500,41),('1999-08-01',100700,34),('1999-08-03',100600,7),('1999-08-03',100600,29),('1999-08-04',100600,13),('1999-08-05',100500,42),('1999-08-05',100600,19),('1999-08-06',100500,42),('1999-08-07',100500,42),('1999-08-08',100500,42),('1999-08-10',100600,2),('1999-08-10',100600,9),('1999-08-10',100600,30),('1999-08-11',100600,14),('1999-08-12',100600,20),('1999-08-17',100500,8),('1999-08-17',100600,31),('1999-08-18',100600,15),('1999-08-19',100600,22),('1999-08-24',100600,3),('1999-08-24',100600,32),('1999-08-27',100500,43),('1999-08-31',100600,33),('1999-09-17',100100,37),('1999-09-18',100100,37),('1999-09-19',100100,37),('2000-12-18',100700,38);

select event_date,type,event_id from t1 WHERE event_date >= "1999-07-01" AND event_date < "1999-07-15" AND (type=100600 OR type=100100) ORDER BY event_date;
explain select event_date,type,event_id from t1 WHERE type = 100601 and event_date >= "1999-07-01" AND event_date < "1999-07-15" AND (type=100600 OR type=100100) ORDER BY event_date;
select event_date,type,event_id from t1 WHERE event_date >= "1999-07-01" AND event_date <= "1999-07-15" AND (type=100600 OR type=100100) or event_date >= "1999-07-01" AND event_date <= "1999-07-15" AND type=100099;
drop table t1;

CREATE TABLE t1 (
  PAPER_ID smallint(6) DEFAULT '0' NOT NULL,
  YEAR smallint(6) DEFAULT '0' NOT NULL,
  ISSUE smallint(6) DEFAULT '0' NOT NULL,
  CLOSED tinyint(4) DEFAULT '0' NOT NULL,
  ISS_DATE date DEFAULT '0000-00-00' NOT NULL,
  PRIMARY KEY (PAPER_ID,YEAR,ISSUE)
);
INSERT INTO t1 VALUES (3,1999,34,0,'1999-07-12');
INSERT INTO t1 VALUES (1,1999,111,0,'1999-03-23');
INSERT INTO t1 VALUES (1,1999,222,0,'1999-03-23');
INSERT INTO t1 VALUES (3,1999,33,0,'1999-07-12');
INSERT INTO t1 VALUES (3,1999,32,0,'1999-07-12');
INSERT INTO t1 VALUES (3,1999,31,0,'1999-07-12');
INSERT INTO t1 VALUES (3,1999,30,0,'1999-07-12');
INSERT INTO t1 VALUES (3,1999,29,0,'1999-07-12');
INSERT INTO t1 VALUES (3,1999,28,0,'1999-07-12');
INSERT INTO t1 VALUES (1,1999,40,1,'1999-05-01');
INSERT INTO t1 VALUES (1,1999,41,1,'1999-05-01');
INSERT INTO t1 VALUES (1,1999,42,1,'1999-05-01');
INSERT INTO t1 VALUES (1,1999,46,1,'1999-05-01');
INSERT INTO t1 VALUES (1,1999,47,1,'1999-05-01');
INSERT INTO t1 VALUES (1,1999,48,1,'1999-05-01');
INSERT INTO t1 VALUES (1,1999,49,1,'1999-05-01');
INSERT INTO t1 VALUES (1,1999,50,0,'1999-05-01');
INSERT INTO t1 VALUES (1,1999,51,0,'1999-05-01');
INSERT INTO t1 VALUES (1,1999,200,0,'1999-06-28');
INSERT INTO t1 VALUES (1,1999,52,0,'1999-06-28');
INSERT INTO t1 VALUES (1,1999,53,0,'1999-06-28');
INSERT INTO t1 VALUES (1,1999,54,0,'1999-06-28');
INSERT INTO t1 VALUES (1,1999,55,0,'1999-06-28');
INSERT INTO t1 VALUES (1,1999,56,0,'1999-07-01');
INSERT INTO t1 VALUES (1,1999,57,0,'1999-07-01');
INSERT INTO t1 VALUES (1,1999,58,0,'1999-07-01');
INSERT INTO t1 VALUES (1,1999,59,0,'1999-07-01');
INSERT INTO t1 VALUES (1,1999,60,0,'1999-07-01');
INSERT INTO t1 VALUES (3,1999,35,0,'1999-07-12');
select YEAR,ISSUE from t1 where PAPER_ID=3 and (YEAR>1999 or (YEAR=1999 and ISSUE>28))  order by YEAR,ISSUE;
check table t1;
repair table t1;
drop table t1;

CREATE TABLE t1 (
  id int(11) NOT NULL auto_increment,
  parent_id int(11) DEFAULT '0' NOT NULL,
  level tinyint(4) DEFAULT '0' NOT NULL,
  PRIMARY KEY (id),
  KEY parent_id (parent_id),
  KEY level (level)
);
INSERT INTO t1 VALUES (1,0,0),(3,1,1),(4,1,1),(8,2,2),(9,2,2),(17,3,2),(22,4,2),(24,4,2),(28,5,2),(29,5,2),(30,5,2),(31,6,2),(32,6,2),(33,6,2),(203,7,2),(202,7,2),(20,3,2),(157,0,0),(193,5,2),(40,7,2),(2,1,1),(15,2,2),(6,1,1),(34,6,2),(35,6,2),(16,3,2),(7,1,1),(36,7,2),(18,3,2),(26,5,2),(27,5,2),(183,4,2),(38,7,2),(25,5,2),(37,7,2),(21,4,2),(19,3,2),(5,1,1),(179,5,2);
SELECT * FROM t1 WHERE level = 1 AND parent_id = 1;
# The following select returned 0 rows in 3.23.8
SELECT * FROM t1 WHERE level = 1 AND parent_id = 1 order by id;
drop table t1;

#
# Testing of bug in range optimizer with many key parts and > and <
#

create table t1(
		Satellite		varchar(25)	not null,
		SensorMode		varchar(25)	not null,
		FullImageCornersUpperLeftLongitude	double	not null,
		FullImageCornersUpperRightLongitude	double	not null,
		FullImageCornersUpperRightLatitude	double	not null,
		FullImageCornersLowerRightLatitude	double	not null,
	        index two (Satellite, SensorMode, FullImageCornersUpperLeftLongitude, FullImageCornersUpperRightLongitude, FullImageCornersUpperRightLatitude, FullImageCornersLowerRightLatitude));

insert into t1 values("OV-3","PAN1",91,-92,40,50);
insert into t1 values("OV-4","PAN1",91,-92,40,50);

select * from t1 where t1.Satellite = "OV-3" and t1.SensorMode = "PAN1" and t1.FullImageCornersUpperLeftLongitude > -90.000000 and t1.FullImageCornersUpperRightLongitude < -82.000000;
drop table t1;

create table t1 ( aString char(100) not null default "", key aString (aString(10)) );
insert t1 (aString) values ( "believe in myself" ), ( "believe" ), ("baaa" ), ( "believe in love");
select * from t1 where aString < "believe in myself" order by aString;
select * from t1 where aString > "believe in love" order by aString;
alter table t1 drop key aString;
select * from t1 where aString < "believe in myself" order by aString;
select * from t1 where aString > "believe in love" order by aString;
drop table t1;

#
# problem med primary key
#

CREATE TABLE t1 (program enum('signup','unique','sliding') not null,  type enum('basic','sliding','signup'),  sites set('mt'),  PRIMARY KEY (program));
# The following should give an error for wrong primary key
ALTER TABLE t1 modify program enum('signup','unique','sliding');
drop table t1;

#
# Test year
#

create table t1 (y year,y2 year(2));
insert into t1 values (0,0),(1999,1999),(2000,2000),(2001,2001),(70,70),(69,69);
select * from t1;
select * from t1 order by y;
select * from t1 order by y2;
drop table t1;

#
# Test of distinct
#

CREATE TABLE t1 (a int(10) unsigned not null primary key,b int(10) unsigned);
INSERT INTO t1 VALUES (1,1),(2,1);
CREATE TABLE t2 (a int(10) unsigned not null, key (A));
INSERT INTO t2 VALUES (1),(2);
CREATE TABLE t3 (a int(10) unsigned, key(A), b text);
INSERT INTO t3 VALUES (1,'1'),(2,'2');
SELECT DISTINCT t3.b FROM t3,t2,t1 WHERE t3.a=t1.b AND t1.a=t2.a;
INSERT INTO t2 values (1),(2),(3);
INSERT INTO t3 VALUES (1,'1'),(2,'2'),(1,'1'),(2,'2');
explain SELECT distinct t3.a FROM t3,t2,t1 WHERE t3.a=t1.b AND t1.a=t2.a;
SELECT distinct t3.a FROM t3,t2,t1 WHERE t3.a=t1.b AND t1.a=t2.a;

# Create a lot of data into t3;
create temporary table t4 select * from t3;
insert into t3 select * from t4;
insert into t4 select * from t3;
insert into t3 select * from t4;
insert into t4 select * from t3;
insert into t3 select * from t4;
insert into t4 select * from t3;
insert into t3 select * from t4;

explain select distinct t1.a from t1,t3 where t1.a=t3.a;
flush status;
select distinct t1.a from t1,t3 where t1.a=t3.a;
show status like 'Handler%';
flush status;
select distinct 1 from t1,t3 where t1.a=t3.a;
show status like 'Handler%';
drop table t1,t2,t3,t4;

#
# test of unsigned int
#

create table t1 (this int unsigned);
insert into t1 values (1);
select * from t1;
drop table t1;

#
# Test of reverse with empty blob
#

create table t1 (a blob);
insert into t1 values ("empty"),("");
select a,reverse(a) from t1;
drop table t1;

#
# Test of heap tables.
#

create table t1 (a int not null,b int not null, primary key (a)) type=heap comment="testing heaps" avg_row_length=100 min_rows=1 max_rows=100;
insert into t1 values(1,1),(2,2),(3,3),(4,4);
delete from t1 where a=1 or a=0;
show table status like "t1";
show keys from t1;
select * from t1;
select * from t1 where a=4;
update t1 set b=5 where a=4;
update t1 set b=b+1 where a>=3;
replace t1 values (3,3);
select * from t1;
alter table t1 add c int not null, add key (c,a);
drop table t1;

create table t1 (a int not null,b int not null, primary key (a)) type=heap comment="testing heaps";
insert into t1 values(1,1),(2,2),(3,3),(4,4);
alter table t1 modify a int not null auto_increment, type=myisam, comment="new myisam table";
show table status like "t1";
select * from t1;
drop table t1;

create table t1 (a int not null) type=heap;
insert into t1 values (869751),(736494),(226312),(802616);
select * from t1 where a > 736494;
alter table t1 add unique uniq_id(a);
select * from t1 where a > 736494;
select * from t1 where a = 736494;
select * from t1 where a=869751 or a=736494;
select * from t1 where a in (869751,736494,226312,802616);
alter table t1 type=myisam;
explain select * from t1 where a in (869751,736494,226312,802616);
drop table t1;

create table t1 (x int not null, y int not null, key x(x), unique y(y))
type=heap;
insert into t1 values (1,1),(2,2),(1,3),(2,4),(2,5),(2,6);
select * from t1 where x=1;
select * from t1,t1 as t2 where t1.x=t2.y;
explain select * from t1,t1 as t2 where t1.x=t2.y;
drop table t1;

create table t1 (a int) type=heap;
insert into t1 values(1);
select max(a) from t1;
drop table t1;

CREATE TABLE t1 ( a int not null default 0, b int not null default 0,  key(a),  key(b)  ) TYPE=HEAP;
insert into t1 values(1,1),(1,2),(2,3),(1,3),(1,4),(1,5),(1,6);
select * from t1 where a=1; 
insert into t1 values(1,1),(1,2),(2,3),(1,3),(1,4),(1,5),(1,6);
select * from t1 where a=1;
drop table t1;

create table t1 (id int unsigned not null, primary key (id)) type=HEAP;
insert into t1 values(1);
select max(id) from t1; 
insert into t1 values(2);
select max(id) from t1; 
replace into t1 values(1);
drop table t1;

create table t1 (n int) type=heap;
drop table t1;

create table t1 (n int) type=heap;
drop table if exists t1;

# Test of non unique index

CREATE table t1(f1 int not null,f2 char(20) not 
null,index(f2)) type=heap;
INSERT into t1 set f1=12,f2="bill";
INSERT into t1 set f1=13,f2="bill";
INSERT into t1 set f1=14,f2="bill";
INSERT into t1 set f1=15,f2="bill";
INSERT into t1 set f1=16,f2="ted";
INSERT into t1 set f1=12,f2="ted";
INSERT into t1 set f1=12,f2="ted";
INSERT into t1 set f1=12,f2="ted";
INSERT into t1 set f1=12,f2="ted";
delete from t1 where f2="bill";
select * from t1;
drop table t1;
#
# Test of auto_increment.
#

create table t1 (a int not null auto_increment,b int, primary key (a)) type=myisam auto_increment=3;
insert into t1 values (1,1),(NULL,3),(NULL,4);
delete from t1 where a=4;
insert into t1 values (NULL,5),(NULL,6);
select * from t1;
delete from t1 where a=6;
show table status like "t1";
replace t1 values (3,1);
ALTER TABLE t1 add c int;
replace t1 values (3,3,3);
insert into t1 values (NULL,7,7);
update t1 set a=8,b=b+1,c=c+1 where a=7;
insert into t1 values (NULL,9,9);
select * from t1;
drop table t1;

create table t1 (a int not null auto_increment,b int, primary key (a)) type=isam;
insert into t1 values (1,1),(NULL,2),(3,3),(NULL,4);
delete from t1 where a=4 or a=2;
insert into t1 values (NULL,4),(NULL,5),(6,6);
select * from t1;
delete from t1 where a=6;
show table status like "t1";
replace t1 values (3,1);
replace t1 values (3,3);
ALTER TABLE t1 add c int;
insert into t1 values (NULL,6,6);
select * from t1;
drop table t1;

create table t1 (
  skey tinyint unsigned NOT NULL auto_increment PRIMARY KEY,
  sval char(20)
);
insert into t1 values (NULL, "hello");
insert into t1 values (NULL, "hey");
select * from t1;
select _rowid,t1._rowid,skey,sval from t1;
drop table t1;

#
# Test auto_increment on sub key
#
create table t1 (a char(10) not null, b int not null auto_increment, primary key(a,b));
insert into t1 values ("a",1),("b",2),("a",2),("c",1);
insert into t1 values ("a",NULL),("b",NULL),("c",NULL),("d",NULL);
insert into t1 (a) values ("a"),("b"),("c"),("d");
insert into t1 (a) values ("a");
insert into t1 values ("d",last_insert_id());
select * from t1;
drop table t1;

create table t1 (ordid int(8) not null auto_increment, ord  varchar(50) not null, primary key (ordid), index(ord,ordid)); 
insert into t1 (ordid,ord) values (NULL,'sdj'),(NULL,'sdj');
select * from t1;
drop table t1;

create table t1 (ordid int(8) not null auto_increment, ord  varchar(50) not null, primary key (ord,ordid));
insert into t1 values (NULL,'sdj'),(NULL,'sdj'),(NULL,"abc"),(NULL,'abc'),(NULL,'zzz'),(NULL,'sdj'),(NULL,'abc');
select * from t1;
drop table t1;

#
# Test of some CREATE TABLE'S that should fail
#

create table t1 (ordid int(8) not null auto_increment, ord  varchar(50) not null, primary key (ord,ordid)) type=isam;
create table t1 (ordid int(8) not null auto_increment, ord  varchar(50) not null, primary key (ord,ordid)) type=heap;
create table t1 (ordid int(8), primary key (ordid));
create table t1 (ordid int(8), unique (ordid)) type=isam;

#
# Test of temporary tables
#
CREATE TABLE t1 (c int not null, d char (10) not null);
insert into t1 values(1,""),(2,"a"),(3,"b");
CREATE TEMPORARY TABLE t1 (a int not null, b char (10) not null);
insert into t1 values(4,"e"),(5,"f"),(6,"g");
alter table t1 rename t2;
select * from t1;
select * from t2;
CREATE TABLE t2 (x int not null, y int not null);
alter table t2 rename t1;
select * from t1;
create TEMPORARY TABLE t2 type=heap select * from t1;

# This should give errors
CREATE TEMPORARY TABLE t1 (a int not null, b char (10) not null);
ALTER TABLE t1 RENAME t2;

select * from t2;
alter table t2 add primary key (a,b);
drop table t1,t2;
select * from t1;
drop table t2;
create temporary table t1 select *,2 as "e" from t1;
select * from t1;
drop table t1;
drop table t1;

#
# test of updating of keys
#

create table t1 (a int auto_increment , primary key (a));
insert into t1 values (NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL),(NULL); 
update t1 set a=a+10 where a > 34;
update t1 set a=a+100 where a > 0;
drop table t1;

CREATE TABLE t1
 (
 place_id int (10) unsigned NOT NULL,
 shows int(10) unsigned DEFAULT '0' NOT NULL,
 ishows int(10) unsigned DEFAULT '0' NOT NULL,
 ushows int(10) unsigned DEFAULT '0' NOT NULL,
 clicks int(10) unsigned DEFAULT '0' NOT NULL,
 iclicks int(10) unsigned DEFAULT '0' NOT NULL,
 uclicks int(10) unsigned DEFAULT '0' NOT NULL,
 ts timestamp(14),
 PRIMARY KEY (place_id,ts)
 );

INSERT INTO t1 (place_id,shows,ishows,ushows,clicks,iclicks,uclicks,ts)
VALUES (1,0,0,0,0,0,0,20000928174434);
UPDATE t1 SET shows=shows+1,ishows=ishows+1,ushows=ushows+1,clicks=clicks+1,iclicks=iclicks+1,uclicks=uclicks+1 WHERE place_id=1 AND ts>="2000-09-28 00:00:00";
select place_id,shows from t1;
drop table t1;

#
# Test of refering to old values
#
create table t1 (a int not null);
insert into t1 values (1);
insert into t1 values (a+2);
insert into t1 values (a+3);
insert into t1 values (4),(a+5);
select * from t1;
drop table t1;

#
# Test of LEFT JOIN + GROUP FUNCTIONS within functions:
#

CREATE TABLE t1 (
  pcode varchar(8) DEFAULT '' NOT NULL
);
INSERT INTO t1 VALUES ('kvw2000'),('kvw2001'),('kvw3000'),('kvw3001'),('kvw3002'),('kvw3500'),('kvw3501'),('kvw3502'),('kvw3800'),('kvw3801'),('kvw3802'),('kvw3900'),('kvw3901'),('kvw3902'),('kvw4000'),('kvw4001'),('kvw4002'),('kvw4200'),('kvw4500'),('kvw5000'),('kvw5001'),('kvw5500'),('kvw5510'),('kvw5600'),('kvw5601'),('kvw6000'),('klw1000'),('klw1020'),('klw1500'),('klw2000'),('klw2001'),('klw2002'),('kld2000'),('klw2500'),('kmw1000'),('kmw1500'),('kmw2000'),('kmw2001'),('kmw2100'),('kmw3000'),('kmw3200');
CREATE TABLE t2 (
  pcode varchar(8) DEFAULT '' NOT NULL,
  KEY pcode (pcode)
);
INSERT INTO t2 VALUES ('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw2000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3000'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw3500'),('kvw6000'),('kvw6000'),('kld2000');

SELECT t1.pcode, IF(ISNULL(t2.pcode), 0, COUNT(*)) AS count FROM t1
LEFT JOIN t2 ON t1.pcode = t2.pcode GROUP BY t1.pcode;
SELECT SQL_BIG_RESULT t1.pcode, IF(ISNULL(t2.pcode), 0, COUNT(*)) AS count FROM t1 LEFT JOIN t2 ON t1.pcode = t2.pcode GROUP BY t1.pcode;
drop table t1,t2;

#
# Another left join problem
#

CREATE TABLE t1 (
  id int(11),
  pid int(11),
  rep_del tinyint(4),
  KEY id (id),
  KEY pid (pid)
);
INSERT INTO t1 VALUES (1,NULL,NULL);
INSERT INTO t1 VALUES (2,1,NULL);
select * from t1 LEFT JOIN t1 t2 ON (t1.id=t2.pid) AND t2.rep_del IS NULL;
create index rep_del ON t1(rep_del);
select * from t1 LEFT JOIN t1 t2 ON (t1.id=t2.pid) AND t2.rep_del IS NULL;
drop table t1;

CREATE TABLE t1 (
  id int(11) DEFAULT '0' NOT NULL,
  name tinytext DEFAULT '' NOT NULL,
  UNIQUE id (id)
);
INSERT INTO t1 VALUES (1,'yes'),(2,'no');
CREATE TABLE t2 (
  id int(11) DEFAULT '0' NOT NULL,
  idx int(11) DEFAULT '0' NOT NULL,
  UNIQUE id (id,idx)
);
INSERT INTO t2 VALUES (1,1);
explain SELECT * from t1 left join t2 on t1.id=t2.id where t2.id IS NULL;
SELECT * from t1 left join t2 on t1.id=t2.id where t2.id IS NULL;
drop table t1,t2;

#
# Test of update and delete with limit
#
create table t1 (a int primary key, b int not null);
insert into t1 () values ();		-- Testing default values
insert into t1 values (1,1),(2,1),(3,1);
update t1 set a=4 where b=1 limit 1;
select * from t1;
update t1 set b=2 where b=1 limit 2;
select * from t1;
update t1 set b=4 where b=1;
select * from t1;
delete from t1 where b=2 limit 1;
select * from t1;
delete from t1 limit 1;
select * from t1;
drop table t1;

#
# Test of BLOB:s with NULL keys.
#

create table t1 (a blob, key (a(10)));
insert into t1 values ("bye"),("hello"),("hello"),("hello word");
select * from t1 where a like "hello%";
drop table t1;

#
# Test of alter table
#

create table t1 (
col1 int not null auto_increment primary key,
col2 varchar(30) not null,
col3 varchar (20) not null,
col4 varchar(4) not null,
col5 enum('PENDING', 'ACTIVE', 'DISABLED') not null,
col6 int not null);
alter table t1
add column col4_5 varchar(20) not null after col4,
add column col7 varchar(30) not null after col6,
add column col8 datetime not null;
drop table t1;

# Check that pack_keys and dynamic length rows are not forced. 

CREATE TABLE t1 (
GROUP_ID int(10) unsigned DEFAULT '0' NOT NULL,
LANG_ID smallint(5) unsigned DEFAULT '0' NOT NULL,
NAME varchar(80) DEFAULT '' NOT NULL,
PRIMARY KEY (GROUP_ID,LANG_ID),
KEY NAME (NAME));
show table status like "t1";
ALTER TABLE t1 CHANGE NAME NAME CHAR(80) not null;
SHOW COLUMNS FROM t1;
DROP TABLE t1;

#
# Problem with INSERT ... SELECT
#

create table t1 (bandID MEDIUMINT UNSIGNED NOT NULL PRIMARY KEY, payoutID SMALLINT UNSIGNED NOT NULL);
insert into t1 (bandID,payoutID) VALUES (1,6),(2,6),(3,4),(4,9),(5,10),(6,1),(7,12),(8,12);
create table t2 (payoutID SMALLINT UNSIGNED NOT NULL PRIMARY KEY);
insert into t2 (payoutID) SELECT DISTINCT payoutID FROM t1;
insert into t2 (payoutID) SELECT payoutID+10 FROM t1;
select * from t2;
drop table t1,t2;

#
# problem with join
#

CREATE TABLE t1 (
  id int(11) NOT NULL auto_increment,
  token varchar(100) DEFAULT '' NOT NULL,
  count int(11) DEFAULT '0' NOT NULL,
  qty int(11),
  phone char(1) DEFAULT '' NOT NULL,
  timestamp datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
  PRIMARY KEY (id),
  KEY token (token(15)),
  KEY timestamp (timestamp),
  UNIQUE token_2 (token(75),count,phone)
);

INSERT INTO t1 VALUES (21,'e45703b64de71482360de8fec94c3ade',3,7800,'n','1999-12-23 17:22:21');
INSERT INTO t1 VALUES (22,'e45703b64de71482360de8fec94c3ade',4,5000,'y','1999-12-23 17:22:21');
INSERT INTO t1 VALUES (18,'346d1cb63c89285b2351f0ca4de40eda',3,13200,'b','1999-12-23 11:58:04');
INSERT INTO t1 VALUES (17,'ca6ddeb689e1b48a04146b1b5b6f936a',4,15000,'b','1999-12-23 11:36:53');
INSERT INTO t1 VALUES (16,'ca6ddeb689e1b48a04146b1b5b6f936a',3,13200,'b','1999-12-23 11:36:53');
INSERT INTO t1 VALUES (26,'a71250b7ed780f6ef3185bfffe027983',5,1500,'b','1999-12-27 09:44:24');
INSERT INTO t1 VALUES (24,'4d75906f3c37ecff478a1eb56637aa09',3,5400,'y','1999-12-23 17:29:12');
INSERT INTO t1 VALUES (25,'4d75906f3c37ecff478a1eb56637aa09',4,6500,'y','1999-12-23 17:29:12');
INSERT INTO t1 VALUES (27,'a71250b7ed780f6ef3185bfffe027983',3,6200,'b','1999-12-27 09:44:24');
INSERT INTO t1 VALUES (28,'a71250b7ed780f6ef3185bfffe027983',3,5400,'y','1999-12-27 09:44:36');
INSERT INTO t1 VALUES (29,'a71250b7ed780f6ef3185bfffe027983',4,17700,'b','1999-12-27 09:45:05');

CREATE TABLE t2 (
  id int(11) NOT NULL auto_increment,
  category int(11) DEFAULT '0' NOT NULL,
  county int(11) DEFAULT '0' NOT NULL,
  state int(11) DEFAULT '0' NOT NULL,
  phones int(11) DEFAULT '0' NOT NULL,
  nophones int(11) DEFAULT '0' NOT NULL,
  PRIMARY KEY (id),
  KEY category (category,county,state)
);
INSERT INTO t2 VALUES (3,2,11,12,5400,7800);
INSERT INTO t2 VALUES (4,2,25,12,6500,11200);
INSERT INTO t2 VALUES (5,1,37,6,10000,12000);

select a.id, b.category as catid, b.state as stateid, b.county as
countyid from t1 a, t2 b where (a.token =
'a71250b7ed780f6ef3185bfffe027983') and (a.count = b.id);
select a.id, b.category as catid, b.state as stateid, b.county as
countyid from t1 a, t2 b where (a.token =
'a71250b7ed780f6ef3185bfffe027983') and (a.count = b.id) order by a.id;

drop table t1, t2;

#
# Test of join of many tables.

create table t1 (a int primary key);
insert into t1 values(1),(2);
select t1.a from t1 as t1 left join t1 as t2 using (a) left join t1 as t3 using (a) left join t1 as t4 using (a) left join t1 as t5 using (a) left join t1 as t6 using (a) left join t1 as t7 using (a) left join t1 as t8 using (a) left join t1 as t9 using (a) left join t1 as t10 using (a) left join t1 as t11 using (a) left join t1 as t12 using (a) left join t1 as t13 using (a) left join t1 as t14 using (a) left join t1 as t15 using (a) left join t1 as t16 using (a) left join t1 as t17 using (a) left join t1 as t18 using (a) left join t1 as t19 using (a) left join t1 as t20 using (a) left join t1 as t21 using (a) left join t1 as t22 using (a) left join t1 as t23 using (a) left join t1 as t24 using (a) left join t1 as t25 using (a) left join t1 as t26 using (a) left join t1 as t27 using (a) left join t1 as t28 using (a) left join t1 as t29 using (a) left join t1 as t30 using (a) left join t1 as t31 using (a) left join t1 as t32 using (a) left join t1 as t33 using (a) left join t1 as t34 using (a) left join t1 as t35 using (a) left join t1 as t36 using (a) left join t1 as t37 using (a) left join t1 as t38 using (a) left join t1 as t39 using (a) left join t1 as t40 using (a) left join t1 as t41 using (a) left join t1 as t42 using (a) left join t1 as t43 using (a) left join t1 as t44 using (a) left join t1 as t45 using (a) left join t1 as t46 using (a) left join t1 as t47 using (a) left join t1 as t48 using (a) left join t1 as t49 using (a) left join t1 as t50 using (a) left join t1 as t51 using (a) left join t1 as t52 using (a) left join t1 as t53 using (a) left join t1 as t54 using (a) left join t1 as t55 using (a) left join t1 as t56 using (a) left join t1 as t57 using (a) left join t1 as t58 using (a) left join t1 as t59 using (a) left join t1 as t60 using (a);
drop table t1;

#
# test of safe selects
#
SET SQL_SAFE_UPDATES=1,SQL_SELECT_LIMIT=4, SQL_MAX_JOIN_SIZE=9;
create table t1 (a int primary key, b char(20));
insert into t1 values(1,"test");
SELECT SQL_BUFFER_RESULT * from t1;
update t1 set b="a" where a=1;
delete from t1 where a=1;
insert into t1 values(1,"test"),(2,"test2");
SELECT SQL_BUFFER_RESULT * from t1;
update t1 set b="a" where a=1;
select 1 from t1,t1 as t2,t1 as t3,t1 as t4;

# The following should give errors:
update t1 set b="a";
update t1 set b="a" where b="test";
delete from t1;
delete from t1 where b="test";
delete from t1 where a+0=1;
select 1 from t1,t1 as t2,t1 as t3,t1 as t4,t1 as t5;

# The following should be ok:
update t1 set b="a" limit 1;
update t1 set b="a" where b="b" limit 2; 
delete from t1 where b="test" limit 1;
delete from t1 where a+0=1 limit 2;
drop table t1;

SET SQL_SAFE_UPDATES=0,SQL_SELECT_LIMIT=DEFAULT, SQL_MAX_JOIN_SIZE=DEFAULT;

# Test of raided tables

DROP TABLE IF EXISTS raidtest;
DROP TABLE IF EXISTS raidnew;
CREATE TABLE raidtest (
id int unsigned not null auto_increment primary key,
c char(255) not null
) RAID_TYPE=STRIPED RAID_CHUNKS=2 RAID_CHUNKSIZE=123;
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
select count(*) from raidtest;
ALTER TABLE raidtest ADD COLUMN x INT UNSIGNED NOT NULL;
ALTER TABLE raidtest ADD KEY c (c);
ALTER TABLE raidtest DROP KEY c;
ALTER TABLE raidtest DROP COLUMN x;
ALTER TABLE raidtest RENAME raidnew;
select count(*) from raidnew;
DROP TABLE raidnew;
/* variable rows */
DROP TABLE IF EXISTS raidnew;
CREATE TABLE raidtest (
id int unsigned not null auto_increment primary key,
c varchar(255) not null
) RAID_TYPE=STRIPED RAID_CHUNKS=5 RAID_CHUNKSIZE=121;
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
INSERT INTO raidtest VALUES 
(NULL,'1'),(NULL,'a'),(NULL,'a'),(NULL,'a'),(NULL,'p'),(NULL,'a'),
(NULL,'2'),(NULL,'b'),(NULL,'a'),(NULL,'a'),(NULL,'q'),(NULL,'a'),
(NULL,'3'),(NULL,'c'),(NULL,'a'),(NULL,'a'),(NULL,'r'),(NULL,'a'),
(NULL,'4'),(NULL,'d'),(NULL,'a'),(NULL,'a'),(NULL,'s'),(NULL,'a'),
(NULL,'5'),(NULL,'e'),(NULL,'a'),(NULL,'a'),(NULL,'t'),(NULL,'a'),
(NULL,'6'),(NULL,'f'),(NULL,'a'),(NULL,'a'),(NULL,'u'),(NULL,'a'),
(NULL,'7'),(NULL,'g'),(NULL,'a'),(NULL,'a'),(NULL,'v'),(NULL,'a'),
(NULL,'8'),(NULL,'h'),(NULL,'a'),(NULL,'a'),(NULL,'w'),(NULL,'a'),
(NULL,'9'),(NULL,'i'),(NULL,'a'),(NULL,'a'),(NULL,'x'),(NULL,'a'),
(NULL,'0'),(NULL,'j'),(NULL,'a'),(NULL,'a'),(NULL,'y'),(NULL,'a'),
(NULL,'1'),(NULL,'k'),(NULL,'a'),(NULL,'a'),(NULL,'z'),(NULL,'a'),
(NULL,'2'),(NULL,'l'),(NULL,'a'),(NULL,'a'),(NULL,'/'),(NULL,'a'),
(NULL,'3'),(NULL,'m'),(NULL,'a'),(NULL,'a'),(NULL,'*'),(NULL,'a'),
(NULL,'4'),(NULL,'n'),(NULL,'a'),(NULL,'a'),(NULL,'+'),(NULL,'a'),
(NULL,'5'),(NULL,'o'),(NULL,'a'),(NULL,'a'),(NULL,'?'),(NULL,'a');
select count(*) from raidtest;
ALTER TABLE raidtest ADD COLUMN x INT UNSIGNED NOT NULL;
ALTER TABLE raidtest ADD KEY c (c);
ALTER TABLE raidtest DROP KEY c;
ALTER TABLE raidtest DROP COLUMN x;
ALTER TABLE raidtest RENAME raidnew;
ALTER TABLE raidnew CHANGE COLUMN c c VARCHAR(251) NOT NULL;
select count(*) from raidnew;
DROP TABLE raidnew;

#
# Problem with count(distinct)
#

create table t1 (libname varchar(21) not null, city text, primary key (libname));
create table t2 (isbn varchar(21) not null, author text, title text, primary key (isbn));
create table t3 (isbn varchar(21) not null, libname varchar(21) not null, quantity int ,primary key (isbn,libname));
insert into t2 values ('001','Daffy','A duck''s life');
insert into t2 values ('002','Bugs','A rabbit\'s life');
insert into t2 values ('003','Cowboy','Life on the range');
insert into t2 values ('000','Anonymous','Wanna buy this book?');
insert into t2 values ('004','Best Seller','One Heckuva book');
insert into t2 values ('005','EveryoneBuys','This very book');
insert into t2 values ('006','San Fran','It is a san fran lifestyle');
insert into t2 values ('007','BerkAuthor','Cool.Berkley.the.book');
insert into t3 values('000','New York Public Libra','1');
insert into t3 values('001','New York Public Libra','2');
insert into t3 values('002','New York Public Libra','3');
insert into t3 values('003','New York Public Libra','4');
insert into t3 values('004','New York Public Libra','5');
insert into t3 values('005','New York Public Libra','6');
insert into t3 values('006','San Fransisco Public','5');
insert into t3 values('007','Berkeley Public1','3');
insert into t3 values('007','Berkeley Public2','3');
insert into t3 values('001','NYC Lib','8');
insert into t1 values ('New York Public Libra','New York');
insert into t1 values ('San Fransisco Public','San Fran');
insert into t1 values ('Berkeley Public1','Berkeley');
insert into t1 values ('Berkeley Public2','Berkeley');
insert into t1 values ('NYC Lib','New York');
select t2.isbn,city,t1.libname,count(t1.libname) as a from t3 left join t1 on t3.libname=t1.libname left join t2 on t3.isbn=t2.isbn group by city,t1.libname;
select t2.isbn,city,t1.libname,count(distinct t1.libname) as a from t3 left join t1 on t3.libname=t1.libname left join t2 on t3.isbn=t2.isbn group by city having count(distinct t1.libname) > 1;
drop table t1, t2, t3;

#
# Problem with table dependencies
#

create table t1 (
    id		int not null,
    name	tinytext not null,
    unique	(id)
);
create table t2 (
    id		int not null,
    idx		int not null,
    unique	(id, idx)
);
create table t3 (
    id		int not null,
    idx		int not null,
    unique	(id, idx)
);
insert into t1 values (1,'yes'), (2,'no');
insert into t2 values (1,1);
insert into t3 values (1,1);
EXPLAIN
SELECT DISTINCT
    t1.id
from
    t1
    straight_join
    t2
    straight_join
    t3
    straight_join
    t1 as j_lj_t2 left join t2 as t2_lj
        on j_lj_t2.id=t2_lj.id
    straight_join
    t1 as j_lj_t3 left join t3 as t3_lj
        on j_lj_t3.id=t3_lj.id
WHERE
    ((t1.id=j_lj_t2.id AND t2_lj.id IS NULL) OR (t1.id=t2.id AND t2.idx=2))
    AND ((t1.id=j_lj_t3.id AND t3_lj.id IS NULL) OR (t1.id=t3.id AND t3.idx=2));
SELECT DISTINCT
    t1.id
from
    t1
    straight_join
    t2
    straight_join
    t3
    straight_join
    t1 as j_lj_t2 left join t2 as t2_lj
        on j_lj_t2.id=t2_lj.id
    straight_join
    t1 as j_lj_t3 left join t3 as t3_lj
        on j_lj_t3.id=t3_lj.id
WHERE
    ((t1.id=j_lj_t2.id AND t2_lj.id IS NULL) OR (t1.id=t2.id AND t2.idx=2))
    AND ((t1.id=j_lj_t3.id AND t3_lj.id IS NULL) OR (t1.id=t3.id AND t3.idx=2));
drop table t1,t2,t3;

#
# Test of hex constants in WHERE:
#

create table t1 (ID int(8) unsigned zerofill not null auto_increment,UNIQ bigint(21) unsigned zerofill not null,primary key (ID),unique (UNIQ) );
insert into t1 set UNIQ=0x38afba1d73e6a18a;
insert into t1 set UNIQ=123; 
explain select * from t1 where UNIQ=0x38afba1d73e6a18a;
drop table t1;

#
# Test of compressed decimal index.
#

CREATE TABLE t1 (
  name varchar(50) DEFAULT '' NOT NULL,
  author varchar(50) DEFAULT '' NOT NULL,
  category decimal(10,0) DEFAULT '0' NOT NULL,
  email varchar(50),
  password varchar(50),
  proxy varchar(50),
  bitmap varchar(20),
  msg varchar(255),
  urlscol varchar(127),
  urlhttp varchar(127),
  timeout decimal(10,0),
  nbcnx decimal(10,0),
  creation decimal(10,0),
  livinguntil decimal(10,0),
  lang decimal(10,0),
  type decimal(10,0),
  subcat decimal(10,0),
  subtype decimal(10,0),
  reg char(1),
  scs varchar(255),
  capacity decimal(10,0),
  userISP varchar(50),
  CCident varchar(50) DEFAULT '' NOT NULL,
  PRIMARY KEY (name,author,category)
);
INSERT INTO t1 VALUES
('patnom','patauteur',0,'p.favre@cryo-networks.fr',NULL,NULL,'#p2sndnq6ae5g1u6t','essai\nsalut','scol://195.242.78.119:patauteur.patnom',NULL,NULL,NULL,950036174,-882087474,NULL,3,0,3,'1','Pub/patnom/futur_divers.scs',NULL,'pat','CC1');
INSERT INTO t1 VALUES
('LeNomDeMonSite','Marc',0,'m.barilley@cryo-networks.fr',NULL,NULL,NULL,NULL,'scol://195.242.78.119:Marc.LeNomDeMonSite',NULL,NULL,NULL,950560434,-881563214,NULL,3,0,3,'1','Pub/LeNomDeMonSite/domus_hibere.scs',NULL,'Marq','CC1');
select * from t1 where name='patnom' and author='patauteur' and category=0;
drop table t1;

#
# Test of group by bug in bugzilla
#

CREATE TABLE t1 (
  bug_id mediumint(9) DEFAULT '0' NOT NULL auto_increment,
  groupset bigint(20) DEFAULT '0' NOT NULL,
  assigned_to mediumint(9) DEFAULT '0' NOT NULL,
  bug_file_loc text,
  bug_severity enum('blocker','critical','major','normal','minor','trivial','enhancement') DEFAULT 'blocker' NOT NULL,
  bug_status enum('NEW','ASSIGNED','REOPENED','RESOLVED','VERIFIED','CLOSED') DEFAULT 'NEW' NOT NULL,
  creation_ts datetime DEFAULT '0000-00-00 00:00:00' NOT NULL,
  delta_ts timestamp(14),
  short_desc mediumtext,
  long_desc mediumtext,
  op_sys enum('All','Windows 3.1','Windows 95','Windows 98','Windows NT','Windows 2000','Linux','other') DEFAULT 'All' NOT NULL,
  priority enum('P1','P2','P3','P4','P5') DEFAULT 'P1' NOT NULL,
  product varchar(64) DEFAULT '' NOT NULL,
  rep_platform enum('All','PC','VTD-8','Other'),
  reporter mediumint(9) DEFAULT '0' NOT NULL,
  version varchar(16) DEFAULT '' NOT NULL,
  component varchar(50) DEFAULT '' NOT NULL,
  resolution enum('','FIXED','INVALID','WONTFIX','LATER','REMIND','DUPLICATE','WORKSFORME') DEFAULT '' NOT NULL,
  target_milestone varchar(20) DEFAULT '' NOT NULL,
  qa_contact mediumint(9) DEFAULT '0' NOT NULL,
  status_whiteboard mediumtext NOT NULL,
  votes mediumint(9) DEFAULT '0' NOT NULL,
  PRIMARY KEY (bug_id),
  KEY assigned_to (assigned_to),
  KEY creation_ts (creation_ts),
  KEY delta_ts (delta_ts),
  KEY bug_severity (bug_severity),
  KEY bug_status (bug_status),
  KEY op_sys (op_sys),
  KEY priority (priority),
  KEY product (product),
  KEY reporter (reporter),
  KEY version (version),
  KEY component (component),
  KEY resolution (resolution),
  KEY target_milestone (target_milestone),
  KEY qa_contact (qa_contact),
  KEY votes (votes)
);

INSERT INTO t1 VALUES (1,0,0,'','normal','','2000-02-10 09:25:12',20000321114747,'','','Linux','P1','TestProduct','PC',3,'other','TestComponent','','M1',0,'',0);
INSERT INTO t1 VALUES (9,0,0,'','enhancement','','2000-03-10 11:49:36',20000321114747,'','','All','P5','AAAAA','PC',3,'2.00 CD - Pre','BBBBBBBBBBBBB - conversion','','',0,'',0);
INSERT INTO t1 VALUES (10,0,0,'','enhancement','','2000-03-10 18:10:16',20000321114747,'','','All','P4','AAAAA','PC',3,'2.00 CD - Pre','BBBBBBBBBBBBB - conversion','','',0,'',0);
INSERT INTO t1 VALUES (7,0,0,'','critical','','2000-03-09 10:50:21',20000321114747,'','','All','P1','AAAAA','PC',3,'2.00 CD - Pre','BBBBBBBBBBBBB - generic','','',0,'',0);
INSERT INTO t1 VALUES (6,0,0,'','normal','','2000-03-09 10:42:44',20000321114747,'','','All','P2','AAAAA','PC',3,'2.00 CD - Pre','kkkkkkkkkkk lllllllllll','','',0,'',0);
INSERT INTO t1 VALUES (8,0,0,'','major','','2000-03-09 11:32:14',20000321114747,'','','All','P3','AAAAA','PC',3,'2.00 CD - Pre','kkkkkkkkkkk lllllllllll','','',0,'',0);
INSERT INTO t1 VALUES (5,0,0,'','enhancement','','2000-03-09 10:38:59',20000321114747,'','','All','P5','CCC/CCCCCC','PC',5,'7.00','Administration','','',0,'',0);
INSERT INTO t1 VALUES (4,0,0,'','normal','','2000-03-08 18:32:14',20000321114747,'','','other','P2','TestProduct','Other',3,'other','TestComponent2','','',0,'',0);
INSERT INTO t1 VALUES (3,0,0,'','normal','','2000-03-08 18:30:52',20000321114747,'','','other','P2','TestProduct','Other',3,'other','TestComponent','','',0,'',0);
INSERT INTO t1 VALUES (2,0,0,'','enhancement','','2000-03-08 18:24:51',20000321114747,'','','All','P2','TestProduct','Other',4,'other','TestComponent2','','',0,'',0);
INSERT INTO t1 VALUES (11,0,0,'','blocker','','2000-03-13 09:43:41',20000321114747,'','','All','P2','CCC/CCCCCC','PC',5,'7.00','DDDDDDDDD','','',0,'',0);
INSERT INTO t1 VALUES (12,0,0,'','normal','','2000-03-13 16:14:31',20000321114747,'','','All','P2','AAAAA','PC',3,'2.00 CD - Pre','kkkkkkkkkkk lllllllllll','','',0,'',0);
INSERT INTO t1 VALUES (13,0,0,'','normal','','2000-03-15 16:20:44',20000321114747,'','','other','P2','TestProduct','Other',3,'other','TestComponent','','',0,'',0);
INSERT INTO t1 VALUES (14,0,0,'','blocker','','2000-03-15 18:13:47',20000321114747,'','','All','P1','AAAAA','PC',3,'2.00 CD - Pre','BBBBBBBBBBBBB - generic','','',0,'',0);
INSERT INTO t1 VALUES (15,0,0,'','minor','','2000-03-16 18:03:28',20000321114747,'','','All','P2','CCC/CCCCCC','Other',5,'7.00','DDDDDDDDD','','',0,'',0);
INSERT INTO t1 VALUES (16,0,0,'','normal','','2000-03-16 18:33:41',20000321114747,'','','All','P2','CCC/CCCCCC','Other',5,'7.00','Administration','','',0,'',0);
INSERT INTO t1 VALUES (17,0,0,'','normal','','2000-03-16 18:34:18',20000321114747,'','','All','P2','CCC/CCCCCC','Other',5,'7.00','Administration','','',0,'',0);
INSERT INTO t1 VALUES (18,0,0,'','normal','','2000-03-16 18:34:56',20000321114747,'','','All','P2','CCC/CCCCCC','Other',5,'7.00','Administration','','',0,'',0);
INSERT INTO t1 VALUES (19,0,0,'','enhancement','','2000-03-16 18:35:34',20000321114747,'','','All','P2','CCC/CCCCCC','Other',5,'7.00','Administration','','',0,'',0);
INSERT INTO t1 VALUES (20,0,0,'','enhancement','','2000-03-16 18:36:23',20000321114747,'','','All','P2','CCC/CCCCCC','Other',5,'7.00','Administration','','',0,'',0);
INSERT INTO t1 VALUES (21,0,0,'','enhancement','','2000-03-16 18:37:23',20000321114747,'','','All','P2','CCC/CCCCCC','Other',5,'7.00','Administration','','',0,'',0);
INSERT INTO t1 VALUES (22,0,0,'','enhancement','','2000-03-16 18:38:16',20000321114747,'','','All','P2','CCC/CCCCCC','Other',5,'7.00','Administration','','',0,'',0);
INSERT INTO t1 VALUES (23,0,0,'','normal','','2000-03-16 18:58:12',20000321114747,'','','All','P2','CCC/CCCCCC','Other',5,'7.00','DDDDDDDDD','','',0,'',0);
INSERT INTO t1 VALUES (24,0,0,'','normal','','2000-03-17 11:08:10',20000321114747,'','','All','P2','AAAAAAAA-AAA','PC',3,'2.8','Web Interface','','',0,'',0);
INSERT INTO t1 VALUES (25,0,0,'','normal','','2000-03-17 11:10:45',20000321114747,'','','All','P2','AAAAAAAA-AAA','PC',3,'2.8','Web Interface','','',0,'',0);
INSERT INTO t1 VALUES (26,0,0,'','normal','','2000-03-17 11:15:47',20000321114747,'','','All','P2','AAAAAAAA-AAA','PC',3,'2.8','Web Interface','','',0,'',0);
INSERT INTO t1 VALUES (27,0,0,'','normal','','2000-03-17 17:45:41',20000321114747,'','','All','P2','CCC/CCCCCC','PC',5,'7.00','DDDDDDDDD','','',0,'',0);
INSERT INTO t1 VALUES (28,0,0,'','normal','','2000-03-20 09:51:45',20000321114747,'','','Windows NT','P2','TestProduct','PC',8,'other','TestComponent','','',0,'',0);
INSERT INTO t1 VALUES (29,0,0,'','normal','','2000-03-20 11:15:09',20000321114747,'','','All','P5','AAAAAAAA-AAA','PC',3,'2.8','Web Interface','','',0,'',0);
CREATE TABLE t2 (
  value tinytext,
  program varchar(64),
  initialowner tinytext NOT NULL,
  initialqacontact tinytext NOT NULL,
  description mediumtext NOT NULL
);

INSERT INTO t2 VALUES ('TestComponent','TestProduct','id0001','','');
INSERT INTO t2 VALUES ('BBBBBBBBBBBBB - conversion','AAAAA','id0001','','');
INSERT INTO t2 VALUES ('BBBBBBBBBBBBB - generic','AAAAA','id0001','','');
INSERT INTO t2 VALUES ('TestComponent2','TestProduct','id0001','','');
INSERT INTO t2 VALUES ('BBBBBBBBBBBBB - eeeeeeeee','AAAAA','id0001','','');
INSERT INTO t2 VALUES ('kkkkkkkkkkk lllllllllll','AAAAA','id0001','','');
INSERT INTO t2 VALUES ('Test Procedures','AAAAA','id0001','','');
INSERT INTO t2 VALUES ('Documentation','AAAAA','id0003','','');
INSERT INTO t2 VALUES ('DDDDDDDDD','CCC/CCCCCC','id0002','','');
INSERT INTO t2 VALUES ('Eeeeeeee Lite','CCC/CCCCCC','id0002','','');
INSERT INTO t2 VALUES ('Eeeeeeee Full','CCC/CCCCCC','id0002','','');
INSERT INTO t2 VALUES ('Administration','CCC/CCCCCC','id0002','','');
INSERT INTO t2 VALUES ('Distribution','CCC/CCCCCC','id0002','','');
INSERT INTO t2 VALUES ('Setup','CCC/CCCCCC','id0002','','');
INSERT INTO t2 VALUES ('Unspecified','CCC/CCCCCC','id0002','','');
INSERT INTO t2 VALUES ('Web Interface','AAAAAAAA-AAA','id0001','','');
INSERT INTO t2 VALUES ('Host communication','AAAAA','id0001','','');
select value,description,bug_id from t2 left join t1 on t2.program=t1.product and t2.value=t1.component where program="AAAAA";
select value,description,COUNT(bug_id) from t2 left join t1 on t2.program=t1.product and t2.value=t1.component where program="AAAAA" group by value;

drop table t1,t2;

#
# Problem with search on partial index
#

create table t1
(
  name_id int not null auto_increment,
  name blob,
  INDEX name_idx (name(5)),
  primary key (name_id)
);

INSERT t1 VALUES(NULL,'/');
INSERT t1 VALUES(NULL,'[T,U]_axpby');         
SELECT * FROM t1 WHERE name='[T,U]_axpy';
SELECT * FROM t1 WHERE name='[T,U]_axpby';
create table t2
(
  name_id int not null auto_increment,
  name char(255) binary,
  INDEX name_idx (name(5)),
  primary key (name_id)
);
INSERT t2 select * from t1;
SELECT * FROM t2 WHERE name='[T,U]_axpy';
SELECT * FROM t2 WHERE name='[T,U]_axpby';
drop table t1,t2;

#
# Problem with many enums
#

CREATE TABLE t1 (
  field enum('001001','001004','001010','001018','001019','001020','001021','001027','001028','001029','001030','001031','001100','002003','002004','002005','002007','002008','002009','002012','002013','002014','003002','003003','003004','003005','003006','003007','003008','003009','003010','003011','003012','003013','003014','003015','003016','003017','003018','003019','004002','004003','004005','004006','004007','004008','004010','004012','004014','004016','004017','004020','004021','004022','004023','004024','004025','004026','006002','006004','006006','006010','006011','006012','006013','006014','007001','007002','007003','007004','007005','007006','007007','007008','007009','007010','007011','007012','007013','007014','007015','007016','007017','007018','007019','007020','007021','007022','007023','007024','007025','007026','007027','007028','007029','007030','007031','007032','007033','007034','007035','007036','007037','007038','007039','007040','007043','007044','009001','009002','009004','009005','009006','009007','009008','009009','009010','009011','009012','009013','010002','010003','010004','010005','010006','010007','010008','010009','010010','010011','010012','010013','010015','010016','010017','010018','010019','010020','010021','010022','010023','010024','010025','010026','010027','010028','011001','011002','011003','011004','011006','011012','011013','011014','011015','011016','012017','012018','012019','012023','012027','012028','012029','012030','012031','012032','012033','012034','012035','012036','012037','012038','012039','014001','016002','016003','016004','016007','016010','016011','016016','016019','016020','016021','016022','016023','016024','016026','016027','016028','016029','016030','016031','016032','016033','016034','017002','018001','019002','019004','020001','020003','020004','020005','020006','020007','020008','020009','022001','022002','022003','023001','023002','023003','023004','023005','023006','023007','023008','023010','023011','023012','023017','023019','023020','023021','023025','023026','023027','023028','023029','023030','023031','023032','023033','023034','023035','025001','025003','025004','025005','025006','025007','025008','025009','025010','025011','025012','025013','025014','025015','025016','025017','025018','025019','025020','025021','025022','025023','025024','025025','025026','025027','025028','025029','025030','025031','025032','025033','025034','025035','025036','025037','025038','025039','025040','025041','025042','025043','025044','025045','025046','025047','025048','025049','025050','025051','025052','025053','025054','025055','025056','025057','025058','025059','025060','025061','025062','025063','027001','027002','027011','035008','035012','036001','037001','037003','037004','037005','037006','037007','037008','037009','038004','038005','038006','038007','038009','039001','039002','039003','039004','039005','039006','046001','046002','046003','046004','046005','046007','046008','046009','046010','046011','046012','046013','046014','047001','047002','048001','051001','051002','051003','051004','052001','052002','052005','053015','053016','053019','053020','053023','053024','053026','053028','053029','053033','053034','053036','053037','053038','053039','053041','053042','053043','053045','053046','053047','053048','053051','053052','053054','053055','053056','053057','053068','053069','053070','053073','053074','053075','053086','053094','053095','053096','053097','053098','053099','053100','053101','053102','053103','053104','053105','053107','053122','053123','053124','053125','053127','053128','054001','054002','054003','054004','054005','054006','054007','054009','054010','056001','056002','056003','056004','056005','056006','056009','056010','056011','056016','056017','056018','056019','056020','056021','056022','057001','057002','057003','057004','058002','058003','058004','058005','060001','060003','060004','060005','060006','060007','061002','061003','061004','061005','061006','069006','069007','069010','069011','069012','069013','069014','069015','069016','069017','069018','069020','069021','069022','069023','069024','071002','071003','071004','071005','071006','071008','071011','071013','071020','071021','071022','072001','073001','073002','073003','073004','074001','074002','074003','074004','074005','074006','074007','074008','074009','074010','074011','074012','075001','075007','076101','076102','076103','077001','077002','077003','077004','077006','077007','077008','077009','078005','079002','079003','079004','079005','079006','079007','081001','082006','082007','082011','082013','082014','082015','082016','082017','082021','082022','082023','082024','082025','082026','082027','082028','082029','082030','082031','082032','082033','082034','082035','082036','082037','082038','082039','082040','082041','082042','082043','082044','084001','084002','084003','084004','084005','084007','084008','084009','084011','084013','084014','084016','084017','084027','084031','084032','084033','084035','084036','084037','084038','084039','084040','084041','084042','084043','084044','084045','084046','084047','084048','084049','084050','084051','085001','085002','085003','085004','085005','085006','085007','085009','085011','085012','085013','085014','085015','085016','085017','085018','085019','085020','085021','085022','085023','085028','085029','085030','085031','085033','085034','085035','085036','085037','085038','085040','085041','085042','085043','085044','085045','085046','085047','085048','085063','085064','085065','085068','085070','085071','085073','085082','085083','085086','085088','085089','085090','085091','085092','085093','085094','085095','085096','085097','085098','085099','085100','085101','085102','085103','085104','085105','085106','085107','085108','085109','085110','085111','085112','085113','085115','085119','085120','085121','085122','085123','085124','085125','085126','085127','085128','085129','085130','085132','085133','085134','085135','085136','085137','086001','086002','086003','086004','086005','088001','088003','088005','088006','088007','088008','088009','089001','090001','090002','090003','090004','090005','090006','090007','090008','090009','090010','090013','090015','090016','090017','090018','090019','090022','090027','090028','091001','091002','091005','091008','091009','091010','091011','091012','091013','091014','091015','091016','091017','091018','093001','093003','093098','093100','093102','093104','093141','093142','093146','093151','093153','093167','093168','093176','094001','094002','094004','094005','095004','099001','099002','100001','101001','102002','102003','105001','105002','106001','113001','113002','113003','113004','113005','113006','113007','113008','113009','113010','113011','113012','113013','113014','113015','113016','113017','113018','113019','113020','113021','113022','113023','113024','113025','113026','113027','113028','114001','115001','115002','115003','115004','115005','115006','115007','115008','115009','115010','115011','115012','115013','115014','115015','115016','115017','115018','115020','115021','115022','115023','115025','115026','115027','115028','115029','115030','115031','115032','115033','115034','115035','115036','115039','115040','115041','115042','115043','115044','115045','115046','115047','115048','115049','115050','115051','115052','115053','115054','115055','115056','115057','115059','115060','115061','115062','115063','115064','115065','115066','115067','115068','115069','115070','115071','115072','115073','115075','115076','115081','115082','115085','115086','115087','115088','115095','115096','115097','115098','115099','115101','115102','115103','115104','115105','115106','115108','115109','115110','115111','115112','115113','115114','115115','115116','115117','115118','115119','115120','115121','115122','116001','116002','116003','116004','116005','116006','116007','116008','116009','116010','116011','116012','117001','117002','117003','123001','124010','124014','124015','124019','124024','124025','124026','124027','124028','124029','124030','124031','124032','124033','124035','124036','124037','124038','124039','124040','124041','124042','124043','124044','124045','124046','124047','124048','124049','124050','124051','124052','124053','124054','124055','124056','124057','124058','124059','124060','124061','124062','124063','124064','124065','126001','126002','126003','126004','126005','126006','126007','126008','126009','126010','126011','126012','130001','132001','132002','132003','133001','133008','133009','133010','133011','133012','133013','133014','133015','133016','133017','133018','133019','133020','133021','133022','133023','133024','133025','133027','133028','133029','133030','133031','134001','135001','135002','135003','135004','135005','135006','135007','135008','135009','135010','136001','137009','137010','137011','137012','137013','137014','137015','137016','137017','137018','137019','138001','138002','138003','138004','139001','139003','140001','141001','141002','141003','141006','141007','141008','141009','141011','141012','141014','141015','141016','141017','141018','141019','141020','141021','141022','141023','141024','141025','141026','141027','141028','142001','142002','142003','142004','142005','142006','142007','142008','142010','142011','142012','144001','145001','145002','145003','145004','145005','145006','145007','145008','145009','145010','145011','145012','145013','145014','145015','145016','147001','150003','150005','150009','150013','150014','150015','150016','150017','150020','150021','152001','152002','152003','152004','152005','152006','152007','154001','154002','154003','155001','155002','155003','155004','155005','155006','159001','159002','159003','159004','160001','160002','160003','161001','162001','162002','162003','162004','162007','162010','162011','162012','163001','163002','163003','163005','163010','163011','163014','163015','163016','165001','165002','165003','165004','165005','165006','165007','165008','165009','165010','165011','165012','165013','165014','165015','165016','165017','165018','165019','165020','165021','165022','165023','165024','165025','165026','165027','165028','165029','165030','165031','165032','165033','165034','165035','165036','167001','168001','168002','168003','168004','168005','168007','168008','168009','168010','168011','168012','168013','168014','169001','169002','169003','169007','169008','169009','169010','170001','171001','171002','171003','171004','171005','171006','171007','171008','171009','172001','174001','174002','174003','176001','176002','176003','177001','177002','179001','179002','179003','179004','179005','179006','179007','179008','179009','179010','179011','179012','179013','179014','179015','179016','179017','179018','179019','179020','179021','179022','179023','179024','179025','179026','179027','179028','179029','179030','179031','179032','179033','179034','179035','179036','179037','179038','179039','179040','179041','179042','179043','179044','179045','179046','179047','180001','180010','180012','180013','180014','180015','180016','180017','180018','180019','180020','180021','180022','180023','180024','180025','180026','180027','180028','180030','180031','180032','180033','180034','180035','180036','180037','180038','180039','180041','180042','180043','180044','180045','180046','180047','180048','180049','180050','180051','180052','180053','180054','180055','180056','180057','180058','180059','180060','180061','180062','180063','180064','180065','180066','180067','180068','180069','180070','180071','182001','184001','184002','184005','184006','184007','184008','184009','184010','184011','185001','185003','187001','188001','188002','188003','188004','188005','188006','188007','188008','188009','188010','188011','191001','191002','192002','194001','194002','194003','194004','194005','194006','194007','195001','195002','195003','195004','195005','195006','195007','196001','196002','197001','197002','197003','197004','197005','197006','198001','198003','198004','198005','198006','198007','198008','198009','198010','198011','198012','198013','198014','198015','198016','198017','201001','201002','201005','202001','203001','203002','203003','203017','203018','203019','204001','204002','204003','205001','208001','208002','208003','208004','208005','209001','209002','209003','210001','210002','210003','210004','210005','210006','210007','210008','210009','210010','210011','210012','210013','211017','212001','212002','212003','212004','212005','212006','212007','212008','212009','212010','212011','212012','212013','218001','218003','218004','218006','218007','218008','218009','218011','218015','218016','218017','218018','218019','218020','218021','218022','218023','218024','218025','218026','218027','218028','218029','218030','218031','218032','218033','218034','218035','218036','221001','221002','221003','221004','221005','221006','221007','221008','221009','221010','221011','221012','221013','223001','223002','223003','224001','224002','224003','224006','224007','224008','225001','225002','225003','225004','225005','225006','225007','225008','225009','225010','225011','225012','225013','226001','226002','226003','226004','226005','226006','226007','226008','226009','227001','227002','227003','227004','227005','227006','227007','227008','227009','227010','227011','227012','227013','227014','227015','227016','227017','227018','227019','227020','227021','227022','227023','227024','227025','227026','227027','227028','227029','227030','227031','227032','227033','227034','227035','227036','227037','227038','227039','227040','227041','227042','227043','227044','227045','227046','227047','227048','227049','227050','227051','227052','227053','227054','227055','227056','227057','227058','227059','227060','227061','227062','227063','227064','227065','227066','227067','227068','227069','227070','227071','227072','227073','227074','227075','227076','227077','227078','227079','227080','227081','227082','227083','227084','227085','227086','227087','227088','227089','227090','227091','227092','227093','227094','227095','227096','227097','227098','227099','227100','227101','227102','227103','227104','227105','227106','227107','227108','227109','227110','227111','227112','227113','227114','227115','227116','227117','227118','227119','227120','227122','227123','227124','227125','227126','227127','227128','227129','227130','227131','227132','227133','227134','227135','227136','227137','227138','227139','227140','227141','227142','227143','227144','227145','227146','227147','227148','227149','227150','227151','227152','228001','229001','229002','229003','229004','229005','230001','230002','232001','233001','233002','233003','233004','233005','233006','233007','233008','234001','234002','234003','234004','234005','234006','234007','234008','234009','234010','234011','234012','234013','234014','234015','234016','234017','234018','234019','234020','234021','234022','234023','234024','234025','234026','234027','234028','234029','234030','235001','235002','235003','235004','235005','236001','236002','236003','237001','238002','238003','238004','238005','238006','238007','238008','333013','333014','333015','333016','333017','333018','333019','333020','333021','333022','333023','333024','333025','333030','333031','333032','333033','333034','333035','334001','334002','334003','334004','334005','334006','334007','336004','337001','337002','337003','337004','339001','339002','343001','344001','344002','344003','344004','344005','345001','345002','345003','347001','347002','348001','348002','348003','348004','348005','349001','349002','349003','350001','353001','353002','353003','353004','355001','355002','355003','355004','355005','355006','356001','358001','359001','359002','360001','360002','360003','360004','360005','366001','366002','366003','366004','369001','373001','373002','373003','373004','373005','373006','373007','373008','373009','373010','373011','373012','373013','373014','373015','373016','373017','373018','373019','373020','373021','374001','374002','374003','374004','374005','374006','374007','374008','374009','374010','374011','374012','374013','374014','374015','374016','376001','376002','376003','376004','376005','376006','376007','376008','376009','376010','376011','376012','376013','376016','376017','376018','376019','376020','376021','379003','382001','382002','383001','384001','384002','385001','385002','386001','386002','386003','386004','386005','386006','386007','386008','386009','386010','386011','386012','386013','386014','387001','389001','389002','389003','389004','392001','393001','393002','393003','393004','395001','396001','397001','397002','399001','399002','399003','400001','400002','401001','401002','401003','402001','402002','402003','402004','402005','403001','403002','403003','504001','504002','504004','504005','504006','504007','504008','504009','504010','504011','504012','504013','504014','504017','504018','504019','504021','504022','504023','504024','504025','506001','506002','508001','508002','511001','511002','511003','511004','511005','511006','511007','511008','511009','511010','511011','511012','511013','511014','511017','511018','511020','511021','511022','511024','511028','511029','513001','513002','513003','513004','514001','515001','515002','515003','515007','515008','515009','515010','515011','515012','515013','515014','515015','518001','518002','518003','520001','520002','521001','521002','521003','521004','521005','521006','521007','521008','521009','521010','521011','521012','521013','521014','521015','521016','523001','523002','523003','523004','523005','523006','523007','524001','700001','701001','701002','701003','702001','702002','702003','702004','702005','702006','702007','702008','703001','703002','703003','704001','704002','704003','704004','705001','706001','706002','707001','707002','707003','708001','709001','709002','710001','710002','711001','711002','712001','713001','713002','714001','714002','715001','716001','718001','718002','719001','719002','991001','991002','991003','991004','991005','991006','991007','991008','992001','995001','996001','996002','996003','998001','998002','998003','998004','998005','998006','998007','999001','999002','011017','011018','034001','034002','071010','208006','239001','519001','519003','126013','184012','053071','374017','374018','374019','374020','374021','404001','405002','405001','405003','405007','405006','405005','405004','240011','240010','240009','240008','240007','240006','240005','240004','240003','240002','240001','240012','240013','240014','240015','240016','240017','357001','235006','235007','712002','355008','355007','056023','999999','046015','019005','126014','241003','241002','241001','240018','240020','240019','242001','242002','242003','242004','242005','242006','089002','406001','406002','406003','406004','406005','406006','243001','243002','243003','243004','243005','243006','243007','243008','010030','010029','407001','407006','407005','407004','407003','407002','408001','366005','133032','016035','077010','996004','025064','011019','407007','407008','407009','409001','115123','504026','039007','039009','039008','039010','039011','039012','180072','240021','240023','408002','405008','235008','525001','525002','525003','525004','410001','410002','410003','410004','410005','410006','410007','410008','410009','410010','410011','410012','410013','410014','410015','410016','344006','240031','240030','240029','240028','240027','240026','240025','240024','240034','240033','240032','410017','410018','411001','411002','411003','411004','411005','411006','411007','411008','203020','203021','203022','412001','412002','412003','412004','069025','244001','244002','244009','244008','244007','244006','244005','244004','244003','244015','244014','244013','244012','244011','244010','244016','244017','240042','240041','240040','240039','240038','240037','240036','240035','405009','405010','240043','504034','504033','504032','504031','504030','504029','504028','504027','504042','504041','504040','504039','504038','504037','504036','504035','800001','410019','410020','410021','244018','244019','244020','399004','413001','504043','198018','198019','344007','082045','010031','010032','010033','010034','010035','504044','515016','801002','801003','801004','801005','802001','801001','414001','414002','414003','141029','141030','803001','803002','803003','803004','803005','803006','803007','803008','803009','803010','803011','803012','803013','803014','803015','803016','803017','410022','410023','803018','803019','803020','415002','415001','244021','011020','011023','011022','011021','025065','165037','165038','165039','416001','416002','416003','417001','418001','504045','803022','803021','240022','419001','420001','804010','804009','804008','804007','804006','804005','804004','804003','804002','804001','804020','804019','804018','804017','804016','804015','804014','804013','804012','804011','804024','804021','804023','804022','511019','511016','511015','511032','511031','511030','511027','511026','511025','511033','511023','133034','133033','169011','344008','344009','244022','244026','244025','244030','244023','244024','244027','244028','244029','244031','082046','082047','082048','126015','126016','416004','416005','421001','421002','016037','016036','115124','115125','115126','240049','240048','240047','240046','240045','240044','244032','244033','422001','422002','422003','422004','422005','184013','239002','805001','805002','805003','805004','805005','056024','423001','344010','235009','212014','056025','056026','802002','244034','244035','244036','244037','244038','244039','515017','504046','203015','245002','245001','071023','056027','056028','056029','056030','056031','056032','424001','056034','056033','805006','805007','805008','805009','805010','422008','422007','422006','422010','422009','422011','209004','150022','150023','100002','056035','023036','185004','185005','246001','247001','247002','425001','416006','165042','165041','165040','165043','010040','010039','010038','010037','010036','422012','422013','422014','422015','426000','248001','248002','248003','248004','248005','249001','249002','249003','249004','249005','249006','250007','250001','250002','250003','250004','250005','250006','250008','250009','250010','250011','250012','250013','251001','251002','422016','422017','422018','806001','806002','116013','235010','235011','091026','091027','091028','091029','091019','091020','091021','091022','091023','091024','091025','252001','243009','249007','249008','249009','011024','011025','427001','428002','428001','169012','429001','429002','429003') DEFAULT '001001' NOT NULL,
  KEY field (field)
);
INSERT INTO t1 VALUES ('001001'),('001001'),('001001'),('001001'),('001001'),('001001'),('001001'),('001001'),('001001'),('001010'),('001010'),('001010'),('001010'),('001010'),('001018'),('001018'),('001018'),('001018'),('001018'),('001018'),('001020'),('001020'),('001020'),('001020'),('001020'),('001020'),('001020'),('001020'),('001021'),('001021'),('001021'),('001021'),('001021'),('001021'),('001027'),('001027'),('001028'),('001030'),('001030'),('001030'),('001030'),('001031'),('001031'),('001031'),('001031'),('001031'),('001100'),('001100'),('002003'),('002003'),('002003'),('002003'),('002003'),('002003'),('002003'),('002003'),('002003'),('002004'),('002004'),('002004'),('002004'),('002004'),('002004'),('002004'),('002004'),('002004'),('002005'),('002005'),('002005'),('002005'),('002005'),('002005'),('002005'),('002005'),('002007'),('002007'),('002007'),('002007'),('002007'),('002007'),('002007'),('002008'),('002008'),('002008'),('002008'),('002008'),('002008'),('002008'),('002008'),('002009'),('002009'),('002009'),('002009'),('002009'),('002009'),('002009'),('002009'),('002012'),('002012'),('002012'),('002012'),('002012'),('002012'),('002012'),('002013'),('002013'),('002013'),('002013'),('002013'),('002013'),('002013'),('002013'),('002013'),('002014'),('002014'),('002014'),('002014'),('002014'),('002014'),('002014'),('002014'),('003002'),('003002'),('003002'),('003002'),('003002'),('003002'),('003003'),('003003'),('003003'),('003003'),('003003'),('003003'),('003004'),('003004'),('003004'),('003004'),('003004'),('003004'),('003005'),('003005'),('003005'),('003005'),('003005'),('003005'),('003005'),('003005'),('003005'),('003006'),('003006'),('003006'),('003006'),('003006'),('003006'),('003006'),('003006'),('003007'),('003007'),('003007'),('003007'),('003007'),('003008'),('003008'),('003008'),('003008'),('003008'),('003008'),('003009'),('003009'),('003009'),('003009'),('003009'),('003009'),('003009'),('003009'),('003009'),('003010'),('003010'),('003010'),('003010'),('003010'),('003010'),('003010'),('003010'),('003010'),('003011'),('003011'),('003011'),('003011'),('003011'),('003011'),('003011'),('003011'),('003012'),('003012'),('003012'),('003012'),('003012'),('003012'),('003012'),('003012'),('003013'),('003013'),('003013'),('003013'),('003013'),('003013'),('003013'),('003013'),('003014'),('003014'),('003014'),('003014'),('003014'),('003014'),('003014'),('003014'),('003015'),('003015'),('003015'),('003015'),('003015'),('003015'),('003016'),('003016'),('003016'),('003016'),('003016'),('003016'),('003017'),('003017'),('003017'),('003017'),('003017'),('003018'),('003018'),('003018'),('003018'),('003018'),('003019'),('003019'),('004003'),('004005'),('004005'),('004005'),('004005'),('004005'),('004005'),('004006'),('004008'),('004010'),('004012'),('004012'),('004014'),('004014'),('004014'),('004014'),('004014'),('004016'),('004017'),('004017'),('004017'),('004017'),('004017'),('004017'),('004017'),('004017'),('004020'),('004020'),('004020'),('004020'),('004020'),('004020'),('004021'),('004021'),('004021'),('004021'),('004021'),('004021'),('004021'),('004022'),('004023'),('004023'),('004023'),('004023'),('004023'),('004023'),('004023'),('004025'),('004026'),('004026'),('004026'),('004026'),('004026'),('006004'),('006006'),('006010'),('006010'),('006010'),('006010'),('006010'),('006010'),('006010'),('006011'),('006011'),('006011'),('006011'),('006011'),('006011'),('006012'),('006012'),('006012'),('006012'),('006012'),('006012'),('006014'),('006014'),('006014'),('007001'),('007001'),('007002'),('007003'),('007005'),('007007'),('007008'),('007009'),('007011'),('007012'),('007013'),('007015'),('007016'),('007017'),('007018'),('007019'),('007019'),('007020'),('007021'),('007021'),('007022'),('007023'),('007023'),('007025'),('007025'),('007025'),('007027'),('007029'),('007031'),('007031'),('007032'),('007034'),('007034'),('007036'),('007036'),('007036'),('007037'),('007037'),('007038'),('007040'),('007040'),('007040'),('007043'),('009001'),('009001'),('009001'),('009001'),('009001'),('009001'),('009001'),('009002'),('009002'),('009002'),('009002'),('009002'),('009004'),('009004'),('009004'),('009004'),('009005'),('009005'),('009005'),('009005'),('009005'),('009005'),('009005'),('009005'),('009006'),('009006'),('009006'),('009006'),('009007'),('009007'),('009007'),('009007'),('009007'),('009007'),('009008'),('009010'),('009010'),('009010'),('009010'),('009010'),('009010'),('009011'),('009011'),('009011'),('009011'),('009011'),('009012'),('009013'),('009013'),('009013'),('010002'),('010002'),('010002'),('010002'),('010002'),('010002'),('010002'),('010002'),('010003'),('010003'),('010003'),('010003'),('010003'),('010003'),('010003'),('010003'),('010003'),('010004'),('010004'),('010004'),('010004'),('010004'),('010004'),('010004'),('010004'),('010004'),('010005'),('010005'),('010005'),('010005'),('010006'),('010006'),('010006'),('010006'),('010006'),('010006'),('010006'),('010006'),('010006'),('010007'),('010007'),('010007'),('010007'),('010007'),('010007'),('010008'),('010008'),('010008'),('010008'),('010008'),('010008'),('010008'),('010009'),('010009'),('010009'),('010009'),('010009'),('010009'),('010010'),('010010'),('010010'),('010010'),('010010'),('010010'),('010010'),('010011'),('010011'),('010011'),('010011'),('010011'),('010011'),('010011'),('010011'),('010012'),('010012'),('010012'),('010012'),('010012'),('010012'),('010012'),('010013'),('010013'),('010013'),('010013'),('010013'),('010013'),('010015'),('010016'),('010016'),('010016'),('010016'),('010016'),('010016'),('010016'),('010016'),('010017'),('010017'),('010017'),('010017'),('010017'),('010017'),('010018'),('010018'),('010018'),('010018'),('010018'),('010018'),('010018'),('010018'),('010018'),('010019'),('010019'),('010019'),('010019'),('010019'),('010019'),('010020'),('010020'),('010020'),('010021'),('010021'),('010021'),('010021'),('010021'),('010021'),('010022'),('010022'),('010022'),('010022'),('010022'),('010022'),('010022'),('010022'),('010023'),('010023'),('010023'),('010023'),('010023'),('010023'),('010023'),('010023'),('010026'),('010027'),('010028'),('010028'),('011001'),('011001'),('011001'),('011001'),('011001'),('011001'),('011001'),('011002'),('011002'),('011002'),('011002'),('011002'),('011002'),('011002'),('011003'),('011003'),('011003'),('011003'),('011003'),('011003'),('011003'),('011003'),('011004'),('011004'),('011004'),('011004'),('011004'),('011004'),('011004'),('011006'),('011006'),('011006'),('011006'),('011006'),('011006'),('011006'),('011012'),('011012'),('011012'),('011013'),('011013'),('011013'),('011013'),('011013'),('011013'),('011014'),('011014'),('011014'),('011014'),('011015'),('011015'),('011015'),('011015'),('011015'),('011016'),('011016'),('011016'),('011016'),('011016'),('012017'),('012017'),('012027'),('012027'),('012032'),('012034'),('012036'),('012036'),('012037'),('012037'),('012038'),('012039'),('014001'),('014001'),('016016'),('016016'),('016016'),('016019'),('016020'),('016020'),('016020'),('016020'),('016020'),('016020'),('016020'),('016020'),('016021'),('016021'),('016021'),('016021'),('016021'),('016021'),('016021'),('016022'),('016022'),('016022'),('016023'),('016023'),('016023'),('016024'),('016024'),('016024'),('016024'),('016024'),('016024'),('016024'),('016026'),('016026'),('016026'),('016026'),('016026'),('016026'),('016028'),('016028'),('016028'),('016028'),('016028'),('016028'),('016028'),('016029'),('016029'),('016030'),('016031'),('016032'),('016032'),('016032'),('016032'),('016032'),('016032'),('016032'),('016033'),('016033'),('016033'),('016033'),('016033'),('016034'),('016034'),('016034'),('016034'),('016034'),('017002'),('017002'),('017002'),('017002'),('017002'),('018001'),('018001'),('018001'),('018001'),('018001'),('018001'),('018001'),('018001'),('019002'),('019002'),('019002'),('019002'),('019002'),('019002'),('019004'),('019004'),('019004'),('019004'),('019004'),('019004'),('020001'),('020001'),('020001'),('020001'),('020004'),('020006'),('020006'),('020006'),('020006'),('020006'),('020006'),('020008'),('020009'),('020009'),('020009'),('020009'),('020009'),('022001'),('022001'),('022001'),('022001'),('022002'),('022002'),('022002'),('022002'),('022003'),('022003'),('022003'),('022003'),('023001'),('023002'),('023002'),('023002'),('023002'),('023002'),('023002'),('023003'),('023003'),('023003'),('023003'),('023004'),('023004'),('023005'),('023005'),('023006'),('023006'),('023006'),('023006'),('023006'),('023006'),('023007'),('023007'),('023010'),('023010'),('023011'),('023011'),('023017'),('023019'),('023019'),('023019'),('023020'),('023020'),('023025'),('023025'),('023025'),('023026'),('023026'),('023026'),('023027'),('023027'),('023027'),('023028'),('023028'),('023029'),('023029'),('023030'),('023030'),('023032'),('023033'),('023033'),('023033'),('023033'),('023033'),('023033'),('023034'),('023035'),('023035'),('025001'),('025001'),('025001'),('025001'),('025001'),('025001'),('025001'),('025003'),('025003'),('025004'),('025004'),('025005'),('025005'),('025007'),('025007'),('025008'),('025008'),('025009'),('025010'),('025010'),('025010'),('025011'),('025011'),('025012'),('025012'),('025013'),('025013'),('025013'),('025014'),('025015'),('025016'),('025018'),('025018'),('025019'),('025019'),('025020'),('025020'),('025021'),('025022'),('025022'),('025023'),('025023'),('025024'),('025025'),('025025'),('025026'),('025026'),('025027'),('025027'),('025027'),('025028'),('025030'),('025031'),('025033'),('025034'),('025035'),('025037'),('025041'),('025042'),('025043'),('025046'),('025048'),('025048'),('025048'),('025049'),('025049'),('025049'),('025050'),('025050'),('025050'),('025051'),('025051'),('025052'),('025052'),('025052'),('025053'),('025053'),('025054'),('025054'),('025054'),('025054'),('025055'),('025056'),('025056'),('025056'),('025056'),('025056'),('025056'),('025056'),('025056'),('025056'),('025057'),('025057'),('025058'),('025058'),('025060'),('025060'),('025061'),('025062'),('025063'),('027001'),('027002'),('027011'),('036001'),('036001'),('036001'),('036001'),('036001'),('037003'),('037006'),('037007'),('037008'),('037008'),('038009'),('039001'),('039001'),('039001'),('039001'),('039001'),('039001'),('039002'),('039002'),('039002'),('039002'),('039002'),('039003'),('039003'),('039003'),('039003'),('039003'),('039003'),('039004'),('039004'),('039004'),('039004'),('039004'),('039005'),('039005'),('039005'),('039005'),('039005'),('039006'),('039006'),('039006'),('039006'),('046001'),('046001'),('046001'),('046001'),('046001'),('046001'),('046001'),('046001'),('046002'),('046002'),('046002'),('046002'),('046002'),('046002'),('046002'),('046002'),('046003'),('046003'),('046003'),('046003'),('046003'),('046003'),('046003'),('046005'),('046005'),('046005'),('046005'),('046005'),('046005'),('046005'),('046007'),('046007'),('046007'),('046007'),('046007'),('046007'),('046008'),('046008'),('046008'),('046008'),('046008'),('046009'),('046009'),('046009'),('046010'),('046012'),('046012'),('046012'),('046013'),('046014'),('046014'),('046014'),('047001'),('047001'),('047001'),('047001'),('047001'),('047001'),('047001'),('047001'),('047002'),('047002'),('047002'),('047002'),('047002'),('047002'),('047002'),('047002'),('048001'),('048001'),('048001'),('048001'),('048001'),('048001'),('048001'),('048001'),('051003'),('051003'),('051003'),('051003'),('051003'),('051004'),('051004'),('051004'),('051004'),('052001'),('052001'),('052001'),('052001'),('052001'),('052001'),('052001'),('052001'),('052002'),('052002'),('052005'),('052005'),('052005'),('052005'),('052005'),('052005'),('053016'),('053019'),('053019'),('053023'),('053023'),('053023'),('053023'),('053024'),('053024'),('053024'),('053026'),('053026'),('053026'),('053026'),('053028'),('053028'),('053029'),('053029'),('053029'),('053029'),('053033'),('053033'),('053033'),('053045'),('053046'),('053051'),('053051'),('053051'),('053054'),('053054'),('053054'),('053054'),('053057'),('053069'),('053069'),('053097'),('053107'),('053125'),('053125'),('053127'),('054001'),('054001'),('054001'),('054001'),('054001'),('054001'),('054001'),('054002'),('054002'),('054002'),('054002'),('054002'),('054002'),('054003'),('054003'),('054003'),('054003'),('054003'),('054003'),('054003'),('054004'),('054004'),('054004'),('054004'),('054004'),('054004'),('054004'),('054006'),('054006'),('054006'),('054007'),('054007'),('054007'),('054007'),('054007'),('054009'),('054009'),('054009'),('054009'),('054010'),('054010'),('054010'),('054010'),('054010'),('054010'),('054010'),('056001'),('056001'),('056001'),('056001'),('056001'),('056001'),('056001'),('056001'),('056001'),('056002'),('056002'),('056002'),('056002'),('056002'),('056002'),('056002'),('056002'),('056003'),('056003'),('056003'),('056003'),('056003'),('056003'),('056004'),('056004'),('056004'),('056004'),('056004'),('056004'),('056004'),('056005'),('056005'),('056005'),('056005'),('056005'),('056005'),('056005'),('056005'),('056005'),('056006'),('056006'),('056006'),('056006'),('056006'),('056006'),('056006'),('056006'),('056006'),('056009'),('056009'),('056009'),('056011'),('056016'),('056016'),('056016'),('056016'),('056016'),('056016'),('056016'),('056017'),('056017'),('056017'),('056017'),('056017'),('056017'),('056017'),('056017'),('056017'),('056018'),('056018'),('056018'),('056018'),('056018'),('056018'),('056019'),('056019'),('056019'),('056019'),('056019'),('056019'),('056019'),('056019'),('056020'),('056020'),('056020'),('056020'),('056022'),('056022'),('056022'),('056022'),('056022'),('057003'),('057003'),('057004'),('058002'),('058002'),('058002'),('058002'),('058003'),('058003'),('058003'),('058003'),('058004'),('058004'),('058004'),('058005'),('058005'),('058005'),('060001'),('060001'),('060001'),('060001'),('060001'),('060004'),('060004'),('060004'),('060004'),('060004'),('060004'),('060005'),('060005'),('060005'),('060005'),('060005'),('060005'),('060007'),('060007'),('060007'),('060007'),('060007'),('060007'),('060007'),('061004'),('061004'),('061004'),('061004'),('061004'),('061004'),('061006'),('061006'),('061006'),('061006'),('061006'),('061006'),('069006'),('069006'),('069006'),('069006'),('069006'),('069006'),('069006'),('069006'),('069006'),('069007'),('069007'),('069007'),('069007'),('069007'),('069007'),('069007'),('069007'),('069010'),('069010'),('069010'),('069010'),('069010'),('069010'),('069011'),('069012'),('069012'),('069012'),('069012'),('069012'),('069012'),('069012'),('069012'),('069012'),('069012'),('069013'),('069013'),('069013'),('069013'),('069013'),('069013'),('069013'),('069013'),('069013'),('069014'),('069014'),('069014'),('069014'),('069014'),('069014'),('069014'),('069014'),('069014'),('069015'),('069015'),('069015'),('069015'),('069015'),('069015'),('069015'),('069015'),('069015'),('069015'),('069016'),('069016'),('069016'),('069016'),('069016'),('069018'),('069018'),('069018'),('069018'),('069018'),('069018'),('069018'),('069018'),('069018'),('069020'),('069020'),('069020'),('069020'),('069021'),('069023'),('071002'),('071002'),('071002'),('071002'),('071002'),('071003'),('071003'),('071003'),('071003'),('071003'),('071004'),('071004'),('071004'),('071004'),('071004'),('071005'),('071005'),('071005'),('071005'),('071005'),('071005'),('071006'),('071006'),('071006'),('071006'),('071008'),('071008'),('071008'),('071008'),('071008'),('071008'),('071011'),('071011'),('071011'),('071011'),('071011'),('071020'),('071020'),('071020'),('071020'),('071020'),('071021'),('071022'),('071022'),('071022'),('072001'),('072001'),('074001'),('074002'),('074002'),('074002'),('074002'),('074002'),('074002'),('074002'),('074002'),('074003'),('074003'),('074003'),('074003'),('074003'),('074003'),('074003'),('074003'),('074004'),('074004'),('074004'),('074004'),('074004'),('074004'),('074004'),('074004'),('074005'),('074005'),('074005'),('074005'),('074005'),('074005'),('074005'),('074005'),('074006'),('074006'),('074006'),('074006'),('074006'),('074006'),('074006'),('074006'),('074007'),('074007'),('074007'),('074007'),('074007'),('074007'),('074007'),('074007'),('074008'),('074008'),('074008'),('074008'),('074008'),('074008'),('074008'),('074008'),('074009'),('074009'),('074009'),('074009'),('074009'),('074009'),('074009'),('074009'),('074010'),('074010'),('074010'),('074010'),('074010'),('074010'),('074010'),('074010'),('074011'),('074011'),('074011'),('074011'),('074011'),('074011'),('074011'),('074011'),('074012'),('074012'),('074012'),('074012'),('074012'),('074012'),('074012'),('075001'),('075001'),('075001'),('075007'),('075007'),('075007'),('075007'),('076101'),('076101'),('076101'),('076101'),('076102'),('076102'),('076102'),('076103'),('076103'),('076103'),('076103'),('076103'),('077001'),('077001'),('077001'),('077002'),('077002'),('077002'),('077002'),('077002'),('077002'),('077002'),('077003'),('077003'),('077003'),('077003'),('077003'),('077003'),('077003'),('077004'),('077004'),('077004'),('077004'),('077004'),('077004'),('077006'),('077006'),('077008'),('077008'),('077008'),('077008'),('077008'),('077008'),('077008'),('077009'),('077009'),('077009'),('077009'),('077009'),('077009'),('077009'),('078005'),('078005'),('078005'),('079002'),('079002'),('079002'),('079002'),('079002'),('079002'),('079002'),('079003'),('079003'),('079004'),('079004'),('079005'),('079005'),('079005'),('079005'),('079005'),('079005'),('079006'),('079006'),('079006'),('079006'),('079007'),('079007'),('079007'),('079007'),('079007'),('081001'),('081001'),('081001'),('081001'),('081001'),('082011'),('082011'),('082011'),('082011'),('082011'),('082013'),('082013'),('082013'),('082013'),('082013'),('082013'),('082014'),('082014'),('082014'),('082014'),('082014'),('082014'),('082014'),('082015'),('082015'),('082015'),('082015'),('082015'),('082016'),('082016'),('082016'),('082016'),('082016'),('082016'),('082017'),('082017'),('082017'),('082017'),('082017'),('082017'),('082017'),('082021'),('082021'),('082022'),('082022'),('082022'),('082022'),('082022'),('082023'),('082023'),('082023'),('082023'),('082023'),('082024'),('082024'),('082024'),('082024'),('082024'),('082025'),('082025'),('082025'),('082025'),('082025'),('082026'),('082026'),('082026'),('082026'),('082026'),('082027'),('082027'),('082027'),('082027'),('082027'),('082028'),('082028'),('082028'),('082028'),('082029'),('082029'),('082029'),('082029'),('082029'),('082030'),('082030'),('082030'),('082030'),('082031'),('082031'),('082031'),('082031'),('082031'),('082032'),('082032'),('082032'),('082033'),('082033'),('082034'),('082034'),('082034'),('082034'),('082034'),('082034'),('082034'),('082035'),('082035'),('082035'),('082036'),('082036'),('082036'),('082036'),('082037'),('082037'),('082037'),('082038'),('082038'),('082038'),('082038'),('082039'),('082039'),('082039'),('082039'),('082040'),('082040'),('082040'),('082040'),('082040'),('082041'),('082041'),('082041'),('082041'),('082042'),('082042'),('082043'),('082043'),('082043'),('082043'),('082043'),('082044'),('082044'),('082044'),('082044'),('084001'),('084002'),('084002'),('084002'),('084002'),('084003'),('084003'),('084003'),('084003'),('084003'),('084003'),('084003'),('084003'),('084004'),('084004'),('084004'),('084004'),('084004'),('084005'),('084005'),('084005'),('084005'),('084005'),('084007'),('084007'),('084007'),('084007'),('084007'),('084007'),('084008'),('084008'),('084008'),('084008'),('084008'),('084008'),('084009'),('084009'),('084009'),('084009'),('084009'),('084009'),('084011'),('084013'),('084013'),('084013'),('084013'),('084013'),('084014'),('084014'),('084014'),('084016'),('084016'),('084016'),('084016'),('084016'),('084016'),('084016'),('084016'),('084017'),('084017'),('084017'),('084017'),('084017'),('084017'),('084017'),('084017'),('084017'),('084027'),('084027'),('084027'),('084027'),('084027'),('084027'),('084032'),('084032'),('084033'),('084033'),('084033'),('084035'),('084035'),('084035'),('084036'),('084036'),('084036'),('084036'),('084036'),('084036'),('084037'),('084037'),('084038'),('084038'),('084038'),('084038'),('084038'),('084038'),('084039'),('084039'),('084039'),('084039'),('084040'),('084040'),('084040'),('084040'),('084040'),('084041'),('084041'),('084041'),('084041'),('084042'),('084042'),('084043'),('084043'),('084043'),('084043'),('084044'),('084044'),('084044'),('084044'),('084044'),('084045'),('084046'),('084046'),('084046'),('084047'),('084048'),('084048'),('084049'),('084049'),('084050'),('084051'),('084051'),('085001'),('085001'),('085001'),('085001'),('085001'),('085001'),('085002'),('085002'),('085002'),('085002'),('085003'),('085003'),('085003'),('085003'),('085003'),('085003'),('085003'),('085004'),('085004'),('085004'),('085004'),('085004'),('085004'),('085004'),('085005'),('085005'),('085005'),('085005'),('085005'),('085005'),('085006'),('085006'),('085006'),('085006'),('085006'),('085006'),('085006'),('085006'),('085007'),('085007'),('085007'),('085007'),('085007'),('085007'),('085007'),('085009'),('085009'),('085009'),('085009'),('085009'),('085009'),('085011'),('085011'),('085011'),('085011'),('085011'),('085011'),('085011'),('085011'),('085012'),('085012'),('085012'),('085012'),('085012'),('085012'),('085012'),('085014'),('085014'),('085014'),('085014'),('085014'),('085014'),('085014'),('085014'),('085014'),('085015'),('085015'),('085015'),('085015'),('085015'),('085015'),('085015'),('085015'),('085016'),('085016'),('085016'),('085016'),('085016'),('085016'),('085016'),('085016'),('085017'),('085017'),('085017'),('085017'),('085017'),('085018'),('085018'),('085018'),('085018'),('085018'),('085019'),('085019'),('085019'),('085019'),('085019'),('085019'),('085019'),('085019'),('085019'),('085020'),('085020'),('085020'),('085020'),('085020'),('085020'),('085022'),('085022'),('085022'),('085022'),('085022'),('085022'),('085023'),('085023'),('085023'),('085023'),('085023'),('085028'),('085028'),('085028'),('085028'),('085028'),('085028'),('085028'),('085029'),('085029'),('085029'),('085029'),('085029'),('085029'),('085029'),('085030'),('085030'),('085030'),('085030'),('085030'),('085030'),('085030'),('085031'),('085031'),('085031'),('085031'),('085031'),('085031'),('085031'),('085033'),('085034'),('085034'),('085034'),('085034'),('085034'),('085034'),('085034'),('085035'),('085035'),('085035'),('085035'),('085035'),('085035'),('085036'),('085036'),('085036'),('085036'),('085036'),('085036'),('085037'),('085037'),('085037'),('085037'),('085037'),('085037'),('085038'),('085038'),('085038'),('085038'),('085038'),('085038'),('085038'),('085040'),('085040'),('085040'),('085040'),('085040'),('085040'),('085040'),('085040'),('085041'),('085041'),('085041'),('085041'),('085041'),('085041'),('085041'),('085041'),('085042'),('085042'),('085042'),('085042'),('085042'),('085042'),('085042'),('085043'),('085043'),('085043'),('085043'),('085043'),('085043'),('085044'),('085044'),('085044'),('085044'),('085044'),('085044'),('085044'),('085045'),('085045'),('085045'),('085045'),('085045'),('085046'),('085046'),('085046'),('085046'),('085046'),('085046'),('085046'),('085046'),('085047'),('085047'),('085047'),('085047'),('085047'),('085047'),('085047'),('085047'),('085048'),('085048'),('085048'),('085048'),('085048'),('085048'),('085048'),('085063'),('085063'),('085063'),('085063'),('085063'),('085064'),('085064'),('085064'),('085064'),('085064'),('085065'),('085065'),('085068'),('085068'),('085068'),('085068'),('085068'),('085068'),('085071'),('085071'),('085071'),('085071'),('085071'),('085071'),('085073'),('085073'),('085082'),('085082'),('085082'),('085082'),('085082'),('085086'),('085086'),('085086'),('085088'),('085088'),('085088'),('085088'),('085088'),('085088'),('085088'),('085089'),('085089'),('085090'),('085090'),('085090'),('085090'),('085090'),('085090'),('085090'),('085090'),('085091'),('085091'),('085091'),('085091'),('085091'),('085092'),('085092'),('085092'),('085093'),('085093'),('085095'),('085095'),('085095'),('085095'),('085095'),('085096'),('085096'),('085096'),('085096'),('085096'),('085096'),('085097'),('085097'),('085097'),('085097'),('085097'),('085098'),('085098'),('085098'),('085098'),('085098'),('085098'),('085098'),('085099'),('085099'),('085099'),('085099'),('085099'),('085099'),('085099'),('085100'),('085100'),('085100'),('085100'),('085100'),('085100'),('085100'),('085100'),('085100'),('085100'),('085101'),('085101'),('085101'),('085101'),('085101'),('085101'),('085101'),('085101'),('085102'),('085102'),('085103'),('085103'),('085103'),('085104'),('085104'),('085104'),('085104'),('085104'),('085105'),('085105'),('085106'),('085106'),('085106'),('085106'),('085106'),('085106'),('085108'),('085108'),('085109'),('085109'),('085109'),('085109'),('085109'),('085109'),('085109'),('085109'),('085110'),('085110'),('085110'),('085110'),('085110'),('085111'),('085111'),('085111'),('085112'),('085112'),('085112'),('085112'),('085113'),('085113'),('085113'),('085113'),('085113'),('085115'),('085120'),('085121'),('085121'),('085121'),('085121'),('085122'),('085122'),('085122'),('085122'),('085122'),('085122'),('085122'),('085122'),('085123'),('085123'),('085123'),('085123'),('085123'),('085123'),('085123'),('085123'),('085125'),('085125'),('085125'),('085125'),('085125'),('085126'),('085126'),('085126'),('085126'),('085126'),('085127'),('085127'),('085127'),('085127'),('085127'),('085127'),('085127'),('085127'),('085128'),('085128'),('085128'),('085128'),('085128'),('085129'),('085129'),('085129'),('085129'),('085129'),('085130'),('085130'),('085130'),('085130'),('085130'),('085132'),('085132'),('085132'),('085132'),('085132'),('085132'),('085133'),('085133'),('085133'),('085133'),('085133'),('085134'),('085134'),('085134'),('085135'),('085135'),('085135'),('085136'),('085136'),('085136'),('085136'),('085137'),('085137'),('085137'),('085137'),('085137'),('085137'),('085137'),('086002'),('086002'),('086002'),('086002'),('086003'),('086003'),('086003'),('086003'),('086005'),('088001'),('088001'),('088001'),('088001'),('088001'),('088003'),('088003'),('088003'),('088003'),('088003'),('088003'),('088005'),('088005'),('088005'),('088005'),('088005'),('088006'),('088006'),('088006'),('088006'),('088006'),('088007'),('088007'),('088007'),('088008'),('088008'),('088008'),('088008'),('088009'),('088009'),('088009'),('088009'),('088009'),('089001'),('089001'),('089001'),('089001'),('089001'),('089001'),('089001'),('090001'),('090001'),('090001'),('090001'),('090001'),('090001'),('090001'),('090002'),('090002'),('090002'),('090002'),('090002'),('090002'),('090003'),('090003'),('090003'),('090003'),('090003'),('090003'),('090003'),('090004'),('090004'),('090004'),('090004'),('090004'),('090004'),('090004'),('090006'),('090006'),('090006'),('090006'),('090006'),('090006'),('090006'),('090008'),('090008'),('090008'),('090008'),('090008'),('090009'),('090009'),('090009'),('090009'),('090009'),('090010'),('090010'),('090013'),('090013'),('090013'),('090016'),('090016'),('090017'),('090018'),('090022'),('090027'),('091001'),('091001'),('091001'),('091001'),('091001'),('091001'),('091002'),('091002'),('091002'),('091002'),('091002'),('091002'),('091009'),('091009'),('091009'),('091009'),('091009'),('091011'),('091011'),('091011'),('091011'),('091011'),('091011'),('091011'),('091012'),('091012'),('091013'),('091013'),('091013'),('091013'),('091013'),('091013'),('091015'),('091015'),('091015'),('091015'),('091015'),('091015'),('091016'),('091016'),('091016'),('091016'),('091016'),('091017'),('091017'),('091018'),('091018'),('091018'),('091018'),('093003'),('093003'),('093003'),('093003'),('093003'),('093003'),('099001'),('099001'),('099001'),('099001'),('099001'),('099001'),('099001'),('100001'),('100001'),('100001'),('100001'),('106001'),('113005'),('113005'),('113005'),('113006'),('113006'),('113018'),('113019'),('113020'),('115001'),('115001'),('115001'),('115002'),('115002'),('115003'),('115004'),('115004'),('115004'),('115004'),('115005'),('115005'),('115005'),('115006'),('115006'),('115006'),('115007'),('115007'),('115007'),('115007'),('115007'),('115008'),('115008'),('115008'),('115009'),('115010'),('115010'),('115010'),('115010'),('115010'),('115011'),('115011'),('115011'),('115011'),('115012'),('115012'),('115013'),('115013'),('115013'),('115014'),('115014'),('115014'),('115014'),('115015'),('115015'),('115015'),('115016'),('115016'),('115016'),('115016'),('115017'),('115017'),('115017'),('115017'),('115017'),('115018'),('115018'),('115020'),('115020'),('115021'),('115021'),('115022'),('115022'),('115022'),('115023'),('115023'),('115023'),('115023'),('115023'),('115025'),('115025'),('115025'),('115026'),('115026'),('115027'),('115027'),('115027'),('115028'),('115028'),('115028'),('115028'),('115029'),('115029'),('115029'),('115030'),('115030'),('115030'),('115031'),('115031'),('115032'),('115032'),('115032'),('115033'),('115033'),('115033'),('115033'),('115034'),('115034'),('115034'),('115035'),('115035'),('115036'),('115036'),('115036'),('115036'),('115036'),('115039'),('115040'),('115040'),('115040'),('115041'),('115041'),('115041'),('115041'),('115041'),('115042'),('115042'),('115042'),('115042'),('115042'),('115043'),('115043'),('115043'),('115044'),('115044'),('115044'),('115044'),('115046'),('115046'),('115046'),('115047'),('115048'),('115050'),('115050'),('115050'),('115050'),('115050'),('115051'),('115051'),('115051'),('115052'),('115053'),('115053'),('115054'),('115054'),('115054'),('115055'),('115055'),('115055'),('115057'),('115059'),('115059'),('115059'),('115059'),('115060'),('115060'),('115060'),('115060'),('115060'),('115060'),('115061'),('115061'),('115061'),('115062'),('115062'),('115062'),('115062'),('115064'),('115064'),('115064'),('115065'),('115065'),('115065'),('115065'),('115066'),('115066'),('115066'),('115067'),('115067'),('115067'),('115068'),('115068'),('115068'),('115069'),('115069'),('115069'),('115069'),('115069'),('115070'),('115070'),('115070'),('115071'),('115071'),('115071'),('115072'),('115072'),('115072'),('115073'),('115073'),('115075'),('115075'),('115075'),('115076'),('115076'),('115076'),('115076'),('115076'),('115076'),('115081'),('115081'),('115081'),('115082'),('115082'),('115082'),('115085'),('115085'),('115085'),('115085'),('115085'),('115086'),('115086'),('115086'),('115087'),('115087'),('115088'),('115088'),('115088'),('115088'),('115088'),('115095'),('115095'),('115095'),('115096'),('115096'),('115097'),('115097'),('115098'),('115098'),('115099'),('115101'),('115102'),('115102'),('115102'),('115103'),('115103'),('115104'),('115104'),('115104'),('115104'),('115105'),('115105'),('115106'),('115106'),('115106'),('115106'),('115106'),('115108'),('115109'),('115111'),('115111'),('115111'),('115111'),('115112'),('115112'),('115112'),('115112'),('115112'),('115113'),('115113'),('115113'),('115114'),('115114'),('115114'),('115114'),('115114'),('115115'),('115115'),('115115'),('115115'),('115116'),('115117'),('115117'),('115117'),('115118'),('115118'),('115119'),('115119'),('115119'),('115119'),('115120'),('115121'),('115121'),('115122'),('115122'),('116001'),('116003'),('116003'),('116003'),('116003'),('116004'),('116004'),('116005'),('116005'),('116006'),('116006'),('116006'),('116007'),('116007'),('116008'),('116008'),('116009'),('116009'),('116009'),('116010'),('116010'),('116010'),('116010'),('116011'),('116011'),('116011'),('116011'),('116012'),('116012'),('123001'),('123001'),('123001'),('123001'),('123001'),('124065'),('126001'),('126001'),('126001'),('126001'),('126001'),('126001'),('126001'),('126001'),('126002'),('126002'),('126002'),('126002'),('126002'),('126002'),('126002'),('126002'),('126003'),('126003'),('126003'),('126003'),('126003'),('126003'),('126003'),('126003'),('126003'),('126004'),('126004'),('126004'),('126004'),('126004'),('126004'),('126004'),('126004'),('126004'),('126004'),('126005'),('126005'),('126005'),('126005'),('126005'),('126005'),('126005'),('126005'),('126005'),('126006'),('126006'),('126006'),('126006'),('126006'),('126006'),('126006'),('126006'),('126006'),('126007'),('126007'),('126007'),('126007'),('126007'),('126007'),('126007'),('126008'),('126008'),('126008'),('126008'),('126008'),('126008'),('126008'),('126008'),('126009'),('126009'),('126009'),('126009'),('126009'),('126009'),('126009'),('126009'),('126010'),('126010'),('126010'),('126010'),('126010'),('126010'),('126010'),('126010'),('126010'),('126011'),('126011'),('126011'),('126011'),('126011'),('126011'),('126011'),('126012'),('126012'),('126012'),('126012'),('130001'),('130001'),('130001'),('130001'),('132001'),('132001'),('132001'),('132001'),('132001'),('132002'),('132002'),('132002'),('132002'),('132002'),('132002'),('132002'),('133001'),('133001'),('133008'),('133009'),('133010'),('133011'),('133011'),('133011'),('133011'),('133011'),('133011'),('133012'),('133015'),('133015'),('133015'),('133015'),('133016'),('133018'),('133018'),('133018'),('133018'),('133018'),('133019'),('133021'),('133021'),('133022'),('133022'),('133023'),('133023'),('133024'),('133024'),('133024'),('133024'),('133024'),('133024'),('133025'),('133027'),('133027'),('133027'),('133027'),('133027'),('133028'),('133028'),('133028'),('133029'),('133029'),('133029'),('133029'),('133029'),('133029'),('133030'),('133030'),('133031'),('133031'),('133031'),('134001'),('134001'),('134001'),('135001'),('135001'),('135001'),('135001'),('135001'),('135002'),('135002'),('135002'),('135004'),('135010'),('135010'),('135010'),('135010'),('135010'),('135010'),('137010'),('137011'),('137012'),('137014'),('137015'),('137015'),('137016'),('137019'),('139001'),('140001'),('140001'),('140001'),('140001'),('140001'),('140001'),('141001'),('141001'),('141001'),('141001'),('141001'),('141002'),('141002'),('141002'),('141002'),('141002'),('141003'),('141003'),('141003'),('141003'),('141003'),('141003'),('141003'),('141003'),('141006'),('141006'),('141006'),('141006'),('141006'),('141006'),('141006'),('141006'),('141007'),('141007'),('141007'),('141007'),('141007'),('141009'),('141009'),('141009'),('141009'),('141009'),('141011'),('141011'),('141011'),('141011'),('141011'),('141011'),('141012'),('141014'),('141014'),('141014'),('141014'),('141014'),('141014'),('141014'),('141014'),('141015'),('141015'),('141015'),('141015'),('141015'),('141016'),('141016'),('141016'),('141016'),('141016'),('141016'),('141017'),('141017'),('141017'),('141017'),('141017'),('141017'),('141018'),('141018'),('141018'),('141018'),('141019'),('141019'),('141019'),('141019'),('141020'),('141020'),('141020'),('141020'),('141020'),('141020'),('141020'),('141021'),('141021'),('141021'),('141021'),('141021'),('141021'),('141022'),('141022'),('141022'),('141022'),('141022'),('141022'),('141023'),('141023'),('141023'),('141023'),('141023'),('141023'),('141023'),('141024'),('141025'),('141025'),('141025'),('141026'),('141026'),('141026'),('141026'),('141026'),('141026'),('141027'),('141027'),('141027'),('141027'),('141027'),('141028'),('141028'),('145001'),('145001'),('145001'),('145001'),('145001'),('145001'),('145001'),('145001'),('145001'),('145002'),('145002'),('145002'),('145002'),('145002'),('145002'),('145002'),('145002'),('145002'),('145003'),('145003'),('145003'),('145003'),('145003'),('145003'),('145003'),('145003'),('145003'),('145003'),('145004'),('145004'),('145004'),('145004'),('145004'),('145004'),('145004'),('145004'),('145004'),('145005'),('145005'),('145005'),('145005'),('145005'),('145005'),('145005'),('145005'),('145005'),('145006'),('145006'),('145006'),('145006'),('145006'),('145006'),('145006'),('145006'),('145006'),('145008'),('145008'),('145008'),('145008'),('145008'),('145008'),('145008'),('145008'),('145009'),('145009'),('145009'),('145009'),('145009'),('145009'),('145009'),('145011'),('145011'),('145011'),('145011'),('145011'),('145011'),('145011'),('145011'),('145012'),('145012'),('145012'),('145012'),('145012'),('145012'),('145012'),('145012'),('145013'),('145013'),('145013'),('145013'),('145013'),('145013'),('145013'),('150009'),('150013'),('150014'),('150015'),('150015'),('150015'),('150016'),('150016'),('150017'),('150017'),('150017'),('150017'),('150020'),('152001'),('152001'),('152001'),('152002'),('152003'),('152003'),('152003'),('152003'),('152004'),('152005'),('152006'),('152006'),('152006'),('152006'),('152007'),('154001'),('154002'),('154002'),('155001'),('155001'),('155002'),('155003'),('155004'),('155004'),('155006'),('159001'),('159003'),('160001'),('160001'),('160001'),('160001'),('160002'),('160002'),('161001'),('162002'),('162002'),('162003'),('162003'),('162003'),('162003'),('162003'),('162007'),('162012'),('162012'),('162012'),('163001'),('163001'),('163001'),('163011'),('163015'),('163016'),('163016'),('165001'),('165001'),('165001'),('165001'),('165002'),('165002'),('165002'),('165002'),('165003'),('165003'),('165003'),('165004'),('165004'),('165004'),('165005'),('165005'),('165005'),('165006'),('165006'),('165006'),('165006'),('165007'),('165007'),('165007'),('165007'),('165008'),('165008'),('165008'),('165008'),('165009'),('165009'),('165009'),('165009'),('165010'),('165010'),('165010'),('165011'),('165011'),('165012'),('165012'),('165012'),('165013'),('165013'),('165013'),('165014'),('165014'),('165014'),('165015'),('165015'),('165015'),('165015'),('165016'),('165016'),('165016'),('165017'),('165017'),('165017'),('165017'),('165018'),('165018'),('165018'),('165018'),('165019'),('165019'),('165019'),('165019'),('165020'),('165020'),('165020'),('165020'),('165021'),('165021'),('165021'),('165021'),('165022'),('165022'),('165022'),('165023'),('165024'),('165024'),('165024'),('165025'),('165025'),('165025'),('165026'),('165026'),('165026'),('165028'),('165029'),('165030'),('165030'),('165030'),('165031'),('165031'),('165033'),('165033'),('165034'),('165034'),('165034'),('165035'),('165035'),('165035'),('165036'),('165036'),('165036'),('168003'),('168003'),('168004'),('168005'),('168014'),('169001'),('169001'),('169001'),('169001'),('169001'),('169001'),('169001'),('169001'),('169001'),('169001'),('169002'),('169002'),('169002'),('169002'),('169002'),('169002'),('169002'),('169002'),('169002'),('169002'),('169003'),('169003'),('169003'),('169003'),('169007'),('169007'),('169007'),('169007'),('169007'),('169007'),('169007'),('169007'),('169007'),('169007'),('169008'),('169008'),('169008'),('169008'),('169008'),('169008'),('169008'),('169009'),('169009'),('169009'),('169009'),('169010'),('171006'),('171006'),('171007'),('171007'),('171008'),('171008'),('171008'),('171009'),('171009'),('171009'),('172001'),('176001'),('176001'),('176001'),('176001'),('176001'),('176001'),('176001'),('176002'),('176002'),('176002'),('176002'),('176002'),('176003'),('176003'),('176003'),('176003'),('176003'),('176003'),('177001'),('177001'),('177001'),('177001'),('177001'),('177001'),('179007'),('179007'),('179012'),('179012'),('179012'),('179012'),('179012'),('179012'),('179013'),('179013'),('179013'),('179013'),('179013'),('179013'),('179042'),('179044'),('179045'),('180001'),('180013'),('180014'),('180014'),('180015'),('180017'),('180018'),('180020'),('180020'),('180021'),('180021'),('180027'),('180030'),('180033'),('180035'),('180036'),('180037'),('180038'),('180041'),('180042'),('180045'),('180045'),('180047'),('180048'),('180049'),('180050'),('180054'),('180060'),('180066'),('180067'),('180068'),('180070'),('182001'),('184001'),('184002'),('184005'),('184005'),('184005'),('184005'),('184006'),('184006'),('184006'),('184006'),('184008'),('184008'),('184008'),('184008'),('184009'),('184009'),('184009'),('184009'),('184010'),('184010'),('184010'),('184010'),('184011'),('184011'),('184011'),('184011'),('185001'),('185001'),('185001'),('185001'),('185001'),('185001'),('185001'),('185003'),('185003'),('185003'),('185003'),('185003'),('185003'),('185003'),('187001'),('191002'),('191002'),('192002'),('194003'),('197001'),('197001'),('197001'),('197001'),('197001'),('197001'),('197001'),('197002'),('197002'),('197002'),('197002'),('197002'),('197002'),('197002'),('197003'),('197003'),('197003'),('197003'),('197003'),('197003'),('197003'),('197004'),('197004'),('197004'),('197004'),('197004'),('197004'),('197004'),('197005'),('197005'),('197005'),('197005'),('197005'),('197005'),('197006'),('197006'),('197006'),('197006'),('197006'),('198001'),('198001'),('198001'),('198001'),('198001'),('198001'),('198003'),('198003'),('198003'),('198004'),('198004'),('198004'),('198004'),('198004'),('198004'),('198005'),('198005'),('198005'),('198005'),('198005'),('198005'),('198005'),('198006'),('198006'),('198006'),('198006'),('198006'),('198006'),('198007'),('198007'),('198007'),('198007'),('198007'),('198007'),('198007'),('198008'),('198008'),('198008'),('198008'),('198008'),('198008'),('198009'),('198009'),('198009'),('198009'),('198009'),('198009'),('198009'),('198010'),('198010'),('198010'),('198010'),('198010'),('198010'),('198011'),('198012'),('198012'),('198012'),('198012'),('198015'),('198015'),('198016'),('198016'),('198016'),('198016'),('198016'),('198016'),('198017'),('198017'),('198017'),('198017'),('198017'),('198017'),('201001'),('201001'),('201001'),('201001'),('201001'),('201002'),('202001'),('202001'),('203001'),('203001'),('203001'),('203001'),('203001'),('203001'),('203001'),('203002'),('203002'),('203002'),('203002'),('203003'),('203003'),('203003'),('203003'),('203003'),('203017'),('203017'),('203017'),('203017'),('203017'),('203017'),('203017'),('203017'),('203017'),('203018'),('203018'),('203018'),('203018'),('203018'),('203019'),('203019'),('203019'),('203019'),('203019'),('204001'),('204002'),('205001'),('205001'),('205001'),('205001'),('205001'),('205001'),('205001'),('208001'),('208001'),('208002'),('208002'),('208002'),('208003'),('208003'),('208003'),('208004'),('208004'),('208004'),('208004'),('208004'),('208004'),('208004'),('208005'),('208005'),('208005'),('208005'),('208005'),('209001'),('209001'),('209001'),('209001'),('209001'),('209002'),('209002'),('209002'),('209002'),('209002'),('209003'),('209003'),('209003'),('209003'),('209003'),('210001'),('210001'),('210001'),('210001'),('210001'),('210004'),('210004'),('210004'),('210004'),('210004'),('210004'),('210009'),('210010'),('212001'),('212001'),('212002'),('212002'),('212002'),('212002'),('212003'),('212003'),('212003'),('212004'),('212004'),('212004'),('212005'),('212005'),('212005'),('212005'),('212005'),('212006'),('212006'),('212006'),('212007'),('212007'),('212008'),('212008'),('212008'),('212008'),('212009'),('212009'),('212009'),('212009'),('212010'),('212010'),('212010'),('212010'),('212011'),('212011'),('212012'),('212012'),('212013'),('212013'),('212013'),('218001'),('218004'),('218009'),('218011'),('218011'),('218015'),('218020'),('218021'),('218021'),('218022'),('218022'),('218022'),('218023'),('218024'),('218025'),('218026'),('218026'),('218027'),('218028'),('218029'),('218029'),('218029'),('218030'),('218031'),('221001'),('221001'),('221001'),('221001'),('221001'),('221001'),('221002'),('221002'),('221002'),('221002'),('221002'),('221002'),('221003'),('221003'),('221003'),('221003'),('221003'),('221003'),('221004'),('221004'),('221004'),('221004'),('221004'),('221004'),('221005'),('221005'),('221005'),('221005'),('221005'),('221006'),('221006'),('221006'),('221006'),('221006'),('221007'),('221007'),('221007'),('221007'),('221007'),('221007'),('221008'),('221008'),('221008'),('221008'),('221008'),('221008'),('221009'),('221009'),('221009'),('221009'),('221009'),('221009'),('221010'),('221010'),('221010'),('221010'),('221011'),('221011'),('221011'),('221011'),('221012'),('221012'),('221012'),('221012'),('221012'),('221012'),('221013'),('221013'),('221013'),('221013'),('221013'),('221013'),('223003'),('223003'),('224001'),('224001'),('224002'),('224002'),('224003'),('224007'),('224008'),('225001'),('225002'),('225002'),('225002'),('225003'),('225003'),('225003'),('225003'),('225004'),('225004'),('225004'),('225005'),('225005'),('225005'),('225005'),('225005'),('225005'),('225006'),('225006'),('225006'),('225007'),('225007'),('225007'),('225008'),('225008'),('225008'),('225008'),('225008'),('225009'),('225009'),('225009'),('225010'),('225010'),('225010'),('225011'),('225011'),('225011'),('225011'),('225011'),('225012'),('225012'),('225012'),('225012'),('225012'),('225012'),('225013'),('225013'),('226001'),('226002'),('226003'),('226003'),('226005'),('226005'),('226006'),('226007'),('226007'),('226007'),('226007'),('227011'),('227015'),('227015'),('227041'),('227045'),('227052'),('227056'),('227063'),('227064'),('227066'),('227067'),('227069'),('227071'),('227073'),('227085'),('227116'),('227119'),('227131'),('227133'),('227147'),('229005'),('229005'),('229005'),('233003'),('233004'),('235001'),('235001'),('235002'),('235003'),('235003'),('235003'),('235004'),('235005'),('235005'),('235005'),('235005'),('235005'),('235005'),('235005'),('236001'),('236001'),('236001'),('236001'),('236002'),('236003'),('236003'),('236003'),('236003'),('236003'),('236003'),('238002'),('238002'),('238002'),('238002'),('238002'),('238002'),('238003'),('238003'),('238003'),('238003'),('238003'),('238003'),('238004'),('238004'),('238004'),('238004'),('238004'),('238005'),('238005'),('238005'),('238007'),('238007'),('238007'),('238007'),('238007'),('238007'),('238007'),('238008'),('238008'),('238008'),('238008'),('238008'),('238008'),('238008'),('334005'),('334006'),('337001'),('337001'),('337001'),('337002'),('337002'),('337003'),('337003'),('337003'),('337004'),('343001'),('343001'),('344001'),('344002'),('344003'),('344004'),('344005'),('344005'),('345001'),('345001'),('348001'),('348004'),('348005'),('348005'),('349001'),('349001'),('349002'),('349002'),('349002'),('350001'),('353002'),('353002'),('353002'),('353003'),('355001'),('355002'),('355005'),('355006'),('355006'),('356001'),('358001'),('358001'),('358001'),('359001'),('359001'),('359002'),('359002'),('359002'),('359002'),('360001'),('360001'),('360002'),('360002'),('360003'),('360003'),('360004'),('360004'),('360005'),('360005'),('360005'),('366001'),('366002'),('366002'),('366003'),('366004'),('369001'),('369001'),('373001'),('373002'),('373002'),('373003'),('373003'),('373005'),('373007'),('373008'),('373009'),('373009'),('373010'),('373010'),('373010'),('373011'),('373011'),('373011'),('373011'),('373012'),('373012'),('373012'),('373013'),('373013'),('373014'),('373014'),('373015'),('373015'),('373015'),('373015'),('373017'),('373017'),('373017'),('373017'),('373018'),('373021'),('374002'),('374004'),('374006'),('374007'),('374008'),('374009'),('374010'),('374011'),('374012'),('374015'),('374016'),('382001'),('382002'),('382002'),('384001'),('386001'),('386001'),('386001'),('386001'),('386001'),('386001'),('386001'),('386002'),('386002'),('386002'),('386002'),('386002'),('386002'),('386002'),('386003'),('386003'),('386003'),('386003'),('386003'),('386003'),('386003'),('386003'),('386003'),('386004'),('386004'),('386004'),('386004'),('386004'),('386004'),('386004'),('386004'),('386005'),('386005'),('386005'),('386005'),('386005'),('386005'),('386005'),('386006'),('386006'),('386006'),('386006'),('386006'),('386006'),('386007'),('386007'),('386007'),('386007'),('386007'),('386007'),('386007'),('386007'),('386007'),('386008'),('386008'),('386008'),('386008'),('386008'),('386008'),('386008'),('386008'),('386009'),('386009'),('386009'),('386010'),('386010'),('386010'),('386010'),('386010'),('386010'),('386010'),('386010'),('386011'),('386011'),('386011'),('386011'),('386011'),('386011'),('386011'),('386011'),('386011'),('386012'),('386012'),('386012'),('386012'),('386012'),('386012'),('386012'),('386012'),('386012'),('386013'),('386013'),('386013'),('386013'),('386013'),('386013'),('386013'),('386014'),('386014'),('386014'),('386014'),('389001'),('389002'),('389002'),('389003'),('389003'),('389003'),('389003'),('389004'),('389004'),('389004'),('389004'),('392001'),('393001'),('393002'),('393002'),('393003'),('393004'),('395001'),('395001'),('397001'),('397001'),('397001'),('397002'),('399001'),('399001'),('399001'),('399001'),('399001'),('399001'),('399001'),('399002'),('399002'),('399002'),('399002'),('399002'),('399002'),('399002'),('399003'),('400001'),('400001'),('400001'),('400001'),('400002'),('403002'),('504001'),('504001'),('504002'),('504002'),('504002'),('504004'),('504004'),('504005'),('504006'),('504007'),('504007'),('504007'),('504008'),('504008'),('504009'),('504009'),('504009'),('504009'),('504009'),('504010'),('504011'),('504011'),('504012'),('504012'),('504014'),('504014'),('504014'),('504014'),('504014'),('504014'),('504014'),('504014'),('504017'),('504017'),('504021'),('504021'),('504021'),('504021'),('504021'),('504021'),('504021'),('504022'),('504023'),('504023'),('504024'),('504024'),('504025'),('504025'),('506001'),('506001'),('506001'),('506001'),('506001'),('506001'),('506002'),('506002'),('506002'),('506002'),('506002'),('511001'),('511001'),('511001'),('511001'),('511001'),('511001'),('511001'),('511002'),('511002'),('511002'),('511002'),('511002'),('511002'),('511002'),('511003'),('511003'),('511003'),('511003'),('511003'),('511003'),('511004'),('511004'),('511004'),('511004'),('511004'),('511004'),('511004'),('511005'),('511005'),('511005'),('511005'),('511005'),('511005'),('511005'),('511006'),('511006'),('511006'),('511006'),('511006'),('511006'),('511006'),('511007'),('511007'),('511007'),('511007'),('511007'),('511008'),('511008'),('511008'),('511008'),('511008'),('511008'),('511009'),('511009'),('511009'),('511009'),('511009'),('511009'),('511010'),('511010'),('511010'),('511010'),('511010'),('511010'),('511011'),('511011'),('511011'),('511011'),('511011'),('511011'),('511012'),('511012'),('511012'),('511012'),('511012'),('511012'),('511012'),('511013'),('511013'),('511013'),('511013'),('511013'),('511013'),('511013'),('511014'),('511014'),('511014'),('511014'),('511014'),('511017'),('511018'),('511020'),('511021'),('511022'),('511024'),('511028'),('511029'),('511029'),('511029'),('511029'),('511029'),('511029'),('513001'),('513001'),('513001'),('513001'),('513001'),('513001'),('513001'),('513001'),('513002'),('513002'),('513002'),('513002'),('513002'),('513002'),('513003'),('513003'),('513003'),('513003'),('513003'),('513003'),('513003'),('513003'),('513004'),('513004'),('513004'),('515001'),('515001'),('515001'),('515001'),('515001'),('515002'),('515002'),('515003'),('515003'),('515007'),('515007'),('515008'),('515011'),('515011'),('515011'),('515011'),('515011'),('515011'),('515012'),('515012'),('515012'),('515012'),('515013'),('515013'),('515013'),('515013'),('515013'),('515014'),('515014'),('515014'),('515014'),('515014'),('515015'),('515015'),('515015'),('515015'),('515015'),('518001'),('518002'),('521001'),('521002'),('521002'),('521002'),('521003'),('521003'),('521003'),('521003'),('521004'),('521004'),('521004'),('521004'),('521005'),('521005'),('521005'),('521005'),('521006'),('521006'),('521006'),('521009'),('521010'),('521010'),('521010'),('521010'),('521011'),('521011'),('521011'),('521011'),('521012'),('521013'),('521013'),('521015'),('521016'),('521016'),('523001'),('523001'),('523001'),('523001'),('523001'),('523001'),('523001'),('523002'),('523002'),('523002'),('523002'),('523002'),('523002'),('523003'),('523003'),('523003'),('523003'),('523003'),('523003'),('523003'),('523004'),('523004'),('523004'),('523004'),('523004'),('523004'),('523005'),('523005'),('523005'),('523005'),('523005'),('523005'),('523005'),('523005'),('523006'),('523006'),('523006'),('523006'),('523006'),('523006'),('523006'),('523007'),('523007'),('523007'),('523007'),('523007'),('523007'),('523007'),('524001'),('700001'),('701001'),('701002'),('701003'),('702001'),('702002'),('702004'),('702005'),('704001'),('704004'),('705001'),('706001'),('706002'),('707001'),('707002'),('707003'),('708001'),('710001'),('710002'),('711001'),('711002'),('712001'),('714001'),('714002'),('715001'),('719001'),('719002'),('991002'),('991002'),('991002'),('991003'),('991003'),('991003'),('991003'),('991003'),('991003'),('991003'),('991004'),('991004'),('991004'),('991005'),('991005'),('991005'),('991006'),('991007'),('995001'),('995001'),('995001'),('995001'),('995001'),('995001'),('995001'),('995001'),('995001'),('995001'),('995001'),('996001'),('996001'),('996001'),('996001'),('996001'),('996001'),('996001'),('996001'),('996002'),('996002'),('996003'),('996003'),('996003'),('996003'),('996003'),('998001'),('998001'),('998001'),('998001'),('998001'),('998001'),('998001'),('998001'),('998001'),('998001'),('998002'),('998002'),('998002'),('998002'),('998002'),('998002'),('998002'),('998002'),('998002'),('998002'),('998003'),('998003'),('998003'),('998003'),('998003'),('998003'),('998003'),('998003'),('998004'),('998004'),('998005'),('998005'),('998006'),('998007'),('999001'),('999001'),('999001'),('999001'),('999001'),('999001'),('999001'),('999001'),('999001'),('999001'),('999001'),('999002'),('999002'),('011017'),('011017'),('011017'),('011017'),('011017'),('011017'),('011017'),('011018'),('011018'),('011018'),('011018'),('034001'),('034001'),('034002'),('034002'),('071010'),('071010'),('071010'),('519001'),('126013'),('126013'),('126013'),('126013'),('126013'),('184012'),('184012'),('184012'),('404001'),('405002'),('405002'),('405001'),('405003'),('405006'),('240011'),('240011'),('240011'),('240011'),('240011'),('240011'),('240010'),('240010'),('240010'),('240009'),('240009'),('240009'),('240009'),('240008'),('240008'),('240008'),('240007'),('240007'),('240007'),('240007'),('240007'),('240007'),('240005'),('240005'),('240005'),('240005'),('240005'),('240004'),('240004'),('240004'),('240004'),('240004'),('240003'),('240003'),('240003'),('240003'),('240002'),('240002'),('240002'),('240002'),('240002'),('240002'),('240002'),('240001'),('240001'),('240001'),('240001'),('240001'),('240012'),('240012'),('240012'),('240012'),('240012'),('240013'),('240014'),('240015'),('240015'),('240015'),('240015'),('240015'),('240015'),('240015'),('240015'),('240016'),('240016'),('240016'),('240016'),('240016'),('240016'),('240017'),('240017'),('240017'),('357001'),('357001'),('235006'),('235006'),('235007'),('235007'),('235007'),('235007'),('235007'),('056023'),('056023'),('056023'),('056023'),('056023'),('046015'),('019005'),('019005'),('126014'),('126014'),('126014'),('126014'),('126014'),('126014'),('241003'),('241003'),('241003'),('241003'),('241003'),('241003'),('241002'),('241002'),('241002'),('241002'),('241002'),('241002'),('241001'),('241001'),('241001'),('241001'),('241001'),('240020'),('240020'),('240020'),('240020'),('240020'),('240020'),('240019'),('240019'),('240019'),('242001'),('242002'),('242004'),('242005'),('242006'),('089002'),('089002'),('089002'),('089002'),('089002'),('089002'),('406001'),('406002'),('406003'),('406004'),('406004'),('243001'),('243005'),('243006'),('243007'),('243008'),('408001'),('408001'),('408001'),('408001'),('408001'),('366005'),('366005'),('016035'),('016035'),('016035'),('016035'),('077010'),('996004'),('996004'),('996004'),('996004'),('996004'),('996004'),('996004'),('996004'),('025064'),('025064'),('025064'),('025064'),('011019'),('011019'),('011019'),('011019'),('011019'),('115123'),('115123'),('504026'),('039007'),('039009'),('039008'),('039008'),('039010'),('039010'),('039011'),('039012'),('180072'),('240021'),('240021'),('240021'),('240021'),('240021'),('240021'),('240021'),('240023'),('240023'),('240023'),('240023'),('405008'),('405008'),('525002'),('410002'),('410002'),('410004'),('410005'),('410005'),('410006'),('410007'),('410007'),('410008'),('410009'),('410010'),('410011'),('410011'),('410012'),('410012'),('410013'),('410013'),('410014'),('410014'),('410016'),('410016'),('344006'),('240031'),('240031'),('240031'),('240031'),('240030'),('240030'),('240030'),('240030'),('240029'),('240029'),('240029'),('240029'),('240028'),('240028'),('240028'),('240028'),('240027'),('240027'),('240026'),('240026'),('240026'),('240025'),('240025'),('240025'),('240025'),('240024'),('240024'),('240034'),('240034'),('240034'),('240033'),('240033'),('240033'),('240032'),('240032'),('240032'),('240032'),('411001'),('411002'),('203020'),('069025'),('069025'),('069025'),('069025'),('069025'),('069025'),('244001'),('244001'),('244001'),('244001'),('244001'),('244001'),('244001'),('244001'),('244001'),('244001'),('244001'),('244002'),('244002'),('244002'),('244002'),('244002'),('244002'),('244002'),('244002'),('244002'),('244002'),('244002'),('244009'),('244009'),('244009'),('244009'),('244009'),('244009'),('244009'),('244009'),('244009'),('244009'),('244009'),('244008'),('244008'),('244008'),('244008'),('244008'),('244008'),('244008'),('244008'),('244008'),('244008'),('244008'),('244007'),('244007'),('244007'),('244007'),('244007'),('244007'),('244007'),('244007'),('244007'),('244007'),('244007'),('244006'),('244006'),('244006'),('244006'),('244006'),('244006'),('244006'),('244006'),('244006'),('244006'),('244006'),('244004'),('244004'),('244004'),('244004'),('244004'),('244004'),('244004'),('244004'),('244004'),('244004'),('244004'),('244003'),('244003'),('244003'),('244003'),('244003'),('244003'),('244003'),('244003'),('244003'),('244003'),('244003'),('244014'),('244014'),('244014'),('244014'),('244014'),('244014'),('244014'),('244014'),('244013'),('244013'),('244013'),('244013'),('244013'),('244013'),('244013'),('244013'),('244012'),('244012'),('244012'),('244012'),('244012'),('244012'),('244012'),('244012'),('244011'),('244011'),('244011'),('244011'),('244011'),('244011'),('244011'),('244011'),('244016'),('244016'),('244016'),('244016'),('244016'),('244016'),('244016'),('244016'),('244016'),('244016'),('244016'),('244017'),('244017'),('244017'),('244017'),('244017'),('244017'),('244017'),('244017'),('244017'),('240040'),('240037'),('405009'),('405009'),('405009'),('405010'),('405010'),('240043'),('240043'),('504028'),('504040'),('800001'),('410019'),('410019'),('410020'),('410020'),('410020'),('410021'),('410021'),('244018'),('244018'),('244018'),('244018'),('244018'),('244018'),('244018'),('244018'),('244018'),('244018'),('244018'),('244019'),('244019'),('244019'),('244019'),('244019'),('244019'),('244019'),('244019'),('244019'),('244019'),('244019'),('244020'),('244020'),('244020'),('244020'),('244020'),('244020'),('244020'),('244020'),('413001'),('344007'),('082045'),('082045'),('082045'),('082045'),('082045'),('010031'),('010031'),('010031'),('010031'),('010032'),('010032'),('010032'),('010032'),('010033'),('010033'),('010033'),('010033'),('010033'),('010034'),('010034'),('010034'),('010034'),('010035'),('010035'),('010035'),('010035'),('504044'),('515016'),('515016'),('515016'),('515016'),('801002'),('801003'),('801004'),('801005'),('802001'),('801001'),('414001'),('141029'),('803001'),('803002'),('803004'),('803005'),('803006'),('803007'),('803008'),('803009'),('803013'),('803014'),('803015'),('803016'),('803017'),('410022'),('410023'),('410023'),('803019'),('415002'),('415001'),('244021'),('244021'),('244021'),('244021'),('244021'),('244021'),('244021'),('011020'),('011020'),('011020'),('011020'),('011023'),('011023'),('011023'),('011023'),('011022'),('011022'),('011022'),('011022'),('011022'),('011022'),('011021'),('011021'),('011021'),('011021'),('025065'),('025065'),('025065'),('025065'),('165037'),('165037'),('165038'),('165038'),('165038'),('165039'),('416001'),('416001'),('416001'),('416001'),('416001'),('416002'),('416003'),('417001'),('418001'),('504045'),('504045'),('504045'),('803022'),('240022'),('240022'),('240022'),('240022'),('420001'),('420001'),('420001'),('420001'),('804010'),('804005'),('804002'),('804018'),('804013'),('511019'),('511016'),('511015'),('511032'),('511031'),('511030'),('511027'),('511026'),('511025'),('511033'),('511023'),('133034'),('133034'),('133034'),('133033'),('169011'),('169011'),('169011'),('169011'),('169011'),('344008'),('244022'),('244022'),('244022'),('244022'),('244022'),('244022'),('244022'),('244026'),('244026'),('244026'),('244026'),('244026'),('244026'),('244025'),('244025'),('244025'),('244025'),('244025'),('244025'),('244025'),('244025'),('244030'),('244030'),('244030'),('244030'),('244030'),('244030'),('244030'),('244030'),('244023'),('244023'),('244023'),('244023'),('244023'),('244023'),('244024'),('244024'),('244024'),('244024'),('244024'),('244024'),('244024'),('244024'),('244027'),('244027'),('244027'),('244027'),('244027'),('244027'),('244027'),('244027'),('244028'),('244028'),('244028'),('244028'),('244028'),('244028'),('244028'),('244028'),('244029'),('244029'),('244029'),('244029'),('244029'),('244029'),('244029'),('244029'),('244031'),('244031'),('244031'),('244031'),('244031'),('244031'),('244031'),('244031'),('082046'),('082046'),('082046'),('082046'),('082047'),('082047'),('082048'),('082048'),('126015'),('126015'),('126016'),('126016'),('126016'),('126016'),('126016'),('416005'),('421001'),('421001'),('421002'),('016037'),('016037'),('016037'),('016037'),('016036'),('016036'),('016036'),('016036'),('115124'),('115124'),('115126'),('240049'),('240049'),('240048'),('240048'),('240047'),('240047'),('240046'),('240046'),('240045'),('240044'),('244032'),('244033'),('422002'),('422004'),('422004'),('422004'),('422005'),('422005'),('184013'),('184013'),('184013'),('805001'),('805002'),('805003'),('805004'),('805005'),('056024'),('056024'),('056024'),('423001'),('344010'),('235009'),('235009'),('235009'),('235009'),('212014'),('212014'),('056025'),('056025'),('056025'),('056026'),('056026'),('056026'),('056026'),('056026'),('056026'),('244034'),('244034'),('244034'),('244034'),('244034'),('244034'),('244035'),('244035'),('244035'),('244035'),('244035'),('244035'),('244035'),('244036'),('244036'),('244036'),('244036'),('244036'),('244036'),('244036'),('244037'),('244037'),('244037'),('244037'),('244037'),('244037'),('244037'),('244038'),('244038'),('244038'),('244038'),('244038'),('244038'),('244038'),('244039'),('244039'),('244039'),('244039'),('244039'),('244039'),('244039'),('203015'),('245002'),('245002'),('245001'),('245001'),('056029'),('056030'),('056032'),('424001'),('056034'),('056034'),('056034'),('056034'),('056033'),('056033'),('056033'),('805006'),('805007'),('805008'),('805009'),('805010'),('422008'),('422008'),('422007'),('422007'),('422006'),('422006'),('422010'),('422009'),('422009'),('422011'),('422011'),('209004'),('209004'),('150022'),('100002'),('056035'),('056035'),('056035'),('023036'),('023036'),('185005'),('246001'),('246001'),('247001'),('247001'),('247001'),('247001'),('247001'),('247001'),('247001'),('247002'),('247002'),('425001'),('416006'),('416006'),('165042'),('165041'),('165040'),('165043'),('010040'),('010039'),('010038'),('010036'),('248001'),('248002'),('248003'),('248004'),('248005'),('249001'),('249003'),('249004'),('249005'),('250007'),('250001'),('250002'),('250003'),('250004'),('250005'),('250006'),('250008'),('250009'),('250010'),('250011'),('250012'),('250013'),('251001'),('251002'),('806001'),('806002'),('235010'),('243009'),('249007'),('249008'),('249009'),('011024'),('011025'),('429001'),('429001'),('429002'),('429002'),('429003'),('429003');
select field from t1 group by field;
drop table t1;

#
# test of problem with date fields
#
create table t1 (a char(16), b date, c datetime);
insert into t1 SET a='test 2000-01-01', b='2000-01-01', c='2000-01-01';
select * from t1 where c = '2000-01-01';
select * from t1 where b = '2000-01-01';
drop table t1;

#
# Test of delete when the delete will cause a node to disappear and reappear
# (This assumes a block size of 1024)
#

create table t1 (a bigint not null, primary key (a,a,a,a,a,a,a,a,a,a));
insert into t1 values (2),(4),(6),(8),(10),(12),(14),(16),(18),(20),(22),(24),(26),(23);
delete from t1 where a=26;
drop table t1;
create table t1 (a bigint not null, primary key (a,a,a,a,a,a,a,a,a,a));
insert into t1 values (2),(4),(6),(8),(10),(12),(14),(16),(18),(20),(22),(24),(26),(23),(27);
delete from t1 where a=27;
drop table t1;

#
# Test of ORDER BY (By found by Dean Edmonds)
#

create table t1 (ID int not null primary key, TransactionID int not null);
insert into t1 (ID, TransactionID) values  (1,  87), (2,  89), (3,  92), (4,  94), (5,  486), (6,  490), (7,  753), (9,  828), (10, 832), (11, 834), (12, 840);
create table t2 (ID int not null primary key, GroupID int not null);
 insert into t2 (ID, GroupID) values (87,  87), (89,  89), (92,  92), (94,  94), (486, 486), (490, 490),(753, 753), (828, 828), (832, 832), (834, 834), (840, 840);
create table t3 (ID int not null primary key, DateOfAction date not null);
insert into t3 (ID, DateOfAction) values  (87,  '1999-07-19'), (89,  '1999-07-19'), (92,  '1999-07-19'), (94,  '1999-07-19'), (486, '1999-07-18'), (490, '2000-03-27'), (753, '2000-03-28'), (828, '1999-07-27'), (832, '1999-07-27'),(834, '1999-07-27'), (840, '1999-07-27');
select t3.DateOfAction, t1.TransactionID from t1 join t2 join t3 where t2.ID = t1.TransactionID and t3.ID = t2.GroupID order by t3.DateOfAction, t1.TransactionID; 
select t3.DateOfAction, t1.TransactionID from t1 join t2 join t3 where t2.ID = t1.TransactionID and t3.ID = t2.GroupID order by t1.TransactionID,t3.DateOfAction; 
drop table t1,t2,t3;

#
# Test of found bug in group on text key
#

CREATE TABLE t1 (
       f1 int(11) DEFAULT '0' NOT NULL,
       f2 varchar(16) DEFAULT '' NOT NULL,
       f5 text,
       KEY index_name (f1,f2,f5(16))
    );
INSERT INTO t1 VALUES (0,'traktor','1111111111111');
INSERT INTO t1 VALUES (1,'traktor','1111111111111111111111111');
select count(*) from t1 where f2='traktor';
drop table t1;

#
# Test of lock tables
#

create table t1 ( n int auto_increment primary key);
lock tables t1 write;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 read, t1 as t2 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 write, t1 as t2 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 write, t1 as t2 read, t1 as t3 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 write, t1 as t2 write;
insert into t1 values(NULL);
unlock tables;
check table t1;
drop table t1;

#
# Test of procedure
#

create table t1 (i int, j int);
insert into t1 values (1,2), (3,4), (5,6), (7,8);
select * from t1 procedure analyse();
create table t2 select * from t1 procedure analyse();
select * from t2;
drop table t1,t2;

#
# Bug when using comparions of strings and integers.
#

CREATE TABLE t1 (id CHAR(12) not null, PRIMARY KEY (id));
insert into t1 values ('000000000001'),('000000000002');
explain select * from t1 where id=000000000001;
select * from t1 where id=000000000001;
delete from t1 where id=000000000002;
select * from t1;
drop table t1;

#
# Test of DATE_ADD
#

CREATE TABLE t1 (
  visitor_id int(10) unsigned DEFAULT '0' NOT NULL,
  group_id int(10) unsigned DEFAULT '0' NOT NULL,
  hits int(10) unsigned DEFAULT '0' NOT NULL,
  sessions int(10) unsigned DEFAULT '0' NOT NULL,
  ts timestamp(14),
  PRIMARY KEY (visitor_id,group_id)
)/*! type=MyISAM */;
INSERT INTO t1 VALUES (465931136,7,2,2,20000318160952);
INSERT INTO t1 VALUES (173865424,2,2,2,20000318233615);
INSERT INTO t1 VALUES (173865424,8,2,2,20000318233615);
INSERT INTO t1 VALUES (173865424,39,2,2,20000318233615);
INSERT INTO t1 VALUES (173865424,7,2,2,20000318233615);
INSERT INTO t1 VALUES (173865424,3,2,2,20000318233615);
INSERT INTO t1 VALUES (173865424,6,2,2,20000318233615);
INSERT INTO t1 VALUES (173865424,60,2,2,20000318233615);
INSERT INTO t1 VALUES (173865424,1502,2,2,20000318233615);
INSERT INTO t1 VALUES (48985536,2,2,2,20000319013932);
INSERT INTO t1 VALUES (48985536,8,2,2,20000319013932);
INSERT INTO t1 VALUES (48985536,39,2,2,20000319013932);
INSERT INTO t1 VALUES (48985536,7,2,2,20000319013932);
INSERT INTO t1 VALUES (465931136,3,2,2,20000318160951);
INSERT INTO t1 VALUES (465931136,119,1,1,20000318160953);
INSERT INTO t1 VALUES (465931136,2,1,1,20000318160950);
INSERT INTO t1 VALUES (465931136,8,1,1,20000318160950);
INSERT INTO t1 VALUES (465931136,39,1,1,20000318160950);
INSERT INTO t1 VALUES (1092858576,14,1,1,20000319013445);
INSERT INTO t1 VALUES (357917728,3,2,2,20000319145026);
INSERT INTO t1 VALUES (357917728,7,2,2,20000319145027);
select visitor_id,max(ts) as mts from t1 group by visitor_id
having mts < DATE_SUB(NOW(),INTERVAL 3 MONTH);
select visitor_id,max(ts) as mts from t1 group by visitor_id
having DATE_ADD(mts,INTERVAL 3 MONTH) < NOW();
drop table t1;

#
# Test of rename table
#

create table t0 SELECT 1,"table 1";
create table t2 SELECT 2,"table 2";
create table t3 SELECT 3,"table 3";
rename table t0 to t1;
rename table t3 to t4, t2 to t3, t1 to t2, t4 to t1;
select * from t1;
rename table t3 to t4, t2 to t3, t1 to t2, t4 to t1;
rename table t3 to t4, t2 to t3, t1 to t2, t4 to t1;
select * from t1;
# The following should give errors;
rename table t1 to t2;
rename table t1 to t1;
rename table t3 to t4, t2 to t3, t1 to t2, t4 to t2;
show tables like "t_";
rename table t3 to t1, t2 to t3, t1 to t2, t4 to t1;
rename table t3 to t4, t5 to t3, t1 to t2, t4 to t1;

select * from t1;
select * from t2;
select * from t3;
drop table if exists t1,t2,t3,t4;

#
# Test bug with long primary key
#

create table t1
(
   SEQNO                         numeric(12 ) not null,
   MOTYPEID                 numeric(12 ) not null,
   MOINSTANCEID     numeric(12 ) not null,
   ATTRID                       numeric(12 ) not null,
   VALUE                         varchar(120) not null,
   primary key (SEQNO, MOTYPEID, MOINSTANCEID, ATTRID, VALUE )
);
INSERT INTO t1 VALUES (1, 1, 1, 1, 'a'); 
INSERT INTO t1 VALUES (1, 1, 1, 1, 'b'); 
INSERT INTO t1 VALUES (1, 1, 1, 1, 'a');  # This should give an error
drop table t1;

#
# Test of database indirection
#

use kf96;

create table test.t1 (a int);
insert into test.t1 values(1);
update test.t1 set a=2 where a=1;
delete from test.t1;
use test;
alter table t1 add b int, rename ok.t2;
lock table ok.t2 WRITE;
alter table ok.t2 rename ok.t3;
lock table ok.t3 WRITE;
alter table ok.t3 rename test.t4;
drop table t4;
UNLOCK TABLES;

select count(ok.station.stationsnr) from ok.station;

#
# Test create and destroy of databases
#

create database bench_test;
create table bench_test.test (a int);
drop database bench_test;
create database bench_test;
create table bench_test.test (a int) raid_type=1;
drop database bench_test;

#
# These should give errors:
#
create table gurgel.test (a int);
select count(ok.station.stationsnr) from station;
select count(station.stationsnr) from test.station;
select count(gurgel.station.stationsnr) from ok.station;
select count(gurgel.station.stationsnr) from gurgel.station;
select 1 from ok.station order by 2;
select 1 from ok.station group by 2;
select 1 from ok.station order by ok.station.gurgel;
select count(*),stationsnr from ok.station;
create table `a/a` (a int);
create table `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa` (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa int);
create table a (`aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa` int);

#
# Test of REPLACE with ISAM and MyISAM
#

CREATE TABLE t1 (
  gesuchnr int(11) DEFAULT '0' NOT NULL,
  benutzer_id int(11) DEFAULT '0' NOT NULL,
  TitelGesuch varchar(60) DEFAULT '' NOT NULL,
  text text DEFAULT '' NOT NULL,
  seconds int(11) DEFAULT '0' NOT NULL,
  neu char(1) DEFAULT '' NOT NULL,
  eingang date DEFAULT '0000-00-00' NOT NULL,
  bewerber_id int(10) unsigned DEFAULT '0' NOT NULL,
  server_id tinyint(4) DEFAULT '0' NOT NULL,
  deleted char(1) DEFAULT '' NOT NULL,
  KEY berwerber_id (bewerber_id),
  PRIMARY KEY (gesuchnr,benutzer_id),
  KEY benutzer_id (benutzer_id)
) type=ISAM;

replace into t1 (gesuchnr,benutzer_id) values (1,1);
replace into t1 (gesuchnr,benutzer_id) values (1,1);
alter table t1 type=myisam;
replace into t1 (gesuchnr,benutzer_id) values (1,1);
drop table t1;

#
# test of MERGE
#
create table t1 (a int not null primary key auto_increment, message char(20));
create table t2 (a int not null primary key auto_increment, message char(20));
INSERT INTO t1 (message) VALUES ("Testing"),("table"),("t1");
INSERT INTO t2 (message) VALUES ("Testing"),("table"),("t2");
create table t3 (a int not null, b char(20), key(a)) type=MERGE UNION=(t1,t2);
select * from t3;
select * from t3 order by a desc;
drop table t3;
insert into t1 select NULL,message from t2;
insert into t2 select NULL,message from t1;
insert into t1 select NULL,message from t2;
insert into t2 select NULL,message from t1;
insert into t1 select NULL,message from t2;
insert into t2 select NULL,message from t1;
insert into t1 select NULL,message from t2;
insert into t2 select NULL,message from t1;
insert into t1 select NULL,message from t2;
insert into t2 select NULL,message from t1;
insert into t1 select NULL,message from t2;
create table t3 (a int not null, b char(20), key(a)) type=MERGE UNION=(t1,t2);
explain select * from t3 where a < 10;
explain select * from t3 where a > 10 and a < 20;
select * from t3 where a = 10;
select * from t3 where a < 10;
select * from t3 where a > 10 and a < 20;
explain select a from t3 order by a desc limit 10;
select a from t3 order by a desc limit 10;
select a from t3 order by a desc limit 300,10;

# The following should give errors
create table t4 (a int not null, b char(10), key(a)) type=MERGE UNION=(t1,t2);

drop table if exists t1,t2,t3,t4;

create table t1 (c char(10)) type=myisam;
create table t2 (c char(10)) type=myisam;
create table t3 (c char(10)) union=(t1,t2) type=merge;
insert into t1 (c) values ('test1');
insert into t1 (c) values ('test1');
insert into t1 (c) values ('test1');
insert into t2 (c) values ('test2');
insert into t2 (c) values ('test2');
insert into t2 (c) values ('test2');
select * from t3;
flush tables;
select * from t3;
delete from t3;
select * from t3;
select * from t1;
drop table t3,t2,t1;

#
# Test of fulltext
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b));
INSERT INTO t1 VALUES('MySQL has now support', 'for full-text search'),('Full-text indexes', 'are called collections'),('Only MyISAM tables','support collections'),('Function MATCH ... AGAINST()','is used to do a search'),('Full-text search in MySQL', 'implements vector space model');
select * from t1 where MATCH(a,b) AGAINST ("collections");
select * from t1 where MATCH(a,b) AGAINST ("indexes");
select * from t1 where MATCH(a,b) AGAINST ("indexes collections");
delete from t1 where a like "MySQL%";
drop table t1;

#
# Test of Berkeley DB
#

CREATE TABLE t1 (
  id int(11) NOT NULL auto_increment,
  parent_id int(11) DEFAULT '0' NOT NULL,
  level tinyint(4) DEFAULT '0' NOT NULL,
  PRIMARY KEY (id),
  KEY parent_id (parent_id),
  KEY level (level)
) type=bdb;
INSERT INTO t1 VALUES (1,0,0),(3,1,1),(4,1,1),(8,2,2),(9,2,2),(17,3,2),(22,4,2),(24,4,2),(28,5,2),(29,5,2),(30,5,2),(31,6,2),(32,6,2),(33,6,2),(203,7,2),(202,7,2),(20,3,2),(157,0,0),(193,5,2),(40,7,2),(2,1,1),(15,2,2),(6,1,1),(34,6,2),(35,6,2),(16,3,2),(7,1,1),(36,7,2),(18,3,2),(26,5,2),(27,5,2),(183,4,2),(38,7,2),(25,5,2),(37,7,2),(21,4,2),(19,3,2),(5,1,1),(179,5,2);
update t1 set parent_id=parent_id+100;
select * from t1 where parent_id=102;
update t1 set id=id+1000;
update t1 set id=1024 where id=1009; 
select * from t1;
update ignore t1 set id=id+1; # This will change all rows
select * from t1;
update ignore t1 set id=1023 where id=1010;
select * from t1 where parent_id=102;
drop table t1;

create table t1 (n int not null primary key) type=bdb;
set autocommit=0;
insert into t1 values (4);
select n, "before rollback" from t1;
rollback;
select n, "after rollback" from t1;
insert into t1 values (4);
select n, "before commit" from t1;
commit;
select n, "after commit" from t1;
commit;
drop table t1;
set autocommit=1;

CREATE TABLE t1 (ID INTEGER NOT NULL PRIMARY KEY, NAME VARCHAR(64)) TYPE=BDB;
INSERT INTO t1 VALUES (1, 'Jochen');
select * from t1;
drop table t1;

CREATE TABLE t1 ( _userid VARCHAR(60) NOT NULL PRIMARY KEY) TYPE=BDB;
set autocommit=0;
INSERT INTO t1  SET _userid='marc@anyware.co.uk';
COMMIT;
SELECT * FROM t1;
SELECT _userid FROM t1 WHERE _userid='marc@anyware.co.uk';
drop table t1;
set autocommit=1;

CREATE TABLE t1 (id char(8) not null primary key, val int not null) type=bdb;
insert into t1 values ('pippo', 12);
insert into t1 values ('pippo', 12); # Gives error
delete from t1;
delete from t1 where id = 'pippo';
select * from t1;

insert into t1 values ('pippo', 12);
set autocommit=0;
delete from t1;
rollback;
select * from t1;
delete from t1;
commit;
select * from t1;
drop table t1;
set autocommit=1;

#
# Test when reading on part of unique key
#
CREATE TABLE t1 (
  user_id int(10) DEFAULT '0' NOT NULL,
  name varchar(100),
  phone varchar(100),
  ref_email varchar(100) DEFAULT '' NOT NULL,
  detail varchar(200),
  PRIMARY KEY (user_id,ref_email)
)type=bdb;

INSERT INTO t1 VALUES (10292,'sanjeev','29153373','sansh777@hotmail.com','xxx'),(10292,'shirish','2333604','shirish@yahoo.com','ddsds'),(10292,'sonali','323232','sonali@bolly.com','filmstar');
select * from t1 where user_id=10292;
INSERT INTO t1 VALUES (10291,'sanjeev','29153373','sansh777@hotmail.com','xxx'),(10293,'shirish','2333604','shirish@yahoo.com','ddsds');
select * from t1 where user_id=10292;
select * from t1 where user_id>=10292;
select * from t1 where user_id>10292;
select * from t1 where user_id<10292;
drop table t1;

#
# Test of ALTER TABLE and BDB tables
#

create table t1 (col1 int not null, col2 char(4) not null, primary key(col1));
alter table t1 type=BDB;
insert into t1 values ('1','1'),('5','2'),('2','3'),('3','4'),('4','4');
select * from t1;
update t1 set col2='7' where col1='4';
select * from t1;
alter table t1 add co3 int not null;
select * from t1;
update t1 set col2='9' where col1='2';
select * from t1;
drop table t1;

#
# INSERT INTO BDB tables
#

create table t1 (a int not null , b int, primary key (a)) type = BDB;
create table t2 (a int not null , b int, primary key (a)) type = myisam;
insert into t1 VALUES (1,3) , (2,3), (3,3);
select * from t1;
insert into t2 select * from t1;
select * from t2;
delete from t1 where b = 3;
select * from t1;
insert into t1 select * from t2;
select * from t1;
select * from t2;
drop table t1,t2;

#
# Search on unique key
#

CREATE TABLE t1 (
  id int(11) NOT NULL auto_increment,
  ggid varchar(32) binary DEFAULT '' NOT NULL,
  email varchar(64) DEFAULT '' NOT NULL,
  passwd varchar(32) binary DEFAULT '' NOT NULL,
  PRIMARY KEY (id),
  UNIQUE ggid (ggid)
) TYPE=BDB;

insert into t1 (ggid,passwd) values ('test1','xxx');
insert into t1 (ggid,passwd) values ('test2','yyy');

select * from t1 where ggid='test1';
select * from t1 where passwd='xxx';
select * from t1 where id=2;
drop table t1;

#
# ORDER BY on not primary key
#

CREATE TABLE t1 (
  user_name varchar(12),
  password text,
  subscribed char(1),
  user_id int(11) DEFAULT '0' NOT NULL,
  quota bigint(20),
  weight double,
  access_date date,
  access_time time,
  approved datetime,
  dummy_primary_key int(11) NOT NULL auto_increment,
  PRIMARY KEY (dummy_primary_key)
) TYPE=BDB;
INSERT INTO t1 VALUES ('user_0','somepassword','N',0,0,0,'2000-09-07','23:06:59','2000-09-07 23:06:59',1);
INSERT INTO t1 VALUES ('user_1','somepassword','Y',1,1,1,'2000-09-07','23:06:59','2000-09-07 23:06:59',2);
INSERT INTO t1 VALUES ('user_2','somepassword','N',2,2,1.4142135623731,'2000-09-07','23:06:59','2000-09-07 23:06:59',3);
INSERT INTO t1 VALUES ('user_3','somepassword','Y',3,3,1.7320508075689,'2000-09-07','23:06:59','2000-09-07 23:06:59',4);
INSERT INTO t1 VALUES ('user_4','somepassword','N',4,4,2,'2000-09-07','23:06:59','2000-09-07 23:06:59',5);
select  user_name, password , subscribed, user_id, quota, weight, access_date, access_time, approved, dummy_primary_key from t1 order by user_name;
drop table t1;
