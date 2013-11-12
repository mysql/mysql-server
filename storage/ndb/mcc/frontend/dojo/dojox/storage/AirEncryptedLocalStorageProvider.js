//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/storage/manager,dojox/storage/Provider"],function(_1,_2,_3){
_2.provide("dojox.storage.AirEncryptedLocalStorageProvider");
_2.require("dojox.storage.manager");
_2.require("dojox.storage.Provider");
if(_2.isAIR){
(function(){
if(!_4){
var _4={};
}
_4.ByteArray=window.runtime.flash.utils.ByteArray;
_4.EncryptedLocalStore=window.runtime.flash.data.EncryptedLocalStore,_2.declare("dojox.storage.AirEncryptedLocalStorageProvider",[_3.storage.Provider],{initialize:function(){
_3.storage.manager.loaded();
},isAvailable:function(){
return true;
},_getItem:function(_5){
var _6=_4.EncryptedLocalStore.getItem("__dojo_"+_5);
return _6?_6.readUTFBytes(_6.length):"";
},_setItem:function(_7,_8){
var _9=new _4.ByteArray();
_9.writeUTFBytes(_8);
_4.EncryptedLocalStore.setItem("__dojo_"+_7,_9);
},_removeItem:function(_a){
_4.EncryptedLocalStore.removeItem("__dojo_"+_a);
},put:function(_b,_c,_d,_e){
if(this.isValidKey(_b)==false){
throw new Error("Invalid key given: "+_b);
}
_e=_e||this.DEFAULT_NAMESPACE;
if(this.isValidKey(_e)==false){
throw new Error("Invalid namespace given: "+_e);
}
try{
var _f=this._getItem("namespaces")||"|";
if(_f.indexOf("|"+_e+"|")==-1){
this._setItem("namespaces",_f+_e+"|");
}
var _10=this._getItem(_e+"_keys")||"|";
if(_10.indexOf("|"+_b+"|")==-1){
this._setItem(_e+"_keys",_10+_b+"|");
}
this._setItem("_"+_e+"_"+_b,_c);
}
catch(e){
_d(this.FAILED,_b,e.toString(),_e);
return;
}
if(_d){
_d(this.SUCCESS,_b,null,_e);
}
},get:function(key,_11){
if(this.isValidKey(key)==false){
throw new Error("Invalid key given: "+key);
}
_11=_11||this.DEFAULT_NAMESPACE;
return this._getItem("_"+_11+"_"+key);
},getNamespaces:function(){
var _12=[this.DEFAULT_NAMESPACE];
var _13=(this._getItem("namespaces")||"|").split("|");
for(var i=0;i<_13.length;i++){
if(_13[i].length&&_13[i]!=this.DEFAULT_NAMESPACE){
_12.push(_13[i]);
}
}
return _12;
},getKeys:function(_14){
_14=_14||this.DEFAULT_NAMESPACE;
if(this.isValidKey(_14)==false){
throw new Error("Invalid namespace given: "+_14);
}
var _15=[];
var _16=(this._getItem(_14+"_keys")||"|").split("|");
for(var i=0;i<_16.length;i++){
if(_16[i].length){
_15.push(_16[i]);
}
}
return _15;
},clear:function(_17){
if(this.isValidKey(_17)==false){
throw new Error("Invalid namespace given: "+_17);
}
var _18=this._getItem("namespaces")||"|";
if(_18.indexOf("|"+_17+"|")!=-1){
this._setItem("namespaces",_18.replace("|"+_17+"|","|"));
}
var _19=(this._getItem(_17+"_keys")||"|").split("|");
for(var i=0;i<_19.length;i++){
if(_19[i].length){
this._removeItem(_17+"_"+_19[i]);
}
}
this._removeItem(_17+"_keys");
},remove:function(key,_1a){
_1a=_1a||this.DEFAULT_NAMESPACE;
var _1b=this._getItem(_1a+"_keys")||"|";
if(_1b.indexOf("|"+key+"|")!=-1){
this._setItem(_1a+"_keys",_1b.replace("|"+key+"|","|"));
}
this._removeItem("_"+_1a+"_"+key);
},putMultiple:function(_1c,_1d,_1e,_1f){
if(this.isValidKeyArray(_1c)===false||!_1d instanceof Array||_1c.length!=_1d.length){
throw new Error("Invalid arguments: keys = ["+_1c+"], values = ["+_1d+"]");
}
if(_1f==null||typeof _1f=="undefined"){
_1f=this.DEFAULT_NAMESPACE;
}
if(this.isValidKey(_1f)==false){
throw new Error("Invalid namespace given: "+_1f);
}
this._statusHandler=_1e;
try{
for(var i=0;i<_1c.length;i++){
this.put(_1c[i],_1d[i],null,_1f);
}
}
catch(e){
if(_1e){
_1e(this.FAILED,_1c,e.toString(),_1f);
}
return;
}
if(_1e){
_1e(this.SUCCESS,_1c,null);
}
},getMultiple:function(_20,_21){
if(this.isValidKeyArray(_20)===false){
throw new Error("Invalid key array given: "+_20);
}
if(_21==null||typeof _21=="undefined"){
_21=this.DEFAULT_NAMESPACE;
}
if(this.isValidKey(_21)==false){
throw new Error("Invalid namespace given: "+_21);
}
var _22=[];
for(var i=0;i<_20.length;i++){
_22[i]=this.get(_20[i],_21);
}
return _22;
},removeMultiple:function(_23,_24){
_24=_24||this.DEFAULT_NAMESPACE;
for(var i=0;i<_23.length;i++){
this.remove(_23[i],_24);
}
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
}});
_3.storage.manager.register("dojox.storage.AirEncryptedLocalStorageProvider",new _3.storage.AirEncryptedLocalStorageProvider());
_3.storage.manager.initialize();
})();
}
});
