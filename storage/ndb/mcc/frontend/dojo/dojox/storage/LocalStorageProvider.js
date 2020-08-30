//>>built
define("dojox/storage/LocalStorageProvider",["dojo/_base/declare","dojox/storage/Provider","dojox/storage/manager","dojo/_base/array","dojo/_base/lang","dojo/json"],function(_1,_2,_3,_4,_5,_6){
var _7=_1("dojox.storage.LocalStorageProvider",[_2],{store:null,initialize:function(){
this.store=localStorage;
this.initialized=true;
_3.loaded();
},isAvailable:function(){
return typeof localStorage!="undefined";
},put:function(_8,_9,_a,_b){
this._assertIsValidKey(_8);
_b=_b||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_b);
var _c=this.getFullKey(_8,_b);
_9=_6.stringify(_9);
try{
this.store.setItem(_c,_9);
if(_a){
_a(this.SUCCESS,_8,null,_b);
}
}
catch(e){
if(_a){
_a(this.FAILED,_8,e.toString(),_b);
}
}
},get:function(_d,_e){
this._assertIsValidKey(_d);
_e=_e||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_e);
_d=this.getFullKey(_d,_e);
return _6.parse(this.store.getItem(_d));
},getKeys:function(_f){
_f=_f||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_f);
_f="__"+_f+"_";
var _10=[];
for(var i=0;i<this.store.length;i++){
var _11=this.store.key(i);
if(this._beginsWith(_11,_f)){
_11=_11.substring(_f.length);
_10.push(_11);
}
}
return _10;
},clear:function(_12){
_12=_12||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_12);
_12="__"+_12+"_";
var _13=[];
for(var i=0;i<this.store.length;i++){
if(this._beginsWith(this.store.key(i),_12)){
_13.push(this.store.key(i));
}
}
_4.forEach(_13,_5.hitch(this.store,"removeItem"));
},remove:function(key,_14){
_14=_14||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_14);
this.store.removeItem(this.getFullKey(key,_14));
},getNamespaces:function(){
var _15=[this.DEFAULT_NAMESPACE];
var _16={};
_16[this.DEFAULT_NAMESPACE]=true;
var _17=/^__([^_]*)_/;
for(var i=0;i<this.store.length;i++){
var _18=this.store.key(i);
if(_17.test(_18)==true){
var _19=_18.match(_17)[1];
if(typeof _16[_19]=="undefined"){
_16[_19]=true;
_15.push(_19);
}
}
}
return _15;
},isPermanent:function(){
return true;
},getMaximumSize:function(){
return dojox.storage.SIZE_NO_LIMIT;
},hasSettingsUI:function(){
return false;
},isValidKey:function(_1a){
if(_1a===null||_1a===undefined){
return false;
}
return /^[0-9A-Za-z_-]*$/.test(_1a);
},isValidNamespace:function(_1b){
if(_1b===null||_1b===undefined){
return false;
}
return /^[0-9A-Za-z-]*$/.test(_1b);
},getFullKey:function(key,_1c){
return "__"+_1c+"_"+key;
},_beginsWith:function(_1d,_1e){
if(_1e.length>_1d.length){
return false;
}
return _1d.substring(0,_1e.length)===_1e;
},_assertIsValidNamespace:function(_1f){
if(this.isValidNamespace(_1f)===false){
throw new Error("Invalid namespace given: "+_1f);
}
},_assertIsValidKey:function(key){
if(this.isValidKey(key)===false){
throw new Error("Invalid key given: "+key);
}
}});
_3.register("dojox.storage.LocalStorageProvider",new _7());
return _7;
});
