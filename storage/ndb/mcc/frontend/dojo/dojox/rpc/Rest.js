//>>built
define("dojox/rpc/Rest",["dojo","dojox"],function(_1,_2){
_1.getObject("rpc.Rest",true,_2);
if(_2.rpc&&_2.rpc.transportRegistry){
_2.rpc.transportRegistry.register("REST",function(_3){
return _3=="REST";
},{getExecutor:function(_4,_5,_6){
return new _2.rpc.Rest(_5.name,(_5.contentType||_6._smd.contentType||"").match(/json|javascript/),null,function(id,_7){
var _8=_6._getRequest(_5,[id]);
_8.url=_8.target+(_8.data?"?"+_8.data:"");
if(_7&&(_7.start>=0||_7.count>=0)){
_8.headers=_8.headers||{};
_8.headers.Range="items="+(_7.start||"0")+"-"+(("count" in _7&&_7.count!=Infinity)?(_7.count+(_7.start||0)-1):"");
}
return _8;
});
}});
}
var _9;
function _a(_b,_c,_d,id){
_b.addCallback(function(_e){
if(_b.ioArgs.xhr&&_d){
_d=_b.ioArgs.xhr.getResponseHeader("Content-Range");
_b.fullLength=_d&&(_d=_d.match(/\/(.*)/))&&parseInt(_d[1]);
}
return _e;
});
return _b;
};
_9=_2.rpc.Rest=function(_f,_10,_11,_12){
var _13;
_13=function(id,_14){
return _9._get(_13,id,_14);
};
_13.isJson=_10;
_13._schema=_11;
_13.cache={serialize:_10?((_2.json&&_2.json.ref)||_1).toJson:function(_15){
return _15;
}};
_13._getRequest=_12||function(id,_16){
if(_1.isObject(id)){
id=_1.objectToQuery(id);
id=id?"?"+id:"";
}
if(_16&&_16.sort&&!_16.queryStr){
id+=(id?"&":"?")+"sort(";
for(var i=0;i<_16.sort.length;i++){
var _17=_16.sort[i];
id+=(i>0?",":"")+(_17.descending?"-":"+")+encodeURIComponent(_17.attribute);
}
id+=")";
}
var _18={url:_f+(id==null?"":id),handleAs:_10?"json":"text",contentType:_10?"application/json":"text/plain",sync:_2.rpc._sync,headers:{Accept:_10?"application/json,application/javascript":"*/*"}};
if(_16&&(_16.start>=0||_16.count>=0)){
_18.headers.Range="items="+(_16.start||"0")+"-"+(("count" in _16&&_16.count!=Infinity)?(_16.count+(_16.start||0)-1):"");
}
_2.rpc._sync=false;
return _18;
};
function _19(_1a){
_13[_1a]=function(id,_1b){
return _9._change(_1a,_13,id,_1b);
};
};
_19("put");
_19("post");
_19("delete");
_13.servicePath=_f;
return _13;
};
_9._index={};
_9._timeStamps={};
_9._change=function(_1c,_1d,id,_1e){
var _1f=_1d._getRequest(id);
_1f[_1c+"Data"]=_1e;
return _a(_1.xhr(_1c.toUpperCase(),_1f,true),_1d);
};
_9._get=function(_20,id,_21){
_21=_21||{};
return _a(_1.xhrGet(_20._getRequest(id,_21)),_20,(_21.start>=0||_21.count>=0),id);
};
return _2.rpc.Rest;
});
