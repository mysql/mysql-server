//>>built
define("dojox/rpc/Service",["dojo","dojox","dojo/AdapterRegistry","dojo/_base/url"],function(_1,_2){
_1.declare("dojox.rpc.Service",null,{constructor:function(_3,_4){
var _5;
var _6=this;
function _7(_8){
_8._baseUrl=new _1._Url((_1.isBrowser?location.href:_1.config.baseUrl),_5||".")+"";
_6._smd=_8;
for(var _9 in _6._smd.services){
var _a=_9.split(".");
var _b=_6;
for(var i=0;i<_a.length-1;i++){
_b=_b[_a[i]]||(_b[_a[i]]={});
}
_b[_a[_a.length-1]]=_6._generateService(_9,_6._smd.services[_9]);
}
};
if(_3){
if((_1.isString(_3))||(_3 instanceof _1._Url)){
if(_3 instanceof _1._Url){
_5=_3+"";
}else{
_5=_3;
}
var _c=_1._getText(_5);
if(!_c){
throw new Error("Unable to load SMD from "+_3);
}else{
_7(_1.fromJson(_c));
}
}else{
_7(_3);
}
}
this._options=(_4?_4:{});
this._requestId=0;
},_generateService:function(_d,_e){
if(this[_e]){
throw new Error("WARNING: "+_d+" already exists for service. Unable to generate function");
}
_e.name=_d;
var _f=_1.hitch(this,"_executeMethod",_e);
var _10=_2.rpc.transportRegistry.match(_e.transport||this._smd.transport);
if(_10.getExecutor){
_f=_10.getExecutor(_f,_e,this);
}
var _11=_e.returns||(_e._schema={});
var _12="/"+_d+"/";
_11._service=_f;
_f.servicePath=_12;
_f._schema=_11;
_f.id=_2.rpc.Service._nextId++;
return _f;
},_getRequest:function(_13,_14){
var smd=this._smd;
var _15=_2.rpc.envelopeRegistry.match(_13.envelope||smd.envelope||"NONE");
var _16=(_13.parameters||[]).concat(smd.parameters||[]);
if(_15.namedParams){
if((_14.length==1)&&_1.isObject(_14[0])){
_14=_14[0];
}else{
var _17={};
for(var i=0;i<_13.parameters.length;i++){
if(typeof _14[i]!="undefined"||!_13.parameters[i].optional){
_17[_13.parameters[i].name]=_14[i];
}
}
_14=_17;
}
if(_13.strictParameters||smd.strictParameters){
for(i in _14){
var _18=false;
for(var j=0;j<_16.length;j++){
if(_16[i].name==i){
_18=true;
}
}
if(!_18){
delete _14[i];
}
}
}
for(i=0;i<_16.length;i++){
var _19=_16[i];
if(!_19.optional&&_19.name&&!_14[_19.name]){
if(_19["default"]){
_14[_19.name]=_19["default"];
}else{
if(!(_19.name in _14)){
throw new Error("Required parameter "+_19.name+" was omitted");
}
}
}
}
}else{
if(_16&&_16[0]&&_16[0].name&&(_14.length==1)&&_1.isObject(_14[0])){
if(_15.namedParams===false){
_14=_2.rpc.toOrdered(_16,_14);
}else{
_14=_14[0];
}
}
}
if(_1.isObject(this._options)){
_14=_1.mixin(_14,this._options);
}
var _1a=_13._schema||_13.returns;
var _1b=_15.serialize.apply(this,[smd,_13,_14]);
_1b._envDef=_15;
var _1c=(_13.contentType||smd.contentType||_1b.contentType);
return _1.mixin(_1b,{sync:_2.rpc._sync,contentType:_1c,headers:_13.headers||smd.headers||_1b.headers||{},target:_1b.target||_2.rpc.getTarget(smd,_13),transport:_13.transport||smd.transport||_1b.transport,envelope:_13.envelope||smd.envelope||_1b.envelope,timeout:_13.timeout||smd.timeout,callbackParamName:_13.callbackParamName||smd.callbackParamName,rpcObjectParamName:_13.rpcObjectParamName||smd.rpcObjectParamName,schema:_1a,handleAs:_1b.handleAs||"auto",preventCache:_13.preventCache||smd.preventCache,frameDoc:this._options.frameDoc||undefined});
},_executeMethod:function(_1d){
var _1e=[];
var i;
for(i=1;i<arguments.length;i++){
_1e.push(arguments[i]);
}
var _1f=this._getRequest(_1d,_1e);
var _20=_2.rpc.transportRegistry.match(_1f.transport).fire(_1f);
_20.addBoth(function(_21){
return _1f._envDef.deserialize.call(this,_21);
});
return _20;
}});
_2.rpc.getTarget=function(smd,_22){
var _23=smd._baseUrl;
if(smd.target){
_23=new _1._Url(_23,smd.target)+"";
}
if(_22.target){
_23=new _1._Url(_23,_22.target)+"";
}
return _23;
};
_2.rpc.toOrdered=function(_24,_25){
if(_1.isArray(_25)){
return _25;
}
var _26=[];
for(var i=0;i<_24.length;i++){
_26.push(_25[_24[i].name]);
}
return _26;
};
_2.rpc.transportRegistry=new _1.AdapterRegistry(true);
_2.rpc.envelopeRegistry=new _1.AdapterRegistry(true);
_2.rpc.envelopeRegistry.register("URL",function(str){
return str=="URL";
},{serialize:function(smd,_27,_28){
var d=_1.objectToQuery(_28);
return {data:d,transport:"POST"};
},deserialize:function(_29){
return _29;
},namedParams:true});
_2.rpc.envelopeRegistry.register("JSON",function(str){
return str=="JSON";
},{serialize:function(smd,_2a,_2b){
var d=_1.toJson(_2b);
return {data:d,handleAs:"json",contentType:"application/json"};
},deserialize:function(_2c){
return _2c;
}});
_2.rpc.envelopeRegistry.register("PATH",function(str){
return str=="PATH";
},{serialize:function(smd,_2d,_2e){
var i;
var _2f=_2.rpc.getTarget(smd,_2d);
if(_1.isArray(_2e)){
for(i=0;i<_2e.length;i++){
_2f+="/"+_2e[i];
}
}else{
for(i in _2e){
_2f+="/"+i+"/"+_2e[i];
}
}
return {data:"",target:_2f};
},deserialize:function(_30){
return _30;
}});
_2.rpc.transportRegistry.register("POST",function(str){
return str=="POST";
},{fire:function(r){
r.url=r.target;
r.postData=r.data;
return _1.rawXhrPost(r);
}});
_2.rpc.transportRegistry.register("GET",function(str){
return str=="GET";
},{fire:function(r){
r.url=r.target+(r.data?"?"+((r.rpcObjectParamName)?r.rpcObjectParamName+"=":"")+r.data:"");
return _1.xhrGet(r);
}});
_2.rpc.transportRegistry.register("JSONP",function(str){
return str=="JSONP";
},{fire:function(r){
r.url=r.target+((r.target.indexOf("?")==-1)?"?":"&")+((r.rpcObjectParamName)?r.rpcObjectParamName+"=":"")+r.data;
r.callbackParamName=r.callbackParamName||"callback";
return _1.io.script.get(r);
}});
_2.rpc.Service._nextId=1;
_1._contentHandlers.auto=function(xhr){
var _31=_1._contentHandlers;
var _32=xhr.getResponseHeader("Content-Type");
var _33=!_32?_31.text(xhr):_32.match(/\/.*json/)?_31.json(xhr):_32.match(/\/javascript/)?_31.javascript(xhr):_32.match(/\/xml/)?_31.xml(xhr):_31.text(xhr);
return _33;
};
return _2.rpc.Service;
});
