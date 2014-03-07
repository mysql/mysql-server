//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/storage/Provider,dojox/storage/manager"],function(_1,_2,_3){
_2.provide("dojox.storage.BehaviorStorageProvider");
_2.require("dojox.storage.Provider");
_2.require("dojox.storage.manager");
_2.declare("dojox.storage.BehaviorStorageProvider",[_3.storage.Provider],{store:null,storeName:"__dojox_BehaviorStorage",keys:[],initialize:function(){
try{
this.store=this._createStore();
this.store.load(this.storeName);
}
catch(e){
throw new Error("Store is not available: "+e);
}
var _4=this.get("keys","dojoxSystemNS");
this.keys=_4||[];
this.initialized=true;
_3.storage.manager.loaded();
},isAvailable:function(){
return _2.isIE&&_2.isIE>=5;
},_createStore:function(){
var _5=_2.create("link",{id:this.storeName+"Node",style:{"display":"none"}},_2.query("head")[0]);
_5.addBehavior("#default#userdata");
return _5;
},put:function(_6,_7,_8,_9){
this._assertIsValidKey(_6);
_9=_9||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_9);
var _a=this.getFullKey(_6,_9);
_7=_2.toJson(_7);
this.store.setAttribute(_a,_7);
this.store.save(this.storeName);
var _b=this.store.getAttribute(_a)===_7;
if(_b){
this._addKey(_a);
this.store.setAttribute("__dojoxSystemNS_keys",_2.toJson(this.keys));
this.store.save(this.storeName);
}
if(_8){
_8(_b?this.SUCCESS:this.FAILED,_6,null,_9);
}
},get:function(_c,_d){
this._assertIsValidKey(_c);
_d=_d||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_d);
_c=this.getFullKey(_c,_d);
return _2.fromJson(this.store.getAttribute(_c));
},getKeys:function(_e){
_e=_e||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_e);
_e="__"+_e+"_";
var _f=[];
for(var i=0;i<this.keys.length;i++){
var _10=this.keys[i];
if(this._beginsWith(_10,_e)){
_10=_10.substring(_e.length);
_f.push(_10);
}
}
return _f;
},clear:function(_11){
_11=_11||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_11);
_11="__"+_11+"_";
var _12=[];
for(var i=0;i<this.keys.length;i++){
var _13=this.keys[i];
if(this._beginsWith(_13,_11)){
_12.push(_13);
}
}
_2.forEach(_12,function(key){
this.store.removeAttribute(key);
this._removeKey(key);
},this);
this.put("keys",this.keys,null,"dojoxSystemNS");
this.store.save(this.storeName);
},remove:function(key,_14){
this._assertIsValidKey(key);
_14=_14||this.DEFAULT_NAMESPACE;
this._assertIsValidNamespace(_14);
key=this.getFullKey(key,_14);
this.store.removeAttribute(key);
this._removeKey(key);
this.put("keys",this.keys,null,"dojoxSystemNS");
this.store.save(this.storeName);
},getNamespaces:function(){
var _15=[this.DEFAULT_NAMESPACE];
var _16={};
_16[this.DEFAULT_NAMESPACE]=true;
var _17=/^__([^_]*)_/;
for(var i=0;i<this.keys.length;i++){
var _18=this.keys[i];
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
return 64;
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
},_addKey:function(key){
this._removeKey(key);
this.keys.push(key);
},_removeKey:function(key){
this.keys=_2.filter(this.keys,function(_20){
return _20!==key;
},this);
}});
_3.storage.manager.register("dojox.storage.BehaviorStorageProvider",new _3.storage.BehaviorStorageProvider());
});
