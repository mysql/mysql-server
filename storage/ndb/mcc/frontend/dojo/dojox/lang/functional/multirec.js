//>>built
define("dojox/lang/functional/multirec",["dojo","dijit","dojox","dojo/require!dojox/lang/functional/lambda,dojox/lang/functional/util"],function(_1,_2,_3){
_1.provide("dojox.lang.functional.multirec");
_1.require("dojox.lang.functional.lambda");
_1.require("dojox.lang.functional.util");
(function(){
var df=_3.lang.functional,_4=df.inlineLambda,_5="_x",_6=["_y.r","_y.o"];
df.multirec=function(_7,_8,_9,_a){
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
ts="_t.apply(this, _x)";
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
as="_a.call(this, _y.r, _y.o)";
_c["_a=_t.a"]=1;
}
var _e=df.keys(_b),_f=df.keys(_c),f=new Function([],"var _y={a:arguments},_x,_r,_z,_i".concat(_e.length?","+_e.join(","):"",_f.length?",_t=arguments.callee,"+_f.join(","):"",t?(_f.length?",_t=_t.t":"_t=arguments.callee.t"):"",";for(;;){for(;;){if(_y.o){_r=",as,";break}_x=_y.a;if(",cs,"){_r=",ts,";break}_y.o=_x;_x=",bs,";_y.r=[];_z=_y;for(_i=_x.length-1;_i>=0;--_i){_y={p:_y,a:_x[_i],z:_z}}}if(!(_z=_y.z)){return _r}_z.r.push(_r);_y=_y.p}"));
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
