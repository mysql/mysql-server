//>>built
define("dojox/rpc/OfflineRest",["dojo","dojox","dojox/data/ClientFilter","dojox/rpc/Rest","dojox/storage"],function(_1,_2){
var _3=_2.rpc.Rest;
var _4="dojox_rpc_OfflineRest";
var _5;
var _6=_3._index;
_2.storage.manager.addOnLoad(function(){
_5=_2.storage.manager.available;
for(var i in _6){
_7(_6[i],i);
}
});
var _8;
function _9(_a){
return _a.replace(/[^0-9A-Za-z_]/g,"_");
};
function _7(_b,id){
if(_5&&!_8&&(id||(_b&&_b.__id))){
_2.storage.put(_9(id||_b.__id),typeof _b=="object"?_2.json.ref.toJson(_b):_b,function(){
},_4);
}
};
function _c(_d){
return _d instanceof Error&&(_d.status==503||_d.status>12000||!_d.status);
};
function _e(){
if(_5){
var _f=_2.storage.get("dirty",_4);
if(_f){
for(var _10 in _f){
_11(_10,_f);
}
}
}
};
var _12;
function _13(){
_12.sendChanges();
_12.downloadChanges();
};
var _14=setInterval(_13,15000);
_1.connect(document,"ononline",_13);
_12=_2.rpc.OfflineRest={turnOffAutoSync:function(){
clearInterval(_14);
},sync:_13,sendChanges:_e,downloadChanges:function(){
},addStore:function(_15,_16){
_12.stores.push(_15);
_15.fetch({queryOptions:{cache:true},query:_16,onComplete:function(_17,_18){
_15._localBaseResults=_17;
_15._localBaseFetch=_18;
}});
}};
_12.stores=[];
var _19=_3._get;
_3._get=function(_1a,id){
try{
_e();
if(window.navigator&&navigator.onLine===false){
throw new Error();
}
var dfd=_19(_1a,id);
}
catch(e){
dfd=new _1.Deferred();
dfd.errback(e);
}
var _1b=_2.rpc._sync;
dfd.addCallback(function(_1c){
_7(_1c,_1a._getRequest(id).url);
return _1c;
});
dfd.addErrback(function(_1d){
if(_5){
if(_c(_1d)){
var _1e={};
var _1f=function(id,_20){
if(_1e[id]){
return _20;
}
var _21=_1.fromJson(_2.storage.get(_9(id),_4))||_20;
_1e[id]=_21;
for(var i in _21){
var val=_21[i];
id=val&&val.$ref;
if(id){
if(id.substring&&id.substring(0,4)=="cid:"){
id=id.substring(4);
}
_21[i]=_1f(id,val);
}
}
if(_21 instanceof Array){
for(i=0;i<_21.length;i++){
if(_21[i]===undefined){
_21.splice(i--,1);
}
}
}
return _21;
};
_8=true;
var _22=_1f(_1a._getRequest(id).url);
if(!_22){
return _1d;
}
_8=false;
return _22;
}else{
return _1d;
}
}else{
if(_1b){
return new Error("Storage manager not loaded, can not continue");
}
dfd=new _1.Deferred();
dfd.addCallback(arguments.callee);
_2.storage.manager.addOnLoad(function(){
dfd.callback();
});
return dfd;
}
});
return dfd;
};
function _23(_24,_25,_26,_27,_28){
if(_24=="delete"){
_2.storage.remove(_9(_25),_4);
}else{
_2.storage.put(_9(_26),_27,function(){
},_4);
}
var _29=_28&&_28._store;
if(_29){
_29.updateResultSet(_29._localBaseResults,_29._localBaseFetch);
_2.storage.put(_9(_28._getRequest(_29._localBaseFetch.query).url),_2.json.ref.toJson(_29._localBaseResults),function(){
},_4);
}
};
_1.addOnLoad(function(){
_1.connect(_2.data,"restListener",function(_2a){
var _2b=_2a.channel;
var _2c=_2a.event.toLowerCase();
var _2d=_2.rpc.JsonRest&&_2.rpc.JsonRest.getServiceAndId(_2b).service;
_23(_2c,_2b,_2c=="post"?_2b+_2a.result.id:_2b,_1.toJson(_2a.result),_2d);
});
});
var _2e=_3._change;
_3._change=function(_2f,_30,id,_31){
if(!_5){
return _2e.apply(this,arguments);
}
var _32=_30._getRequest(id).url;
_23(_2f,_32,_2.rpc.JsonRest._contentId,_31,_30);
var _33=_2.storage.get("dirty",_4)||{};
if(_2f=="put"||_2f=="delete"){
var _34=_32;
}else{
_34=0;
for(var i in _33){
if(!isNaN(parseInt(i))){
_34=i;
}
}
_34++;
}
_33[_34]={method:_2f,id:_32,content:_31};
return _11(_34,_33);
};
function _11(_35,_36){
var _37=_36[_35];
var _38=_2.rpc.JsonRest.getServiceAndId(_37.id);
var _39=_2e(_37.method,_38.service,_38.id,_37.content);
_36[_35]=_37;
_2.storage.put("dirty",_36,function(){
},_4);
_39.addBoth(function(_3a){
if(_c(_3a)){
return null;
}
var _3b=_2.storage.get("dirty",_4)||{};
delete _3b[_35];
_2.storage.put("dirty",_3b,function(){
},_4);
return _3a;
});
return _39;
};
_1.connect(_6,"onLoad",_7);
_1.connect(_6,"onUpdate",_7);
return _12;
});
