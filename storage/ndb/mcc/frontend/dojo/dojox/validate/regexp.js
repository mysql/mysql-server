//>>built
define("dojox/validate/regexp",["dojo/_base/lang","dojo/regexp","dojox/main"],function(_1,_2,_3){
var _4=_1.getObject("validate.regexp",true,_3);
_4=_3.validate.regexp={ipAddress:function(_5){
_5=(typeof _5=="object")?_5:{};
if(typeof _5.allowDottedDecimal!="boolean"){
_5.allowDottedDecimal=true;
}
if(typeof _5.allowDottedHex!="boolean"){
_5.allowDottedHex=true;
}
if(typeof _5.allowDottedOctal!="boolean"){
_5.allowDottedOctal=true;
}
if(typeof _5.allowDecimal!="boolean"){
_5.allowDecimal=true;
}
if(typeof _5.allowHex!="boolean"){
_5.allowHex=true;
}
if(typeof _5.allowIPv6!="boolean"){
_5.allowIPv6=true;
}
if(typeof _5.allowHybrid!="boolean"){
_5.allowHybrid=true;
}
var _6="((\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])";
var _7="(0[xX]0*[\\da-fA-F]?[\\da-fA-F]\\.){3}0[xX]0*[\\da-fA-F]?[\\da-fA-F]";
var _8="(0+[0-3][0-7][0-7]\\.){3}0+[0-3][0-7][0-7]";
var _9="(0|[1-9]\\d{0,8}|[1-3]\\d{9}|4[01]\\d{8}|42[0-8]\\d{7}|429[0-3]\\d{6}|"+"4294[0-8]\\d{5}|42949[0-5]\\d{4}|429496[0-6]\\d{3}|4294967[01]\\d{2}|42949672[0-8]\\d|429496729[0-5])";
var _a="0[xX]0*[\\da-fA-F]{1,8}";
var _b="([\\da-fA-F]{1,4}\\:){7}[\\da-fA-F]{1,4}";
var _c="([\\da-fA-F]{1,4}\\:){6}"+"((\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[1-9]\\d|1\\d\\d|2[0-4]\\d|25[0-5])";
var a=[];
if(_5.allowDottedDecimal){
a.push(_6);
}
if(_5.allowDottedHex){
a.push(_7);
}
if(_5.allowDottedOctal){
a.push(_8);
}
if(_5.allowDecimal){
a.push(_9);
}
if(_5.allowHex){
a.push(_a);
}
if(_5.allowIPv6){
a.push(_b);
}
if(_5.allowHybrid){
a.push(_c);
}
var _d="";
if(a.length>0){
_d="("+a.join("|")+")";
}
return _d;
},host:function(_e){
_e=(typeof _e=="object")?_e:{};
if(typeof _e.allowIP!="boolean"){
_e.allowIP=true;
}
if(typeof _e.allowLocal!="boolean"){
_e.allowLocal=false;
}
if(typeof _e.allowPort!="boolean"){
_e.allowPort=true;
}
if(typeof _e.allowNamed!="boolean"){
_e.allowNamed=false;
}
var _f="(?:[\\da-zA-Z](?:[-\\da-zA-Z]{0,61}[\\da-zA-Z])?)";
var _10="(?:[a-zA-Z](?:[-\\da-zA-Z]{0,6}[\\da-zA-Z])?)";
var _11=_e.allowPort?"(\\:\\d+)?":"";
var _12="((?:"+_f+"\\.)+"+_10+"\\.?)";
if(_e.allowIP){
_12+="|"+_4.ipAddress(_e);
}
if(_e.allowLocal){
_12+="|localhost";
}
if(_e.allowNamed){
_12+="|^[^-][a-zA-Z0-9_-]*";
}
return "("+_12+")"+_11;
},url:function(_13){
_13=(typeof _13=="object")?_13:{};
if(!("scheme" in _13)){
_13.scheme=[true,false];
}
var _14=_2.buildGroupRE(_13.scheme,function(q){
if(q){
return "(https?|ftps?)\\://";
}
return "";
});
var _15="(/(?:[^?#\\s/]+/)*(?:[^?#\\s/]+(?:\\?[^?#\\s/]*)?(?:#[A-Za-z][\\w.:-]*)?)?)?";
return _14+_4.host(_13)+_15;
},emailAddress:function(_16){
_16=(typeof _16=="object")?_16:{};
if(typeof _16.allowCruft!="boolean"){
_16.allowCruft=false;
}
_16.allowPort=false;
var _17="([!#-'*+\\-\\/-9=?A-Z^-~]+[.])*[!#-'*+\\-\\/-9=?A-Z^-~]+";
var _18=_17+"@"+_4.host(_16);
if(_16.allowCruft){
_18="<?(mailto\\:)?"+_18+">?";
}
return _18;
},emailAddressList:function(_19){
_19=(typeof _19=="object")?_19:{};
if(typeof _19.listSeparator!="string"){
_19.listSeparator="\\s;,";
}
var _1a=_4.emailAddress(_19);
var _1b="("+_1a+"\\s*["+_19.listSeparator+"]\\s*)*"+_1a+"\\s*["+_19.listSeparator+"]?\\s*";
return _1b;
},numberFormat:function(_1c){
_1c=(typeof _1c=="object")?_1c:{};
if(typeof _1c.format=="undefined"){
_1c.format="###-###-####";
}
var _1d=function(_1e){
return _2.escapeString(_1e,"?").replace(/\?/g,"\\d?").replace(/#/g,"\\d");
};
return _2.buildGroupRE(_1c.format,_1d);
},ca:{postalCode:function(){
return "([A-Z][0-9][A-Z] [0-9][A-Z][0-9])";
},province:function(){
return "(AB|BC|MB|NB|NL|NS|NT|NU|ON|PE|QC|SK|YT)";
}},us:{state:function(_1f){
_1f=(typeof _1f=="object")?_1f:{};
if(typeof _1f.allowTerritories!="boolean"){
_1f.allowTerritories=true;
}
if(typeof _1f.allowMilitary!="boolean"){
_1f.allowMilitary=true;
}
var _20="AL|AK|AZ|AR|CA|CO|CT|DE|DC|FL|GA|HI|ID|IL|IN|IA|KS|KY|LA|ME|MD|MA|MI|MN|MS|MO|MT|"+"NE|NV|NH|NJ|NM|NY|NC|ND|OH|OK|OR|PA|RI|SC|SD|TN|TX|UT|VT|VA|WA|WV|WI|WY";
var _21="AS|FM|GU|MH|MP|PW|PR|VI";
var _22="AA|AE|AP";
if(_1f.allowTerritories){
_20+="|"+_21;
}
if(_1f.allowMilitary){
_20+="|"+_22;
}
return "("+_20+")";
}}};
return _4;
});
