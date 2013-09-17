//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/flash,dojox/storage/manager,dojox/storage/Provider"],function(_1,_2,_3){
_2.provide("dojox.storage.FlashStorageProvider");
_2.require("dojox.flash");
_2.require("dojox.storage.manager");
_2.require("dojox.storage.Provider");
_2.declare("dojox.storage.FlashStorageProvider",_3.storage.Provider,{initialized:false,_available:null,_statusHandler:null,_flashReady:false,_pageReady:false,initialize:function(){
if(_2.config["disableFlashStorage"]==true){
return;
}
_3.flash.addLoadedListener(_2.hitch(this,function(){
this._flashReady=true;
if(this._flashReady&&this._pageReady){
this._loaded();
}
}));
var _4=_2.moduleUrl("dojox","storage/Storage.swf").toString();
_3.flash.setSwf(_4,false);
_2.connect(_2,"loaded",this,function(){
this._pageReady=true;
if(this._flashReady&&this._pageReady){
this._loaded();
}
});
},setFlushDelay:function(_5){
if(_5===null||typeof _5==="undefined"||isNaN(_5)){
throw new Error("Invalid argunment: "+_5);
}
_3.flash.comm.setFlushDelay(String(_5));
},getFlushDelay:function(){
return Number(_3.flash.comm.getFlushDelay());
},flush:function(_6){
if(_6==null||typeof _6=="undefined"){
_6=_3.storage.DEFAULT_NAMESPACE;
}
_3.flash.comm.flush(_6);
},isAvailable:function(){
return (this._available=!_2.config["disableFlashStorage"]);
},put:function(_7,_8,_9,_a){
if(!this.isValidKey(_7)){
throw new Error("Invalid key given: "+_7);
}
if(!_a){
_a=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_a)){
throw new Error("Invalid namespace given: "+_a);
}
this._statusHandler=_9;
if(_2.isString(_8)){
_8="string:"+_8;
}else{
_8=_2.toJson(_8);
}
_3.flash.comm.put(_7,_8,_a);
},putMultiple:function(_b,_c,_d,_e){
if(!this.isValidKeyArray(_b)||!_c instanceof Array||_b.length!=_c.length){
throw new Error("Invalid arguments: keys = ["+_b+"], values = ["+_c+"]");
}
if(!_e){
_e=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_e)){
throw new Error("Invalid namespace given: "+_e);
}
this._statusHandler=_d;
var _f=_b.join(",");
var _10=[];
for(var i=0;i<_c.length;i++){
if(_2.isString(_c[i])){
_c[i]="string:"+_c[i];
}else{
_c[i]=_2.toJson(_c[i]);
}
_10[i]=_c[i].length;
}
var _11=_c.join("");
var _12=_10.join(",");
_3.flash.comm.putMultiple(_f,_11,_12,_e);
},get:function(key,_13){
if(!this.isValidKey(key)){
throw new Error("Invalid key given: "+key);
}
if(!_13){
_13=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_13)){
throw new Error("Invalid namespace given: "+_13);
}
var _14=_3.flash.comm.get(key,_13);
if(_14==""){
return null;
}
return this._destringify(_14);
},getMultiple:function(_15,_16){
if(!this.isValidKeyArray(_15)){
throw new ("Invalid key array given: "+_15);
}
if(!_16){
_16=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_16)){
throw new Error("Invalid namespace given: "+_16);
}
var _17=_15.join(",");
var _18=_3.flash.comm.getMultiple(_17,_16);
var _19=eval("("+_18+")");
for(var i=0;i<_19.length;i++){
_19[i]=(_19[i]=="")?null:this._destringify(_19[i]);
}
return _19;
},_destringify:function(_1a){
if(_2.isString(_1a)&&(/^string:/.test(_1a))){
_1a=_1a.substring("string:".length);
}else{
_1a=_2.fromJson(_1a);
}
return _1a;
},getKeys:function(_1b){
if(!_1b){
_1b=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_1b)){
throw new Error("Invalid namespace given: "+_1b);
}
var _1c=_3.flash.comm.getKeys(_1b);
if(_1c==null||_1c=="null"){
_1c="";
}
_1c=_1c.split(",");
_1c.sort();
return _1c;
},getNamespaces:function(){
var _1d=_3.flash.comm.getNamespaces();
if(_1d==null||_1d=="null"){
_1d=_3.storage.DEFAULT_NAMESPACE;
}
_1d=_1d.split(",");
_1d.sort();
return _1d;
},clear:function(_1e){
if(!_1e){
_1e=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_1e)){
throw new Error("Invalid namespace given: "+_1e);
}
_3.flash.comm.clear(_1e);
},remove:function(key,_1f){
if(!_1f){
_1f=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_1f)){
throw new Error("Invalid namespace given: "+_1f);
}
_3.flash.comm.remove(key,_1f);
},removeMultiple:function(_20,_21){
if(!this.isValidKeyArray(_20)){
_2.raise("Invalid key array given: "+_20);
}
if(!_21){
_21=_3.storage.DEFAULT_NAMESPACE;
}
if(!this.isValidKey(_21)){
throw new Error("Invalid namespace given: "+_21);
}
var _22=_20.join(",");
_3.flash.comm.removeMultiple(_22,_21);
},isPermanent:function(){
return true;
},getMaximumSize:function(){
return _3.storage.SIZE_NO_LIMIT;
},hasSettingsUI:function(){
return true;
},showSettingsUI:function(){
_3.flash.comm.showSettings();
_3.flash.obj.setVisible(true);
_3.flash.obj.center();
},hideSettingsUI:function(){
_3.flash.obj.setVisible(false);
if(_2.isFunction(_3.storage.onHideSettingsUI)){
_3.storage.onHideSettingsUI.call(null);
}
},getResourceList:function(){
return [];
},_loaded:function(){
this._allNamespaces=this.getNamespaces();
this.initialized=true;
_3.storage.manager.loaded();
},_onStatus:function(_23,key,_24){
var ds=_3.storage;
var dfo=_3.flash.obj;
if(_23==ds.PENDING){
dfo.center();
dfo.setVisible(true);
}else{
dfo.setVisible(false);
}
if(ds._statusHandler){
ds._statusHandler.call(null,_23,key,null,_24);
}
}});
_3.storage.manager.register("dojox.storage.FlashStorageProvider",new _3.storage.FlashStorageProvider());
});
