//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/wire/ml/RestHandler,dojox/wire/_base,dojox/wire/ml/util"],function(_1,_2,_3){
_2.provide("dojox.wire.ml.JsonHandler");
_2.require("dojox.wire.ml.RestHandler");
_2.require("dojox.wire._base");
_2.require("dojox.wire.ml.util");
_2.declare("dojox.wire.ml.JsonHandler",_3.wire.ml.RestHandler,{contentType:"text/json",handleAs:"json",headers:{"Accept":"*/json"},_getContent:function(_4,_5){
var _6=null;
if(_4=="POST"||_4=="PUT"){
var p=(_5?_5[0]:undefined);
if(p){
if(_2.isString(p)){
_6=p;
}else{
_6=_2.toJson(p);
}
}
}
return _6;
}});
});
