//>>built
define("dojox/lang/functional/binrec",["dojo","dijit","dojox","dojo/require!dojox/lang/functional/lambda,dojox/lang/functional/util"],function(_1,_2,_3){
_1.provide("dojox.lang.functional.binrec");
_1.require("dojox.lang.functional.lambda");
_1.require("dojox.lang.functional.util");
(function(){
var df=_3.lang.functional,_4=df.inlineLambda,_5="_x",_6=["_z.r","_r","_z.a"];
df.binrec=function(_7,_8,_9,_a){
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
as="_a.call(this, _z.r, _r, _z.a)";
_c["_a=_t.a"]=1;
}
var _e=df.keys(_b),_f=df.keys(_c),f=new Function([],"var _x=arguments,_y,_z,_r".concat(_e.length?","+_e.join(","):"",_f.length?",_t=_x.callee,"+_f.join(","):"",t?(_f.length?",_t=_t.t":"_t=_x.callee.t"):"",";while(!",cs,"){_r=",bs,";_y={p:_y,a:_r[1]};_z={p:_z,a:_x};_x=_r[0]}for(;;){do{_r=",ts,";if(!_z)return _r;while(\"r\" in _z){_r=",as,";if(!(_z=_z.p))return _r}_z.r=_r;_x=_y.a;_y=_y.p}while(",cs,");do{_r=",bs,";_y={p:_y,a:_r[1]};_z={p:_z,a:_x};_x=_r[0]}while(!",cs,")}"));
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
