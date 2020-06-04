//>>built
define("dojox/dtl/filter/dates",["dojo/_base/lang","../_base","../utils/date"],function(_1,dd,_2){
var _3=_1.getObject("filter.dates",true,dd);
_1.mixin(_3,{_toDate:function(_4){
if(_4 instanceof Date){
return _4;
}
_4=new Date(_4);
if(_4.getTime()==new Date(0).getTime()){
return "";
}
return _4;
},date:function(_5,_6){
_5=_3._toDate(_5);
if(!_5){
return "";
}
_6=_6||"N j, Y";
return _2.format(_5,_6);
},time:function(_7,_8){
_7=_3._toDate(_7);
if(!_7){
return "";
}
_8=_8||"P";
return _2.format(_7,_8);
},timesince:function(_9,_a){
_9=_3._toDate(_9);
if(!_9){
return "";
}
var _b=_2.timesince;
if(_a){
return _b(_a,_9);
}
return _b(_9);
},timeuntil:function(_c,_d){
_c=_3._toDate(_c);
if(!_c){
return "";
}
var _e=_2.timesince;
if(_d){
return _e(_d,_c);
}
return _e(new Date(),_c);
}});
return _3;
});
