/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/rpc/JsonService",["../main","./RpcService"],function(_1){
_1.declare("dojo.rpc.JsonService",_1.rpc.RpcService,{bustCache:false,contentType:"application/json-rpc",lastSubmissionId:0,callRemote:function(_2,_3){
var _4=new _1.Deferred();
this.bind(_2,_3,_4);
return _4;
},bind:function(_5,_6,_7,_8){
var _9=_1.rawXhrPost({url:_8||this.serviceUrl,postData:this.createRequest(_5,_6),contentType:this.contentType,timeout:this.timeout,handleAs:"json-comment-optional"});
_9.addCallbacks(this.resultCallback(_7),this.errorCallback(_7));
},createRequest:function(_a,_b){
var _c={"params":_b,"method":_a,"id":++this.lastSubmissionId};
return _1.toJson(_c);
},parseResults:function(_d){
if(_1.isObject(_d)){
if("result" in _d){
return _d.result;
}
if("Result" in _d){
return _d.Result;
}
if("ResultSet" in _d){
return _d.ResultSet;
}
}
return _d;
}});
return _1.rpc.JsonService;
});
