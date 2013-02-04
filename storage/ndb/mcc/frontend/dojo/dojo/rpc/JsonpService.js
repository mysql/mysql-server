/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/rpc/JsonpService",["../main","./RpcService","../io/script"],function(_1){
_1.declare("dojo.rpc.JsonpService",_1.rpc.RpcService,{constructor:function(_2,_3){
if(this.required){
if(_3){
_1.mixin(this.required,_3);
}
_1.forEach(this.required,function(_4){
if(_4==""||_4==undefined){
throw new Error("Required Service Argument not found: "+_4);
}
});
}
},strictArgChecks:false,bind:function(_5,_6,_7,_8){
var _9=_1.io.script.get({url:_8||this.serviceUrl,callbackParamName:this.callbackParamName||"callback",content:this.createRequest(_6),timeout:this.timeout,handleAs:"json",preventCache:true});
_9.addCallbacks(this.resultCallback(_7),this.errorCallback(_7));
},createRequest:function(_a){
var _b=(_1.isArrayLike(_a)&&_a.length==1)?_a[0]:{};
_1.mixin(_b,this.required);
return _b;
}});
return _1.rpc.JsonpService;
});
