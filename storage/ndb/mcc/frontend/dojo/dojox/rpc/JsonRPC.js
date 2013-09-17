//>>built
define("dojox/rpc/JsonRPC",["dojo","dojox","dojox/rpc/Service"],function(_1,_2){
function _3(_4){
return {serialize:function(_5,_6,_7,_8){
var d={id:this._requestId++,method:_6.name,params:_7};
if(_4){
d.jsonrpc=_4;
}
return {data:_1.toJson(d),handleAs:"json",contentType:"application/json",transport:"POST"};
},deserialize:function(_9){
if("Error"==_9.name){
_9=_1.fromJson(_9.responseText);
}
if(_9.error){
var e=new Error(_9.error.message||_9.error);
e._rpcErrorObject=_9.error;
return e;
}
return _9.result;
}};
};
_2.rpc.envelopeRegistry.register("JSON-RPC-1.0",function(_a){
return _a=="JSON-RPC-1.0";
},_1.mixin({namedParams:false},_3()));
_2.rpc.envelopeRegistry.register("JSON-RPC-2.0",function(_b){
return _b=="JSON-RPC-2.0";
},_3("2.0"));
});
