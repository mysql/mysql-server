# These are tests for JSON vs JSON datatypes.
# Comparator >
#-----------------------
echo "Test for JSON vs JSON";
echo "";
echo "";

--disable_query_log
select 
a.col as side1,  
b.col as side2,
JSON_TYPE(CAST(a.col as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a.col >  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) >
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from jj a , jj b
where a.col is not NULL 
and b.col is not NULL
and ((a.col >  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) 
> 
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
))AND JSON_TYPE(CAST(a.col as JSON)) !='BLOB'
AND JSON_TYPE(CAST(b.col as JSON)) !='BLOB'
AND GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) != GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)));

echo "Comparator <";
echo "==============";
# Comparator <
#-----------------------

select 
a.col as side1,  
b.col as side2,
JSON_TYPE(CAST(a.col as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a.col <  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) <
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from jj a , jj b
where a.col is not NULL 
and b.col is not NULL
and ((a.col <  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) 
< 
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
))AND JSON_TYPE(CAST(a.col as JSON)) !='BLOB'
AND JSON_TYPE(CAST(b.col as JSON)) !='BLOB'
AND GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) != GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)));

echo "Comparator >=";
echo "==============";
# Comparator >=
#-----------------------

select 
a.col as side1,  
b.col as side2,
JSON_TYPE(CAST(a.col as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a.col >=  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) >=
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from jj a , jj b
where a.col is not NULL 
and b.col is not NULL
and ((a.col >=  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) 
>= 
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
))AND JSON_TYPE(CAST(a.col as JSON)) !='BLOB'
AND JSON_TYPE(CAST(b.col as JSON)) !='BLOB'
AND GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) != GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)));

echo "Comparator <=";
echo "==============";
# Comparator <=
#-----------------------

select 
a.col as side1,  
b.col as side2,
JSON_TYPE(CAST(a.col as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a.col <=  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) <=
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from jj a , jj b
where a.col is not NULL 
and b.col is not NULL
and ((a.col <=  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) 
<= 
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
))AND JSON_TYPE(CAST(a.col as JSON)) !='BLOB'
AND JSON_TYPE(CAST(b.col as JSON)) !='BLOB'
AND GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) != GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)));

echo "Comparator =";
echo "==============";
# Comparator =
#-----------------------

select 
a.col as side1,  
b.col as side2,
JSON_TYPE(CAST(a.col as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a.col =  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) =
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from jj a , jj b
where a.col is not NULL 
and b.col is not NULL
and ((a.col =  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) 
= 
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
))AND JSON_TYPE(CAST(a.col as JSON)) !='BLOB'
AND JSON_TYPE(CAST(b.col as JSON)) !='BLOB'
AND GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) != GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)));

echo "Comparator <>";
echo "==============";
# Comparator <>
#-----------------------
select 
a.col as side1,  
b.col as side2,
JSON_TYPE(CAST(a.col as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a.col <>  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) <>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from jj a , jj b
where a.col is not NULL 
and b.col is not NULL
and ((a.col <>  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) 
<> 
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
))AND JSON_TYPE(CAST(a.col as JSON)) !='BLOB'
AND JSON_TYPE(CAST(b.col as JSON)) !='BLOB'
AND GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) != GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)));

echo "Comparator <=>";
echo "==============";
# Comparator <=>
#-----------------------
select 
a.col as side1,  
b.col as side2,
JSON_TYPE(CAST(a.col as JSON)) as side1_json_type,
JSON_TYPE(CAST(b.col as JSON)) as side2_json_type,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) as side1_json_weightage,
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as side2_json_weightage,
a.col <=>  b.col as json_compare,
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) <=>
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON))) as first_level_validation
from jj a , jj b
where a.col is not NULL 
and b.col is not NULL
and ((a.col <=>  b.col) != (
GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) 
<=> 
GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)))
))AND JSON_TYPE(CAST(a.col as JSON)) !='BLOB'
AND JSON_TYPE(CAST(b.col as JSON)) !='BLOB'
AND GET_JSON_WEIGHT(JSON_TYPE(CAST(a.col as JSON))) != GET_JSON_WEIGHT(JSON_TYPE(CAST(b.col as JSON)));
