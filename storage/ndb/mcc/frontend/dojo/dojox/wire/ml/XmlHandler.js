//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/wire/ml/RestHandler,dojox/xml/parser,dojox/wire/_base,dojox/wire/ml/util"],function(_1,_2,_3){
_2.provide("dojox.wire.ml.XmlHandler");
_2.require("dojox.wire.ml.RestHandler");
_2.require("dojox.xml.parser");
_2.require("dojox.wire._base");
_2.require("dojox.wire.ml.util");
_2.declare("dojox.wire.ml.XmlHandler",_3.wire.ml.RestHandler,{contentType:"text/xml",handleAs:"xml",_getContent:function(_4,_5){
var _6=null;
if(_4=="POST"||_4=="PUT"){
var p=_5[0];
if(p){
if(_2.isString(p)){
_6=p;
}else{
var _7=p;
if(_7 instanceof _3.wire.ml.XmlElement){
_7=_7.element;
}else{
if(_7.nodeType===9){
_7=_7.documentElement;
}
}
var _8="<?xml version=\"1.0\"?>";
_6=_8+_3.xml.parser.innerXML(_7);
}
}
}
return _6;
},_getResult:function(_9){
if(_9){
_9=new _3.wire.ml.XmlElement(_9);
}
return _9;
}});
});
