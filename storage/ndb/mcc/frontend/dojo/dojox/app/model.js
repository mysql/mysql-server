//>>built
define("dojox/app/model",["dojo/_base/lang","dojo/Deferred","dojo/when"],function(_1,_2,_3){
return function(_4,_5,_6){
this.app=_6||_5;
this.defCount=0;
var _7={};
var _8=new _2();
if(_5.loadedModels){
_1.mixin(_7,_5.loadedModels);
}
if(_4){
for(var _9 in _4){
if(_9.charAt(0)!=="_"){
this.defCount++;
}
}
if(this.defCount==0){
return _7;
}
for(var _a in _4){
if(_a.charAt(0)!=="_"){
_b(_4,_a,_5,_8,_7);
}
}
return _8;
}else{
return _7;
}
};
function _b(_c,_d,_e,_f,_10){
var _11=_c[_d].params?_c[_d].params:{};
var def=new _2();
var _12=_c[_d].modelLoader?_c[_d].modelLoader:"dojox/app/utils/simpleModel";
require([_12],function(_13){
def.resolve(_13);
});
var _14=new _2();
return _3(def,_1.hitch(this,function(_15){
var _16;
try{
_16=_15(_c,_11,_d);
}
catch(ex){
console.warn("load model error in model.",ex);
_14.reject("load model error in model.",ex);
return _14.promise;
}
if(_16.then){
_3(_16,_1.hitch(this,function(_17){
_10[_d]=_17;
this.app.log("in app/model, for item=[",_d,"] loadedModels =",_10);
this.defCount--;
if(this.defCount==0){
_f.resolve(_10);
}
_14.resolve(_10);
return _10;
}),function(){
_14.reject("load model error in models.");
});
return _14;
}else{
_10[_d]=_16;
this.app.log("in app/model else path, for item=[",_d,"] loadedModels=",_10);
this.defCount--;
if(this.defCount==0){
_f.resolve(_10);
}
_14.resolve(_10);
return _10;
}
}));
};
});
