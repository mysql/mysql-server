//>>built
define("dojox/data/PersevereStore",["dojo","dojox","require","dojox/data/JsonQueryRestStore","dojox/rpc/Client","dojo/_base/url"],function(_1,_2,_3){
_2.json.ref.serializeFunctions=true;
var _4=_1.declare("dojox.data.PersevereStore",_2.data.JsonQueryRestStore,{useFullIdInQueries:true,jsonQueryPagination:false});
_4.getStores=function(_5,_6){
_5=(_5&&(_5.match(/\/$/)?_5:(_5+"/")))||"/";
if(_5.match(/^\w*:\/\//)){
_3("dojox/io/xhrScriptPlugin");
_2.io.xhrScriptPlugin(_5,"callback",_2.io.xhrPlugins.fullHttpAdapter);
}
var _7=_1.xhr;
_1.xhr=function(_8,_9){
(_9.headers=_9.headers||{})["Server-Methods"]="false";
return _7.apply(_1,arguments);
};
var _a=_2.rpc.Rest(_5,true);
_2.rpc._sync=_6;
var _b=_a("Class/");
var _c;
var _d={};
var _e=0;
_b.addCallback(function(_f){
_2.json.ref.resolveJson(_f,{index:_2.rpc.Rest._index,idPrefix:"/Class/",assignAbsoluteIds:true});
function _10(_11){
if(_11["extends"]&&_11["extends"].prototype){
if(!_11.prototype||!_11.prototype.isPrototypeOf(_11["extends"].prototype)){
_10(_11["extends"]);
_2.rpc.Rest._index[_11.prototype.__id]=_11.prototype=_1.mixin(_1.delegate(_11["extends"].prototype),_11.prototype);
}
}
};
function _12(_13,_14){
if(_13&&_14){
for(var j in _13){
var _15=_13[j];
if(_15.runAt!="client"&&!_14[j]){
_14[j]=(function(_16){
return function(){
var _17=_1.rawXhrPost({url:this.__id,postData:_2.json.ref.toJson({method:_16,id:_e++,params:_1._toArray(arguments)}),handleAs:"json"});
_17.addCallback(function(_18){
return _18.error?new Error(_18.error):_18.result;
});
return _17;
};
})(j);
}
}
}
};
for(var i in _f){
if(typeof _f[i]=="object"){
var _19=_f[i];
_10(_19);
_12(_19.methods,_19.prototype=_19.prototype||{});
_12(_19.staticMethods,_19);
_d[_f[i].id]=new _2.data.PersevereStore({target:new _1._Url(_5,_f[i].id)+"/",schema:_19});
}
}
return (_c=_d);
});
_1.xhr=_7;
return _6?_c:_b;
};
_4.addProxy=function(){
_3("dojox/io/xhrPlugins");
_2.io.xhrPlugins.addProxy("/proxy/");
};
return _4;
});
