//>>built
define("dojox/app/utils/mvcModel",["dojo/_base/lang","dojo/Deferred","dojo/when","dojox/mvc/getStateful"],function(_1,_2,_3,_4){
return function(_5,_6,_7){
var _8={};
var _9=new _2();
var _a=function(_b){
var _c={};
for(var _d in _b){
if(_d.charAt(0)!=="_"){
_c[_d]=_b[_d];
}
}
return (_c);
};
var _e;
if(_6.store){
_e={"store":_6.store.store,"query":_6.query?_a(_6.query):_6.store.query?_a(_6.store.query):{}};
}else{
if(_6.datastore){
try{
var _f=require("dojo/store/DataStore");
}
catch(e){
throw new Error("When using datastore the dojo/store/DataStore module must be listed in the dependencies");
}
_e={"store":new _f({store:_6.datastore.store}),"query":_a(_6.query)};
}else{
if(_6.data){
if(_6.data&&_1.isString(_6.data)){
_6.data=_1.getObject(_6.data);
}
_e={"data":_6.data,query:{}};
}else{
console.warn("mvcModel: Missing parameters.");
}
}
}
var _10=_5[_7].type?_5[_7].type:"dojox/mvc/EditStoreRefListController";
try{
var _11=require(_10);
}
catch(e){
throw new Error(_10+" must be listed in the dependencies");
}
var _12=new _11(_e);
var _13;
try{
if(_12.queryStore){
_13=_12.queryStore(_e.query);
}else{
var _14=_12._refSourceModelProp||_12._refModelProp||"model";
_12.set(_14,_4(_e.data));
_13=_12;
}
}
catch(ex){
_9.reject("load mvc model error.");
return _9.promise;
}
_3(_13,_1.hitch(this,function(){
_8=_12;
_9.resolve(_8);
return _8;
}),function(){
_9.reject("load model error.");
});
return _9;
};
});
