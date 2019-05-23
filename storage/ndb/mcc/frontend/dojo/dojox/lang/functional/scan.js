//>>built
define("dojox/lang/functional/scan",["dojo/_base/kernel","dojo/_base/lang","./lambda"],function(_1,_2,df){
var _3={};
_2.mixin(df,{scanl:function(a,f,z,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_1.global;
f=df.lambda(f);
var t,n,i;
if(_2.isArray(a)){
t=new Array((n=a.length)+1);
t[0]=z;
for(i=0;i<n;z=f.call(o,z,a[i],i,a),t[++i]=z){
}
}else{
if(typeof a.hasNext=="function"&&typeof a.next=="function"){
t=[z];
for(i=0;a.hasNext();t.push(z=f.call(o,z,a.next(),i++,a))){
}
}else{
t=[z];
for(i in a){
if(!(i in _3)){
t.push(z=f.call(o,z,a[i],i,a));
}
}
}
}
return t;
},scanl1:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_1.global;
f=df.lambda(f);
var t,n,z,_4=true;
if(_2.isArray(a)){
t=new Array(n=a.length);
t[0]=z=a[0];
for(var i=1;i<n;t[i]=z=f.call(o,z,a[i],i,a),++i){
}
}else{
if(typeof a.hasNext=="function"&&typeof a.next=="function"){
if(a.hasNext()){
t=[z=a.next()];
for(i=1;a.hasNext();t.push(z=f.call(o,z,a.next(),i++,a))){
}
}
}else{
for(i in a){
if(!(i in _3)){
if(_4){
t=[z=a[i]];
_4=false;
}else{
t.push(z=f.call(o,z,a[i],i,a));
}
}
}
}
}
return t;
},scanr:function(a,f,z,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_1.global;
f=df.lambda(f);
var n=a.length,t=new Array(n+1),i=n;
t[n]=z;
for(;i>0;--i,z=f.call(o,z,a[i],i,a),t[i]=z){
}
return t;
},scanr1:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_1.global;
f=df.lambda(f);
var n=a.length,t=new Array(n),z=a[n-1],i=n-1;
t[i]=z;
for(;i>0;--i,z=f.call(o,z,a[i],i,a),t[i]=z){
}
return t;
}});
});
