/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/rpc/RpcService",["../main","../_base/url"],function(_1){
_1.declare("dojo.rpc.RpcService",null,{constructor:function(_2){
if(_2){
if((_1.isString(_2))||(_2 instanceof _1._Url)){
if(_2 instanceof _1._Url){
var _3=_2+"";
}else{
_3=_2;
}
var _4=_1.xhrGet({url:_3,handleAs:"json-comment-optional",sync:true});
_4.addCallback(this,"processSmd");
_4.addErrback(function(){
throw new Error("Unable to load SMD from "+_2);
});
}else{
if(_2.smdStr){
this.processSmd(_1.eval("("+_2.smdStr+")"));
}else{
if(_2.serviceUrl){
this.serviceUrl=_2.serviceUrl;
}
this.timeout=_2.timeout||3000;
if("strictArgChecks" in _2){
this.strictArgChecks=_2.strictArgChecks;
}
this.processSmd(_2);
}
}
}
},strictArgChecks:true,serviceUrl:"",parseResults:function(_5){
return _5;
},errorCallback:function(_6){
return function(_7){
_6.errback(_7.message);
};
},resultCallback:function(_8){
return _1.hitch(this,function(_9){
if(_9.error!=null){
var _a;
if(typeof _9.error=="object"){
_a=new Error(_9.error.message);
_a.code=_9.error.code;
_a.error=_9.error.error;
}else{
_a=new Error(_9.error);
}
_a.id=_9.id;
_a.errorObject=_9;
_8.errback(_a);
}else{
_8.callback(this.parseResults(_9));
}
});
},generateMethod:function(_b,_c,_d){
return _1.hitch(this,function(){
var _e=new _1.Deferred();
if((this.strictArgChecks)&&(_c!=null)&&(arguments.length!=_c.length)){
throw new Error("Invalid number of parameters for remote method.");
}else{
this.bind(_b,_1._toArray(arguments),_e,_d);
}
return _e;
});
},processSmd:function(_f){
if(_f.methods){
_1.forEach(_f.methods,function(m){
if(m&&m.name){
this[m.name]=this.generateMethod(m.name,m.parameters,m.url||m.serviceUrl||m.serviceURL);
if(!_1.isFunction(this[m.name])){
throw new Error("RpcService: Failed to create"+m.name+"()");
}
}
},this);
}
this.serviceUrl=_f.serviceUrl||_f.serviceURL;
this.required=_f.required;
this.smd=_f;
}});
return _1.rpc.RpcService;
});
