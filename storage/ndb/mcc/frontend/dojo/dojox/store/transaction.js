//>>built
define("dojox/store/transaction",["dojo/store/Memory","dojo/store/Cache","dojo/when","dojo/aspect","dojo/_base/lang"],function(_1,_2,_3,_4,_5){
var _6;
var _7={};
var _8=1;
return function(_9){
_9=_9||{};
var _a=_9.masterStore;
var _b=_9.cachingStore;
var _c=_a.id||_a.storeName||_a.name||(_a.id=_8++);
if(_c){
_7[_c]=_a;
}
var _d=_9.transactionLogStore||_6||(_6=new _1());
var _e=true;
function _f(_10){
return function execute(_11,_12){
var _13=this;
if(_e){
var _14=_a[_10](_11,_12);
_3(_14,null,function(e){
if(_13.errorHandler(e)){
_e=false;
_12.error=e;
_15.call(_13,_11,_12);
_e=true;
}
});
return _14;
}else{
var _16=_10==="remove"?_11:_13.getIdentity(_11);
if(_16!==undefined){
var _17=_b.get(_16);
}
return _3(_17,function(_18){
return _3(_d.add({objectId:_16,method:_10,target:_11,previous:_18,options:_12,storeId:_c}),function(){
return _11;
});
});
}
};
};
_4.before(_a,"notify",function(_19,_1a){
if(_19){
_b.put(_19);
}else{
_b.remove(_1a);
}
});
return new _2(_5.delegate(_a,{put:_f("put"),add:_f("add"),remove:_f("remove"),errorHandler:function(_1b){
console.error(_1b);
return true;
},commit:function(){
_e=true;
var _1c=this;
return _d.query({}).map(function(_1d){
var _1e=_1d.method;
var _1f=_7[_1d.storeId];
var _20=_1d.target;
var _21;
try{
_21=_1f[_1e](_20,_1d.options);
}
catch(e){
_21=_1c.errorHandler(e);
if(_21===true){
return e;
}else{
if(_21===false){
if(_1e==="add"){
_b.remove(_1d.objectId);
}else{
_b.put(_20);
}
_1f.notify&&_1f.notify(_1e==="add"?null:_1d.previous,_1e==="remove"?undefined:_1d.objectId);
}
}
_21=e;
}
_d.remove(_1d.id);
return _21;
});
},transaction:function(){
_e=false;
var _22=this;
return {commit:function(){
return _22.commit();
}};
}}),_b,_9);
};
});
