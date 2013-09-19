//>>built
define("dojox/uuid/generateTimeBasedUuid",["dojo/_base/lang","./_base"],function(_1){
dojox.uuid.generateTimeBasedUuid=function(_2){
var _3=dojox.uuid.generateTimeBasedUuid._generator.generateUuidString(_2);
return _3;
};
dojox.uuid.generateTimeBasedUuid.isValidNode=function(_4){
var _5=16;
var _6=parseInt(_4,_5);
var _7=_1.isString(_4)&&_4.length==12&&isFinite(_6);
return _7;
};
dojox.uuid.generateTimeBasedUuid.setNode=function(_8){
dojox.uuid.assert((_8===null)||this.isValidNode(_8));
this._uniformNode=_8;
};
dojox.uuid.generateTimeBasedUuid.getNode=function(){
return this._uniformNode;
};
dojox.uuid.generateTimeBasedUuid._generator=new function(){
this.GREGORIAN_CHANGE_OFFSET_IN_HOURS=3394248;
var _9=null;
var _a=null;
var _b=null;
var _c=0;
var _d=null;
var _e=null;
var _f=16;
function _10(_11){
_11[2]+=_11[3]>>>16;
_11[3]&=65535;
_11[1]+=_11[2]>>>16;
_11[2]&=65535;
_11[0]+=_11[1]>>>16;
_11[1]&=65535;
dojox.uuid.assert((_11[0]>>>16)===0);
};
function _12(x){
var _13=new Array(0,0,0,0);
_13[3]=x%65536;
x-=_13[3];
x/=65536;
_13[2]=x%65536;
x-=_13[2];
x/=65536;
_13[1]=x%65536;
x-=_13[1];
x/=65536;
_13[0]=x;
return _13;
};
function _14(_15,_16){
dojox.uuid.assert(_1.isArray(_15));
dojox.uuid.assert(_1.isArray(_16));
dojox.uuid.assert(_15.length==4);
dojox.uuid.assert(_16.length==4);
var _17=new Array(0,0,0,0);
_17[3]=_15[3]+_16[3];
_17[2]=_15[2]+_16[2];
_17[1]=_15[1]+_16[1];
_17[0]=_15[0]+_16[0];
_10(_17);
return _17;
};
function _18(_19,_1a){
dojox.uuid.assert(_1.isArray(_19));
dojox.uuid.assert(_1.isArray(_1a));
dojox.uuid.assert(_19.length==4);
dojox.uuid.assert(_1a.length==4);
var _1b=false;
if(_19[0]*_1a[0]!==0){
_1b=true;
}
if(_19[0]*_1a[1]!==0){
_1b=true;
}
if(_19[0]*_1a[2]!==0){
_1b=true;
}
if(_19[1]*_1a[0]!==0){
_1b=true;
}
if(_19[1]*_1a[1]!==0){
_1b=true;
}
if(_19[2]*_1a[0]!==0){
_1b=true;
}
dojox.uuid.assert(!_1b);
var _1c=new Array(0,0,0,0);
_1c[0]+=_19[0]*_1a[3];
_10(_1c);
_1c[0]+=_19[1]*_1a[2];
_10(_1c);
_1c[0]+=_19[2]*_1a[1];
_10(_1c);
_1c[0]+=_19[3]*_1a[0];
_10(_1c);
_1c[1]+=_19[1]*_1a[3];
_10(_1c);
_1c[1]+=_19[2]*_1a[2];
_10(_1c);
_1c[1]+=_19[3]*_1a[1];
_10(_1c);
_1c[2]+=_19[2]*_1a[3];
_10(_1c);
_1c[2]+=_19[3]*_1a[2];
_10(_1c);
_1c[3]+=_19[3]*_1a[3];
_10(_1c);
return _1c;
};
function _1d(_1e,_1f){
while(_1e.length<_1f){
_1e="0"+_1e;
}
return _1e;
};
function _20(){
var _21=Math.floor((Math.random()%1)*Math.pow(2,32));
var _22=_21.toString(_f);
while(_22.length<8){
_22="0"+_22;
}
return _22;
};
this.generateUuidString=function(_23){
if(_23){
dojox.uuid.assert(dojox.uuid.generateTimeBasedUuid.isValidNode(_23));
}else{
if(dojox.uuid.generateTimeBasedUuid._uniformNode){
_23=dojox.uuid.generateTimeBasedUuid._uniformNode;
}else{
if(!_9){
var _24=32768;
var _25=Math.floor((Math.random()%1)*Math.pow(2,15));
var _26=(_24|_25).toString(_f);
_9=_26+_20();
}
_23=_9;
}
}
if(!_a){
var _27=32768;
var _28=Math.floor((Math.random()%1)*Math.pow(2,14));
_a=(_27|_28).toString(_f);
}
var now=new Date();
var _29=now.valueOf();
var _2a=_12(_29);
if(!_d){
var _2b=_12(60*60);
var _2c=_12(dojox.uuid.generateTimeBasedUuid._generator.GREGORIAN_CHANGE_OFFSET_IN_HOURS);
var _2d=_18(_2c,_2b);
var _2e=_12(1000);
_d=_18(_2d,_2e);
_e=_12(10000);
}
var _2f=_2a;
var _30=_14(_d,_2f);
var _31=_18(_30,_e);
if(now.valueOf()==_b){
_31[3]+=_c;
_10(_31);
_c+=1;
if(_c==10000){
while(now.valueOf()==_b){
now=new Date();
}
}
}else{
_b=now.valueOf();
_c=1;
}
var _32=_31[2].toString(_f);
var _33=_31[3].toString(_f);
var _34=_1d(_32,4)+_1d(_33,4);
var _35=_31[1].toString(_f);
_35=_1d(_35,4);
var _36=_31[0].toString(_f);
_36=_1d(_36,3);
var _37="-";
var _38="1";
var _39=_34+_37+_35+_37+_38+_36+_37+_a+_37+_23;
_39=_39.toLowerCase();
return _39;
};
}();
return dojox.uuid.generateTimeBasedUuid;
});
