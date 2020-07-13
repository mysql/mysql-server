//>>built
define("dojox/storage/manager",["dojo/_base/config","dojo/_base/lang","dojo/_base/array"],function(_1,_2,_3){
var _4=new function(){
this.currentProvider=null;
this.available=false;
this.providers=[];
this._initialized=false;
this._onLoadListeners=[];
this.initialize=function(){
this.autodetect();
};
this.register=function(_5,_6){
this.providers.push(_6);
this.providers[_5]=_6;
};
this.setProvider=function(_7){
};
this.autodetect=function(){
if(this._initialized){
return;
}
var _8=_1["forceStorageProvider"]||false;
var _9;
for(var i=0;i<this.providers.length;i++){
_9=this.providers[i];
if(_8&&_8==_9.declaredClass){
_9.isAvailable();
break;
}else{
if(!_8&&_9.isAvailable()){
break;
}
}
}
if(!_9){
this._initialized=true;
this.available=false;
this.currentProvider=null;
console.warn("No storage provider found for this platform");
this.loaded();
return;
}
this.currentProvider=_9;
_2.mixin(dojox.storage,this.currentProvider);
dojox.storage.initialize();
this._initialized=true;
this.available=true;
};
this.isAvailable=function(){
return this.available;
};
this.addOnLoad=function(_a){
this._onLoadListeners.push(_a);
if(this.isInitialized()){
this._fireLoaded();
}
};
this.removeOnLoad=function(_b){
for(var i=0;i<this._onLoadListeners.length;i++){
if(_b==this._onLoadListeners[i]){
this._onLoadListeners.splice(i,1);
break;
}
}
};
this.isInitialized=function(){
if(this.currentProvider!=null&&this.currentProvider.declaredClass=="dojox.storage.FlashStorageProvider"&&dojox.flash.ready==false){
return false;
}else{
return this._initialized;
}
};
this.supportsProvider=function(_c){
try{
var _d=eval("new "+_c+"()");
var _e=_d.isAvailable();
if(!_e){
return false;
}
return _e;
}
catch(e){
return false;
}
};
this.getProvider=function(){
return this.currentProvider;
};
this.loaded=function(){
this._fireLoaded();
};
this._fireLoaded=function(){
_3.forEach(this._onLoadListeners,function(i){
try{
i();
}
catch(e){
}
});
};
this.getResourceList=function(){
var _f=[];
_3.forEach(dojox.storage.manager.providers,function(_10){
_f=_f.concat(_10.getResourceList());
});
return _f;
};
};
return _4;
});
