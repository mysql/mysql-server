//>>built
define("dojox/encoding/digests/_sha2",[],function(){
return function(_1,_2,_3,_4){
function _5(_6,_7){
_7=_7||_1.outputTypes.Base64;
var wa=_1.digest(_1.toWord(_6),_6.length*8,_4,_2);
switch(_7){
case _1.outputTypes.Raw:
return wa;
case _1.outputTypes.Hex:
return _1.toHex(wa);
case _1.outputTypes.String:
return _1._toString(wa);
default:
return _1.toBase64(wa);
}
};
_5.hmac=function(_8,_9,_a){
_a=_a||_1.outputTypes.Base64;
var wa=_1.toWord(_9);
if(wa.length>16){
wa=_1.digest(wa,_9.length*8,_4,_2);
}
var _b=_3/32,_c=new Array(_b),_d=new Array(_b);
for(var i=0;i<_b;i++){
_c[i]=wa[i]^909522486;
_d[i]=wa[i]^1549556828;
}
var r1=_1.digest(_c.concat(_1.toWord(_8)),_3+_8.length*8,_4,_2);
var r2=_1.digest(_d.concat(r1),_3+_2,_4,_2);
switch(_a){
case _1.outputTypes.Raw:
return r2;
case _1.outputTypes.Hex:
return _1.toHex(r2);
case _1.outputTypes.String:
return _1._toString(r2);
default:
return _1.toBase64(r2);
}
};
_5._hmac=_5.hmac;
return _5;
};
});
