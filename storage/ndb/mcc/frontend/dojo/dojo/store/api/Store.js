/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/store/api/Store",["dojo/_base/declare"],function(_1){
var _2=_1("dojo.store.api.Store",null,{idProperty:"id",queryEngine:null,get:function(id){
},getIdentity:function(_3){
},put:function(_4,_5){
},add:function(_6,_7){
},remove:function(id){
delete this.index[id];
var _8=this.data,_9=this.idProperty;
for(var i=0,l=_8.length;i<l;i++){
if(_8[i][_9]==id){
_8.splice(i,1);
return;
}
}
},query:function(_a,_b){
},transaction:function(){
},getChildren:function(_c,_d){
},getMetadata:function(_e){
}});
_2.PutDirectives=function(id,_f,_10,_11){
this.id=id;
this.before=_f;
this.parent=_10;
this.overwrite=_11;
};
_2.SortInformation=function(_12,_13){
this.attribute=_12;
this.descending=_13;
};
_2.QueryOptions=function(_14,_15,_16){
this.sort=_14;
this.start=_15;
this.count=_16;
};
_1("dojo.store.api.Store.QueryResults",null,{forEach:function(_17,_18){
},filter:function(_19,_1a){
},map:function(_1b,_1c){
},then:function(_1d,_1e){
},observe:function(_1f,_20){
},total:0});
_1("dojo.store.api.Store.Transaction",null,{commit:function(){
},abort:function(_21,_22){
}});
return _2;
});
