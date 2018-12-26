//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/xmpp/bosh,dojox/xmpp/util,dojox/data/dom"],function(_1,_2,_3){
_2.provide("dojox.xmpp.TransportSession");
_2.require("dojox.xmpp.bosh");
_2.require("dojox.xmpp.util");
_2.require("dojox.data.dom");
_3.xmpp.TransportSession=function(_4){
this.sendTimeout=(this.wait+20)*1000;
if(_4&&_2.isObject(_4)){
_2.mixin(this,_4);
if(this.useScriptSrcTransport){
this.transportIframes=[];
}
}
};
_2.extend(_3.xmpp.TransportSession,{rid:0,hold:1,polling:1000,secure:false,wait:60,lang:"en",submitContentType:"text/xml; charset=utf=8",serviceUrl:"/httpbind",defaultResource:"dojoIm",domain:"imserver.com",sendTimeout:0,useScriptSrcTransport:false,keepAliveTimer:null,state:"NotReady",transmitState:"Idle",protocolPacketQueue:[],outboundQueue:[],outboundRequests:{},inboundQueue:[],deferredRequests:{},matchTypeIdAttribute:{},open:function(){
this.status="notReady";
this.rid=Math.round(Math.random()*1000000000);
this.protocolPacketQueue=[];
this.outboundQueue=[];
this.outboundRequests={};
this.inboundQueue=[];
this.deferredRequests={};
this.matchTypeIdAttribute={};
this.keepAliveTimer=setTimeout(_2.hitch(this,"_keepAlive"),10000);
if(this.useScriptSrcTransport){
_3.xmpp.bosh.initialize({iframes:this.hold+1,load:_2.hitch(this,function(){
this._sendLogin();
})});
}else{
this._sendLogin();
}
},_sendLogin:function(){
var _5=this.rid++;
var _6={content:this.submitContentType,hold:this.hold,rid:_5,to:this.domain,secure:this.secure,wait:this.wait,"xml:lang":this.lang,"xmpp:version":"1.0",xmlns:_3.xmpp.xmpp.BODY_NS,"xmlns:xmpp":"urn:xmpp:xbosh"};
var _7=_3.xmpp.util.createElement("body",_6,true);
this.addToOutboundQueue(_7,_5);
},_sendRestart:function(){
var _8=this.rid++;
var _9={rid:_8,sid:this.sid,to:this.domain,"xmpp:restart":"true","xml:lang":this.lang,xmlns:_3.xmpp.xmpp.BODY_NS,"xmlns:xmpp":"urn:xmpp:xbosh"};
var _a=_3.xmpp.util.createElement("body",_9,true);
this.addToOutboundQueue(_a,_8);
},processScriptSrc:function(_b,_c){
var _d=_3.xml.parser.parse(_b,"text/xml");
if(_d){
this.processDocument(_d,_c);
}else{
}
},_keepAlive:function(){
if(this.state=="wait"||this.isTerminated()){
return;
}
this._dispatchPacket();
this.keepAliveTimer=setTimeout(_2.hitch(this,"_keepAlive"),10000);
},close:function(_e){
var _f=this.rid++;
var req={sid:this.sid,rid:_f,type:"terminate"};
var _10=null;
if(_e){
_10=new _3.string.Builder(_3.xmpp.util.createElement("body",req,false));
_10.append(_e);
_10.append("</body>");
}else{
_10=new _3.string.Builder(_3.xmpp.util.createElement("body",req,false));
}
this.addToOutboundQueue(_10.toString(),_f);
this.state=="Terminate";
},dispatchPacket:function(msg,_11,_12,_13){
if(msg){
this.protocolPacketQueue.push(msg);
}
var def=new _2.Deferred();
if(_11&&_12){
def.protocolMatchType=_11;
def.matchId=_12;
def.matchProperty=_13||"id";
if(def.matchProperty!="id"){
this.matchTypeIdAttribute[_11]=def.matchProperty;
}
}
this.deferredRequests[def.protocolMatchType+"-"+def.matchId]=def;
if(!this.dispatchTimer){
this.dispatchTimer=setTimeout(_2.hitch(this,"_dispatchPacket"),600);
}
return def;
},_dispatchPacket:function(){
clearTimeout(this.dispatchTimer);
delete this.dispatchTimer;
if(!this.sid){
return;
}
if(!this.authId){
return;
}
if(this.transmitState!="error"&&(this.protocolPacketQueue.length==0)&&(this.outboundQueue.length>0)){
return;
}
if(this.state=="wait"||this.isTerminated()){
return;
}
var req={sid:this.sid,xmlns:_3.xmpp.xmpp.BODY_NS};
var _14;
if(this.protocolPacketQueue.length>0){
req.rid=this.rid++;
_14=new _3.string.Builder(_3.xmpp.util.createElement("body",req,false));
_14.append(this.processProtocolPacketQueue());
_14.append("</body>");
delete this.lastPollTime;
}else{
if(this.lastPollTime){
var now=new Date().getTime();
if(now-this.lastPollTime<this.polling){
this.dispatchTimer=setTimeout(_2.hitch(this,"_dispatchPacket"),this.polling-(now-this.lastPollTime)+10);
return;
}
}
req.rid=this.rid++;
this.lastPollTime=new Date().getTime();
_14=new _3.string.Builder(_3.xmpp.util.createElement("body",req,true));
}
this.addToOutboundQueue(_14.toString(),req.rid);
},redispatchPacket:function(rid){
var env=this.outboundRequests[rid];
this.sendXml(env,rid);
},addToOutboundQueue:function(msg,rid){
this.outboundQueue.push({msg:msg,rid:rid});
this.outboundRequests[rid]=msg;
this.sendXml(msg,rid);
},removeFromOutboundQueue:function(rid){
for(var i=0;i<this.outboundQueue.length;i++){
if(rid==this.outboundQueue[i]["rid"]){
this.outboundQueue.splice(i,1);
break;
}
}
delete this.outboundRequests[rid];
},processProtocolPacketQueue:function(){
var _15=new _3.string.Builder();
for(var i=0;i<this.protocolPacketQueue.length;i++){
_15.append(this.protocolPacketQueue[i]);
}
this.protocolPacketQueue=[];
return _15.toString();
},sendXml:function(_16,rid){
if(this.isTerminated()){
return false;
}
this.transmitState="transmitting";
var def=null;
if(this.useScriptSrcTransport){
def=_3.xmpp.bosh.get({rid:rid,url:this.serviceUrl+"?"+encodeURIComponent(_16),error:_2.hitch(this,function(res,io){
this.setState("Terminate","error");
return false;
}),timeout:this.sendTimeout});
}else{
def=_2.rawXhrPost({contentType:"text/xml",url:this.serviceUrl,postData:_16,handleAs:"xml",error:_2.hitch(this,function(res,io){
return this.processError(io.xhr.responseXML,io.xhr.status,rid);
}),timeout:this.sendTimeout});
}
def.addCallback(this,function(res){
return this.processDocument(res,rid);
});
return def;
},processDocument:function(doc,rid){
if(this.isTerminated()||!doc.firstChild){
return false;
}
this.transmitState="idle";
var _17=doc.firstChild;
if(_17.nodeName!="body"){
}
if(this.outboundQueue.length<1){
return false;
}
var _18=this.outboundQueue[0]["rid"];
if(rid==_18){
this.removeFromOutboundQueue(rid);
this.processResponse(_17,rid);
this.processInboundQueue();
}else{
var gap=rid-_18;
if(gap<this.hold+2){
this.addToInboundQueue(doc,rid);
}else{
}
}
return doc;
},processInboundQueue:function(){
while(this.inboundQueue.length>0){
var _19=this.inboundQueue.shift();
this.processDocument(_19["doc"],_19["rid"]);
}
},addToInboundQueue:function(doc,rid){
for(var i=0;i<this.inboundQueue.length;i++){
if(rid<this.inboundQueue[i]["rid"]){
continue;
}
this.inboundQueue.splice(i,0,{doc:doc,rid:rid});
}
},processResponse:function(_1a,rid){
if(_1a.getAttribute("type")=="terminate"){
var _1b=_1a.firstChild.firstChild;
var _1c="";
if(_1b.nodeName=="conflict"){
_1c="conflict";
}
this.setState("Terminate",_1c);
return;
}
if((this.state!="Ready")&&(this.state!="Terminate")){
var sid=_1a.getAttribute("sid");
if(sid){
this.sid=sid;
}else{
throw new Error("No sid returned during xmpp session startup");
}
this.authId=_1a.getAttribute("authid");
if(this.authId==""){
if(this.authRetries--<1){
console.error("Unable to obtain Authorization ID");
this.terminateSession();
}
}
this.wait=_1a.getAttribute("wait");
if(_1a.getAttribute("polling")){
this.polling=parseInt(_1a.getAttribute("polling"))*1000;
}
this.inactivity=_1a.getAttribute("inactivity");
this.setState("Ready");
}
_2.forEach(_1a.childNodes,function(_1d){
this.processProtocolResponse(_1d,rid);
},this);
if(this.transmitState=="idle"){
this.dispatchPacket();
}
},processProtocolResponse:function(msg,rid){
this.onProcessProtocolResponse(msg);
var key=msg.nodeName+"-"+msg.getAttribute("id");
var def=this.deferredRequests[key];
if(def){
def.callback(msg);
delete this.deferredRequests[key];
}
},setState:function(_1e,_1f){
if(this.state!=_1e){
if(this["on"+_1e]){
this["on"+_1e](_1e,this.state,_1f);
}
this.state=_1e;
}
},isTerminated:function(){
return this.state=="Terminate";
},processError:function(err,_20,rid){
if(this.isTerminated()){
return false;
}
if(_20!=200){
if(_20>=400&&_20<500){
this.setState("Terminate",_21);
return false;
}else{
this.removeFromOutboundQueue(rid);
setTimeout(_2.hitch(this,function(){
this.dispatchPacket();
}),200);
return true;
}
return false;
}
if(err&&err.dojoType&&err.dojoType=="timeout"){
}
this.removeFromOutboundQueue(rid);
if(err&&err.firstChild){
if(err.firstChild.getAttribute("type")=="terminate"){
var _22=err.firstChild.firstChild;
var _21="";
if(_22&&_22.nodeName=="conflict"){
_21="conflict";
}
this.setState("Terminate",_21);
return false;
}
}
this.transmitState="error";
setTimeout(_2.hitch(this,function(){
this.dispatchPacket();
}),200);
return true;
},onTerminate:function(_23,_24,_25){
},onProcessProtocolResponse:function(msg){
},onReady:function(_26,_27){
}});
});
