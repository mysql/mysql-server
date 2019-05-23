//>>built
define("dojox/lang/functional/linrec",["dojo","dijit","dojox","dojo/require!dojox/lang/functional/lambda,dojox/lang/functional/util"],function(_1,_2,_3){
_1.provide("dojox.lang.functional.linrec");
_1.require("dojox.lang.functional.lambda");
_1.require("dojox.lang.functional.util");
(function(){
var df=_3.lang.functional,_4=df.inlineLambda,_5="_x",_6=["_r","_y.a"];
df.linrec=function(_7,_8,_9,_a){
var c,t,b,a,cs,ts,bs,as,_b={},_c={},_d=function(x){
_b[x]=1;
};
if(typeof _7=="string"){
cs=_4(_7,_5,_d);
}else{
c=df.lambda(_7);
cs="_c.apply(this, _x)";
_c["_c=_t.c"]=1;
}
if(typeof _8=="string"){
ts=_4(_8,_5,_d);
}else{
t=df.lambda(_8);
ts="_t.t.apply(this, _x)";
}
if(typeof _9=="string"){
bs=_4(_9,_5,_d);
}else{
b=df.lambda(_9);
bs="_b.apply(this, _x)";
_c["_b=_t.b"]=1;
}
if(typeof _a=="string"){
as=_4(_a,_6,_d);
}else{
a=df.lambda(_a);
as="_a.call(this, _r, _y.a)";
_c["_a=_t.a"]=1;
}
var _e=df.keys(_b),_f=df.keys(_c),f=new Function([],"var _x=arguments,_y,_r".concat(_e.length?","+_e.join(","):"",_f.length?",_t=_x.callee,"+_f.join(","):t?",_t=_x.callee":"",";for(;!",cs,";_x=",bs,"){_y={p:_y,a:_x}}_r=",ts,";for(;_y;_y=_y.p){_r=",as,"}return _r"));
if(c){
f.c=c;
}
if(t){
f.t=t;
}
if(b){
f.b=b;
}
if(a){
f.a=a;
}
return f;
};
})();
});
