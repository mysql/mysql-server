//>>built
define("dojox/dtl/filter/integers",["dojo/_base/lang","../_base"],function(_1,dd){
var _2=_1.getObject("filter.integers",true,dd);
_1.mixin(_2,{add:function(_3,_4){
_3=parseInt(_3,10);
_4=parseInt(_4,10);
return isNaN(_4)?_3:_3+_4;
},get_digit:function(_5,_6){
_5=parseInt(_5,10);
_6=parseInt(_6,10)-1;
if(_6>=0){
_5+="";
if(_6<_5.length){
_5=parseInt(_5.charAt(_6),10);
}else{
_5=0;
}
}
return (isNaN(_5)?0:_5);
}});
return _2;
});
