/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/store/DataStore",["../_base/lang","../_base/declare","../_base/Deferred","../_base/array","./util/QueryResults"],function(_1,_2,_3,_4,_5){
return _2("dojo.store.DataStore",null,{target:"",constructor:function(_6){
_1.mixin(this,_6);
if(!"idProperty" in _6){
var _7;
try{
_7=this.store.getIdentityAttributes();
}
catch(e){
}
this.idProperty=(!_7||!idAttributes[0])||this.idProperty;
}
var _8=this.store.getFeatures();
if(!_8["dojo.data.api.Read"]){
this.get=null;
}
if(!_8["dojo.data.api.Identity"]){
this.getIdentity=null;
}
if(!_8["dojo.data.api.Write"]){
this.put=this.add=null;
}
},idProperty:"id",store:null,_objectConverter:function(_9){
var _a=this.store;
var _b=this.idProperty;
return function(_c){
var _d={};
var _e=_a.getAttributes(_c);
for(var i=0;i<_e.length;i++){
_d[_e[i]]=_a.getValue(_c,_e[i]);
}
if(!(_b in _d)){
_d[_b]=_a.getIdentity(_c);
}
return _9(_d);
};
},get:function(id,_f){
var _10,_11;
var _12=new _3();
this.store.fetchItemByIdentity({identity:id,onItem:this._objectConverter(function(_13){
_12.resolve(_10=_13);
}),onError:function(_14){
_12.reject(_11=_14);
}});
if(_10){
return _10;
}
if(_11){
throw _11;
}
return _12.promise;
},put:function(_15,_16){
var id=_16&&typeof _16.id!="undefined"||this.getIdentity(_15);
var _17=this.store;
var _18=this.idProperty;
if(typeof id=="undefined"){
_17.newItem(_15);
}else{
_17.fetchItemByIdentity({identity:id,onItem:function(_19){
if(_19){
for(var i in _15){
if(i!=_18&&_17.getValue(_19,i)!=_15[i]){
_17.setValue(_19,i,_15[i]);
}
}
}else{
_17.newItem(_15);
}
}});
}
},remove:function(id){
var _1a=this.store;
this.store.fetchItemByIdentity({identity:id,onItem:function(_1b){
_1a.deleteItem(_1b);
}});
},query:function(_1c,_1d){
var _1e;
var _1f=new _3(function(){
_1e.abort&&_1e.abort();
});
_1f.total=new _3();
var _20=this._objectConverter(function(_21){
return _21;
});
_1e=this.store.fetch(_1.mixin({query:_1c,onBegin:function(_22){
_1f.total.resolve(_22);
},onComplete:function(_23){
_1f.resolve(_4.map(_23,_20));
},onError:function(_24){
_1f.reject(_24);
}},_1d));
return _5(_1f);
},getIdentity:function(_25){
return _25[this.idProperty];
}});
});
