//>>built
define("dojox/mobile/parser",["dojo/_base/kernel","dojo/_base/config","dojo/_base/lang","dojo/_base/window","dojo/ready"],function(_1,_2,_3,_4,_5){
var dm=_3.getObject("dojox.mobile",true);
var _6=new function(){
this.instantiate=function(_7,_8,_9){
_8=_8||{};
_9=_9||{};
var i,ws=[];
if(_7){
for(i=0;i<_7.length;i++){
var n=_7[i];
var _a=_3.getObject(n.getAttribute("dojoType")||n.getAttribute("data-dojo-type"));
var _b=_a.prototype;
var _c={},_d,v,t;
_3.mixin(_c,eval("({"+(n.getAttribute("data-dojo-props")||"")+"})"));
_3.mixin(_c,_9.defaults);
_3.mixin(_c,_8);
for(_d in _b){
v=n.getAttributeNode(_d);
v=v&&v.nodeValue;
t=typeof _b[_d];
if(!v&&(t!=="boolean"||v!=="")){
continue;
}
if(t==="string"){
_c[_d]=v;
}else{
if(t==="number"){
_c[_d]=v-0;
}else{
if(t==="boolean"){
_c[_d]=(v!=="false");
}else{
if(t==="object"){
_c[_d]=eval("("+v+")");
}
}
}
}
}
_c["class"]=n.className;
_c.style=n.style&&n.style.cssText;
v=n.getAttribute("data-dojo-attach-point");
if(v){
_c.dojoAttachPoint=v;
}
v=n.getAttribute("data-dojo-attach-event");
if(v){
_c.dojoAttachEvent=v;
}
var _e=new _a(_c,n);
ws.push(_e);
var _f=n.getAttribute("jsId")||n.getAttribute("data-dojo-id");
if(_f){
_3.setObject(_f,_e);
}
}
for(i=0;i<ws.length;i++){
var w=ws[i];
!_9.noStart&&w.startup&&!w._started&&w.startup();
}
}
return ws;
};
this.parse=function(_10,_11){
if(!_10){
_10=_4.body();
}else{
if(!_11&&_10.rootNode){
_11=_10;
_10=_10.rootNode;
}
}
var _12=_10.getElementsByTagName("*");
var i,_13=[];
for(i=0;i<_12.length;i++){
var n=_12[i];
if(n.getAttribute("dojoType")||n.getAttribute("data-dojo-type")){
_13.push(n);
}
}
var _14=_11&&_11.template?{template:true}:null;
return this.instantiate(_13,_14,_11);
};
}();
if(_2.parseOnLoad){
_5(100,_6,"parse");
}
dm.parser=_6;
_1.parser=_6;
return _6;
});
