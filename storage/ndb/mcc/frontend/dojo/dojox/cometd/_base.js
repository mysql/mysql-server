//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/AdapterRegistry"],function(_1,_2,_3){
_2.provide("dojox.cometd._base");
_2.require("dojo.AdapterRegistry");
_3.cometd={Connection:function(_4){
_2.mixin(this,{prefix:_4,_status:"unconnected",_handshook:false,_initialized:false,_polling:false,expectedNetworkDelay:10000,connectTimeout:0,version:"1.0",minimumVersion:"0.9",clientId:null,messageId:0,batch:0,_isXD:false,handshakeReturn:null,currentTransport:null,url:null,lastMessage:null,_messageQ:[],handleAs:"json",_advice:{},_backoffInterval:0,_backoffIncrement:1000,_backoffMax:60000,_deferredSubscribes:{},_deferredUnsubscribes:{},_subscriptions:[],_extendInList:[],_extendOutList:[]});
this.state=function(){
return this._status;
};
this.init=function(_5,_6,_7){
_6=_6||{};
_6.version=this.version;
_6.minimumVersion=this.minimumVersion;
_6.channel="/meta/handshake";
_6.id=""+this.messageId++;
this.url=_5||_2.config["cometdRoot"];
if(!this.url){
throw "no cometd root";
return null;
}
var _8="^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?$";
var _9=(""+window.location).match(new RegExp(_8));
if(_9[4]){
var _a=_9[4].split(":");
var _b=_a[0];
var _c=_a[1]||"80";
_9=this.url.match(new RegExp(_8));
if(_9[4]){
_a=_9[4].split(":");
var _d=_a[0];
var _e=_a[1]||"80";
this._isXD=((_d!=_b)||(_e!=_c));
}
}
if(!this._isXD){
_6.supportedConnectionTypes=_2.map(_3.cometd.connectionTypes.pairs,"return item[0]");
}
_6=this._extendOut(_6);
var _f={url:this.url,handleAs:this.handleAs,content:{"message":_2.toJson([_6])},load:_2.hitch(this,function(msg){
this._backon();
this._finishInit(msg);
}),error:_2.hitch(this,function(e){
this._backoff();
this._finishInit(e);
}),timeout:this.expectedNetworkDelay};
if(_7){
_2.mixin(_f,_7);
}
this._props=_6;
for(var _10 in this._subscriptions){
for(var sub in this._subscriptions[_10]){
if(this._subscriptions[_10][sub].topic){
_2.unsubscribe(this._subscriptions[_10][sub].topic);
}
}
}
this._messageQ=[];
this._subscriptions=[];
this._initialized=true;
this._status="handshaking";
this.batch=0;
this.startBatch();
var r;
if(this._isXD){
_f.callbackParamName="jsonp";
r=_2.io.script.get(_f);
}else{
r=_2.xhrPost(_f);
}
return r;
};
this.publish=function(_11,_12,_13){
var _14={data:_12,channel:_11};
if(_13){
_2.mixin(_14,_13);
}
this._sendMessage(_14);
};
this.subscribe=function(_15,_16,_17,_18){
_18=_18||{};
if(_16){
var _19=_4+_15;
var _1a=this._subscriptions[_19];
if(!_1a||_1a.length==0){
_1a=[];
_18.channel="/meta/subscribe";
_18.subscription=_15;
this._sendMessage(_18);
var _1b=this._deferredSubscribes;
if(_1b[_15]){
_1b[_15].cancel();
delete _1b[_15];
}
_1b[_15]=new _2.Deferred();
}
for(var i in _1a){
if(_1a[i].objOrFunc===_16&&(!_1a[i].funcName&&!_17||_1a[i].funcName==_17)){
return null;
}
}
var _1c=_2.subscribe(_19,_16,_17);
_1a.push({topic:_1c,objOrFunc:_16,funcName:_17});
this._subscriptions[_19]=_1a;
}
var ret=this._deferredSubscribes[_15]||{};
ret.args=_2._toArray(arguments);
return ret;
};
this.unsubscribe=function(_1d,_1e,_1f,_20){
if((arguments.length==1)&&(!_2.isString(_1d))&&(_1d.args)){
return this.unsubscribe.apply(this,_1d.args);
}
var _21=_4+_1d;
var _22=this._subscriptions[_21];
if(!_22||_22.length==0){
return null;
}
var s=0;
for(var i in _22){
var sb=_22[i];
if((!_1e)||(sb.objOrFunc===_1e&&(!sb.funcName&&!_1f||sb.funcName==_1f))){
_2.unsubscribe(_22[i].topic);
delete _22[i];
}else{
s++;
}
}
if(s==0){
_20=_20||{};
_20.channel="/meta/unsubscribe";
_20.subscription=_1d;
delete this._subscriptions[_21];
this._sendMessage(_20);
this._deferredUnsubscribes[_1d]=new _2.Deferred();
if(this._deferredSubscribes[_1d]){
this._deferredSubscribes[_1d].cancel();
delete this._deferredSubscribes[_1d];
}
}
return this._deferredUnsubscribes[_1d];
};
this.disconnect=function(){
for(var _23 in this._subscriptions){
for(var sub in this._subscriptions[_23]){
if(this._subscriptions[_23][sub].topic){
_2.unsubscribe(this._subscriptions[_23][sub].topic);
}
}
}
this._subscriptions=[];
this._messageQ=[];
if(this._initialized&&this.currentTransport){
this._initialized=false;
this.currentTransport.disconnect();
}
if(!this._polling){
this._publishMeta("connect",false);
}
this._initialized=false;
this._handshook=false;
this._status="disconnected";
this._publishMeta("disconnect",true);
};
this.subscribed=function(_24,_25){
};
this.unsubscribed=function(_26,_27){
};
this.tunnelInit=function(_28,_29){
};
this.tunnelCollapse=function(){
};
this._backoff=function(){
if(!this._advice){
this._advice={reconnect:"retry",interval:0};
}else{
if(!this._advice.interval){
this._advice.interval=0;
}
}
if(this._backoffInterval<this._backoffMax){
this._backoffInterval+=this._backoffIncrement;
}
};
this._backon=function(){
this._backoffInterval=0;
};
this._interval=function(){
var i=this._backoffInterval+(this._advice?(this._advice.interval?this._advice.interval:0):0);
if(i>0){
}
return i;
};
this._publishMeta=function(_2a,_2b,_2c){
try{
var _2d={cometd:this,action:_2a,successful:_2b,state:this.state()};
if(_2c){
_2.mixin(_2d,_2c);
}
_2.publish(this.prefix+"/meta",[_2d]);
}
catch(e){
}
};
this._finishInit=function(_2e){
if(this._status!="handshaking"){
return;
}
var _2f=this._handshook;
var _30=false;
var _31={};
if(_2e instanceof Error){
_2.mixin(_31,{reestablish:false,failure:true,error:_2e,advice:this._advice});
}else{
_2e=_2e[0];
_2e=this._extendIn(_2e);
this.handshakeReturn=_2e;
if(_2e["advice"]){
this._advice=_2e.advice;
}
_30=_2e.successful?_2e.successful:false;
if(_2e.version<this.minimumVersion){
if(console.log){
}
_30=false;
this._advice.reconnect="none";
}
_2.mixin(_31,{reestablish:_30&&_2f,response:_2e});
}
this._publishMeta("handshake",_30,_31);
if(this._status!="handshaking"){
return;
}
if(_30){
this._status="connecting";
this._handshook=true;
this.currentTransport=_3.cometd.connectionTypes.match(_2e.supportedConnectionTypes,_2e.version,this._isXD);
var _32=this.currentTransport;
_32._cometd=this;
_32.version=_2e.version;
this.clientId=_2e.clientId;
this.tunnelInit=_32.tunnelInit&&_2.hitch(_32,"tunnelInit");
this.tunnelCollapse=_32.tunnelCollapse&&_2.hitch(_32,"tunnelCollapse");
_32.startup(_2e);
}else{
if(!this._advice||this._advice["reconnect"]!="none"){
setTimeout(_2.hitch(this,"init",this.url,this._props),this._interval());
}
}
};
this._extendIn=function(_33){
_2.forEach(_3.cometd._extendInList,function(f){
_33=f(_33)||_33;
});
return _33;
};
this._extendOut=function(_34){
_2.forEach(_3.cometd._extendOutList,function(f){
_34=f(_34)||_34;
});
return _34;
};
this.deliver=function(_35){
_2.forEach(_35,this._deliver,this);
return _35;
};
this._deliver=function(_36){
_36=this._extendIn(_36);
if(!_36["channel"]){
if(_36["success"]!==true){
return;
}
}
this.lastMessage=_36;
if(_36.advice){
this._advice=_36.advice;
}
var _37=null;
if((_36["channel"])&&(_36.channel.length>5)&&(_36.channel.substr(0,5)=="/meta")){
switch(_36.channel){
case "/meta/connect":
var _38={response:_36};
if(_36.successful){
if(this._status!="connected"){
this._status="connected";
this.endBatch();
}
}
if(this._initialized){
this._publishMeta("connect",_36.successful,_38);
}
break;
case "/meta/subscribe":
_37=this._deferredSubscribes[_36.subscription];
try{
if(!_36.successful){
if(_37){
_37.errback(new Error(_36.error));
}
this.currentTransport.cancelConnect();
return;
}
if(_37){
_37.callback(true);
}
this.subscribed(_36.subscription,_36);
}
catch(e){
log.warn(e);
}
break;
case "/meta/unsubscribe":
_37=this._deferredUnsubscribes[_36.subscription];
try{
if(!_36.successful){
if(_37){
_37.errback(new Error(_36.error));
}
this.currentTransport.cancelConnect();
return;
}
if(_37){
_37.callback(true);
}
this.unsubscribed(_36.subscription,_36);
}
catch(e){
log.warn(e);
}
break;
default:
if(_36.successful&&!_36.successful){
this.currentTransport.cancelConnect();
return;
}
}
}
this.currentTransport.deliver(_36);
if(_36.data){
try{
var _39=[_36];
var _3a=_4+_36.channel;
var _3b=_36.channel.split("/");
var _3c=_4;
for(var i=1;i<_3b.length-1;i++){
_2.publish(_3c+"/**",_39);
_3c+="/"+_3b[i];
}
_2.publish(_3c+"/**",_39);
_2.publish(_3c+"/*",_39);
_2.publish(_3a,_39);
}
catch(e){
}
}
};
this._sendMessage=function(_3d){
if(this.currentTransport&&!this.batch){
return this.currentTransport.sendMessages([_3d]);
}else{
this._messageQ.push(_3d);
return null;
}
};
this.startBatch=function(){
this.batch++;
};
this.endBatch=function(){
if(--this.batch<=0&&this.currentTransport&&this._status=="connected"){
this.batch=0;
var _3e=this._messageQ;
this._messageQ=[];
if(_3e.length>0){
this.currentTransport.sendMessages(_3e);
}
}
};
this._onUnload=function(){
_2.addOnUnload(_3.cometd,"disconnect");
};
this._connectTimeout=function(){
var _3f=0;
if(this._advice&&this._advice.timeout&&this.expectedNetworkDelay>0){
_3f=this._advice.timeout+this.expectedNetworkDelay;
}
if(this.connectTimeout>0&&this.connectTimeout<_3f){
return this.connectTimeout;
}
return _3f;
};
},connectionTypes:new _2.AdapterRegistry(true)};
_3.cometd.Connection.call(_3.cometd,"/cometd");
_2.addOnUnload(_3.cometd,"_onUnload");
});
