//>>built
define("dojox/grid/enhanced/plugins/filter/FilterLayer",["dojo/_base/declare","dojo/_base/lang","dojo/_base/kernel","dojo/_base/json","../_StoreLayer"],function(_1,_2,_3,_4,_5){
var _6="filter",_7="clear",_8=function(_9,_a){
return _a?_2.hitch(_9||_3.global,_a):function(){
};
},_b=function(_c){
var _d={};
if(_c&&_2.isObject(_c)){
for(var _e in _c){
_d[_e]=_c[_e];
}
}
return _d;
};
var _f=_1("dojox.grid.enhanced.plugins.filter._FilterLayerMixin",null,{tags:["sizeChange"],name:function(){
return "filter";
},onFilterDefined:function(_10){
},onFiltered:function(_11,_12){
}});
var _13=_1("dojox.grid.enhanced.plugins.filter.ServerSideFilterLayer",[_5._ServerSideLayer,_f],{constructor:function(_14){
this._onUserCommandLoad=_14.setupFilterQuery||this._onUserCommandLoad;
this.filterDef(null);
},filterDef:function(_15){
if(_15){
this._filter=_15;
var obj=_15.toObject();
this.command(_6,this._isStateful?_4.toJson(obj):obj);
this.command(_7,null);
this.useCommands(true);
this.onFilterDefined(_15);
}else{
if(_15===null){
this._filter=null;
this.command(_6,null);
this.command(_7,true);
this.useCommands(true);
this.onFilterDefined(null);
}
}
return this._filter;
},onCommandLoad:function(_16,_17){
this.inherited(arguments);
var _18=_17.onBegin;
if(this._isStateful){
var _19;
if(_16){
this.command(_6,null);
this.command(_7,null);
this.useCommands(false);
var _1a=_16.split(",");
if(_1a.length>=2){
_19=this._filteredSize=parseInt(_1a[0],10);
this.onFiltered(_19,parseInt(_1a[1],10));
}else{
return;
}
}else{
_19=this._filteredSize;
}
if(this.enabled()){
_17.onBegin=function(_1b,req){
_8(_17.scope,_18)(_19,req);
};
}
}else{
var _1c=this;
_17.onBegin=function(_1d,req){
if(!_1c._filter){
_1c._storeSize=_1d;
}
_1c.onFiltered(_1d,_1c._storeSize||_1d);
req.onBegin=_18;
_8(_17.scope,_18)(_1d,req);
};
}
}});
var _1e=_1("dojox.grid.enhanced.plugins.filter.ClientSideFilterLayer",[_5._StoreLayer,_f],{_storeSize:-1,_fetchAll:true,constructor:function(_1f){
this.filterDef(null);
_1f=_2.isObject(_1f)?_1f:{};
this.fetchAllOnFirstFilter(_1f.fetchAll);
this._getter=_2.isFunction(_1f.getter)?_1f.getter:this._defaultGetter;
},_defaultGetter:function(_20,_21,_22,_23){
return _23.getValue(_20,_21);
},filterDef:function(_24){
if(_24!==undefined){
this._filter=_24;
this.invalidate();
this.onFilterDefined(_24);
}
return this._filter;
},setGetter:function(_25){
if(_2.isFunction(_25)){
this._getter=_25;
}
},fetchAllOnFirstFilter:function(_26){
if(_26!==undefined){
this._fetchAll=!!_26;
}
return this._fetchAll;
},invalidate:function(){
this._items=[];
this._nextUnfetchedIdx=0;
this._result=[];
this._indexMap=[];
this._resultStartIdx=0;
},_fetch:function(_27,_28){
if(!this._filter){
var _29=_27.onBegin,_2a=this;
_27.onBegin=function(_2b,r){
_8(_27.scope,_29)(_2b,r);
_2a.onFiltered(_2b,_2b);
};
this.originFetch(_27);
return _27;
}
try{
var _2c=_28?_28._nextResultItemIdx:_27.start;
_2c=_2c||0;
if(!_28){
this._result=[];
this._resultStartIdx=_2c;
var _2d;
if(_2.isArray(_27.sort)&&_27.sort.length>0&&(_2d=_4.toJson(_27.sort))!=this._lastSortInfo){
this.invalidate();
this._lastSortInfo=_2d;
}
}
var end=typeof _27.count=="number"?_2c+_27.count-this._result.length:this._items.length;
if(this._result.length){
this._result=this._result.concat(this._items.slice(_2c,end));
}else{
this._result=this._items.slice(_27.start,typeof _27.count=="number"?_27.start+_27.count:this._items.length);
}
if(this._result.length>=_27.count||this._hasReachedStoreEnd()){
this._completeQuery(_27);
}else{
if(!_28){
_28=_b(_27);
_28.onBegin=_2.hitch(this,this._onFetchBegin);
_28.onComplete=_2.hitch(this,function(_2e,req){
this._nextUnfetchedIdx+=_2e.length;
this._doFilter(_2e,req.start,_27);
this._fetch(_27,req);
});
}
_28.start=this._nextUnfetchedIdx;
if(this._fetchAll){
delete _28.count;
}
_28._nextResultItemIdx=end<this._items.length?end:this._items.length;
this.originFetch(_28);
}
}
catch(e){
if(_27.onError){
_8(_27.scope,_27.onError)(e,_27);
}else{
throw e;
}
}
return _27;
},_hasReachedStoreEnd:function(){
return this._storeSize>=0&&this._nextUnfetchedIdx>=this._storeSize;
},_applyFilter:function(_2f,_30){
var g=this._getter,s=this._store;
try{
return !!(this._filter.applyRow(_2f,function(_31,arg){
return g(_31,arg,_30,s);
}).getValue());
}
catch(e){
console.warn("FilterLayer._applyFilter() error: ",e);
return false;
}
},_doFilter:function(_32,_33,_34){
for(var i=0,cnt=0;i<_32.length;++i){
if(this._applyFilter(_32[i],_33+i)){
_8(_34.scope,_34.onItem)(_32[i],_34);
cnt+=this._addCachedItems(_32[i],this._items.length);
this._indexMap.push(_33+i);
}
}
},_onFetchBegin:function(_35,req){
this._storeSize=_35;
},_completeQuery:function(_36){
var _37=this._items.length;
if(this._nextUnfetchedIdx<this._storeSize){
_37++;
}
_8(_36.scope,_36.onBegin)(_37,_36);
this.onFiltered(this._items.length,this._storeSize);
_8(_36.scope,_36.onComplete)(this._result,_36);
},_addCachedItems:function(_38,_39){
if(!_2.isArray(_38)){
_38=[_38];
}
for(var k=0;k<_38.length;++k){
this._items[_39+k]=_38[k];
}
return _38.length;
},onRowMappingChange:function(_3a){
if(this._filter){
var m=_2.clone(_3a),_3b={};
for(var r in m){
r=parseInt(r,10);
_3a[this._indexMap[r]]=this._indexMap[m[r]];
if(!_3b[this._indexMap[r]]){
_3b[this._indexMap[r]]=true;
}
if(!_3b[r]){
_3b[r]=true;
delete _3a[r];
}
}
}
}});
return _2.mixin({ServerSideFilterLayer:_13,ClientSideFilterLayer:_1e},_5);
});
