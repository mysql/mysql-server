//>>built
define("dojox/data/ClientFilter",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/Deferred","dojo/data/util/filter"],function(_1,_2,_3,_4,_5){
var _6=function(_7,_8,_9){
return function(_a){
_7._updates.push({create:_8&&_a,remove:_9&&_a});
_b.onUpdate();
};
};
var _b=_1("dojox.data.ClientFilter",null,{cacheByDefault:false,constructor:function(){
this.onSet=_6(this,true,true);
this.onNew=_6(this,true,false);
this.onDelete=_6(this,false,true);
this._updates=[];
this._fetchCache=[];
},clearCache:function(){
this._fetchCache=[];
},updateResultSet:function(_c,_d){
if(this.isUpdateable(_d)){
for(var i=_d._version||0;i<this._updates.length;i++){
var _e=this._updates[i].create;
var _f=this._updates[i].remove;
if(_f){
for(var j=0;j<_c.length;j++){
if(this.getIdentity(_c[j])==this.getIdentity(_f)){
_c.splice(j--,1);
var _10=true;
}
}
}
if(_e&&this.matchesQuery(_e,_d)&&_3.indexOf(_c,_e)==-1){
_c.push(_e);
_10=true;
}
}
if(_d.sort&&_10){
_c.sort(this.makeComparator(_d.sort.concat()));
}
_c._fullLength=_c.length;
if(_d.count&&_10&&_d.count!==Infinity){
_c.splice(_d.count,_c.length);
}
_d._version=this._updates.length;
return _10?2:1;
}
return 0;
},querySuperSet:function(_11,_12){
if(_11.query==_12.query){
return {};
}
if(!(_12.query instanceof Object&&(!_11.query||typeof _11.query=="object"))){
return false;
}
var _13=_2.mixin({},_12.query);
for(var i in _11.query){
if(_13[i]==_11.query[i]){
delete _13[i];
}else{
if(!(typeof _11.query[i]=="string"&&_5.patternToRegExp(_11.query[i]).test(_13[i]))){
return false;
}
}
}
return _13;
},serverVersion:0,cachingFetch:function(_14){
var _15=this;
for(var i=0;i<this._fetchCache.length;i++){
var _16=this._fetchCache[i];
var _17=this.querySuperSet(_16,_14);
if(_17!==false){
var _18=_16._loading;
if(!_18){
_18=new _4();
_18.callback(_16.cacheResults);
}
_18.addCallback(function(_19){
_19=_15.clientSideFetch(_2.mixin(_2.mixin({},_14),{query:_17}),_19);
_18.fullLength=_19._fullLength;
return _19;
});
_14._version=_16._version;
break;
}
}
if(!_18){
var _1a=_2.mixin({},_14);
var _1b=(_14.queryOptions||0).cache;
var _1c=this._fetchCache;
if(_1b===undefined?this.cacheByDefault:_1b){
if(_14.start||_14.count){
delete _1a.start;
delete _1a.count;
_14.clientQuery=_2.mixin(_14.clientQuery||{},{start:_14.start,count:_14.count});
}
_14=_1a;
_1c.push(_14);
}
_18=_14._loading=this._doQuery(_14);
_18.addErrback(function(){
_1c.splice(_3.indexOf(_1c,_14),1);
});
}
var _1d=this.serverVersion;
_18.addCallback(function(_1e){
delete _14._loading;
if(_1e){
_14._version=typeof _14._version=="number"?_14._version:_1d;
_15.updateResultSet(_1e,_14);
_14.cacheResults=_1e;
if(!_14.count||_1e.length<_14.count){
_18.fullLength=((_14.start)?_14.start:0)+_1e.length;
}
}
return _1e;
});
return _18;
},isUpdateable:function(_1f){
return !_1f.query||typeof _1f.query=="object";
},clientSideFetch:function(_20,_21){
if(_20.queryOptions&&_20.queryOptions.results){
_21=_20.queryOptions.results;
}
if(_20.query){
var _22=[];
for(var i=0;i<_21.length;i++){
var _23=_21[i];
if(_23&&this.matchesQuery(_23,_20)){
_22.push(_21[i]);
}
}
}else{
_22=_20.sort?_21.concat():_21;
}
if(_20.sort){
_22.sort(this.makeComparator(_20.sort.concat()));
}
return this.clientSidePaging(_20,_22);
},clientSidePaging:function(_24,_25){
var _26=_24.start||0;
var _27=(_26||_24.count)?_25.slice(_26,_26+(_24.count||_25.length)):_25;
_27._fullLength=_25.length;
return _27;
},matchesQuery:function(_28,_29){
var _2a=_29.query;
var _2b=_29.queryOptions&&_29.queryOptions.ignoreCase;
for(var i in _2a){
var _2c=_2a[i];
var _2d=this.getValue(_28,i);
if((typeof _2c=="string"&&(_2c.match(/[\*\.]/)||_2b))?!_5.patternToRegExp(_2c,_2b).test(_2d):_2d!=_2c){
return false;
}
}
return true;
},makeComparator:function(_2e){
var _2f=_2e.shift();
if(!_2f){
return function(){
return 0;
};
}
var _30=_2f.attribute;
var _31=!!_2f.descending;
var _32=this.makeComparator(_2e);
var _33=this;
return function(a,b){
var av=_33.getValue(a,_30);
var bv=_33.getValue(b,_30);
if(av!=bv){
return av<bv==_31?1:-1;
}
return _32(a,b);
};
}});
_b.onUpdate=function(){
};
return _b;
});
