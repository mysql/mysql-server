//>>built
define("dojox/mvc/equals",["dojo/_base/array","dojo/_base/lang","dojo/Stateful","./StatefulArray"],function(_1,_2,_3,_4){
var _5={getType:function(v){
return _2.isArray(v)?"array":_2.isFunction((v||{}).getTime)?"date":v!=null&&({}.toString.call(v)=="[object Object]"||_2.isFunction((v||{}).set)&&_2.isFunction((v||{}).watch))?"object":"value";
},equalsArray:function(_6,_7){
for(var i=0,l=Math.max(_6.length,_7.length);i<l;i++){
if(!_8(_6[i],_7[i])){
return false;
}
}
return true;
},equalsDate:function(_9,_a){
return _9.getTime()==_a.getTime();
},equalsObject:function(_b,_c){
var _d=_2.mixin({},_b,_c);
for(var s in _d){
if(!(s in _3.prototype)&&s!="_watchCallbacks"&&!_8(_b[s],_c[s])){
return false;
}
}
return true;
},equalsValue:function(_e,_f){
return _e===_f;
}};
var _8=function(dst,src,_10){
var _11=_10||_8,_12=[_11.getType(dst),_11.getType(src)];
return _12[0]!=_12[1]?false:_11["equals"+_12[0].replace(/^[a-z]/,function(c){
return c.toUpperCase();
})](dst,src);
};
return _2.setObject("dojox.mvc.equals",_2.mixin(_8,_5));
});
