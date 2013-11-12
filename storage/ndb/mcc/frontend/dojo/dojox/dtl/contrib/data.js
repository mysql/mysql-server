//>>built
define("dojox/dtl/contrib/data",["dojo/_base/kernel","dojo/_base/lang","../_base","dojo/_base/array"],function(_1,_2,dd,_3){
_2.getObject("dojox.dtl.contrib.data",true);
var _4=dd.contrib.data;
var _5=true;
_4._BoundItem=_2.extend(function(_6,_7){
this.item=_6;
this.store=_7;
},{get:function(_8){
var _9=this.store;
var _a=this.item;
if(_8=="getLabel"){
return _9.getLabel(_a);
}else{
if(_8=="getAttributes"){
return _9.getAttributes(_a);
}else{
if(_8=="getIdentity"){
if(_9.getIdentity){
return _9.getIdentity(_a);
}
return "Store has no identity API";
}else{
if(!_9.hasAttribute(_a,_8)){
if(_8.slice(-1)=="s"){
if(_5){
_5=false;
_1.deprecated("You no longer need an extra s to call getValues, it can be figured out automatically");
}
_8=_8.slice(0,-1);
}
if(!_9.hasAttribute(_a,_8)){
return;
}
}
var _b=_9.getValues(_a,_8);
if(!_b){
return;
}
if(!_2.isArray(_b)){
return new _4._BoundItem(_b,_9);
}
_b=_3.map(_b,function(_c){
if(_2.isObject(_c)&&_9.isItem(_c)){
return new _4._BoundItem(_c,_9);
}
return _c;
});
_b.get=_4._get;
return _b;
}
}
}
}});
_4._BoundItem.prototype.get.safe=true;
_4.BindDataNode=_2.extend(function(_d,_e,_f,_10){
this.items=_d&&new dd._Filter(_d);
this.query=_e&&new dd._Filter(_e);
this.store=new dd._Filter(_f);
this.alias=_10;
},{render:function(_11,_12){
var _13=this.items&&this.items.resolve(_11);
var _14=this.query&&this.query.resolve(_11);
var _15=this.store.resolve(_11);
if(!_15||!_15.getFeatures){
throw new Error("data_bind didn't receive a store");
}
if(_14){
var _16=false;
_15.fetch({query:_14,sync:true,scope:this,onComplete:function(it){
_16=true;
_13=it;
}});
if(!_16){
throw new Error("The bind_data tag only works with a query if the store executed synchronously");
}
}
var _17=[];
if(_13){
for(var i=0,_18;_18=_13[i];i++){
_17.push(new _4._BoundItem(_18,_15));
}
}
_11[this.alias]=_17;
return _12;
},unrender:function(_19,_1a){
return _1a;
},clone:function(){
return this;
}});
_2.mixin(_4,{_get:function(key){
if(this.length){
return (this[0] instanceof _4._BoundItem)?this[0].get(key):this[0][key];
}
},bind_data:function(_1b,_1c){
var _1d=_1c.contents.split();
if(_1d[2]!="to"||_1d[4]!="as"||!_1d[5]){
throw new Error("data_bind expects the format: 'data_bind items to store as varName'");
}
return new _4.BindDataNode(_1d[1],null,_1d[3],_1d[5]);
},bind_query:function(_1e,_1f){
var _20=_1f.contents.split();
if(_20[2]!="to"||_20[4]!="as"||!_20[5]){
throw new Error("data_bind expects the format: 'bind_query query to store as varName'");
}
return new _4.BindDataNode(null,_20[1],_20[3],_20[5]);
}});
_4._get.safe=true;
dd.register.tags("dojox.dtl.contrib",{"data":["bind_data","bind_query"]});
return dojox.dtl.contrib.data;
});
