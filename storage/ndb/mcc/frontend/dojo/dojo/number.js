/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/number",["./_base/lang","./i18n","./i18n!./cldr/nls/number","./string","./regexp"],function(_1,_2,_3,_4,_5){
var _6={};
_1.setObject("dojo.number",_6);
_6.format=function(_7,_8){
_8=_1.mixin({},_8||{});
var _9=_2.normalizeLocale(_8.locale),_a=_2.getLocalization("dojo.cldr","number",_9);
_8.customs=_a;
var _b=_8.pattern||_a[(_8.type||"decimal")+"Format"];
if(isNaN(_7)||Math.abs(_7)==Infinity){
return null;
}
return _6._applyPattern(_7,_b,_8);
};
_6._numberPatternRE=/[#0,]*[#0](?:\.0*#*)?/;
_6._applyPattern=function(_c,_d,_e){
_e=_e||{};
var _f=_e.customs.group,_10=_e.customs.decimal,_11=_d.split(";"),_12=_11[0];
_d=_11[(_c<0)?1:0]||("-"+_12);
if(_d.indexOf("%")!=-1){
_c*=100;
}else{
if(_d.indexOf("‰")!=-1){
_c*=1000;
}else{
if(_d.indexOf("¤")!=-1){
_f=_e.customs.currencyGroup||_f;
_10=_e.customs.currencyDecimal||_10;
_d=_d.replace(/([\s\xa0]*)(\u00a4{1,3})([\s\xa0]*)/,function(_13,_14,_15,_16){
var _17=["symbol","currency","displayName"][_15.length-1],_18=_e[_17]||_e.currency||"";
if(!_18){
return "";
}
return _14+_18+_16;
});
}else{
if(_d.indexOf("E")!=-1){
throw new Error("exponential notation not supported");
}
}
}
}
var _19=_6._numberPatternRE;
var _1a=_12.match(_19);
if(!_1a){
throw new Error("unable to find a number expression in pattern: "+_d);
}
if(_e.fractional===false){
_e.places=0;
}
return _d.replace(_19,_6._formatAbsolute(_c,_1a[0],{decimal:_10,group:_f,places:_e.places,round:_e.round}));
};
_6.round=function(_1b,_1c,_1d){
var _1e=10/(_1d||10);
return (_1e*+_1b).toFixed(_1c)/_1e;
};
if((0.9).toFixed()==0){
var _1f=_6.round;
_6.round=function(v,p,m){
var d=Math.pow(10,-p||0),a=Math.abs(v);
if(!v||a>=d){
d=0;
}else{
a/=d;
if(a<0.5||a>=0.95){
d=0;
}
}
return _1f(v,p,m)+(v>0?d:-d);
};
}
_6._formatAbsolute=function(_20,_21,_22){
_22=_22||{};
if(_22.places===true){
_22.places=0;
}
if(_22.places===Infinity){
_22.places=6;
}
var _23=_21.split("."),_24=typeof _22.places=="string"&&_22.places.indexOf(","),_25=_22.places;
if(_24){
_25=_22.places.substring(_24+1);
}else{
if(!(_25>=0)){
_25=(_23[1]||[]).length;
}
}
if(!(_22.round<0)){
_20=_6.round(_20,_25,_22.round);
}
var _26=String(Math.abs(_20)).split("."),_27=_26[1]||"";
if(_23[1]||_22.places){
if(_24){
_22.places=_22.places.substring(0,_24);
}
var pad=_22.places!==undefined?_22.places:(_23[1]&&_23[1].lastIndexOf("0")+1);
if(pad>_27.length){
_26[1]=_4.pad(_27,pad,"0",true);
}
if(_25<_27.length){
_26[1]=_27.substr(0,_25);
}
}else{
if(_26[1]){
_26.pop();
}
}
var _28=_23[0].replace(",","");
pad=_28.indexOf("0");
if(pad!=-1){
pad=_28.length-pad;
if(pad>_26[0].length){
_26[0]=_4.pad(_26[0],pad);
}
if(_28.indexOf("#")==-1){
_26[0]=_26[0].substr(_26[0].length-pad);
}
}
var _29=_23[0].lastIndexOf(","),_2a,_2b;
if(_29!=-1){
_2a=_23[0].length-_29-1;
var _2c=_23[0].substr(0,_29);
_29=_2c.lastIndexOf(",");
if(_29!=-1){
_2b=_2c.length-_29-1;
}
}
var _2d=[];
for(var _2e=_26[0];_2e;){
var off=_2e.length-_2a;
_2d.push((off>0)?_2e.substr(off):_2e);
_2e=(off>0)?_2e.slice(0,off):"";
if(_2b){
_2a=_2b;
_2b=undefined;
}
}
_26[0]=_2d.reverse().join(_22.group||",");
return _26.join(_22.decimal||".");
};
_6.regexp=function(_2f){
return _6._parseInfo(_2f).regexp;
};
_6._parseInfo=function(_30){
_30=_30||{};
var _31=_2.normalizeLocale(_30.locale),_32=_2.getLocalization("dojo.cldr","number",_31),_33=_30.pattern||_32[(_30.type||"decimal")+"Format"],_34=_32.group,_35=_32.decimal,_36=1;
if(_33.indexOf("%")!=-1){
_36/=100;
}else{
if(_33.indexOf("‰")!=-1){
_36/=1000;
}else{
var _37=_33.indexOf("¤")!=-1;
if(_37){
_34=_32.currencyGroup||_34;
_35=_32.currencyDecimal||_35;
}
}
}
var _38=_33.split(";");
if(_38.length==1){
_38.push("-"+_38[0]);
}
var re=_5.buildGroupRE(_38,function(_39){
_39="(?:"+_5.escapeString(_39,".")+")";
return _39.replace(_6._numberPatternRE,function(_3a){
var _3b={signed:false,separator:_30.strict?_34:[_34,""],fractional:_30.fractional,decimal:_35,exponent:false},_3c=_3a.split("."),_3d=_30.places;
if(_3c.length==1&&_36!=1){
_3c[1]="###";
}
if(_3c.length==1||_3d===0){
_3b.fractional=false;
}else{
if(_3d===undefined){
_3d=_30.pattern?_3c[1].lastIndexOf("0")+1:Infinity;
}
if(_3d&&_30.fractional==undefined){
_3b.fractional=true;
}
if(!_30.places&&(_3d<_3c[1].length)){
_3d+=","+_3c[1].length;
}
_3b.places=_3d;
}
var _3e=_3c[0].split(",");
if(_3e.length>1){
_3b.groupSize=_3e.pop().length;
if(_3e.length>1){
_3b.groupSize2=_3e.pop().length;
}
}
return "("+_6._realNumberRegexp(_3b)+")";
});
},true);
if(_37){
re=re.replace(/([\s\xa0]*)(\u00a4{1,3})([\s\xa0]*)/g,function(_3f,_40,_41,_42){
var _43=["symbol","currency","displayName"][_41.length-1],_44=_5.escapeString(_30[_43]||_30.currency||"");
if(!_44){
return "";
}
_40=_40?"[\\s\\xa0]":"";
_42=_42?"[\\s\\xa0]":"";
if(!_30.strict){
if(_40){
_40+="*";
}
if(_42){
_42+="*";
}
return "(?:"+_40+_44+_42+")?";
}
return _40+_44+_42;
});
}
return {regexp:re.replace(/[\xa0 ]/g,"[\\s\\xa0]"),group:_34,decimal:_35,factor:_36};
};
_6.parse=function(_45,_46){
var _47=_6._parseInfo(_46),_48=(new RegExp("^"+_47.regexp+"$")).exec(_45);
if(!_48){
return NaN;
}
var _49=_48[1];
if(!_48[1]){
if(!_48[2]){
return NaN;
}
_49=_48[2];
_47.factor*=-1;
}
_49=_49.replace(new RegExp("["+_47.group+"\\s\\xa0"+"]","g"),"").replace(_47.decimal,".");
return _49*_47.factor;
};
_6._realNumberRegexp=function(_4a){
_4a=_4a||{};
if(!("places" in _4a)){
_4a.places=Infinity;
}
if(typeof _4a.decimal!="string"){
_4a.decimal=".";
}
if(!("fractional" in _4a)||/^0/.test(_4a.places)){
_4a.fractional=[true,false];
}
if(!("exponent" in _4a)){
_4a.exponent=[true,false];
}
if(!("eSigned" in _4a)){
_4a.eSigned=[true,false];
}
var _4b=_6._integerRegexp(_4a),_4c=_5.buildGroupRE(_4a.fractional,function(q){
var re="";
if(q&&(_4a.places!==0)){
re="\\"+_4a.decimal;
if(_4a.places==Infinity){
re="(?:"+re+"\\d+)?";
}else{
re+="\\d{"+_4a.places+"}";
}
}
return re;
},true);
var _4d=_5.buildGroupRE(_4a.exponent,function(q){
if(q){
return "([eE]"+_6._integerRegexp({signed:_4a.eSigned})+")";
}
return "";
});
var _4e=_4b+_4c;
if(_4c){
_4e="(?:(?:"+_4e+")|(?:"+_4c+"))";
}
return _4e+_4d;
};
_6._integerRegexp=function(_4f){
_4f=_4f||{};
if(!("signed" in _4f)){
_4f.signed=[true,false];
}
if(!("separator" in _4f)){
_4f.separator="";
}else{
if(!("groupSize" in _4f)){
_4f.groupSize=3;
}
}
var _50=_5.buildGroupRE(_4f.signed,function(q){
return q?"[-+]":"";
},true);
var _51=_5.buildGroupRE(_4f.separator,function(sep){
if(!sep){
return "(?:\\d+)";
}
sep=_5.escapeString(sep);
if(sep==" "){
sep="\\s";
}else{
if(sep==" "){
sep="\\s\\xa0";
}
}
var grp=_4f.groupSize,_52=_4f.groupSize2;
if(_52){
var _53="(?:0|[1-9]\\d{0,"+(_52-1)+"}(?:["+sep+"]\\d{"+_52+"})*["+sep+"]\\d{"+grp+"})";
return ((grp-_52)>0)?"(?:"+_53+"|(?:0|[1-9]\\d{0,"+(grp-1)+"}))":_53;
}
return "(?:0|[1-9]\\d{0,"+(grp-1)+"}(?:["+sep+"]\\d{"+grp+"})*)";
},true);
return _50+_51;
};
return _6;
});
