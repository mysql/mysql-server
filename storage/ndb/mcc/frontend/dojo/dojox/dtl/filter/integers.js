//>>built
define("dojox/dtl/filter/integers",["dojo/_base/lang","../_base"],function(_1,dd){
_1.getObject("dojox.dtl.filter.integers",true);
_1.mixin(dd.filter.integers,{add:function(_2,_3){
_2=parseInt(_2,10);
_3=parseInt(_3,10);
return isNaN(_3)?_2:_2+_3;
},get_digit:function(_4,_5){
_4=parseInt(_4,10);
_5=parseInt(_5,10)-1;
if(_5>=0){
_4+="";
if(_5<_4.length){
_4=parseInt(_4.charAt(_5),10);
}else{
_4=0;
}
}
return (isNaN(_4)?0:_4);
}});
return dojox.dtl.filter.integers;
});
