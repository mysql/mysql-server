//>>built
define("dojox/lang/functional/tailrec",["dojo","dijit","dojox","dojo/require!dojox/lang/functional/lambda,dojox/lang/functional/util"],function(_1,_2,_3){
_1.provide("dojox.lang.functional.tailrec");
_1.require("dojox.lang.functional.lambda");
_1.require("dojox.lang.functional.util");
(function(){
var df=_3.lang.functional,_4=df.inlineLambda,_5="_x";
df.tailrec=function(_6,_7,_8){
var c,t,b,cs,ts,bs,_9={},_a={},_b=function(x){
_9[x]=1;
};
if(typeof _6=="string"){
cs=_4(_6,_5,_b);
}else{
c=df.lambda(_6);
cs="_c.apply(this, _x)";
_a["_c=_t.c"]=1;
}
if(typeof _7=="string"){
ts=_4(_7,_5,_b);
}else{
t=df.lambda(_7);
ts="_t.t.apply(this, _x)";
}
if(typeof _8=="string"){
bs=_4(_8,_5,_b);
}else{
b=df.lambda(_8);
bs="_b.apply(this, _x)";
_a["_b=_t.b"]=1;
}
var _c=df.keys(_9),_d=df.keys(_a),f=new Function([],"var _x=arguments,_t=_x.callee,_c=_t.c,_b=_t.b".concat(_c.length?","+_c.join(","):"",_d.length?",_t=_x.callee,"+_d.join(","):t?",_t=_x.callee":"",";for(;!",cs,";_x=",bs,");return ",ts));
if(c){
f.c=c;
}
if(t){
f.t=t;
}
if(b){
f.b=b;
}
return f;
};
})();
});
