//>>built
define("dojox/uuid/_base",["dojo/_base/kernel","dojo/_base/lang"],function(_1){
_1.getObject("uuid",true,dojox);
dojox.uuid.NIL_UUID="00000000-0000-0000-0000-000000000000";
dojox.uuid.version={UNKNOWN:0,TIME_BASED:1,DCE_SECURITY:2,NAME_BASED_MD5:3,RANDOM:4,NAME_BASED_SHA1:5};
dojox.uuid.variant={NCS:"0",DCE:"10",MICROSOFT:"110",UNKNOWN:"111"};
dojox.uuid.assert=function(_2,_3){
if(!_2){
if(!_3){
_3="An assert statement failed.\n"+"The method dojox.uuid.assert() was called with a 'false' value.\n";
}
throw new Error(_3);
}
};
dojox.uuid.generateNilUuid=function(){
return dojox.uuid.NIL_UUID;
};
dojox.uuid.isValid=function(_4){
_4=_4.toString();
var _5=(_1.isString(_4)&&(_4.length==36)&&(_4==_4.toLowerCase()));
if(_5){
var _6=_4.split("-");
_5=((_6.length==5)&&(_6[0].length==8)&&(_6[1].length==4)&&(_6[2].length==4)&&(_6[3].length==4)&&(_6[4].length==12));
var _7=16;
for(var i in _6){
var _8=_6[i];
var _9=parseInt(_8,_7);
_5=_5&&isFinite(_9);
}
}
return _5;
};
dojox.uuid.getVariant=function(_a){
if(!dojox.uuid._ourVariantLookupTable){
var _b=dojox.uuid.variant;
var _c=[];
_c[0]=_b.NCS;
_c[1]=_b.NCS;
_c[2]=_b.NCS;
_c[3]=_b.NCS;
_c[4]=_b.NCS;
_c[5]=_b.NCS;
_c[6]=_b.NCS;
_c[7]=_b.NCS;
_c[8]=_b.DCE;
_c[9]=_b.DCE;
_c[10]=_b.DCE;
_c[11]=_b.DCE;
_c[12]=_b.MICROSOFT;
_c[13]=_b.MICROSOFT;
_c[14]=_b.UNKNOWN;
_c[15]=_b.UNKNOWN;
dojox.uuid._ourVariantLookupTable=_c;
}
_a=_a.toString();
var _d=_a.charAt(19);
var _e=16;
var _f=parseInt(_d,_e);
dojox.uuid.assert((_f>=0)&&(_f<=16));
return dojox.uuid._ourVariantLookupTable[_f];
};
dojox.uuid.getVersion=function(_10){
var _11="dojox.uuid.getVersion() was not passed a DCE Variant UUID.";
dojox.uuid.assert(dojox.uuid.getVariant(_10)==dojox.uuid.variant.DCE,_11);
_10=_10.toString();
var _12=_10.charAt(14);
var _13=16;
var _14=parseInt(_12,_13);
return _14;
};
dojox.uuid.getNode=function(_15){
var _16="dojox.uuid.getNode() was not passed a TIME_BASED UUID.";
dojox.uuid.assert(dojox.uuid.getVersion(_15)==dojox.uuid.version.TIME_BASED,_16);
_15=_15.toString();
var _17=_15.split("-");
var _18=_17[4];
return _18;
};
dojox.uuid.getTimestamp=function(_19,_1a){
var _1b="dojox.uuid.getTimestamp() was not passed a TIME_BASED UUID.";
dojox.uuid.assert(dojox.uuid.getVersion(_19)==dojox.uuid.version.TIME_BASED,_1b);
_19=_19.toString();
if(!_1a){
_1a=null;
}
switch(_1a){
case "string":
case String:
return dojox.uuid.getTimestamp(_19,Date).toUTCString();
break;
case "hex":
var _1c=_19.split("-");
var _1d=_1c[0];
var _1e=_1c[1];
var _1f=_1c[2];
_1f=_1f.slice(1);
var _20=_1f+_1e+_1d;
dojox.uuid.assert(_20.length==15);
return _20;
break;
case null:
case "date":
case Date:
var _21=3394248;
var _22=16;
var _23=_19.split("-");
var _24=parseInt(_23[0],_22);
var _25=parseInt(_23[1],_22);
var _26=parseInt(_23[2],_22);
var _27=_26&4095;
_27<<=16;
_27+=_25;
_27*=4294967296;
_27+=_24;
var _28=_27/10000;
var _29=60*60;
var _2a=_21;
var _2b=_2a*_29;
var _2c=_2b*1000;
var _2d=_28-_2c;
var _2e=new Date(_2d);
return _2e;
break;
default:
dojox.uuid.assert(false,"dojox.uuid.getTimestamp was not passed a valid returnType: "+_1a);
break;
}
};
return dojox.uuid;
});
