//>>built
define("dojox/lang/functional/numrec",["dojo","dijit","dojox","dojo/require!dojox/lang/functional/lambda,dojox/lang/functional/util"],function(_1,_2,_3){
_1.provide("dojox.lang.functional.numrec");
_1.require("dojox.lang.functional.lambda");
_1.require("dojox.lang.functional.util");
(function(){
var df=_3.lang.functional,_4=df.inlineLambda,_5=["_r","_i"];
df.numrec=function(_6,_7){
var a,as,_8={},_9=function(x){
_8[x]=1;
};
if(typeof _7=="string"){
as=_4(_7,_5,_9);
}else{
a=df.lambda(_7);
as="_a.call(this, _r, _i)";
}
var _a=df.keys(_8),f=new Function(["_x"],"var _t=arguments.callee,_r=_t.t,_i".concat(_a.length?","+_a.join(","):"",a?",_a=_t.a":"",";for(_i=1;_i<=_x;++_i){_r=",as,"}return _r"));
f.t=_6;
if(a){
f.a=a;
}
return f;
};
})();
});
