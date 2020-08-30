//>>built
define("dojox/grid/enhanced/plugins/_RowMapLayer",["dojo/_base/array","dojo/_base/declare","dojo/_base/kernel","dojo/_base/lang","./_StoreLayer"],function(_1,_2,_3,_4,_5){
var _6=function(a){
a.sort(function(v1,v2){
return v1-v2;
});
var _7=[[a[0]]];
for(var i=1,j=0;i<a.length;++i){
if(a[i]==a[i-1]+1){
_7[j].push(a[i]);
}else{
_7[++j]=[a[i]];
}
}
return _7;
},_8=function(_9,_a){
return _a?_4.hitch(_9||_3.global,_a):function(){
};
};
return _2("dojox.grid.enhanced.plugins._RowMapLayer",_5._StoreLayer,{tags:["reorder"],constructor:function(_b){
this._map={};
this._revMap={};
this.grid=_b;
this._oldOnDelete=_b._onDelete;
var _c=this;
_b._onDelete=function(_d){
_c._onDelete(_d);
_c._oldOnDelete.call(_b,_d);
};
this._oldSort=_b.sort;
_b.sort=function(){
_c.clearMapping();
_c._oldSort.apply(_b,arguments);
};
},uninitialize:function(){
this.grid._onDelete=this._oldOnDelete;
this.grid.sort=this._oldSort;
},setMapping:function(_e){
this._store.forEachLayer(function(_f){
if(_f.name()==="rowmap"){
return false;
}else{
if(_f.onRowMappingChange){
_f.onRowMappingChange(_e);
}
}
return true;
},false);
var _10,to,_11,_12={};
for(_10 in _e){
_10=parseInt(_10,10);
to=_e[_10];
if(typeof to=="number"){
if(_10 in this._revMap){
_11=this._revMap[_10];
delete this._revMap[_10];
}else{
_11=_10;
}
if(_11==to){
delete this._map[_11];
_12[to]="eq";
}else{
this._map[_11]=to;
_12[to]=_11;
}
}
}
for(to in _12){
if(_12[to]==="eq"){
delete this._revMap[parseInt(to,10)];
}else{
this._revMap[parseInt(to,10)]=_12[to];
}
}
},clearMapping:function(){
this._map={};
this._revMap={};
},_onDelete:function(_13){
var idx=this.grid._getItemIndex(_13,true);
if(idx in this._revMap){
var _14=[],r,i,_15=this._revMap[idx];
delete this._map[_15];
delete this._revMap[idx];
for(r in this._revMap){
r=parseInt(r,10);
if(this._revMap[r]>_15){
--this._revMap[r];
}
}
for(r in this._revMap){
r=parseInt(r,10);
if(r>idx){
_14.push(r);
}
}
_14.sort(function(a,b){
return b-a;
});
for(i=_14.length-1;i>=0;--i){
r=_14[i];
this._revMap[r-1]=this._revMap[r];
delete this._revMap[r];
}
this._map={};
for(r in this._revMap){
this._map[this._revMap[r]]=r;
}
}
},_fetch:function(_16){
var _17=0,r;
var _18=_16.start||0;
for(r in this._revMap){
r=parseInt(r,10);
if(r>=_18){
++_17;
}
}
if(_17>0){
var _19=[],i,map={},_1a=_16.count>0?_16.count:-1;
if(_1a>0){
for(i=0;i<_1a;++i){
r=_18+i;
r=r in this._revMap?this._revMap[r]:r;
map[r]=i;
_19.push(r);
}
}else{
for(i=0;;++i){
r=_18+i;
if(r in this._revMap){
--_17;
r=this._revMap[r];
}
map[r]=i;
_19.push(r);
if(_17<=0){
break;
}
}
}
this._subFetch(_16,this._getRowArrays(_19),0,[],map,_16.onComplete,_18,_1a);
return _16;
}else{
return _4.hitch(this._store,this._originFetch)(_16);
}
},_getRowArrays:function(_1b){
return _6(_1b);
},_subFetch:function(_1c,_1d,_1e,_1f,map,_20,_21,_22){
var arr=_1d[_1e],_23=this;
var _24=_1c.start=arr[0];
_1c.count=arr[arr.length-1]-arr[0]+1;
_1c.onComplete=function(_25){
_1.forEach(_25,function(_26,i){
var r=_24+i;
if(r in map){
_1f[map[r]]=_26;
}
});
if(++_1e==_1d.length){
if(_22>0){
_1c.start=_21;
_1c.count=_22;
_1c.onComplete=_20;
_8(_1c.scope,_20)(_1f,_1c);
}else{
_1c.start=_1c.start+_25.length;
delete _1c.count;
_1c.onComplete=function(_27){
_1f=_1f.concat(_27);
_1c.start=_21;
_1c.onComplete=_20;
_8(_1c.scope,_20)(_1f,_1c);
};
_23.originFetch(_1c);
}
}else{
_23._subFetch(_1c,_1d,_1e,_1f,map,_20,_21,_22);
}
};
_23.originFetch(_1c);
}});
});
