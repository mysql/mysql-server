//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/storage/Provider,dojox/storage/manager"],function(_1,_2,_3){
_2.provide("dojox.storage.WhatWGStorageProvider");
_2.require("dojox.storage.Provider");
_2.require("dojox.storage.manager");
_2.declare("dojox.storage.WhatWGStorageProvider",[_3.storage.Provider],{initialized:false,_domain:null,_available:null,_statusHandler:null,_allNamespaces:null,_storageEventListener:null,initialize:function(){
if(_2.config["disableWhatWGStorage"]==true){
return;
}
this._domain=location.hostname;
this.initialized=true;
_3.storage.manager.loaded();
},isAvailable:function(){
try{
var _4=globalStorage[location.hostname];
}
catch(e){
this._available=false;
return this._available;
}
this._available=true;
return this._available;
},put:function(_5,_6,_7,_8){
if(this.isValidKey(_5)==false){
throw new Error("Invalid key given: "+_5);
}
_8=_8||this.DEFAULT_NAMESPACE;
_5=this.getFullKey(_5,_8);
this._statusHandler=_7;
if(_2.isString(_6)){
_6="string:"+_6;
}else{
_6=_2.toJson(_6);
}
var _9=_2.hitch(this,function(_a){
window.removeEventListener("storage",_9,false);
if(_7){
_7.call(null,this.SUCCESS,_5,null,_8);
}
});
window.addEventListener("storage",_9,false);
try{
var _b=globalStorage[this._domain];
_b.setItem(_5,_6);
}
catch(e){
this._statusHandler.call(null,this.FAILED,_5,e.toString(),_8);
}
},get:function(_c,_d){
if(this.isValidKey(_c)==false){
throw new Error("Invalid key given: "+_c);
}
_d=_d||this.DEFAULT_NAMESPACE;
_c=this.getFullKey(_c,_d);
var _e=globalStorage[this._domain];
var _f=_e.getItem(_c);
if(_f==null||_f==""){
return null;
}
_f=_f.value;
if(_2.isString(_f)&&(/^string:/.test(_f))){
_f=_f.substring("string:".length);
}else{
_f=_2.fromJson(_f);
}
return _f;
},getNamespaces:function(){
var _10=[this.DEFAULT_NAMESPACE];
var _11={};
var _12=globalStorage[this._domain];
var _13=/^__([^_]*)_/;
for(var i=0;i<_12.length;i++){
var _14=_12.key(i);
if(_13.test(_14)==true){
var _15=_14.match(_13)[1];
if(typeof _11[_15]=="undefined"){
_11[_15]=true;
_10.push(_15);
}
}
}
return _10;
},getKeys:function(_16){
_16=_16||this.DEFAULT_NAMESPACE;
if(this.isValidKey(_16)==false){
throw new Error("Invalid namespace given: "+_16);
}
var _17;
if(_16==this.DEFAULT_NAMESPACE){
_17=new RegExp("^([^_]{2}.*)$");
}else{
_17=new RegExp("^__"+_16+"_(.*)$");
}
var _18=globalStorage[this._domain];
var _19=[];
for(var i=0;i<_18.length;i++){
var _1a=_18.key(i);
if(_17.test(_1a)==true){
_1a=_1a.match(_17)[1];
_19.push(_1a);
}
}
return _19;
},clear:function(_1b){
_1b=_1b||this.DEFAULT_NAMESPACE;
if(this.isValidKey(_1b)==false){
throw new Error("Invalid namespace given: "+_1b);
}
var _1c;
if(_1b==this.DEFAULT_NAMESPACE){
_1c=new RegExp("^[^_]{2}");
}else{
_1c=new RegExp("^__"+_1b+"_");
}
var _1d=globalStorage[this._domain];
var _1e=[];
for(var i=0;i<_1d.length;i++){
if(_1c.test(_1d.key(i))==true){
_1e[_1e.length]=_1d.key(i);
}
}
_2.forEach(_1e,_2.hitch(_1d,"removeItem"));
},remove:function(key,_1f){
key=this.getFullKey(key,_1f);
var _20=globalStorage[this._domain];
_20.removeItem(key);
},isPermanent:function(){
return true;
},getMaximumSize:function(){
return this.SIZE_NO_LIMIT;
},hasSettingsUI:function(){
return false;
},showSettingsUI:function(){
throw new Error(this.declaredClass+" does not support a storage settings user-interface");
},hideSettingsUI:function(){
throw new Error(this.declaredClass+" does not support a storage settings user-interface");
},getFullKey:function(key,_21){
_21=_21||this.DEFAULT_NAMESPACE;
if(this.isValidKey(_21)==false){
throw new Error("Invalid namespace given: "+_21);
}
if(_21==this.DEFAULT_NAMESPACE){
return key;
}else{
return "__"+_21+"_"+key;
}
}});
_3.storage.manager.register("dojox.storage.WhatWGStorageProvider",new _3.storage.WhatWGStorageProvider());
});
