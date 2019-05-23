//>>built
define("dojox/charting/scaler/common",["dojo/_base/lang"],function(_1){
var eq=function(a,b){
return Math.abs(a-b)<=0.000001*(Math.abs(a)+Math.abs(b));
};
var _2=_1.getObject("dojox.charting.scaler.common",true);
var _3={};
return _1.mixin(_2,{doIfLoaded:function(_4,_5,_6){
if(_3[_4]==undefined){
try{
_3[_4]=require(_4);
}
catch(e){
_3[_4]=null;
}
}
if(_3[_4]){
return _5(_3[_4]);
}else{
return _6();
}
},getNumericLabel:function(_7,_8,_9){
var _a="";
_2.doIfLoaded("dojo/number",function(_b){
_a=(_9.fixed?_b.format(_7,{places:_8<0?-_8:0}):_b.format(_7))||"";
},function(){
_a=_9.fixed?_7.toFixed(_8<0?-_8:0):_7.toString();
});
if(_9.labelFunc){
var r=_9.labelFunc(_a,_7,_8);
if(r){
return r;
}
}
if(_9.labels){
var l=_9.labels,lo=0,hi=l.length;
while(lo<hi){
var _c=Math.floor((lo+hi)/2),_d=l[_c].value;
if(_d<_7){
lo=_c+1;
}else{
hi=_c;
}
}
if(lo<l.length&&eq(l[lo].value,_7)){
return l[lo].text;
}
--lo;
if(lo>=0&&lo<l.length&&eq(l[lo].value,_7)){
return l[lo].text;
}
lo+=2;
if(lo<l.length&&eq(l[lo].value,_7)){
return l[lo].text;
}
}
return _a;
}});
});
