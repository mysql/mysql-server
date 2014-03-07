//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/storage/manager,dojox/storage/Provider"],function(_1,_2,_3){
_2.provide("dojox.storage.AirFileStorageProvider");
_2.require("dojox.storage.manager");
_2.require("dojox.storage.Provider");
if(_2.isAIR){
(function(){
if(!_4){
var _4={};
}
_4.File=window.runtime.flash.filesystem.File;
_4.FileStream=window.runtime.flash.filesystem.FileStream;
_4.FileMode=window.runtime.flash.filesystem.FileMode;
_2.declare("dojox.storage.AirFileStorageProvider",[_3.storage.Provider],{initialized:false,_storagePath:"__DOJO_STORAGE/",initialize:function(){
this.initialized=false;
try{
var _5=_4.File.applicationStorageDirectory.resolvePath(this._storagePath);
if(!_5.exists){
_5.createDirectory();
}
this.initialized=true;
}
catch(e){
}
_3.storage.manager.loaded();
},isAvailable:function(){
return true;
},put:function(_6,_7,_8,_9){
if(this.isValidKey(_6)==false){
throw new Error("Invalid key given: "+_6);
}
_9=_9||this.DEFAULT_NAMESPACE;
if(this.isValidKey(_9)==false){
throw new Error("Invalid namespace given: "+_9);
}
try{
this.remove(_6,_9);
var _a=_4.File.applicationStorageDirectory.resolvePath(this._storagePath+_9);
if(!_a.exists){
_a.createDirectory();
}
var _b=_a.resolvePath(_6);
var _c=new _4.FileStream();
_c.open(_b,_4.FileMode.WRITE);
_c.writeObject(_7);
_c.close();
}
catch(e){
_8(this.FAILED,_6,e.toString(),_9);
return;
}
if(_8){
_8(this.SUCCESS,_6,null,_9);
}
},get:function(_d,_e){
if(this.isValidKey(_d)==false){
throw new Error("Invalid key given: "+_d);
}
_e=_e||this.DEFAULT_NAMESPACE;
var _f=null;
var _10=_4.File.applicationStorageDirectory.resolvePath(this._storagePath+_e+"/"+_d);
if(_10.exists&&!_10.isDirectory){
var _11=new _4.FileStream();
_11.open(_10,_4.FileMode.READ);
_f=_11.readObject();
_11.close();
}
return _f;
},getNamespaces:function(){
var _12=[this.DEFAULT_NAMESPACE];
var dir=_4.File.applicationStorageDirectory.resolvePath(this._storagePath);
var _13=dir.getDirectoryListing(),i;
for(i=0;i<_13.length;i++){
if(_13[i].isDirectory&&_13[i].name!=this.DEFAULT_NAMESPACE){
_12.push(_13[i].name);
}
}
return _12;
},getKeys:function(_14){
_14=_14||this.DEFAULT_NAMESPACE;
if(this.isValidKey(_14)==false){
throw new Error("Invalid namespace given: "+_14);
}
var _15=[];
var dir=_4.File.applicationStorageDirectory.resolvePath(this._storagePath+_14);
if(dir.exists&&dir.isDirectory){
var _16=dir.getDirectoryListing(),i;
for(i=0;i<_16.length;i++){
_15.push(_16[i].name);
}
}
return _15;
},clear:function(_17){
if(this.isValidKey(_17)==false){
throw new Error("Invalid namespace given: "+_17);
}
var dir=_4.File.applicationStorageDirectory.resolvePath(this._storagePath+_17);
if(dir.exists&&dir.isDirectory){
dir.deleteDirectory(true);
}
},remove:function(key,_18){
_18=_18||this.DEFAULT_NAMESPACE;
var _19=_4.File.applicationStorageDirectory.resolvePath(this._storagePath+_18+"/"+key);
if(_19.exists&&!_19.isDirectory){
_19.deleteFile();
}
},putMultiple:function(_1a,_1b,_1c,_1d){
if(this.isValidKeyArray(_1a)===false||!_1b instanceof Array||_1a.length!=_1b.length){
throw new Error("Invalid arguments: keys = ["+_1a+"], values = ["+_1b+"]");
}
if(_1d==null||typeof _1d=="undefined"){
_1d=this.DEFAULT_NAMESPACE;
}
if(this.isValidKey(_1d)==false){
throw new Error("Invalid namespace given: "+_1d);
}
this._statusHandler=_1c;
try{
for(var i=0;i<_1a.length;i++){
this.put(_1a[i],_1b[i],null,_1d);
}
}
catch(e){
if(_1c){
_1c(this.FAILED,_1a,e.toString(),_1d);
}
return;
}
if(_1c){
_1c(this.SUCCESS,_1a,null,_1d);
}
},getMultiple:function(_1e,_1f){
if(this.isValidKeyArray(_1e)===false){
throw new Error("Invalid key array given: "+_1e);
}
if(_1f==null||typeof _1f=="undefined"){
_1f=this.DEFAULT_NAMESPACE;
}
if(this.isValidKey(_1f)==false){
throw new Error("Invalid namespace given: "+_1f);
}
var _20=[];
for(var i=0;i<_1e.length;i++){
_20[i]=this.get(_1e[i],_1f);
}
return _20;
},removeMultiple:function(_21,_22){
_22=_22||this.DEFAULT_NAMESPACE;
for(var i=0;i<_21.length;i++){
this.remove(_21[i],_22);
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
_3.storage.manager.register("dojox.storage.AirFileStorageProvider",new _3.storage.AirFileStorageProvider());
_3.storage.manager.initialize();
})();
}
});
