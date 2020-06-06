//>>built
define("dojox/lang/functional/reversed",["dojo/_base/lang","dojo/_base/kernel","./lambda"],function(_1,_2,df){
_1.mixin(df,{filterRev:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_2.global;
f=df.lambda(f);
var t=[],v,i=a.length-1;
for(;i>=0;--i){
v=a[i];
if(f.call(o,v,i,a)){
t.push(v);
}
}
return t;
},forEachRev:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_2.global;
f=df.lambda(f);
for(var i=a.length-1;i>=0;f.call(o,a[i],i,a),--i){
}
},mapRev:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_2.global;
f=df.lambda(f);
var n=a.length,t=new Array(n),i=n-1,j=0;
for(;i>=0;t[j++]=f.call(o,a[i],i,a),--i){
}
return t;
},everyRev:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_2.global;
f=df.lambda(f);
for(var i=a.length-1;i>=0;--i){
if(!f.call(o,a[i],i,a)){
return false;
}
}
return true;
},someRev:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_2.global;
f=df.lambda(f);
for(var i=a.length-1;i>=0;--i){
if(f.call(o,a[i],i,a)){
return true;
}
}
return false;
}});
return df;
});
