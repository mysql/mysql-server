//>>built
define("dojox/data/FileStore",["dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/_base/json","dojo/_base/xhr"],function(_1,_2,_3,_4,_5){
return _1("dojox.data.FileStore",null,{constructor:function(_6){
if(_6&&_6.label){
this.label=_6.label;
}
if(_6&&_6.url){
this.url=_6.url;
}
if(_6&&_6.options){
if(_2.isArray(_6.options)){
this.options=_6.options;
}else{
if(_2.isString(_6.options)){
this.options=_6.options.split(",");
}
}
}
if(_6&&_6.pathAsQueryParam){
this.pathAsQueryParam=true;
}
if(_6&&"urlPreventCache" in _6){
this.urlPreventCache=_6.urlPreventCache?true:false;
}
},url:"",_storeRef:"_S",label:"name",_identifier:"path",_attributes:["children","directory","name","path","modified","size","parentDir"],pathSeparator:"/",options:[],failOk:false,urlPreventCache:true,_assertIsItem:function(_7){
if(!this.isItem(_7)){
throw new Error("dojox.data.FileStore: a function was passed an item argument that was not an item");
}
},_assertIsAttribute:function(_8){
if(typeof _8!=="string"){
throw new Error("dojox.data.FileStore: a function was passed an attribute argument that was not an attribute name string");
}
},pathAsQueryParam:false,getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
},getValue:function(_9,_a,_b){
var _c=this.getValues(_9,_a);
if(_c&&_c.length>0){
return _c[0];
}
return _b;
},getAttributes:function(_d){
return this._attributes;
},hasAttribute:function(_e,_f){
this._assertIsItem(_e);
this._assertIsAttribute(_f);
return (_f in _e);
},getIdentity:function(_10){
return this.getValue(_10,this._identifier);
},getIdentityAttributes:function(_11){
return [this._identifier];
},isItemLoaded:function(_12){
var _13=this.isItem(_12);
if(_13&&typeof _12._loaded=="boolean"&&!_12._loaded){
_13=false;
}
return _13;
},loadItem:function(_14){
var _15=_14.item;
var _16=this;
var _17=_14.scope||_3.global;
var _18={};
if(this.options.length>0){
_18.options=_4.toJson(this.options);
}
if(this.pathAsQueryParam){
_18.path=_15.parentPath+this.pathSeparator+_15.name;
}
var _19={url:this.pathAsQueryParam?this.url:this.url+"/"+_15.parentPath+"/"+_15.name,handleAs:"json-comment-optional",content:_18,preventCache:this.urlPreventCache,failOk:this.failOk};
var _1a=_5.get(_19);
_1a.addErrback(function(_1b){
if(_14.onError){
_14.onError.call(_17,_1b);
}
});
_1a.addCallback(function(_1c){
delete _15.parentPath;
delete _15._loaded;
_2.mixin(_15,_1c);
_16._processItem(_15);
if(_14.onItem){
_14.onItem.call(_17,_15);
}
});
},getLabel:function(_1d){
return this.getValue(_1d,this.label);
},getLabelAttributes:function(_1e){
return [this.label];
},containsValue:function(_1f,_20,_21){
var _22=this.getValues(_1f,_20);
for(var i=0;i<_22.length;i++){
if(_22[i]==_21){
return true;
}
}
return false;
},getValues:function(_23,_24){
this._assertIsItem(_23);
this._assertIsAttribute(_24);
var _25=_23[_24];
if(typeof _25!=="undefined"&&!_2.isArray(_25)){
_25=[_25];
}else{
if(typeof _25==="undefined"){
_25=[];
}
}
return _25;
},isItem:function(_26){
if(_26&&_26[this._storeRef]===this){
return true;
}
return false;
},close:function(_27){
},fetch:function(_28){
_28=_28||{};
if(!_28.store){
_28.store=this;
}
var _29=this;
var _2a=_28.scope||_3.global;
var _2b={};
if(_28.query){
_2b.query=_4.toJson(_28.query);
}
if(_28.sort){
_2b.sort=_4.toJson(_28.sort);
}
if(_28.queryOptions){
_2b.queryOptions=_4.toJson(_28.queryOptions);
}
if(typeof _28.start=="number"){
_2b.start=""+_28.start;
}
if(typeof _28.count=="number"){
_2b.count=""+_28.count;
}
if(this.options.length>0){
_2b.options=_4.toJson(this.options);
}
var _2c={url:this.url,preventCache:this.urlPreventCache,failOk:this.failOk,handleAs:"json-comment-optional",content:_2b};
var _2d=_5.get(_2c);
_2d.addCallback(function(_2e){
_29._processResult(_2e,_28);
});
_2d.addErrback(function(_2f){
if(_28.onError){
_28.onError.call(_2a,_2f,_28);
}
});
},fetchItemByIdentity:function(_30){
var _31=_30.identity;
var _32=this;
var _33=_30.scope||_3.global;
var _34={};
if(this.options.length>0){
_34.options=_4.toJson(this.options);
}
if(this.pathAsQueryParam){
_34.path=_31;
}
var _35={url:this.pathAsQueryParam?this.url:this.url+"/"+_31,handleAs:"json-comment-optional",content:_34,preventCache:this.urlPreventCache,failOk:this.failOk};
var _36=_5.get(_35);
_36.addErrback(function(_37){
if(_30.onError){
_30.onError.call(_33,_37);
}
});
_36.addCallback(function(_38){
var _39=_32._processItem(_38);
if(_30.onItem){
_30.onItem.call(_33,_39);
}
});
},_processResult:function(_3a,_3b){
var _3c=_3b.scope||_3.global;
try{
if(_3a.pathSeparator){
this.pathSeparator=_3a.pathSeparator;
}
if(_3b.onBegin){
_3b.onBegin.call(_3c,_3a.total,_3b);
}
var _3d=this._processItemArray(_3a.items);
if(_3b.onItem){
var i;
for(i=0;i<_3d.length;i++){
_3b.onItem.call(_3c,_3d[i],_3b);
}
_3d=null;
}
if(_3b.onComplete){
_3b.onComplete.call(_3c,_3d,_3b);
}
}
catch(e){
if(_3b.onError){
_3b.onError.call(_3c,e,_3b);
}else{
}
}
},_processItemArray:function(_3e){
var i;
for(i=0;i<_3e.length;i++){
this._processItem(_3e[i]);
}
return _3e;
},_processItem:function(_3f){
if(!_3f){
return null;
}
_3f[this._storeRef]=this;
if(_3f.children&&_3f.directory){
if(_2.isArray(_3f.children)){
var _40=_3f.children;
var i;
for(i=0;i<_40.length;i++){
var _41=_40[i];
if(_2.isObject(_41)){
_40[i]=this._processItem(_41);
}else{
_40[i]={name:_41,_loaded:false,parentPath:_3f.path};
_40[i][this._storeRef]=this;
}
}
}else{
delete _3f.children;
}
}
return _3f;
}});
});
