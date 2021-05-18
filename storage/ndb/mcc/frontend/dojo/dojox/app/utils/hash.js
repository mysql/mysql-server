//>>built
define("dojox/app/utils/hash",["dojo/_base/lang"],function(_1){
var _2={getParams:function(_3){
var _4;
if(_3&&_3.length){
while(_3.indexOf("(")>0){
var _5=_3.indexOf("(");
var _6=_3.indexOf(")");
var _7=_3.substring(_5,_6+1);
if(!_4){
_4={};
}
_4=_2.getParamObj(_4,_7);
var _8=_7.substring(1,_7.indexOf("&"));
_3=_3.replace(_7,_8);
}
for(var _9=_3.split("&"),x=0;x<_9.length;x++){
var tp=_9[x].split("="),_a=tp[0],_b=encodeURIComponent(tp[1]||"");
if(_a&&_b){
if(!_4){
_4={};
}
_4[_a]=_b;
}
}
}
return _4;
},getParamObj:function(_c,_d){
var _e;
var _f=_d.substring(1,_d.indexOf("&"));
var _10=_d.substring(_d.indexOf("&"),_d.length-1);
for(var _11=_10.split("&"),x=0;x<_11.length;x++){
var tp=_11[x].split("="),_12=tp[0],_13=encodeURIComponent(tp[1]||"");
if(_12&&_13){
if(!_e){
_e={};
}
_e[_12]=_13;
}
}
_c[_f]=_e;
return _c;
},buildWithParams:function(_14,_15){
if(_14.charAt(0)!=="#"){
_14="#"+_14;
}
for(var _16 in _15){
var _17=_15[_16];
if(_1.isObject(_17)){
_14=_2.addViewParams(_14,_16,_17);
}else{
if(_16&&_17!=null){
_14=_14+"&"+_16+"="+_15[_16];
}
}
}
return _14;
},addViewParams:function(_18,_19,_1a){
if(_18.charAt(0)!=="#"){
_18="#"+_18;
}
var _1b=_18.indexOf(_19);
if(_1b>0){
if((_18.charAt(_1b-1)=="#"||_18.charAt(_1b-1)=="+")&&(_18.charAt(_1b+_19.length)=="&"||_18.charAt(_1b+_19.length)=="+"||_18.charAt(_1b+_19.length)=="-")){
var _1c=_18.substring(_1b-1,_1b+_19.length+1);
var _1d=_2.getParamString(_1a);
var _1e=_18.charAt(_1b-1)+"("+_19+_1d+")"+_18.charAt(_1b+_19.length);
_18=_18.replace(_1c,_1e);
}
}
return _18;
},getParamString:function(_1f){
var _20="";
for(var _21 in _1f){
var _22=_1f[_21];
if(_21&&_22!=null){
_20=_20+"&"+_21+"="+_1f[_21];
}
}
return _20;
},getTarget:function(_23,_24){
if(!_24){
_24="";
}
while(_23.indexOf("(")>0){
var _25=_23.indexOf("(");
var _26=_23.indexOf(")");
var _27=_23.substring(_25,_26+1);
var _28=_27.substring(1,_27.indexOf("&"));
_23=_23.replace(_27,_28);
}
return (((_23&&_23.charAt(0)=="#")?_23.substr(1):_23)||_24).split("&")[0];
}};
return _2;
});
