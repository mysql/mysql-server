//>>built
define("dojox/mobile/parser",["dojo/_base/kernel","dojo/_base/array","dojo/_base/config","dojo/_base/lang","dojo/_base/window","dojo/ready"],function(_1,_2,_3,_4,_5,_6){
var dm=_4.getObject("dojox.mobile",true);
var _7=function(){
var _8={};
var _9=function(_a,_b){
if(typeof (_b)==="string"){
var t=_a+":"+_b.replace(/ /g,"");
return _8[t]||(_8[t]=_9(_a).createSubclass(_2.map(_b.split(/, */),_9)));
}
return _8[_a]||(_8[_a]=_4.getObject(_a)||require(_a));
};
var _c=function(js){
return eval(js);
};
this.instantiate=function(_d,_e,_f){
_e=_e||{};
_f=_f||{};
var i,ws=[];
if(_d){
for(i=0;i<_d.length;i++){
var n=_d[i],_10=n._type,_11=_9(_10,n.getAttribute("data-dojo-mixins")),_12=_11.prototype,_13={},_14,v,t;
_4.mixin(_13,_c.call(_f.propsThis,"({"+(n.getAttribute("data-dojo-props")||"")+"})"));
_4.mixin(_13,_f.defaults);
_4.mixin(_13,_e);
for(_14 in _12){
v=n.getAttributeNode(_14);
v=v&&v.nodeValue;
t=typeof _12[_14];
if(!v&&(t!=="boolean"||v!=="")){
continue;
}
if(_4.isArray(_12[_14])){
_13[_14]=v.split(/\s*,\s*/);
}else{
if(t==="string"){
_13[_14]=v;
}else{
if(t==="number"){
_13[_14]=v-0;
}else{
if(t==="boolean"){
_13[_14]=(v!=="false");
}else{
if(t==="object"){
_13[_14]=eval("("+v+")");
}else{
if(t==="function"){
_13[_14]=_4.getObject(v,false)||new Function(v);
n.removeAttribute(_14);
}
}
}
}
}
}
}
_13["class"]=n.className;
if(!_13.style){
_13.style=n.style.cssText;
}
v=n.getAttribute("data-dojo-attach-point");
if(v){
_13.dojoAttachPoint=v;
}
v=n.getAttribute("data-dojo-attach-event");
if(v){
_13.dojoAttachEvent=v;
}
var _15=new _11(_13,n);
ws.push(_15);
var _16=n.getAttribute("jsId")||n.getAttribute("data-dojo-id");
if(_16){
_4.setObject(_16,_15);
}
}
for(i=0;i<ws.length;i++){
var w=ws[i];
!_f.noStart&&w.startup&&!w._started&&w.startup();
}
}
return ws;
};
this.parse=function(_17,_18){
if(!_17){
_17=_5.body();
}else{
if(!_18&&_17.rootNode){
_18=_17;
_17=_17.rootNode;
}
}
var _19=_17.getElementsByTagName("*");
var i,j,_1a=[];
for(i=0;i<_19.length;i++){
var n=_19[i],_1b=(n._type=n.getAttribute("dojoType")||n.getAttribute("data-dojo-type"));
if(_1b){
if(n._skip){
n._skip="";
continue;
}
if(_9(_1b).prototype.stopParser&&!(_18&&_18.template)){
var arr=n.getElementsByTagName("*");
for(j=0;j<arr.length;j++){
arr[j]._skip="1";
}
}
_1a.push(n);
}
}
var _1c=_18&&_18.template?{template:true}:null;
return this.instantiate(_1a,_1c,_18);
};
};
var _1d=new _7();
if(_3.parseOnLoad){
_6(100,function(){
try{
if(!require("dojo/parser")){
_1d.parse();
}
}
catch(e){
_1d.parse();
}
});
}
dm.parser=_1d;
_1.parser=_1.parser||_1d;
return _1d;
});
