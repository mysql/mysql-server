//>>built
define("dojox/rpc/JsonRPC",["dojo","dojox","dojox/rpc/Service","dojo/errors/RequestError"],function(_1,_2,_3,_4){
function _5(_6){
return {serialize:function(_7,_8,_9,_a){
var d={id:this._requestId++,method:_8.name,params:_9};
if(_6){
d.jsonrpc=_6;
}
return {data:_1.toJson(d),handleAs:"json",contentType:"application/json",transport:"POST"};
},deserialize:function(_b){
if("Error"==_b.name||_b instanceof _4){
_b=_1.fromJson(_b.responseText);
}
if(_b.error){
var e=new Error(_b.error.message||_b.error);
e._rpcErrorObject=_b.error;
return e;
}
return _b.result;
}};
};
_2.rpc.envelopeRegistry.register("JSON-RPC-1.0",function(_c){
return _c=="JSON-RPC-1.0";
},_1.mixin({namedParams:false},_5()));
_2.rpc.envelopeRegistry.register("JSON-RPC-2.0",function(_d){
return _d=="JSON-RPC-2.0";
},_1.mixin({namedParams:true},_5("2.0")));
});
