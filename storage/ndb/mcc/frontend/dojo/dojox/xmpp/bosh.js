//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/io/script,dojo/io/iframe,dojox/xml/parser"],function(_1,_2,_3){
_2.provide("dojox.xmpp.bosh");
_2.require("dojo.io.script");
_2.require("dojo.io.iframe");
_2.require("dojox.xml.parser");
_3.xmpp.bosh={transportIframes:[],initialize:function(_4){
this.transportIframes=[];
var _5=_3._scopeName+".xmpp.bosh";
var c=_2.connect(_2.getObject(_5),"_iframeOnload",this,function(_6){
if(_6==0){
_4.load();
_2.disconnect(c);
}
});
for(var i=0;i<_4.iframes;i++){
var _7="xmpp-transport-"+i;
var _8=_2.byId("xmpp-transport-"+i);
if(_8){
if(window[_7]){
window[_7]=null;
}
if(window.frames[_7]){
window.frames[_7]=null;
}
_2.destroy(_8);
}
_8=_2.io.iframe.create("xmpp-transport-"+i,_5+"._iframeOnload("+i+");");
this.transportIframes.push(_8);
}
},_iframeOnload:function(_9){
var _a=_2.io.iframe.doc(_2.byId("xmpp-transport-"+_9));
_a.write("<script>var isLoaded=true; var rid=0; var transmiting=false; function _BOSH_(msg) { transmiting=false; parent.dojox.xmpp.bosh.handle(msg, rid); } </script>");
},findOpenIframe:function(){
for(var i=0;i<this.transportIframes.length;i++){
var _b=this.transportIframes[i];
var _c=_b.contentWindow;
if(_c.isLoaded&&!_c.transmiting){
return _b;
}
}
return false;
},handle:function(_d,_e){
var _f=this["rid"+_e];
var _10=_3.xml.parser.parse(_d,"text/xml");
if(_10){
_f.ioArgs.xmppMessage=_10;
}else{
_f.errback(new Error("Recieved bad document from server: "+_d));
}
},get:function(_11){
var _12=this.findOpenIframe();
var _13=_2.io.iframe.doc(_12);
_11.frameDoc=_13;
var dfd=this._makeScriptDeferred(_11);
var _14=dfd.ioArgs;
_12.contentWindow.rid=_14.rid;
_12.contentWindow.transmiting=true;
_2._ioAddQueryToUrl(_14);
_2._ioNotifyStart(dfd);
_2.io.script.attach(_14.id,_14.url,_13);
_2._ioWatch(dfd,this._validCheck,this._ioCheck,this._resHandle);
return dfd;
},remove:function(id,_15){
_2.destroy(_2.byId(id,_15));
if(this[id]){
delete this[id];
}
},_makeScriptDeferred:function(_16){
var dfd=_2._ioSetArgs(_16,this._deferredCancel,this._deferredOk,this._deferredError);
var _17=dfd.ioArgs;
_17.id="rid"+_16.rid;
_17.rid=_16.rid;
_17.canDelete=true;
_17.frameDoc=_16.frameDoc;
this[_17.id]=dfd;
return dfd;
},_deferredCancel:function(dfd){
dfd.canceled=true;
if(dfd.ioArgs.canDelete){
_3.xmpp.bosh._addDeadScript(dfd.ioArgs);
}
},_deferredOk:function(dfd){
var _18=dfd.ioArgs;
if(_18.canDelete){
_3.xmpp.bosh._addDeadScript(_18);
}
return _18.xmppMessage||_18;
},_deferredError:function(_19,dfd){
if(dfd.ioArgs.canDelete){
if(_19.dojoType=="timeout"){
_3.xmpp.bosh.remove(dfd.ioArgs.id,dfd.ioArgs.frameDoc);
}else{
_3.xmpp.bosh._addDeadScript(dfd.ioArgs);
}
}
return _19;
},_deadScripts:[],_addDeadScript:function(_1a){
_3.xmpp.bosh._deadScripts.push({id:_1a.id,frameDoc:_1a.frameDoc});
_1a.frameDoc=null;
},_validCheck:function(dfd){
var _1b=_3.xmpp.bosh;
var _1c=_1b._deadScripts;
if(_1c&&_1c.length>0){
for(var i=0;i<_1c.length;i++){
_1b.remove(_1c[i].id,_1c[i].frameDoc);
_1c[i].frameDoc=null;
}
_3.xmpp.bosh._deadScripts=[];
}
return true;
},_ioCheck:function(dfd){
var _1d=dfd.ioArgs;
if(_1d.xmppMessage){
return true;
}
return false;
},_resHandle:function(dfd){
if(_3.xmpp.bosh._ioCheck(dfd)){
dfd.callback(dfd);
}else{
dfd.errback(new Error("inconceivable dojox.xmpp.bosh._resHandle error"));
}
}};
});
