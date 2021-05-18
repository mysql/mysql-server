//>>built
define("dojox/grid/enhanced/plugins/Search",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/data/util/filter","../../EnhancedGrid","../_Plugin"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_3("dojox.grid.enhanced.plugins.Search",_7,{name:"search",constructor:function(_9,_a){
this.grid=_9;
_a=(_a&&_2.isObject(_a))?_a:{};
this._cacheSize=_a.cacheSize||-1;
_9.searchRow=_2.hitch(this,"searchRow");
},searchRow:function(_b,_c){
if(!_2.isFunction(_c)){
return;
}
if(_2.isString(_b)){
_b=_5.patternToRegExp(_b);
}
var _d=false;
if(_b instanceof RegExp){
_d=true;
}else{
if(_2.isObject(_b)){
var _e=true;
for(var _f in _b){
if(_2.isString(_b[_f])){
_b[_f]=_5.patternToRegExp(_b[_f]);
}
_e=false;
}
if(_e){
return;
}
}else{
return;
}
}
this._search(_b,0,_c,_d);
},_search:function(_10,_11,_12,_13){
var _14=this,cnt=this._cacheSize,_15={start:_11,query:this.grid.query,sort:this.grid.getSortProps(),queryOptions:this.grid.queryOptions,onBegin:function(_16){
_14._storeSize=_16;
},onComplete:function(_17){
if(!_4.some(_17,function(_18,i){
if(_14._checkRow(_18,_10,_13)){
_12(_11+i,_18);
return true;
}
return false;
})){
if(cnt>0&&_11+cnt<_14._storeSize){
_14._search(_10,_11+cnt,_12,_13);
}else{
_12(-1,null);
}
}
}};
if(cnt>0){
_15.count=cnt;
}
this.grid._storeLayerFetch(_15);
},_checkRow:function(_19,_1a,_1b){
var g=this.grid,s=g.store,i,_1c,_1d=_4.filter(g.layout.cells,function(_1e){
return !_1e.hidden;
});
if(_1b){
return _4.some(_1d,function(_1f){
try{
if(_1f.field){
return String(s.getValue(_19,_1f.field)).search(_1a)>=0;
}
}
catch(e){
}
return false;
});
}else{
for(_1c in _1a){
if(_1a[_1c] instanceof RegExp){
for(i=_1d.length-1;i>=0;--i){
if(_1d[i].field==_1c){
try{
if(String(s.getValue(_19,_1c)).search(_1a[_1c])<0){
return false;
}
break;
}
catch(e){
return false;
}
}
}
if(i<0){
return false;
}
}
}
return true;
}
}});
_6.registerPlugin(_8);
return _8;
});
