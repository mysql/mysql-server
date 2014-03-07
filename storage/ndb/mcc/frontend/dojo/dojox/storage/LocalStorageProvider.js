//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/storage/Provider,dojox/storage/manager"],function(_1,_2,_3){
_2.provide("dojox.storage.LocalStorageProvider");
_2.require("dojox.storage.Provider");
_2.require("dojox.storage.manager");
_2.declare("dojox.storage.LocalStorageProvider",[_3.storage.Provider],{store:null,initialize:function(){
this.store=localStorage;
this.initialized=true;
_3.storage.manager.loaded();
},isAvailable:function(){
return typeof localStorage!="undefined";
},put:function(_4,_5,_6,_7){
this._assertIsValidKey(_4);
_7=_7||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_7);
var _8=this.getFullKey(_4,_7);
_5=_2.toJson(_5);
try{
this.store.setItem(_8,_5);
if(_6){
_6(this.SUCCESS,_4,null,_7);
}
}
catch(e){
if(_6){
_6(this.FAILED,_4,e.toString(),_7);
}
}
},get:function(_9,_a){
this._assertIsValidKey(_9);
_a=_a||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_a);
_9=this.getFullKey(_9,_a);
return _2.fromJson(this.store.getItem(_9));
},getKeys:function(_b){
_b=_b||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_b);
_b="__"+_b+"_";
var _c=[];
for(var i=0;i<this.store.length;i++){
var _d=this.store.key(i);
if(this._beginsWith(_d,_b)){
_d=_d.substring(_b.length);
_c.push(_d);
}
}
return _c;
},clear:function(_e){
_e=_e||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_e);
_e="__"+_e+"_";
var _f=[];
for(var i=0;i<this.store.length;i++){
if(this._beginsWith(this.store.key(i),_e)){
_f.push(this.store.key(i));
}
}
_2.forEach(_f,_2.hitch(this.store,"removeItem"));
},remove:function(key,_10){
_10=_10||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_10);
this.store.removeItem(this.getFullKey(key,_10));
},getNamespaces:function(){
var _11=[this.DEFAULT_NAMESPACE];
var _12={};
_12[this.DEFAULT_NAMESPACE]=true;
var _13=/^__([^_]*)_/;
for(var i=0;i<this.store.length;i++){
var _14=this.store.key(i);
if(_13.test(_14)==true){
var _15=_14.match(_13)[1];
if(typeof _12[_15]=="undefined"){
_12[_15]=true;
_11.push(_15);
}
}
}
return _11;
},isPermanent:function(){
return true;
},getMaximumSize:function(){
return _3.storage.SIZE_NO_LIMIT;
},hasSettingsUI:function(){
return false;
},isValidKey:function(_16){
if(_16===null||_16===undefined){
return false;
}
return /^[0-9A-Za-z_-]*$/.test(_16);
},isValidNamespace:function(_17){
if(_17===null||_17===undefined){
return false;
}
return /^[0-9A-Za-z-]*$/.test(_17);
},getFullKey:function(key,_18){
return "__"+_18+"_"+key;
},_beginsWith:function(_19,_1a){
if(_1a.length>_19.length){
return false;
}
return _19.substring(0,_1a.length)===_1a;
},_assertIsValidNamespace:function(_1b){
if(this.isValidNamespace(_1b)===false){
throw new Error("Invalid namespace given: "+_1b);
}
},_assertIsValidKey:function(key){
if(this.isValidKey(key)===false){
throw new Error("Invalid key given: "+key);
}
}});
_3.storage.manager.register("dojox.storage.LocalStorageProvider",new _3.storage.LocalStorageProvider());
});
