//>>built
define("dojox/data/KeyValueStore",["dojo/_base/declare","dojo/_base/lang","dojo/_base/xhr","dojo/_base/kernel","dojo/data/util/simpleFetch","dojo/data/util/filter"],function(_1,_2,_3,_4,_5,_6){
var _7=_1("dojox.data.KeyValueStore",null,{constructor:function(_8){
if(_8.url){
this.url=_8.url;
}
this._keyValueString=_8.data;
this._keyValueVar=_8.dataVar;
this._keyAttribute="key";
this._valueAttribute="value";
this._storeProp="_keyValueStore";
this._features={"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
this._loadInProgress=false;
this._queuedFetches=[];
if(_8&&"urlPreventCache" in _8){
this.urlPreventCache=_8.urlPreventCache?true:false;
}
},url:"",data:"",urlPreventCache:false,_assertIsItem:function(_9){
if(!this.isItem(_9)){
throw new Error("dojox.data.KeyValueStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_a,_b){
if(!_2.isString(_b)){
throw new Error("dojox.data.KeyValueStore: a function was passed an attribute argument that was not an attribute object nor an attribute name string");
}
},getValue:function(_c,_d,_e){
this._assertIsItem(_c);
this._assertIsAttribute(_c,_d);
var _f;
if(_d==this._keyAttribute){
_f=_c[this._keyAttribute];
}else{
_f=_c[this._valueAttribute];
}
if(_f===undefined){
_f=_e;
}
return _f;
},getValues:function(_10,_11){
var _12=this.getValue(_10,_11);
return (_12?[_12]:[]);
},getAttributes:function(_13){
return [this._keyAttribute,this._valueAttribute,_13[this._keyAttribute]];
},hasAttribute:function(_14,_15){
this._assertIsItem(_14);
this._assertIsAttribute(_14,_15);
return (_15==this._keyAttribute||_15==this._valueAttribute||_15==_14[this._keyAttribute]);
},containsValue:function(_16,_17,_18){
var _19=undefined;
if(typeof _18==="string"){
_19=_6.patternToRegExp(_18,false);
}
return this._containsValue(_16,_17,_18,_19);
},_containsValue:function(_1a,_1b,_1c,_1d){
var _1e=this.getValues(_1a,_1b);
for(var i=0;i<_1e.length;++i){
var _1f=_1e[i];
if(typeof _1f==="string"&&_1d){
return (_1f.match(_1d)!==null);
}else{
if(_1c===_1f){
return true;
}
}
}
return false;
},isItem:function(_20){
if(_20&&_20[this._storeProp]===this){
return true;
}
return false;
},isItemLoaded:function(_21){
return this.isItem(_21);
},loadItem:function(_22){
},getFeatures:function(){
return this._features;
},close:function(_23){
},getLabel:function(_24){
return _24[this._keyAttribute];
},getLabelAttributes:function(_25){
return [this._keyAttribute];
},_fetchItems:function(_26,_27,_28){
var _29=this;
var _2a=function(_2b,_2c){
var _2d=null;
if(_2b.query){
_2d=[];
var _2e=_2b.queryOptions?_2b.queryOptions.ignoreCase:false;
var _2f={};
for(var key in _2b.query){
var _30=_2b.query[key];
if(typeof _30==="string"){
_2f[key]=_6.patternToRegExp(_30,_2e);
}
}
for(var i=0;i<_2c.length;++i){
var _31=true;
var _32=_2c[i];
for(var key in _2b.query){
var _30=_2b.query[key];
if(!_29._containsValue(_32,key,_30,_2f[key])){
_31=false;
}
}
if(_31){
_2d.push(_32);
}
}
}else{
if(_2b.identity){
_2d=[];
var _33;
for(var key in _2c){
_33=_2c[key];
if(_33[_29._keyAttribute]==_2b.identity){
_2d.push(_33);
break;
}
}
}else{
if(_2c.length>0){
_2d=_2c.slice(0,_2c.length);
}
}
}
_27(_2d,_2b);
};
if(this._loadFinished){
_2a(_26,this._arrayOfAllItems);
}else{
if(this.url!==""){
if(this._loadInProgress){
this._queuedFetches.push({args:_26,filter:_2a});
}else{
this._loadInProgress=true;
var _34={url:_29.url,handleAs:"json-comment-filtered",preventCache:this.urlPreventCache};
var _35=_3.get(_34);
_35.addCallback(function(_36){
_29._processData(_36);
_2a(_26,_29._arrayOfAllItems);
_29._handleQueuedFetches();
});
_35.addErrback(function(_37){
_29._loadInProgress=false;
throw _37;
});
}
}else{
if(this._keyValueString){
this._processData(eval(this._keyValueString));
this._keyValueString=null;
_2a(_26,this._arrayOfAllItems);
}else{
if(this._keyValueVar){
this._processData(this._keyValueVar);
this._keyValueVar=null;
_2a(_26,this._arrayOfAllItems);
}else{
throw new Error("dojox.data.KeyValueStore: No source data was provided as either URL, String, or Javascript variable data input.");
}
}
}
}
},_handleQueuedFetches:function(){
if(this._queuedFetches.length>0){
for(var i=0;i<this._queuedFetches.length;i++){
var _38=this._queuedFetches[i];
var _39=_38.filter;
var _3a=_38.args;
if(_39){
_39(_3a,this._arrayOfAllItems);
}else{
this.fetchItemByIdentity(_38.args);
}
}
this._queuedFetches=[];
}
},_processData:function(_3b){
this._arrayOfAllItems=[];
for(var i=0;i<_3b.length;i++){
this._arrayOfAllItems.push(this._createItem(_3b[i]));
}
this._loadFinished=true;
this._loadInProgress=false;
},_createItem:function(_3c){
var _3d={};
_3d[this._storeProp]=this;
for(var i in _3c){
_3d[this._keyAttribute]=i;
_3d[this._valueAttribute]=_3c[i];
break;
}
return _3d;
},getIdentity:function(_3e){
if(this.isItem(_3e)){
return _3e[this._keyAttribute];
}
return null;
},getIdentityAttributes:function(_3f){
return [this._keyAttribute];
},fetchItemByIdentity:function(_40){
_40.oldOnItem=_40.onItem;
_40.onItem=null;
_40.onComplete=this._finishFetchItemByIdentity;
this.fetch(_40);
},_finishFetchItemByIdentity:function(_41,_42){
var _43=_42.scope||_4.global;
if(_41.length){
_42.oldOnItem.call(_43,_41[0]);
}else{
_42.oldOnItem.call(_43,null);
}
}});
_2.extend(_7,_5);
return _7;
});
