/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/store/DataStore",["../_base/lang","../_base/declare","../Deferred","../_base/array","./util/QueryResults","./util/SimpleQueryEngine"],function(_1,_2,_3,_4,_5,_6){
var _7=null;
return _2("dojo.store.DataStore",_7,{target:"",constructor:function(_8){
_1.mixin(this,_8);
if(!("idProperty" in _8)){
var _9;
try{
_9=this.store.getIdentityAttributes();
}
catch(e){
}
this.idProperty=(_1.isArray(_9)?_9[0]:_9)||this.idProperty;
}
var _a=this.store.getFeatures();
if(!_a["dojo.data.api.Read"]){
this.get=null;
}
if(!_a["dojo.data.api.Identity"]){
this.getIdentity=null;
}
if(!_a["dojo.data.api.Write"]){
this.put=this.add=null;
}
},idProperty:"id",store:null,queryEngine:_6,_objectConverter:function(_b){
var _c=this.store;
var _d=this.idProperty;
function _e(_f){
var _10={};
var _11=_c.getAttributes(_f);
for(var i=0;i<_11.length;i++){
var _12=_11[i];
var _13=_c.getValues(_f,_12);
if(_13.length>1){
for(var j=0;j<_13.length;j++){
var _14=_13[j];
if(typeof _14=="object"&&_c.isItem(_14)){
_13[j]=_e(_14);
}
}
_14=_13;
}else{
var _14=_c.getValue(_f,_12);
if(typeof _14=="object"&&_c.isItem(_14)){
_14=_e(_14);
}
}
_10[_11[i]]=_14;
}
if(!(_d in _10)&&_c.getIdentity){
_10[_d]=_c.getIdentity(_f);
}
return _10;
};
return function(_15){
return _b(_15&&_e(_15));
};
},get:function(id,_16){
var _17,_18;
var _19=new _3();
this.store.fetchItemByIdentity({identity:id,onItem:this._objectConverter(function(_1a){
_19.resolve(_17=_1a);
}),onError:function(_1b){
_19.reject(_18=_1b);
}});
if(_17!==undefined){
return _17==null?undefined:_17;
}
if(_18){
throw _18;
}
return _19.promise;
},put:function(_1c,_1d){
_1d=_1d||{};
var id=typeof _1d.id!="undefined"?_1d.id:this.getIdentity(_1c);
var _1e=this.store;
var _1f=this.idProperty;
var _20=new _3();
if(typeof id=="undefined"){
var _21=_1e.newItem(_1c);
_1e.save({onComplete:function(){
_20.resolve(_21);
},onError:function(_22){
_20.reject(_22);
}});
}else{
_1e.fetchItemByIdentity({identity:id,onItem:function(_23){
if(_23){
if(_1d.overwrite===false){
return _20.reject(new Error("Overwriting existing object not allowed"));
}
for(var i in _1c){
if(i!=_1f&&_1c.hasOwnProperty(i)&&_1e.getValue(_23,i)!=_1c[i]){
_1e.setValue(_23,i,_1c[i]);
}
}
}else{
if(_1d.overwrite===true){
return _20.reject(new Error("Creating new object not allowed"));
}
var _23=_1e.newItem(_1c);
}
_1e.save({onComplete:function(){
_20.resolve(_23);
},onError:function(_24){
_20.reject(_24);
}});
},onError:function(_25){
_20.reject(_25);
}});
}
return _20.promise;
},add:function(_26,_27){
(_27=_27||{}).overwrite=false;
return this.put(_26,_27);
},remove:function(id){
var _28=this.store;
var _29=new _3();
this.store.fetchItemByIdentity({identity:id,onItem:function(_2a){
try{
if(_2a==null){
_29.resolve(false);
}else{
_28.deleteItem(_2a);
_28.save();
_29.resolve(true);
}
}
catch(error){
_29.reject(error);
}
},onError:function(_2b){
_29.reject(_2b);
}});
return _29.promise;
},query:function(_2c,_2d){
var _2e;
var _2f=new _3(function(){
_2e.abort&&_2e.abort();
});
_2f.total=new _3();
var _30=this._objectConverter(function(_31){
return _31;
});
_2e=this.store.fetch(_1.mixin({query:_2c,onBegin:function(_32){
_2f.total.resolve(_32);
},onComplete:function(_33){
_2f.resolve(_4.map(_33,_30));
},onError:function(_34){
_2f.reject(_34);
}},_2d));
return _5(_2f);
},getIdentity:function(_35){
return _35[this.idProperty];
}});
});
