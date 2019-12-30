//>>built
define("dojox/mvc/EditStoreRefController",["dojo/_base/declare","dojo/_base/lang","dojo/when","./getPlainValue","./EditModelRefController","./StoreRefController"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.mvc.EditStoreRefController",[_6,_5],{getPlainValueOptions:null,_removals:[],_resultsWatchHandle:null,_refSourceModelProp:"sourceModel",queryStore:function(_7,_8){
if(!(this.store||{}).query){
return;
}
if(this._resultsWatchHandle){
this._resultsWatchHandle.unwatch();
}
this._removals=[];
var _9=this,_a=this.inherited(arguments),_b=_3(_a,function(_c){
if(_9._beingDestroyed){
return;
}
if(_2.isArray(_c)){
_9._resultsWatchHandle=_c.watchElements(function(_d,_e,_f){
[].push.apply(_9._removals,_e);
});
}
return _c;
});
if(_b.then){
_b=_2.delegate(_b);
}
for(var s in _a){
if(isNaN(s)&&_a.hasOwnProperty(s)&&_2.isFunction(_a[s])){
_b[s]=_a[s];
}
}
return _b;
},getStore:function(id,_10){
if(this._resultsWatchHandle){
this._resultsWatchHandle.unwatch();
}
return this.inherited(arguments);
},commit:function(){
if(this._removals){
for(var i=0;i<this._removals.length;i++){
this.store.remove(this.store.getIdentity(this._removals[i]));
}
this._removals=[];
}
var _11=_4(this.get(this._refEditModelProp),this.getPlainValueOptions);
if(_2.isArray(_11)){
for(var i=0;i<_11.length;i++){
this.store.put(_11[i]);
}
}else{
this.store.put(_11);
}
this.inherited(arguments);
},reset:function(){
this.inherited(arguments);
this._removals=[];
},destroy:function(){
if(this._resultsWatchHandle){
this._resultsWatchHandle.unwatch();
}
this.inherited(arguments);
}});
});
