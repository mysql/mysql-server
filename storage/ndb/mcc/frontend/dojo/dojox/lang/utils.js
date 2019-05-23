//>>built
define("dojox/lang/utils",["..","dojo/_base/lang"],function(_1,_2){
var du=_2.getObject("lang.utils",true,_1);
var _3={},_4=Object.prototype.toString;
var _5=function(o){
if(o){
switch(_4.call(o)){
case "[object Array]":
return o.slice(0);
case "[object Object]":
return _2.delegate(o);
}
}
return o;
};
_2.mixin(du,{coerceType:function(_6,_7){
switch(typeof _6){
case "number":
return Number(eval("("+_7+")"));
case "string":
return String(_7);
case "boolean":
return Boolean(eval("("+_7+")"));
}
return eval("("+_7+")");
},updateWithObject:function(_8,_9,_a){
if(!_9){
return _8;
}
for(var x in _8){
if(x in _9&&!(x in _3)){
var t=_8[x];
if(t&&typeof t=="object"){
du.updateWithObject(t,_9[x],_a);
}else{
_8[x]=_a?du.coerceType(t,_9[x]):_5(_9[x]);
}
}
}
return _8;
},updateWithPattern:function(_b,_c,_d,_e){
if(!_c||!_d){
return _b;
}
for(var x in _d){
if(x in _c&&!(x in _3)){
_b[x]=_e?du.coerceType(_d[x],_c[x]):_5(_c[x]);
}
}
return _b;
},merge:function(_f,_10){
if(_10){
var _11=_4.call(_f),_12=_4.call(_10),t,i,l,m;
switch(_12){
case "[object Array]":
if(_12==_11){
t=new Array(Math.max(_f.length,_10.length));
for(i=0,l=t.length;i<l;++i){
t[i]=du.merge(_f[i],_10[i]);
}
return t;
}
return _10.slice(0);
case "[object Object]":
if(_12==_11&&_f){
t=_2.delegate(_f);
for(i in _10){
if(i in _f){
l=_f[i];
m=_10[i];
if(m!==l){
t[i]=du.merge(l,m);
}
}else{
t[i]=_2.clone(_10[i]);
}
}
return t;
}
return _2.clone(_10);
}
}
return _10;
}});
return du;
});
