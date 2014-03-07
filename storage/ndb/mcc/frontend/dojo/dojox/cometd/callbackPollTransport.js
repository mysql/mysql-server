//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/cometd/_base,dojox/cometd/longPollTransport,dojo/io/script"],function(_1,_2,_3){
_2.provide("dojox.cometd.callbackPollTransport");
_2.require("dojox.cometd._base");
_2.require("dojox.cometd.longPollTransport");
_2.require("dojo.io.script");
_3.cometd.callbackPollTransport=new function(){
this._connectionType="callback-polling";
this._cometd=null;
this.check=function(_4,_5,_6){
return (_2.indexOf(_4,"callback-polling")>=0);
};
this.tunnelInit=function(){
var _7={channel:"/meta/connect",clientId:this._cometd.clientId,connectionType:this._connectionType,id:""+this._cometd.messageId++};
_7=this._cometd._extendOut(_7);
this.openTunnelWith([_7]);
};
this.tunnelCollapse=_3.cometd.longPollTransport.tunnelCollapse;
this._connect=_3.cometd.longPollTransport._connect;
this.deliver=_3.cometd.longPollTransport.deliver;
this.openTunnelWith=function(_8,_9){
this._cometd._polling=true;
var _a={load:_2.hitch(this,function(_b){
this._cometd._polling=false;
this._cometd.deliver(_b);
this._cometd._backon();
this.tunnelCollapse();
}),error:_2.hitch(this,function(_c){
this._cometd._polling=false;
this._cometd._publishMeta("connect",false);
this._cometd._backoff();
this.tunnelCollapse();
}),url:(_9||this._cometd.url),content:{message:_2.toJson(_8)},callbackParamName:"jsonp"};
var _d=this._cometd._connectTimeout();
if(_d>0){
_a.timeout=_d;
}
_2.io.script.get(_a);
};
this.sendMessages=function(_e){
for(var i=0;i<_e.length;i++){
_e[i].clientId=this._cometd.clientId;
_e[i].id=""+this._cometd.messageId++;
_e[i]=this._cometd._extendOut(_e[i]);
}
var _f={url:this._cometd.url||_2.config["cometdRoot"],load:_2.hitch(this._cometd,"deliver"),callbackParamName:"jsonp",content:{message:_2.toJson(_e)},error:_2.hitch(this,function(err){
this._cometd._publishMeta("publish",false,{messages:_e});
}),timeout:this._cometd.expectedNetworkDelay};
return _2.io.script.get(_f);
};
this.startup=function(_10){
if(this._cometd._connected){
return;
}
this.tunnelInit();
};
this.disconnect=_3.cometd.longPollTransport.disconnect;
this.disconnect=function(){
var _11={channel:"/meta/disconnect",clientId:this._cometd.clientId,id:""+this._cometd.messageId++};
_11=this._cometd._extendOut(_11);
_2.io.script.get({url:this._cometd.url||_2.config["cometdRoot"],callbackParamName:"jsonp",content:{message:_2.toJson([_11])}});
};
this.cancelConnect=function(){
};
};
_3.cometd.connectionTypes.register("callback-polling",_3.cometd.callbackPollTransport.check,_3.cometd.callbackPollTransport);
});
