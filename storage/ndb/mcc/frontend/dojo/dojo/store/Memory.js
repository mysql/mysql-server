/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/store/Memory",["../_base/declare","./util/QueryResults","./util/SimpleQueryEngine"],function(_1,_2,_3){
return _1("dojo.store.Memory",null,{constructor:function(_4){
for(var i in _4){
this[i]=_4[i];
}
this.setData(this.data||[]);
},data:null,idProperty:"id",index:null,queryEngine:_3,get:function(id){
return this.data[this.index[id]];
},getIdentity:function(_5){
return _5[this.idProperty];
},put:function(_6,_7){
var _8=this.data,_9=this.index,_a=this.idProperty;
var id=(_7&&"id" in _7)?_7.id:_a in _6?_6[_a]:Math.random();
if(id in _9){
if(_7&&_7.overwrite===false){
throw new Error("Object already exists");
}
_8[_9[id]]=_6;
}else{
_9[id]=_8.push(_6)-1;
}
return id;
},add:function(_b,_c){
(_c=_c||{}).overwrite=false;
return this.put(_b,_c);
},remove:function(id){
var _d=this.index;
var _e=this.data;
if(id in _d){
_e.splice(_d[id],1);
this.setData(_e);
return true;
}
},query:function(_f,_10){
return _2(this.queryEngine(_f,_10)(this.data));
},setData:function(_11){
if(_11.items){
this.idProperty=_11.identifier;
_11=this.data=_11.items;
}else{
this.data=_11;
}
this.index={};
for(var i=0,l=_11.length;i<l;i++){
this.index[_11[i][this.idProperty]]=i;
}
}});
});
