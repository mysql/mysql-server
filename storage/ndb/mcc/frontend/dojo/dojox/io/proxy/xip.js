//>>built
define("dojox/io/proxy/xip",["dojo/main","dojo/io/iframe","dojox/data/dom","dojo/_base/xhr","dojo/_base/url"],function(_1,_2,_3){
_1.getObject("io.proxy.xip",true,dojox);
dojox.io.proxy.xip={xipClientUrl:((_1.config||djConfig)["xipClientUrl"])||_1.moduleUrl("dojox.io.proxy","xip_client.html").toString(),urlLimit:4000,_callbackName:(dojox._scopeName||"dojox")+".io.proxy.xip.fragmentReceived",_state:{},_stateIdCounter:0,_isWebKit:navigator.userAgent.indexOf("WebKit")!=-1,send:function(_4){
var _5=this.xipClientUrl;
if(_5.split(":")[0].match(/javascript/i)||_4._ifpServerUrl.split(":")[0].match(/javascript/i)){
return null;
}
var _6=_5.indexOf(":");
var _7=_5.indexOf("/");
if(_6==-1||_7<_6){
var _8=window.location.href;
if(_7==0){
_5=_8.substring(0,_8.indexOf("/",9))+_5;
}else{
_5=_8.substring(0,(_8.lastIndexOf("/")+1))+_5;
}
}
this.fullXipClientUrl=_5;
if(typeof document.postMessage!="undefined"){
document.addEventListener("message",_1.hitch(this,this.fragmentReceivedEvent),false);
}
this.send=this._realSend;
return this._realSend(_4);
},_realSend:function(_9){
var _a="XhrIframeProxy"+(this._stateIdCounter++);
_9._stateId=_a;
var _b=_9._ifpServerUrl+"#0:init:id="+_a+"&client="+encodeURIComponent(this.fullXipClientUrl)+"&callback="+encodeURIComponent(this._callbackName);
this._state[_a]={facade:_9,stateId:_a,clientFrame:_2.create(_a,"",_b),isSending:false,serverUrl:_9._ifpServerUrl,requestData:null,responseMessage:"",requestParts:[],idCounter:1,partIndex:0,serverWindow:null};
return _a;
},receive:function(_c,_d){
var _e={};
var _f=_d.split("&");
for(var i=0;i<_f.length;i++){
if(_f[i]){
var _10=_f[i].split("=");
_e[decodeURIComponent(_10[0])]=decodeURIComponent(_10[1]);
}
}
var _11=this._state[_c];
var _12=_11.facade;
_12._setResponseHeaders(_e.responseHeaders);
if(_e.status==0||_e.status){
_12.status=parseInt(_e.status,10);
}
if(_e.statusText){
_12.statusText=_e.statusText;
}
if(_e.responseText){
_12.responseText=_e.responseText;
var _13=_12.getResponseHeader("Content-Type");
if(_13){
var _14=_13.split(";")[0];
if(_14.indexOf("application/xml")==0||_14.indexOf("text/xml")==0){
_12.responseXML=_3.createDocument(_e.responseText,_13);
}
}
}
_12.readyState=4;
this.destroyState(_c);
},frameLoaded:function(_15){
var _16=this._state[_15];
var _17=_16.facade;
var _18=[];
for(var _19 in _17._requestHeaders){
_18.push(_19+": "+_17._requestHeaders[_19]);
}
var _1a={uri:_17._uri};
if(_18.length>0){
_1a.requestHeaders=_18.join("\r\n");
}
if(_17._method){
_1a.method=_17._method;
}
if(_17._bodyData){
_1a.data=_17._bodyData;
}
this.sendRequest(_15,_1.objectToQuery(_1a));
},destroyState:function(_1b){
var _1c=this._state[_1b];
if(_1c){
delete this._state[_1b];
var _1d=_1c.clientFrame.parentNode;
_1d.removeChild(_1c.clientFrame);
_1c.clientFrame=null;
_1c=null;
}
},createFacade:function(){
if(arguments&&arguments[0]&&arguments[0].iframeProxyUrl){
return new dojox.io.proxy.xip.XhrIframeFacade(arguments[0].iframeProxyUrl);
}else{
return dojox.io.proxy.xip._xhrObjOld.apply(_1,arguments);
}
},sendRequest:function(_1e,_1f){
var _20=this._state[_1e];
if(!_20.isSending){
_20.isSending=true;
_20.requestData=_1f||"";
_20.serverWindow=frames[_20.stateId];
if(!_20.serverWindow){
_20.serverWindow=document.getElementById(_20.stateId).contentWindow;
}
if(typeof document.postMessage=="undefined"){
if(_20.serverWindow.contentWindow){
_20.serverWindow=_20.serverWindow.contentWindow;
}
}
this.sendRequestStart(_1e);
}
},sendRequestStart:function(_21){
var _22=this._state[_21];
_22.requestParts=[];
var _23=_22.requestData;
var _24=_22.serverUrl.length;
var _25=this.urlLimit-_24;
var _26=0;
while((_23.length-_26)+_24>this.urlLimit){
var _27=_23.substring(_26,_26+_25);
var _28=_27.lastIndexOf("%");
if(_28==_27.length-1||_28==_27.length-2){
_27=_27.substring(0,_28);
}
_22.requestParts.push(_27);
_26+=_27.length;
}
_22.requestParts.push(_23.substring(_26,_23.length));
_22.partIndex=0;
this.sendRequestPart(_21);
},sendRequestPart:function(_29){
var _2a=this._state[_29];
if(_2a.partIndex<_2a.requestParts.length){
var _2b=_2a.requestParts[_2a.partIndex];
var cmd="part";
if(_2a.partIndex+1==_2a.requestParts.length){
cmd="end";
}else{
if(_2a.partIndex==0){
cmd="start";
}
}
this.setServerUrl(_29,cmd,_2b);
_2a.partIndex++;
}
},setServerUrl:function(_2c,cmd,_2d){
var _2e=this.makeServerUrl(_2c,cmd,_2d);
var _2f=this._state[_2c];
if(this._isWebKit){
_2f.serverWindow.location=_2e;
}else{
_2f.serverWindow.location.replace(_2e);
}
},makeServerUrl:function(_30,cmd,_31){
var _32=this._state[_30];
var _33=_32.serverUrl+"#"+(_32.idCounter++)+":"+cmd;
if(_31){
_33+=":"+_31;
}
return _33;
},fragmentReceivedEvent:function(evt){
if(evt.uri.split("#")[0]==this.fullXipClientUrl){
this.fragmentReceived(evt.data);
}
},fragmentReceived:function(_34){
var _35=_34.indexOf("#");
var _36=_34.substring(0,_35);
var _37=_34.substring(_35+1,_34.length);
var msg=this.unpackMessage(_37);
var _38=this._state[_36];
switch(msg.command){
case "loaded":
this.frameLoaded(_36);
break;
case "ok":
this.sendRequestPart(_36);
break;
case "start":
_38.responseMessage=""+msg.message;
this.setServerUrl(_36,"ok");
break;
case "part":
_38.responseMessage+=msg.message;
this.setServerUrl(_36,"ok");
break;
case "end":
this.setServerUrl(_36,"ok");
_38.responseMessage+=msg.message;
this.receive(_36,_38.responseMessage);
break;
}
},unpackMessage:function(_39){
var _3a=_39.split(":");
var _3b=_3a[1];
_39=_3a[2]||"";
var _3c=null;
if(_3b=="init"){
var _3d=_39.split("&");
_3c={};
for(var i=0;i<_3d.length;i++){
var _3e=_3d[i].split("=");
_3c[decodeURIComponent(_3e[0])]=decodeURIComponent(_3e[1]);
}
}
return {command:_3b,message:_39,config:_3c};
}};
dojox.io.proxy.xip._xhrObjOld=_1._xhrObj;
_1._xhrObj=dojox.io.proxy.xip.createFacade;
dojox.io.proxy.xip.XhrIframeFacade=function(_3f){
this._requestHeaders={};
this._allResponseHeaders=null;
this._responseHeaders={};
this._method=null;
this._uri=null;
this._bodyData=null;
this.responseText=null;
this.responseXML=null;
this.status=null;
this.statusText=null;
this.readyState=0;
this._ifpServerUrl=_3f;
this._stateId=null;
};
_1.extend(dojox.io.proxy.xip.XhrIframeFacade,{open:function(_40,uri){
this._method=_40;
this._uri=uri;
this.readyState=1;
},setRequestHeader:function(_41,_42){
this._requestHeaders[_41]=_42;
},send:function(_43){
this._bodyData=_43;
this._stateId=dojox.io.proxy.xip.send(this);
this.readyState=2;
},abort:function(){
dojox.io.proxy.xip.destroyState(this._stateId);
},getAllResponseHeaders:function(){
return this._allResponseHeaders;
},getResponseHeader:function(_44){
return this._responseHeaders[_44];
},_setResponseHeaders:function(_45){
if(_45){
this._allResponseHeaders=_45;
_45=_45.replace(/\r/g,"");
var _46=_45.split("\n");
for(var i=0;i<_46.length;i++){
if(_46[i]){
var _47=_46[i].split(": ");
this._responseHeaders[_47[0]]=_47[1];
}
}
}
}});
return dojox.io.proxy.xip;
});
