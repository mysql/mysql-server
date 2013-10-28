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
},findString:function(_7,_8){
_7=_7.toLowerCase();
for(var i=0;i<_8.length;++i){
if(_7==_8[i]){
return true;
}
}
return false;
},getNumericLabel:function(_9,_a,_b){
var _c="";
_2.doIfLoaded("dojo/number",function(_d){
_c=(_b.fixed?_d.format(_9,{places:_a<0?-_a:0}):_d.format(_9))||"";
},function(){
_c=_b.fixed?_9.toFixed(_a<0?-_a:0):_9.toString();
});
if(_b.labelFunc){
var r=_b.labelFunc(_c,_9,_a);
if(r){
return r;
}
}
if(_b.labels){
var l=_b.labels,lo=0,hi=l.length;
while(lo<hi){
var _e=Math.floor((lo+hi)/2),_f=l[_e].value;
if(_f<_9){
lo=_e+1;
}else{
hi=_e;
}
}
if(lo<l.length&&eq(l[lo].value,_9)){
return l[lo].text;
}
--lo;
if(lo>=0&&lo<l.length&&eq(l[lo].value,_9)){
return l[lo].text;
}
lo+=2;
if(lo<l.length&&eq(l[lo].value,_9)){
return l[lo].text;
}
}
return _c;
}});
});
