echo "Test for SQL vs JSON";
echo "Comparator >";
echo "";
echo "Testcase for Tinyint";
#====================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._tin as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._tin as side1,
b.col as side2,
JSON_TYPE(CAST(a._tin as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._tin as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._tin >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._tin as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_tin a , jj b
where a._tin is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._tin as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._tin >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._tin as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));


echo "Testcase for Boolean";
#=====================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._boo as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._boo as side1,
b.col as side2,
JSON_TYPE(CAST(a._boo as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._boo as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._boo >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._boo as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_boo a , jj b
where a._boo is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._boo as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._boo >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._boo as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for small Int Signed";
#==============================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._sms as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._sms as side1,
b.col as side2,
JSON_TYPE(CAST(a._sms as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._sms as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._sms >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._sms as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_sms a , jj b
where a._sms is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._sms as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._sms >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._sms as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));



echo "Testcase for Signed Medium Int";
#===============================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mes as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._mes as side1,
b.col as side2,
JSON_TYPE(CAST(a._mes as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mes as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._mes >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mes as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_mes a , jj b
where a._mes is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._mes as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._mes >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mes as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));


echo "Testcase for unsigned Medium Int";
#==================================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._meu as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._meu as side1,
b.col as side2,
JSON_TYPE(CAST(a._meu as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._meu as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._meu >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._meu as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_meu a , jj b
where a._meu is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._meu as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._meu >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._meu as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for signed Int";
#========================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ins as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._ins as side1,
b.col as side2,
JSON_TYPE(CAST(a._ins as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ins as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._ins >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ins as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_ins a , jj b
where a._ins is not NULL
and b.col is not NULL
and ((a._ins >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ins as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Unsigned Int";
#========================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._inu as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._inu as side1,
b.col as side2,
JSON_TYPE(CAST(a._inu as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._inu as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._inu >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._inu as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_inu a , jj b
where a._inu is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._inu as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._inu >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._inu as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));


echo "Testcase for Big Int";
#========================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._bis as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._bis as side1,
b.col as side2,
JSON_TYPE(CAST(a._bis as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._bis as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._bis >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._bis as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_bis a , jj b
where a._bis is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._bis as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._bis >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._bis as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Big Int Unsigned";
#==============================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._biu as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._biu as side1,
b.col as side2,
JSON_TYPE(CAST(a._biu as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._biu as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._biu >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._biu as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_biu a , jj b
where a._biu is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._biu as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._biu >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._biu as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Decimal";
#=====================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._dec as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._dec as side1,
b.col as side2,
JSON_TYPE(CAST(a._dec as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._dec as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._dec >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._dec as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_dec a , jj b
where a._dec is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._dec as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._dec >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._dec as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Double";
#=====================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._dou as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._dou as side1,
b.col as side2,
JSON_TYPE(CAST(a._dou as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._dou as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._dou >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._dou as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_dou a , jj b
where a._dou is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._dou as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._dou >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._dou as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for CHAR";
#===================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._chr as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._chr as side1,
b.col as side2,
JSON_TYPE(CAST(a._chr as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._chr as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._chr >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._chr as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_chr a , jj b
where a._chr is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._chr as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._chr >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._chr as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for VARCHAR";
#=====================

select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._vch as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._vch as side1,
b.col as side2,
JSON_TYPE(CAST(a._vch as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._vch as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._vch >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._vch as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_vch a , jj b
where a._vch is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._vch as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._vch >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._vch as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Binary(255)";
#==========================

select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._bin as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._bin as side1,
b.col as side2,
JSON_TYPE(CAST(a._bin as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._bin as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._bin >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._bin as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_bin a , jj b
where a._bin is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._bin as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._bin >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._bin as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));


echo "Testcase for Variable Binary";
#=============================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._vbn as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._vbn as side1,
b.col as side2,
JSON_TYPE(CAST(a._vbn as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._vbn as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._vbn >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._vbn as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_vbn a , jj b
where a._vbn is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._vbn as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._vbn >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._vbn as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for TinyBlob";
#======================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._tbl as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._tbl as side1,
b.col as side2,
JSON_TYPE(CAST(a._tbl as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._tbl as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._tbl >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._tbl as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_tbl a , jj b
where a._tbl is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._tbl as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._tbl >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._tbl as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for TinyText";
#======================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ttx as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._ttx as side1,
b.col as side2,
JSON_TYPE(CAST(a._ttx as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ttx as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._ttx >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ttx as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_ttx a , jj b
where a._ttx is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._ttx as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._ttx >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ttx as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Blob";
#======================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._blb as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._blb as side1,
b.col as side2,
JSON_TYPE(CAST(a._blb as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._blb as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._blb >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._blb as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_blb a , jj b
where a._blb is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._blb as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._blb >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._blb as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));


echo "Testcase for Text";
#======================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._txt as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._txt as side1,
b.col as side2,
JSON_TYPE(CAST(a._txt as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._txt as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._txt >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._txt as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_txt a , jj b
where a._txt is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._txt as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._txt >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._txt as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Medium Blob";
#=========================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mbb as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._mbb as side1,
b.col as side2,
JSON_TYPE(CAST(a._mbb as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mbb as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._mbb >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mbb as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_mbb a , jj b
where a._mbb is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._mbb as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._mbb >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mbb as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Medium Text";
#=========================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mtx as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._mtx as side1,
b.col as side2,
JSON_TYPE(CAST(a._mtx as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mtx as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._mtx >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mtx as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_mtx a , jj b
where a._mtx is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._mtx as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._mtx >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._mtx as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Long Blob";
#=========================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._lbb as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._lbb as side1,
b.col as side2,
JSON_TYPE(CAST(a._lbb as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._lbb as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._lbb >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._lbb as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_lbb a , jj b
where a._lbb is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._lbb as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._lbb >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._lbb as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Long Text";
#=========================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ltx as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._ltx as side1,
b.col as side2,
JSON_TYPE(CAST(a._ltx as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ltx as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._ltx >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ltx as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_ltx a , jj b
where a._ltx is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._ltx as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._ltx >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._ltx as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Enum";
#==================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._enu as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._enu as side1,
b.col as side2,
JSON_TYPE(CAST(a._enu as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._enu as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._enu >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._enu as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_enu a , jj b
where a._enu is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._enu as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._enu >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._enu as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));

echo "Testcase for Set";
#==================
select
IF(GET_JSON_WEIGHT(JSON_TYPE(CAST(a._set as JSON)))=GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))),'2nd Level','1st Level') validation_stage,
a._set as side1,
b.col as side2,
JSON_TYPE(CAST(a._set as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._set as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a._set >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._set as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from t_set a , jj b
where a._set is not NULL
and b.col is not NULL
and JSON_TYPE(CAST(a._set as JSON))!='BLOB'
and JSON_TYPE(CAST(b.col as JSON))!='BLOB'
and ((a._set >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a._set as JSON)))
>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
));
