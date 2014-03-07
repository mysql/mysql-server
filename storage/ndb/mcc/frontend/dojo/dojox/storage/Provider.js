//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.storage.Provider");
_2.declare("dojox.storage.Provider",null,{constructor:function(){
},SUCCESS:"success",FAILED:"failed",PENDING:"pending",SIZE_NOT_AVAILABLE:"Size not available",SIZE_NO_LIMIT:"No size limit",DEFAULT_NAMESPACE:"default",onHideSettingsUI:null,initialize:function(){
console.warn("dojox.storage.initialize not implemented");
},isAvailable:function(){
console.warn("dojox.storage.isAvailable not implemented");
},put:function(_4,_5,_6,_7){
console.warn("dojox.storage.put not implemented");
},get:function(_8,_9){
console.warn("dojox.storage.get not implemented");
},hasKey:function(_a,_b){
return !!this.get(_a,_b);
},getKeys:function(_c){
console.warn("dojox.storage.getKeys not implemented");
},clear:function(_d){
console.warn("dojox.storage.clear not implemented");
},remove:function(_e,_f){
console.warn("dojox.storage.remove not implemented");
},getNamespaces:function(){
console.warn("dojox.storage.getNamespaces not implemented");
},isPermanent:function(){
console.warn("dojox.storage.isPermanent not implemented");
},getMaximumSize:function(){
console.warn("dojox.storage.getMaximumSize not implemented");
},putMultiple:function(_10,_11,_12,_13){
for(var i=0;i<_10.length;i++){
_3.storage.put(_10[i],_11[i],_12,_13);
}
},getMultiple:function(_14,_15){
var _16=[];
for(var i=0;i<_14.length;i++){
_16.push(_3.storage.get(_14[i],_15));
}
return _16;
},removeMultiple:function(_17,_18){
for(var i=0;i<_17.length;i++){
_3.storage.remove(_17[i],_18);
}
},isValidKeyArray:function(_19){
if(_19===null||_19===undefined||!_2.isArray(_19)){
return false;
}
return !_2.some(_19,function(key){
return !this.isValidKey(key);
},this);
},hasSettingsUI:function(){
return false;
},showSettingsUI:function(){
console.warn("dojox.storage.showSettingsUI not implemented");
},hideSettingsUI:function(){
console.warn("dojox.storage.hideSettingsUI not implemented");
},isValidKey:function(_1a){
if(_1a===null||_1a===undefined){
return false;
}
return /^[0-9A-Za-z_]*$/.test(_1a);
},getResourceList:function(){
return [];
}});
});
