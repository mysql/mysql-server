//>>built
define("dojox/lang/functional/array",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","./lambda"],function(_1,_2,_3,df){
var _4={};
_2.mixin(df,{filter:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_1.global;
f=df.lambda(f);
var t=[],v,i,n;
if(_2.isArray(a)){
for(i=0,n=a.length;i<n;++i){
v=a[i];
if(f.call(o,v,i,a)){
t.push(v);
}
}
}else{
if(typeof a.hasNext=="function"&&typeof a.next=="function"){
for(i=0;a.hasNext();){
v=a.next();
if(f.call(o,v,i++,a)){
t.push(v);
}
}
}else{
for(i in a){
if(!(i in _4)){
v=a[i];
if(f.call(o,v,i,a)){
t.push(v);
}
}
}
}
}
return t;
},forEach:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_1.global;
f=df.lambda(f);
var i,n;
if(_2.isArray(a)){
for(i=0,n=a.length;i<n;f.call(o,a[i],i,a),++i){
}
}else{
if(typeof a.hasNext=="function"&&typeof a.next=="function"){
for(i=0;a.hasNext();f.call(o,a.next(),i++,a)){
}
}else{
for(i in a){
if(!(i in _4)){
f.call(o,a[i],i,a);
}
}
}
}
return o;
},map:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_1.global;
f=df.lambda(f);
var t,n,i;
if(_2.isArray(a)){
t=new Array(n=a.length);
for(i=0;i<n;t[i]=f.call(o,a[i],i,a),++i){
}
}else{
if(typeof a.hasNext=="function"&&typeof a.next=="function"){
t=[];
for(i=0;a.hasNext();t.push(f.call(o,a.next(),i++,a))){
}
}else{
t=[];
for(i in a){
if(!(i in _4)){
t.push(f.call(o,a[i],i,a));
}
}
}
}
return t;
},every:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_1.global;
f=df.lambda(f);
var i,n;
if(_2.isArray(a)){
for(i=0,n=a.length;i<n;++i){
if(!f.call(o,a[i],i,a)){
return false;
}
}
}else{
if(typeof a.hasNext=="function"&&typeof a.next=="function"){
for(i=0;a.hasNext();){
if(!f.call(o,a.next(),i++,a)){
return false;
}
}
}else{
for(i in a){
if(!(i in _4)){
if(!f.call(o,a[i],i,a)){
return false;
}
}
}
}
}
return true;
},some:function(a,f,o){
if(typeof a=="string"){
a=a.split("");
}
o=o||_1.global;
f=df.lambda(f);
var i,n;
if(_2.isArray(a)){
for(i=0,n=a.length;i<n;++i){
if(f.call(o,a[i],i,a)){
return true;
}
}
}else{
if(typeof a.hasNext=="function"&&typeof a.next=="function"){
for(i=0;a.hasNext();){
if(f.call(o,a.next(),i++,a)){
return true;
}
}
}else{
for(i in a){
if(!(i in _4)){
if(f.call(o,a[i],i,a)){
return true;
}
}
}
}
}
return false;
}});
return df;
});
