//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.secure.fromJson");
_3.secure.fromJson=typeof JSON!="undefined"?JSON.parse:(function(){
var _4="(?:-?\\b(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\\b)";
var _5="(?:[^\\0-\\x08\\x0a-\\x1f\"\\\\]"+"|\\\\(?:[\"/\\\\bfnrt]|u[0-9A-Fa-f]{4}))";
var _6="(?:\""+_5+"*\")";
var _7=new RegExp("(?:false|true|null|[\\{\\}\\[\\]]"+"|"+_4+"|"+_6+")","g");
var _8=new RegExp("\\\\(?:([^u])|u(.{4}))","g");
var _9={"\"":"\"","/":"/","\\":"\\","b":"\b","f":"\f","n":"\n","r":"\r","t":"\t"};
function _a(_b,ch,_c){
return ch?_9[ch]:String.fromCharCode(parseInt(_c,16));
};
var _d=new String("");
var _e="\\";
var _f={"{":Object,"[":Array};
var hop=Object.hasOwnProperty;
return function(_10,_11){
var _12=_10.match(_7);
var _13;
var tok=_12[0];
var _14=false;
if("{"===tok){
_13={};
}else{
if("["===tok){
_13=[];
}else{
_13=[];
_14=true;
}
}
var key;
var _15=[_13];
for(var i=1-_14,n=_12.length;i<n;++i){
tok=_12[i];
var _16;
switch(tok.charCodeAt(0)){
default:
_16=_15[0];
_16[key||_16.length]=+(tok);
key=void 0;
break;
case 34:
tok=tok.substring(1,tok.length-1);
if(tok.indexOf(_e)!==-1){
tok=tok.replace(_8,_a);
}
_16=_15[0];
if(!key){
if(_16 instanceof Array){
key=_16.length;
}else{
key=tok||_d;
break;
}
}
_16[key]=tok;
key=void 0;
break;
case 91:
_16=_15[0];
_15.unshift(_16[key||_16.length]=[]);
key=void 0;
break;
case 93:
_15.shift();
break;
case 102:
_16=_15[0];
_16[key||_16.length]=false;
key=void 0;
break;
case 110:
_16=_15[0];
_16[key||_16.length]=null;
key=void 0;
break;
case 116:
_16=_15[0];
_16[key||_16.length]=true;
key=void 0;
break;
case 123:
_16=_15[0];
_15.unshift(_16[key||_16.length]={});
key=void 0;
break;
case 125:
_15.shift();
break;
}
}
if(_14){
if(_15.length!==1){
throw new Error();
}
_13=_13[0];
}else{
if(_15.length){
throw new Error();
}
}
if(_11){
var _17=function(_18,key){
var _19=_18[key];
if(_19&&typeof _19==="object"){
var _1a=null;
for(var k in _19){
if(hop.call(_19,k)&&_19!==_18){
var v=_17(_19,k);
if(v!==void 0){
_19[k]=v;
}else{
if(!_1a){
_1a=[];
}
_1a.push(k);
}
}
}
if(_1a){
for(var i=_1a.length;--i>=0;){
delete _19[_1a[i]];
}
}
}
return _11.call(_18,key,_19);
};
_13=_17({"":_13},"");
}
return _13;
};
})();
});
