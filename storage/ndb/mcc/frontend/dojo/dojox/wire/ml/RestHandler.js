//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/wire/_base,dojox/wire/ml/util"],function(_1,_2,_3){
_2.provide("dojox.wire.ml.RestHandler");
_2.require("dojox.wire._base");
_2.require("dojox.wire.ml.util");
_2.declare("dojox.wire.ml.RestHandler",null,{contentType:"text/plain",handleAs:"text",bind:function(_4,_5,_6,_7){
_4=_4.toUpperCase();
var _8=this;
var _9={url:this._getUrl(_4,_5,_7),contentType:this.contentType,handleAs:this.handleAs,headers:this.headers,preventCache:this.preventCache};
var d=null;
if(_4=="POST"){
_9.postData=this._getContent(_4,_5);
d=_2.rawXhrPost(_9);
}else{
if(_4=="PUT"){
_9.putData=this._getContent(_4,_5);
d=_2.rawXhrPut(_9);
}else{
if(_4=="DELETE"){
d=_2.xhrDelete(_9);
}else{
d=_2.xhrGet(_9);
}
}
}
d.addCallbacks(function(_a){
_6.callback(_8._getResult(_a));
},function(_b){
_6.errback(_b);
});
},_getUrl:function(_c,_d,_e){
var _f;
if(_c=="GET"||_c=="DELETE"){
if(_d.length>0){
_f=_d[0];
}
}else{
if(_d.length>1){
_f=_d[1];
}
}
if(_f){
var _10="";
for(var _11 in _f){
var _12=_f[_11];
if(_12){
_12=encodeURIComponent(_12);
var _13="{"+_11+"}";
var _14=_e.indexOf(_13);
if(_14>=0){
_e=_e.substring(0,_14)+_12+_e.substring(_14+_13.length);
}else{
if(_10){
_10+="&";
}
_10+=(_11+"="+_12);
}
}
}
if(_10){
_e+="?"+_10;
}
}
return _e;
},_getContent:function(_15,_16){
if(_15=="POST"||_15=="PUT"){
return (_16?_16[0]:null);
}else{
return null;
}
},_getResult:function(_17){
return _17;
}});
});
