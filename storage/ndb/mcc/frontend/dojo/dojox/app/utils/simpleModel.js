//>>built
define("dojox/app/utils/simpleModel",["dojo/_base/lang","dojo/Deferred","dojo/when"],function(_1,_2,_3){
return function(_4,_5,_6){
var _7={};
var _8=new _2();
var _9=function(_a){
var _b={};
for(var _c in _a){
if(_c.charAt(0)!=="_"){
_b[_c]=_a[_c];
}
}
return (_b);
};
var _d,_e;
if(_5.store){
if(!_5.store.params){
throw new Error("Invalid store for model ["+_6+"]");
}else{
if((_5.store.params.data||_5.store.params.store)){
_d={"store":_5.store.store,"query":_5.query?_9(_5.query):_5.store.query?_9(_5.store.query):{}};
}else{
if(_5.store.params.url){
try{
_e=require("dojo/store/DataStore");
}
catch(e){
throw new Error("dojo/store/DataStore must be listed in the dependencies");
}
_d={"store":new _e({store:_5.store.store}),"query":_5.query?_9(_5.query):_5.store.query?_9(_5.store.query):{}};
}else{
if(_5.store.store){
_d={"store":_5.store.store,"query":_5.query?_9(_5.query):_5.store.query?_9(_5.store.query):{}};
}
}
}
}
}else{
if(_5.datastore){
try{
_e=require("dojo/store/DataStore");
}
catch(e){
throw new Error("When using datastore the dojo/store/DataStore module must be listed in the dependencies");
}
_d={"store":new _e({store:_5.datastore.store}),"query":_9(_5.query)};
}else{
if(_5.data){
if(_5.data&&_1.isString(_5.data)){
_5.data=_1.getObject(_5.data);
}
_d={"data":_5.data,query:{}};
}else{
console.warn("simpleModel: Missing parameters.");
}
}
}
var _f;
try{
if(_d.store){
_f=_d.store.query();
}else{
_f=_d.data;
}
}
catch(ex){
_8.reject("load simple model error.");
return _8.promise;
}
if(_f.then){
_3(_f,_1.hitch(this,function(_10){
_7=_10;
_8.resolve(_7);
return _7;
}),function(){
loadModelLoaderDeferred.reject("load model error.");
});
}else{
_7=_f;
_8.resolve(_7);
return _7;
}
return _8;
};
});
