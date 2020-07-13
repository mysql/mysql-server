//>>built
define("dojox/app/utils/model",["dojo/_base/lang","dojo/Deferred","dojo/promise/all","dojo/when"],function(_1,_2,_3,_4){
return function(_5,_6,_7){
var _8={};
if(_6.loadedModels){
_1.mixin(_8,_6.loadedModels);
}
if(_5){
var _9=[];
for(var _a in _5){
if(_a.charAt(0)!=="_"){
_9.push(_b(_5,_a,_7,_8));
}
}
return (_9.length==0)?_8:_3(_9);
}else{
return _8;
}
};
function _b(_c,_d,_e,_f){
var _10=_c[_d].params?_c[_d].params:{};
var _11=_c[_d].modelLoader?_c[_d].modelLoader:"dojox/app/utils/simpleModel";
try{
var _12=require(_11);
}
catch(e){
throw new Error(_11+" must be listed in the dependencies");
}
var _13=new _2();
var _14;
try{
_14=_12(_c,_10,_d);
}
catch(e){
throw new Error("Error creating "+_11+" for model named ["+_d+"]: "+e.message);
}
_4(_14,_1.hitch(this,function(_15){
_f[_d]=_15;
_e.log("in app/model, for item=[",_d,"] loadedModels =",_f);
_13.resolve(_f);
return _f;
}),function(e){
throw new Error("Error loading model named ["+_d+"]: "+e.message);
});
return _13;
};
});
