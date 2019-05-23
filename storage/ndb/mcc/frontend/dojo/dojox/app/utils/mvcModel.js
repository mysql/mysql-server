//>>built
define("dojox/app/utils/mvcModel",["dojo/_base/lang","dojo/Deferred","dojo/when","dojo/_base/config","dojo/store/DataStore","dojox/mvc/getStateful","dojo/has"],function(_1,_2,_3,_4,_5,_6,_7){
return function(_8,_9,_a){
var _b={};
var _c=new _2();
var _d=function(_e){
var _f={};
for(var _10 in _e){
if(_10.charAt(0)!=="_"){
_f[_10]=_e[_10];
}
}
return (_f);
};
var _11;
if(_9.store){
_11={"store":_9.store.store,"query":_9.query?_d(_9.query):_9.store.query?_d(_9.store.query):{}};
}else{
if(_9.datastore){
_11={"store":new _5({store:_9.datastore.store}),"query":_d(_9.query)};
}else{
if(_9.data){
if(_9.data&&_1.isString(_9.data)){
_9.data=_1.getObject(_9.data);
}
_11={"data":_9.data,query:{}};
}
}
}
var _12=null;
var _13=_8[_a].type?_8[_a].type:"dojox/mvc/EditStoreRefListController";
var def=new _2();
require([_13],function(_14){
def.resolve(_14);
});
_3(def,function(_15){
_12=new _15(_11);
var _16;
try{
if(_12.queryStore){
_16=_12.queryStore(_11.query);
}else{
var _17=_12._refSourceModelProp||_12._refModelProp||"model";
_12.set(_17,_6(_11.data));
_16=_12;
}
}
catch(ex){
_c.reject("load mvc model error.");
return _c.promise;
}
if(_16.then){
_3(_16,_1.hitch(this,function(){
_b=_12;
_c.resolve(_b);
return _b;
}),function(){
loadModelLoaderDeferred.reject("load model error.");
});
}else{
_b=_12;
_c.resolve(_b);
return _b;
}
});
return _c;
};
});
