//>>built
define("dojox/data/PersevereStore",["dojo","dojox","require","dojox/data/JsonQueryRestStore","dojox/rpc/Client","dojo/_base/url"],function(_1,_2,_3){
_2.json.ref.serializeFunctions=true;
_1.declare("dojox.data.PersevereStore",_2.data.JsonQueryRestStore,{useFullIdInQueries:true,jsonQueryPagination:false});
_2.data.PersevereStore.getStores=function(_4,_5){
_4=(_4&&(_4.match(/\/$/)?_4:(_4+"/")))||"/";
if(_4.match(/^\w*:\/\//)){
_3("dojox/io/xhrScriptPlugin");
_2.io.xhrScriptPlugin(_4,"callback",_2.io.xhrPlugins.fullHttpAdapter);
}
var _6=_1.xhr;
_1.xhr=function(_7,_8){
(_8.headers=_8.headers||{})["Server-Methods"]="false";
return _6.apply(_1,arguments);
};
var _9=_2.rpc.Rest(_4,true);
_2.rpc._sync=_5;
var _a=_9("Class/");
var _b;
var _c={};
var _d=0;
_a.addCallback(function(_e){
_2.json.ref.resolveJson(_e,{index:_2.rpc.Rest._index,idPrefix:"/Class/",assignAbsoluteIds:true});
function _f(_10){
if(_10["extends"]&&_10["extends"].prototype){
if(!_10.prototype||!_10.prototype.isPrototypeOf(_10["extends"].prototype)){
_f(_10["extends"]);
_2.rpc.Rest._index[_10.prototype.__id]=_10.prototype=_1.mixin(_1.delegate(_10["extends"].prototype),_10.prototype);
}
}
};
function _11(_12,_13){
if(_12&&_13){
for(var j in _12){
var _14=_12[j];
if(_14.runAt!="client"&&!_13[j]){
_13[j]=(function(_15){
return function(){
var _16=_1.rawXhrPost({url:this.__id,postData:_2.json.ref.toJson({method:_15,id:_d++,params:_1._toArray(arguments)}),handleAs:"json"});
_16.addCallback(function(_17){
return _17.error?new Error(_17.error):_17.result;
});
return _16;
};
})(j);
}
}
}
};
for(var i in _e){
if(typeof _e[i]=="object"){
var _18=_e[i];
_f(_18);
_11(_18.methods,_18.prototype=_18.prototype||{});
_11(_18.staticMethods,_18);
_c[_e[i].id]=new _2.data.PersevereStore({target:new _1._Url(_4,_e[i].id)+"/",schema:_18});
}
}
return (_b=_c);
});
_1.xhr=_6;
return _5?_b:_a;
};
_2.data.PersevereStore.addProxy=function(){
_3("dojox/io/xhrPlugins");
_2.io.xhrPlugins.addProxy("/proxy/");
};
return _2.data.PersevereStore;
});
