//>>built
define("dojox/lang/functional/object",["dojo/_base/kernel","dojo/_base/lang","./lambda"],function(_1,_2,df){
var _3={};
_2.mixin(df,{keys:function(_4){
var t=[];
for(var i in _4){
if(!(i in _3)){
t.push(i);
}
}
return t;
},values:function(_5){
var t=[];
for(var i in _5){
if(!(i in _3)){
t.push(_5[i]);
}
}
return t;
},filterIn:function(_6,f,o){
o=o||_1.global;
f=df.lambda(f);
var t={},v,i;
for(i in _6){
if(!(i in _3)){
v=_6[i];
if(f.call(o,v,i,_6)){
t[i]=v;
}
}
}
return t;
},forIn:function(_7,f,o){
o=o||_1.global;
f=df.lambda(f);
for(var i in _7){
if(!(i in _3)){
f.call(o,_7[i],i,_7);
}
}
return o;
},mapIn:function(_8,f,o){
o=o||_1.global;
f=df.lambda(f);
var t={},i;
for(i in _8){
if(!(i in _3)){
t[i]=f.call(o,_8[i],i,_8);
}
}
return t;
}});
return df;
});
