//>>built
define("dojox/grid/enhanced/plugins/_RowMapLayer",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","./_StoreLayer"],function(_1,_2,_3,_4){
var _5=function(a){
a.sort(function(v1,v2){
return v1-v2;
});
var _6=[[a[0]]];
for(var i=1,j=0;i<a.length;++i){
if(a[i]==a[i-1]+1){
_6[j].push(a[i]);
}else{
_6[++j]=[a[i]];
}
}
return _6;
},_7=function(_8,_9){
return _9?_3.hitch(_8||_3.global,_9):function(){
};
};
return _1("dojox.grid.enhanced.plugins._RowMapLayer",_4._StoreLayer,{tags:["reorder"],constructor:function(_a){
this._map={};
this._revMap={};
this.grid=_a;
this._oldOnDelete=_a._onDelete;
var _b=this;
_a._onDelete=function(_c){
_b._onDelete(_c);
_b._oldOnDelete.call(_a,_c);
};
this._oldSort=_a.sort;
_a.sort=function(){
_b.clearMapping();
_b._oldSort.apply(_a,arguments);
};
},uninitialize:function(){
this.grid._onDelete=this._oldOnDelete;
this.grid.sort=this._oldSort;
},setMapping:function(_d){
this._store.forEachLayer(function(_e){
if(_e.name()==="rowmap"){
return false;
}else{
if(_e.onRowMappingChange){
_e.onRowMappingChange(_d);
}
}
return true;
},false);
var _f,to,_10,_11={};
for(_f in _d){
_f=parseInt(_f,10);
to=_d[_f];
if(typeof to=="number"){
if(_f in this._revMap){
_10=this._revMap[_f];
delete this._revMap[_f];
}else{
_10=_f;
}
if(_10==to){
delete this._map[_10];
_11[to]="eq";
}else{
this._map[_10]=to;
_11[to]=_10;
}
}
}
for(to in _11){
if(_11[to]==="eq"){
delete this._revMap[parseInt(to,10)];
}else{
this._revMap[parseInt(to,10)]=_11[to];
}
}
},clearMapping:function(){
this._map={};
this._revMap={};
},_onDelete:function(_12){
var idx=this.grid._getItemIndex(_12,true);
if(idx in this._revMap){
var _13=[],r,i,_14=this._revMap[idx];
delete this._map[_14];
delete this._revMap[idx];
for(r in this._revMap){
r=parseInt(r,10);
if(this._revMap[r]>_14){
--this._revMap[r];
}
}
for(r in this._revMap){
r=parseInt(r,10);
if(r>idx){
_13.push(r);
}
}
_13.sort(function(a,b){
return b-a;
});
for(i=_13.length-1;i>=0;--i){
r=_13[i];
this._revMap[r-1]=this._revMap[r];
delete this._revMap[r];
}
this._map={};
for(r in this._revMap){
this._map[this._revMap[r]]=r;
}
}
},_fetch:function(_15){
var _16=0,r;
var _17=_15.start||0;
for(r in this._revMap){
r=parseInt(r,10);
if(r>=_17){
++_16;
}
}
if(_16>0){
var _18=[],i,map={},_19=_15.count>0?_15.count:-1;
if(_19>0){
for(i=0;i<_19;++i){
r=_17+i;
r=r in this._revMap?this._revMap[r]:r;
map[r]=i;
_18.push(r);
}
}else{
for(i=0;;++i){
r=_17+i;
if(r in this._revMap){
--_16;
r=this._revMap[r];
}
map[r]=i;
_18.push(r);
if(_16<=0){
break;
}
}
}
this._subFetch(_15,this._getRowArrays(_18),0,[],map,_15.onComplete,_17,_19);
return _15;
}else{
return _3.hitch(this._store,this._originFetch)(_15);
}
},_getRowArrays:function(_1a){
return _5(_1a);
},_subFetch:function(_1b,_1c,_1d,_1e,map,_1f,_20,_21){
var arr=_1c[_1d],_22=this;
var _23=_1b.start=arr[0];
_1b.count=arr[arr.length-1]-arr[0]+1;
_1b.onComplete=function(_24){
_2.forEach(_24,function(_25,i){
var r=_23+i;
if(r in map){
_1e[map[r]]=_25;
}
});
if(++_1d==_1c.length){
if(_21>0){
_1b.start=_20;
_1b.count=_21;
_1b.onComplete=_1f;
_7(_1b.scope,_1f)(_1e,_1b);
}else{
_1b.start=_1b.start+_24.length;
delete _1b.count;
_1b.onComplete=function(_26){
_1e=_1e.concat(_26);
_1b.start=_20;
_1b.onComplete=_1f;
_7(_1b.scope,_1f)(_1e,_1b);
};
_22.originFetch(_1b);
}
}else{
_22._subFetch(_1b,_1c,_1d,_1e,map,_1f,_20,_21);
}
};
_22.originFetch(_1b);
}});
});
