//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.storage.manager");
_3.storage.manager=new function(){
this.currentProvider=null;
this.available=false;
this.providers=[];
this._initialized=false;
this._onLoadListeners=[];
this.initialize=function(){
this.autodetect();
};
this.register=function(_4,_5){
this.providers.push(_5);
this.providers[_4]=_5;
};
this.setProvider=function(_6){
};
this.autodetect=function(){
if(this._initialized){
return;
}
var _7=_2.config["forceStorageProvider"]||false;
var _8;
for(var i=0;i<this.providers.length;i++){
_8=this.providers[i];
if(_7&&_7==_8.declaredClass){
_8.isAvailable();
break;
}else{
if(!_7&&_8.isAvailable()){
break;
}
}
}
if(!_8){
this._initialized=true;
this.available=false;
this.currentProvider=null;
console.warn("No storage provider found for this platform");
this.loaded();
return;
}
this.currentProvider=_8;
_2.mixin(_3.storage,this.currentProvider);
_3.storage.initialize();
this._initialized=true;
this.available=true;
};
this.isAvailable=function(){
return this.available;
};
this.addOnLoad=function(_9){
this._onLoadListeners.push(_9);
if(this.isInitialized()){
this._fireLoaded();
}
};
this.removeOnLoad=function(_a){
for(var i=0;i<this._onLoadListeners.length;i++){
if(_a==this._onLoadListeners[i]){
this._onLoadListeners.splice(i,1);
break;
}
}
};
this.isInitialized=function(){
if(this.currentProvider!=null&&this.currentProvider.declaredClass=="dojox.storage.FlashStorageProvider"&&_3.flash.ready==false){
return false;
}else{
return this._initialized;
}
};
this.supportsProvider=function(_b){
try{
var _c=eval("new "+_b+"()");
var _d=_c.isAvailable();
if(!_d){
return false;
}
return _d;
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
_2.forEach(this._onLoadListeners,function(i){
try{
i();
}
catch(e){
}
});
};
this.getResourceList=function(){
var _e=[];
_2.forEach(_3.storage.manager.providers,function(_f){
_e=_e.concat(_f.getResourceList());
});
return _e;
};
};
});
