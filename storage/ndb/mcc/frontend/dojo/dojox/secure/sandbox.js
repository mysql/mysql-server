//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/secure/DOM,dojox/secure/capability,dojo/NodeList-fx,dojo/_base/url"],function(_1,_2,_3){
_2.provide("dojox.secure.sandbox");
_2.require("dojox.secure.DOM");
_2.require("dojox.secure.capability");
_2.require("dojo.NodeList-fx");
_2.require("dojo._base.url");
(function(){
var _4=setTimeout;
var _5=setInterval;
if({}.__proto__){
var _6=function(_7){
var _8=Array.prototype[_7];
if(_8&&!_8.fixed){
(Array.prototype[_7]=function(){
if(this==window){
throw new TypeError("Called with wrong this");
}
return _8.apply(this,arguments);
}).fixed=true;
}
};
_6("concat");
_6("reverse");
_6("sort");
_6("slice");
_6("forEach");
_6("filter");
_6("reduce");
_6("reduceRight");
_6("every");
_6("map");
_6("some");
}
var _9=function(){
return _2.xhrGet.apply(_2,arguments);
};
_3.secure.sandbox=function(_a){
var _b=_3.secure.DOM(_a);
_a=_b(_a);
var _c=_a.ownerDocument;
var _d,_2=_3.secure._safeDojoFunctions(_a,_b);
var _e=[];
var _f=["isNaN","isFinite","parseInt","parseFloat","escape","unescape","encodeURI","encodeURIComponent","decodeURI","decodeURIComponent","alert","confirm","prompt","Error","EvalError","RangeError","ReferenceError","SyntaxError","TypeError","Date","RegExp","Number","Object","Array","String","Math","setTimeout","setInterval","clearTimeout","clearInterval","dojo","get","set","forEach","load","evaluate"];
for(var i in _2){
_f.push(i);
_e.push("var "+i+"=dojo."+i);
}
eval(_e.join(";"));
function get(obj,_10){
_10=""+_10;
if(_3.secure.badProps.test(_10)){
throw new Error("bad property access");
}
if(obj.__get__){
return obj.__get__(_10);
}
return obj[_10];
};
function set(obj,_11,_12){
_11=""+_11;
get(obj,_11);
if(obj.__set){
return obj.__set(_11);
}
obj[_11]=_12;
return _12;
};
function _13(obj,fun){
if(typeof fun!="function"){
throw new TypeError();
}
if("length" in obj){
if(obj.__get__){
var len=obj.__get__("length");
for(var i=0;i<len;i++){
if(i in obj){
fun.call(obj,obj.__get__(i),i,obj);
}
}
}else{
len=obj.length;
for(i=0;i<len;i++){
if(i in obj){
fun.call(obj,obj[i],i,obj);
}
}
}
}else{
for(i in obj){
fun.call(obj,get(obj,i),i,obj);
}
}
};
function _14(_15,_16,_17){
var _18,_19,_1a;
var arg;
for(var i=0,l=arguments.length;typeof (arg=arguments[i])=="function"&&i<l;i++){
if(_18){
_d(_18,arg.prototype);
}else{
_19=arg;
var F=function(){
};
F.prototype=arg.prototype;
_18=new F;
}
}
if(arg){
for(var j in arg){
var _1b=arg[j];
if(typeof _1b=="function"){
arg[j]=function(){
if(this instanceof _14){
return arguments.callee.__rawMethod__.apply(this,arguments);
}
throw new Error("Method called on wrong object");
};
arg[j].__rawMethod__=_1b;
}
}
if(arg.hasOwnProperty("constructor")){
_1a=arg.constructor;
}
}
_18=_18?_d(_18,arg):arg;
function _14(){
if(_19){
_19.apply(this,arguments);
}
if(_1a){
_1a.apply(this,arguments);
}
};
_d(_14,arguments[i]);
_18.constructor=_14;
_14.prototype=_18;
return _14;
};
function _1c(_1d){
if(typeof _1d!="function"){
throw new Error("String is not allowed in setTimeout/setInterval");
}
};
function _1e(_1f,_20){
_1c(_1f);
return _4(_1f,_20);
};
function _21(_22,_23){
_1c(_22);
return _5(_22,_23);
};
function _24(_25){
return _b.evaluate(_25);
};
var _26=_b.load=function(url){
if(url.match(/^[\w\s]*:/)){
throw new Error("Access denied to cross-site requests");
}
return _9({url:(new _2._Url(_b.rootUrl,url))+"",secure:true});
};
_b.evaluate=function(_27){
_3.secure.capability.validate(_27,_f,{document:1,element:1});
if(_27.match(/^\s*[\[\{]/)){
var _28=eval("("+_27+")");
}else{
eval(_27);
}
};
return {loadJS:function(url){
_b.rootUrl=url;
return _9({url:url,secure:true}).addCallback(function(_29){
_24(_29,_a);
});
},loadHTML:function(url){
_b.rootUrl=url;
return _9({url:url,secure:true}).addCallback(function(_2a){
_a.innerHTML=_2a;
});
},evaluate:function(_2b){
return _b.evaluate(_2b);
}};
};
})();
_3.secure._safeDojoFunctions=function(_2c,_2d){
var _2e=["mixin","require","isString","isArray","isFunction","isObject","isArrayLike","isAlien","hitch","delegate","partial","trim","disconnect","subscribe","unsubscribe","Deferred","toJson","style","attr"];
var doc=_2c.ownerDocument;
var _2f=_3.secure.unwrap;
_2.NodeList.prototype.addContent.safetyCheck=function(_30){
_2d.safeHTML(_30);
};
_2.NodeList.prototype.style.safetyCheck=function(_31,_32){
if(_31=="behavior"){
throw new Error("Can not set behavior");
}
_2d.safeCSS(_32);
};
_2.NodeList.prototype.attr.safetyCheck=function(_33,_34){
if(_34&&(_33=="src"||_33=="href"||_33=="style")){
throw new Error("Illegal to set "+_33);
}
};
var _35={query:function(_36,_37){
return _2d(_2.query(_36,_2f(_37||_2c)));
},connect:function(el,_38){
var obj=el;
arguments[0]=_2f(el);
if(obj!=arguments[0]&&_38.substring(0,2)!="on"){
throw new Error("Invalid event name for element");
}
return _2.connect.apply(_2,arguments);
},body:function(){
return _2c;
},byId:function(id){
return _2c.ownerDocument.getElementById(id);
},fromJson:function(str){
_3.secure.capability.validate(str,[],{});
return _2.fromJson(str);
}};
for(var i=0;i<_2e.length;i++){
_35[_2e[i]]=_2[_2e[i]];
}
return _35;
};
});
