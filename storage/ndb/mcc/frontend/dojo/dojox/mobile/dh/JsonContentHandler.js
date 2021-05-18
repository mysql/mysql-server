//>>built
define("dojox/mobile/dh/JsonContentHandler",["dojo/_base/kernel","dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/Deferred","dojo/json","dojo/dom-construct"],function(_1,_2,_3,_4,_5,_6,_7){
return _3("dojox.mobile.dh.JsonContentHandler",null,{parse:function(_8,_9,_a){
var _b,_c=_7.create("DIV");
_9.insertBefore(_c,_a);
this._ws=[];
this._req=[];
var _d=_6.parse(_8);
return _5.when(this._loadPrereqs(_d),_4.hitch(this,function(){
_b=this._instantiate(_d,_c);
_b.style.visibility="hidden";
_2.forEach(this._ws,function(w){
if(!w._started&&w.startup){
w.startup();
}
});
this._ws=null;
return _b.id;
}));
},_loadPrereqs:function(_e){
var d=new _5();
var _f=this._collectRequires(_e);
if(_f.length===0){
return true;
}
if(_1.require){
_2.forEach(_f,function(c){
_1["require"](c);
});
return true;
}else{
_f=_2.map(_f,function(s){
return s.replace(/\./g,"/");
});
require(_f,function(){
d.resolve(true);
});
}
return d;
},_collectRequires:function(obj){
var _10=obj["class"];
for(var key in obj){
if(key.charAt(0)=="@"||key==="children"){
continue;
}
var cls=_10||key.replace(/:.*/,"");
this._req.push(cls);
if(!cls){
continue;
}
var _11=_10?[obj]:(_4.isArray(obj[key])?obj[key]:[obj[key]]);
for(var i=0;i<_11.length;i++){
if(!_10){
this._collectRequires(_11[i]);
}else{
if(_11[i].children){
for(var j=0;j<_11[i].children.length;j++){
this._collectRequires(_11[i].children[j]);
}
}
}
}
}
return this._req;
},_instantiate:function(obj,_12,_13){
var _14;
var _15=obj["class"];
for(var key in obj){
if(key.charAt(0)=="@"||key==="children"){
continue;
}
var cls=_4.getObject(_15||key.replace(/:.*/,""));
if(!cls){
continue;
}
var _16=cls.prototype,_17=_15?[obj]:(_4.isArray(obj[key])?obj[key]:[obj[key]]);
for(var i=0;i<_17.length;i++){
var _18={};
for(var _19 in _17[i]){
if(_19.charAt(0)=="@"){
var v=_17[i][_19];
_19=_19.substring(1);
var t=typeof _16[_19];
if(_4.isArray(_16[_19])){
_18[_19]=v.split(/\s*,\s*/);
}else{
if(t==="string"){
_18[_19]=v;
}else{
if(t==="number"){
_18[_19]=v-0;
}else{
if(t==="boolean"){
_18[_19]=(v!=="false");
}else{
if(t==="object"){
_18[_19]=_6.parse(v);
}else{
if(t==="function"){
_18[_19]=_4.getObject(v,false)||new Function(v);
}
}
}
}
}
}
}
}
_14=new cls(_18,_12);
if(_12){
this._ws.push(_14);
}
if(_13){
_14.placeAt(_13.containerNode||_13.domNode);
}
if(!_15){
this._instantiate(_17[i],null,_14);
}else{
if(_17[i].children){
for(var j=0;j<_17[i].children.length;j++){
this._instantiate(_17[i].children[j],null,_14);
}
}
}
}
}
return _14&&_14.domNode;
}});
});
