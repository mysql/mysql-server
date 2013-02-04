//>>built
define("dojox/data/ServiceStore",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array"],function(_1,_2,_3){
return _1("dojox.data.ServiceStore",_2.getObject("dojox.data.ClientFilter",0)||null,{service:null,constructor:function(_4){
this.byId=this.fetchItemByIdentity;
this._index={};
if(_4){
_2.mixin(this,_4);
}
this.idAttribute=(_4&&_4.idAttribute)||(this.schema&&this.schema._idAttr);
},schema:null,idAttribute:"id",labelAttribute:"label",syncMode:false,estimateCountFactor:1,getSchema:function(){
return this.schema;
},loadLazyValues:true,getValue:function(_5,_6,_7){
var _8=_5[_6];
return _8||(_6 in _5?_8:_5._loadObject?(dojox.rpc._sync=true)&&arguments.callee.call(this,dojox.data.ServiceStore.prototype.loadItem({item:_5})||{},_6,_7):_7);
},getValues:function(_9,_a){
var _b=this.getValue(_9,_a);
return _b instanceof Array?_b:_b===undefined?[]:[_b];
},getAttributes:function(_c){
var _d=[];
for(var i in _c){
if(_c.hasOwnProperty(i)&&!(i.charAt(0)=="_"&&i.charAt(1)=="_")){
_d.push(i);
}
}
return _d;
},hasAttribute:function(_e,_f){
return _f in _e;
},containsValue:function(_10,_11,_12){
return _3.indexOf(this.getValues(_10,_11),_12)>-1;
},isItem:function(_13){
return (typeof _13=="object")&&_13&&!(_13 instanceof Date);
},isItemLoaded:function(_14){
return _14&&!_14._loadObject;
},loadItem:function(_15){
var _16;
if(_15.item._loadObject){
_15.item._loadObject(function(_17){
_16=_17;
delete _16._loadObject;
var _18=_17 instanceof Error?_15.onError:_15.onItem;
if(_18){
_18.call(_15.scope,_17);
}
});
}else{
if(_15.onItem){
_15.onItem.call(_15.scope,_15.item);
}
}
return _16;
},_currentId:0,_processResults:function(_19,_1a){
if(_19&&typeof _19=="object"){
var id=_19.__id;
if(!id){
if(this.idAttribute){
id=_19[this.idAttribute];
}else{
id=this._currentId++;
}
if(id!==undefined){
var _1b=this._index[id];
if(_1b){
for(var j in _1b){
delete _1b[j];
}
_19=_2.mixin(_1b,_19);
}
_19.__id=id;
this._index[id]=_19;
}
}
for(var i in _19){
_19[i]=this._processResults(_19[i],_1a).items;
}
var _1c=_19.length;
}
return {totalCount:_1a.request.count==_1c?(_1a.request.start||0)+_1c*this.estimateCountFactor:_1c,items:_19};
},close:function(_1d){
return _1d&&_1d.abort&&_1d.abort();
},fetch:function(_1e){
_1e=_1e||{};
if("syncMode" in _1e?_1e.syncMode:this.syncMode){
dojox.rpc._sync=true;
}
var _1f=this;
var _20=_1e.scope||_1f;
var _21=this.cachingFetch?this.cachingFetch(_1e):this._doQuery(_1e);
_21.request=_1e;
_21.addCallback(function(_22){
if(_1e.clientFetch){
_22=_1f.clientSideFetch({query:_1e.clientFetch,sort:_1e.sort,start:_1e.start,count:_1e.count},_22);
}
var _23=_1f._processResults(_22,_21);
_22=_1e.results=_23.items;
if(_1e.onBegin){
_1e.onBegin.call(_20,_23.totalCount,_1e);
}
if(_1e.onItem){
for(var i=0;i<_22.length;i++){
_1e.onItem.call(_20,_22[i],_1e);
}
}
if(_1e.onComplete){
_1e.onComplete.call(_20,_1e.onItem?null:_22,_1e);
}
return _22;
});
_21.addErrback(_1e.onError&&function(err){
return _1e.onError.call(_20,err,_1e);
});
_1e.abort=function(){
_21.cancel();
};
_1e.store=this;
return _1e;
},_doQuery:function(_24){
var _25=typeof _24.queryStr=="string"?_24.queryStr:_24.query;
return this.service(_25);
},getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Identity":true,"dojo.data.api.Schema":this.schema};
},getLabel:function(_26){
return this.getValue(_26,this.labelAttribute);
},getLabelAttributes:function(_27){
return [this.labelAttribute];
},getIdentity:function(_28){
return _28.__id;
},getIdentityAttributes:function(_29){
return [this.idAttribute];
},fetchItemByIdentity:function(_2a){
var _2b=this._index[(_2a._prefix||"")+_2a.identity];
if(_2b){
if(_2b._loadObject){
_2a.item=_2b;
return this.loadItem(_2a);
}else{
if(_2a.onItem){
_2a.onItem.call(_2a.scope,_2b);
}
}
}else{
return this.fetch({query:_2a.identity,onComplete:_2a.onItem,onError:_2a.onError,scope:_2a.scope}).results;
}
return _2b;
}});
});
