//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/xmpp/TransportSession,dojox/xmpp/RosterService,dojox/xmpp/PresenceService,dojox/xmpp/UserService,dojox/xmpp/ChatService,dojox/xmpp/sasl"],function(_1,_2,_3){
_2.provide("dojox.xmpp.xmppSession");
_2.require("dojox.xmpp.TransportSession");
_2.require("dojox.xmpp.RosterService");
_2.require("dojox.xmpp.PresenceService");
_2.require("dojox.xmpp.UserService");
_2.require("dojox.xmpp.ChatService");
_2.require("dojox.xmpp.sasl");
_3.xmpp.xmpp={STREAM_NS:"http://etherx.jabber.org/streams",CLIENT_NS:"jabber:client",STANZA_NS:"urn:ietf:params:xml:ns:xmpp-stanzas",SASL_NS:"urn:ietf:params:xml:ns:xmpp-sasl",BIND_NS:"urn:ietf:params:xml:ns:xmpp-bind",SESSION_NS:"urn:ietf:params:xml:ns:xmpp-session",BODY_NS:"http://jabber.org/protocol/httpbind",XHTML_BODY_NS:"http://www.w3.org/1999/xhtml",XHTML_IM_NS:"http://jabber.org/protocol/xhtml-im",INACTIVE:"Inactive",CONNECTED:"Connected",ACTIVE:"Active",TERMINATE:"Terminate",LOGIN_FAILURE:"LoginFailure",INVALID_ID:-1,NO_ID:0,error:{BAD_REQUEST:"bad-request",CONFLICT:"conflict",FEATURE_NOT_IMPLEMENTED:"feature-not-implemented",FORBIDDEN:"forbidden",GONE:"gone",INTERNAL_SERVER_ERROR:"internal-server-error",ITEM_NOT_FOUND:"item-not-found",ID_MALFORMED:"jid-malformed",NOT_ACCEPTABLE:"not-acceptable",NOT_ALLOWED:"not-allowed",NOT_AUTHORIZED:"not-authorized",SERVICE_UNAVAILABLE:"service-unavailable",SUBSCRIPTION_REQUIRED:"subscription-required",UNEXPECTED_REQUEST:"unexpected-request"}};
_3.xmpp.xmppSession=function(_4){
this.roster=[];
this.chatRegister=[];
this._iqId=Math.round(Math.random()*1000000000);
if(_4&&_2.isObject(_4)){
_2.mixin(this,_4);
}
this.session=new _3.xmpp.TransportSession(_4);
_2.connect(this.session,"onReady",this,"onTransportReady");
_2.connect(this.session,"onTerminate",this,"onTransportTerminate");
_2.connect(this.session,"onProcessProtocolResponse",this,"processProtocolResponse");
};
_2.extend(_3.xmpp.xmppSession,{roster:[],chatRegister:[],_iqId:0,open:function(_5,_6,_7){
if(!_5){
throw new Error("User id cannot be null");
}else{
this.jid=_5;
if(_5.indexOf("@")==-1){
this.jid=this.jid+"@"+this.domain;
}
}
if(_6){
this.password=_6;
}
if(_7){
this.resource=_7;
}
this.session.open();
},close:function(){
this.state=_3.xmpp.xmpp.TERMINATE;
this.session.close(_3.xmpp.util.createElement("presence",{type:"unavailable",xmlns:_3.xmpp.xmpp.CLIENT_NS},true));
},processProtocolResponse:function(_8){
var _9=_8.nodeName;
var _a=_9.indexOf(":");
if(_a>0){
_9=_9.substring(_a+1);
}
switch(_9){
case "iq":
case "presence":
case "message":
case "features":
this[_9+"Handler"](_8);
break;
default:
if(_8.getAttribute("xmlns")==_3.xmpp.xmpp.SASL_NS){
this.saslHandler(_8);
}
}
},messageHandler:function(_b){
switch(_b.getAttribute("type")){
case "chat":
this.chatHandler(_b);
break;
case "normal":
default:
this.simpleMessageHandler(_b);
}
},iqHandler:function(_c){
if(_c.getAttribute("type")=="set"){
this.iqSetHandler(_c);
return;
}else{
if(_c.getAttribute("type")=="get"){
return;
}
}
},presenceHandler:function(_d){
switch(_d.getAttribute("type")){
case "subscribe":
this.presenceSubscriptionRequest(_d.getAttribute("from"));
break;
case "subscribed":
case "unsubscribed":
break;
case "error":
this.processXmppError(_d);
break;
default:
this.presenceUpdate(_d);
break;
}
},featuresHandler:function(_e){
var _f=[];
var _10=false;
var _11=false;
if(_e.hasChildNodes()){
for(var i=0;i<_e.childNodes.length;i++){
var n=_e.childNodes[i];
switch(n.nodeName){
case "mechanisms":
for(var x=0;x<n.childNodes.length;x++){
_f.push(n.childNodes[x].firstChild.nodeValue);
}
break;
case "bind":
_10=true;
break;
case "session":
_11=true;
}
}
}
if(this.state==_3.xmpp.xmpp.CONNECTED){
if(!this.auth){
for(var i=0;i<_f.length;i++){
try{
this.auth=_3.xmpp.sasl.registry.match(_f[i],this);
break;
}
catch(e){
console.warn("No suitable auth mechanism found for: ",_f[i]);
}
}
}else{
if(_10){
this.bindResource(_11);
}
}
}
},saslHandler:function(msg){
if(msg.nodeName=="success"){
this.auth.onSuccess();
return;
}
if(msg.nodeName=="challenge"){
this.auth.onChallenge(msg);
return;
}
if(msg.hasChildNodes()){
this.onLoginFailure(msg.firstChild.nodeName);
this.session.setState("Terminate",msg.firstChild.nodeName);
}
},sendRestart:function(){
this.session._sendRestart();
},chatHandler:function(msg){
var _12={from:msg.getAttribute("from"),to:msg.getAttribute("to")};
var _13=null;
for(var i=0;i<msg.childNodes.length;i++){
var n=msg.childNodes[i];
if(n.hasChildNodes()){
switch(n.nodeName){
case "thread":
_12.chatid=n.firstChild.nodeValue;
break;
case "body":
if(!n.getAttribute("xmlns")||(n.getAttribute("xmlns")=="")){
_12.body=n.firstChild.nodeValue;
}
break;
case "subject":
_12.subject=n.firstChild.nodeValue;
case "html":
if(n.getAttribute("xmlns")==_3.xmpp.xmpp.XHTML_IM_NS){
_12.xhtml=n.getElementsByTagName("body")[0];
}
break;
case "x":
break;
default:
}
}
}
var _14=-1;
if(_12.chatid){
for(var i=0;i<this.chatRegister.length;i++){
var ci=this.chatRegister[i];
if(ci&&ci.chatid==_12.chatid){
_14=i;
break;
}
}
}else{
for(var i=0;i<this.chatRegister.length;i++){
var ci=this.chatRegister[i];
if(ci){
if(ci.uid==this.getBareJid(_12.from)){
_14=i;
}
}
}
}
if(_14>-1&&_13){
var _15=this.chatRegister[_14];
_15.setState(_13);
if(_15.firstMessage){
if(_13==_3.xmpp.chat.ACTIVE_STATE){
_15.useChatState=(_13!=null)?true:false;
_15.firstMessage=false;
}
}
}
if((!_12.body||_12.body=="")&&!_12.xhtml){
return;
}
if(_14>-1){
var _15=this.chatRegister[_14];
_15.recieveMessage(_12);
}else{
var _16=new _3.xmpp.ChatService();
_16.uid=this.getBareJid(_12.from);
_16.chatid=_12.chatid;
_16.firstMessage=true;
if(!_13||_13!=_3.xmpp.chat.ACTIVE_STATE){
this.useChatState=false;
}
this.registerChatInstance(_16,_12);
}
},simpleMessageHandler:function(msg){
},registerChatInstance:function(_17,_18){
_17.setSession(this);
this.chatRegister.push(_17);
this.onRegisterChatInstance(_17,_18);
_17.recieveMessage(_18,true);
},iqSetHandler:function(msg){
if(msg.hasChildNodes()){
var fn=msg.firstChild;
switch(fn.nodeName){
case "query":
if(fn.getAttribute("xmlns")=="jabber:iq:roster"){
this.rosterSetHandler(fn);
this.sendIqResult(msg.getAttribute("id"),msg.getAttribute("from"));
}
break;
default:
break;
}
}
},sendIqResult:function(_19,to){
var req={id:_19,to:to||this.domain,type:"result",from:this.jid+"/"+this.resource};
this.dispatchPacket(_3.xmpp.util.createElement("iq",req,true));
},rosterSetHandler:function(_1a){
for(var i=0;i<_1a.childNodes.length;i++){
var n=_1a.childNodes[i];
if(n.nodeName=="item"){
var _1b=false;
var _1c=-1;
var _1d=null;
var _1e=null;
for(var x=0;x<this.roster.length;x++){
var r=this.roster[x];
if(n.getAttribute("jid")==r.jid){
_1b=true;
if(n.getAttribute("subscription")=="remove"){
_1d={id:r.jid,name:r.name,groups:[]};
for(var y=0;y<r.groups.length;y++){
_1d.groups.push(r.groups[y]);
}
this.roster.splice(x,1);
_1c=_3.xmpp.roster.REMOVED;
}else{
_1e=_2.clone(r);
var _1f=n.getAttribute("name");
if(_1f){
this.roster[x].name=_1f;
}
r.groups=[];
if(n.getAttribute("subscription")){
r.status=n.getAttribute("subscription");
}
r.substatus=_3.xmpp.presence.SUBSCRIPTION_SUBSTATUS_NONE;
if(n.getAttribute("ask")=="subscribe"){
r.substatus=_3.xmpp.presence.SUBSCRIPTION_REQUEST_PENDING;
}
for(var y=0;y<n.childNodes.length;y++){
var _20=n.childNodes[y];
if((_20.nodeName=="group")&&(_20.hasChildNodes())){
var _21=_20.firstChild.nodeValue;
r.groups.push(_21);
}
}
_1d=r;
_1c=_3.xmpp.roster.CHANGED;
}
break;
}
}
if(!_1b&&(n.getAttribute("subscription")!="remove")){
r=this.createRosterEntry(n);
_1d=r;
_1c=_3.xmpp.roster.ADDED;
}
switch(_1c){
case _3.xmpp.roster.ADDED:
this.onRosterAdded(_1d);
break;
case _3.xmpp.roster.REMOVED:
this.onRosterRemoved(_1d);
break;
case _3.xmpp.roster.CHANGED:
this.onRosterChanged(_1d,_1e);
break;
}
}
}
},presenceUpdate:function(msg){
if(msg.getAttribute("to")){
var jid=this.getBareJid(msg.getAttribute("to"));
if(jid!=this.jid){
return;
}
}
var _22=this.getResourceFromJid(msg.getAttribute("from"));
var p={from:this.getBareJid(msg.getAttribute("from")),resource:_22,show:_3.xmpp.presence.STATUS_ONLINE,priority:5,hasAvatar:false};
if(msg.getAttribute("type")=="unavailable"){
p.show=_3.xmpp.presence.STATUS_OFFLINE;
}
for(var i=0;i<msg.childNodes.length;i++){
var n=msg.childNodes[i];
if(n.hasChildNodes()){
switch(n.nodeName){
case "status":
case "show":
p[n.nodeName]=n.firstChild.nodeValue;
break;
case "status":
p.priority=parseInt(n.firstChild.nodeValue);
break;
case "x":
if(n.firstChild&&n.firstChild.firstChild&&n.firstChild.firstChild.nodeValue!=""){
p.avatarHash=n.firstChild.firstChild.nodeValue;
p.hasAvatar=true;
}
break;
}
}
}
this.onPresenceUpdate(p);
},retrieveRoster:function(){
var _23={id:this.getNextIqId(),from:this.jid+"/"+this.resource,type:"get"};
var req=new _3.string.Builder(_3.xmpp.util.createElement("iq",_23,false));
req.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:roster"},true));
req.append("</iq>");
var def=this.dispatchPacket(req,"iq",_23.id);
def.addCallback(this,"onRetrieveRoster");
},getRosterIndex:function(jid){
if(jid.indexOf("@")==-1){
jid+="@"+this.domain;
}
for(var i=0;i<this.roster.length;i++){
if(jid==this.roster[i].jid){
return i;
}
}
return -1;
},createRosterEntry:function(_24){
var re={name:_24.getAttribute("name"),jid:_24.getAttribute("jid"),groups:[],status:_3.xmpp.presence.SUBSCRIPTION_NONE,substatus:_3.xmpp.presence.SUBSCRIPTION_SUBSTATUS_NONE};
if(!re.name){
re.name=re.id;
}
for(var i=0;i<_24.childNodes.length;i++){
var n=_24.childNodes[i];
if(n.nodeName=="group"&&n.hasChildNodes()){
re.groups.push(n.firstChild.nodeValue);
}
}
if(_24.getAttribute("subscription")){
re.status=_24.getAttribute("subscription");
}
if(_24.getAttribute("ask")=="subscribe"){
re.substatus=_3.xmpp.presence.SUBSCRIPTION_REQUEST_PENDING;
}
return re;
},bindResource:function(_25){
var _26={id:this.getNextIqId(),type:"set"};
var _27=new _3.string.Builder(_3.xmpp.util.createElement("iq",_26,false));
_27.append(_3.xmpp.util.createElement("bind",{xmlns:_3.xmpp.xmpp.BIND_NS},false));
if(this.resource){
_27.append(_3.xmpp.util.createElement("resource"));
_27.append(this.resource);
_27.append("</resource>");
}
_27.append("</bind></iq>");
var def=this.dispatchPacket(_27,"iq",_26.id);
def.addCallback(this,function(msg){
this.onBindResource(msg,_25);
return msg;
});
},getNextIqId:function(){
return "im_"+this._iqId++;
},presenceSubscriptionRequest:function(msg){
this.onSubscriptionRequest(msg);
},dispatchPacket:function(msg,_28,_29){
if(this.state!="Terminate"){
return this.session.dispatchPacket(msg,_28,_29);
}else{
}
},setState:function(_2a,_2b){
if(this.state!=_2a){
if(this["on"+_2a]){
this["on"+_2a](_2a,this.state,_2b);
}
this.state=_2a;
}
},search:function(_2c,_2d,_2e){
var req={id:this.getNextIqId(),"xml:lang":this.lang,type:"set",from:this.jid+"/"+this.resource,to:_2d};
var _2f=new _3.string.Builder(_3.xmpp.util.createElement("iq",req,false));
_2f.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:search"},false));
_2f.append(_3.xmpp.util.createElement(_2e,{},false));
_2f.append(_2c);
_2f.append("</").append(_2e).append(">");
_2f.append("</query></iq>");
var def=this.dispatchPacket(_2f.toString,"iq",req.id);
def.addCallback(this,"_onSearchResults");
},_onSearchResults:function(msg){
if((msg.getAttribute("type")=="result")&&(msg.hasChildNodes())){
this.onSearchResults([]);
}
},onLogin:function(){
this.retrieveRoster();
},onLoginFailure:function(msg){
},onBindResource:function(msg,_30){
if(msg.getAttribute("type")=="result"){
if((msg.hasChildNodes())&&(msg.firstChild.nodeName=="bind")){
var _31=msg.firstChild;
if((_31.hasChildNodes())&&(_31.firstChild.nodeName=="jid")){
if(_31.firstChild.hasChildNodes()){
var _32=_31.firstChild.firstChild.nodeValue;
this.jid=this.getBareJid(_32);
this.resource=this.getResourceFromJid(_32);
}
}
if(_30){
var _33={id:this.getNextIqId(),type:"set"};
var _34=new _3.string.Builder(_3.xmpp.util.createElement("iq",_33,false));
_34.append(_3.xmpp.util.createElement("session",{xmlns:_3.xmpp.xmpp.SESSION_NS},true));
_34.append("</iq>");
var def=this.dispatchPacket(_34,"iq",_33.id);
def.addCallback(this,"onBindSession");
return;
}
}else{
}
this.onLogin();
}else{
if(msg.getAttribute("type")=="error"){
var err=this.processXmppError(msg);
this.onLoginFailure(err);
}
}
},onBindSession:function(msg){
if(msg.getAttribute("type")=="error"){
var err=this.processXmppError(msg);
this.onLoginFailure(err);
}else{
this.onLogin();
}
},onSearchResults:function(_35){
},onRetrieveRoster:function(msg){
if((msg.getAttribute("type")=="result")&&msg.hasChildNodes()){
var _36=msg.getElementsByTagName("query")[0];
if(_36.getAttribute("xmlns")=="jabber:iq:roster"){
for(var i=0;i<_36.childNodes.length;i++){
if(_36.childNodes[i].nodeName=="item"){
this.roster[i]=this.createRosterEntry(_36.childNodes[i]);
}
}
}
}else{
if(msg.getAttribute("type")=="error"){
}
}
this.setState(_3.xmpp.xmpp.ACTIVE);
this.onRosterUpdated();
return msg;
},onRosterUpdated:function(){
},onSubscriptionRequest:function(req){
},onPresenceUpdate:function(p){
},onTransportReady:function(){
this.setState(_3.xmpp.xmpp.CONNECTED);
this.rosterService=new _3.xmpp.RosterService(this);
this.presenceService=new _3.xmpp.PresenceService(this);
this.userService=new _3.xmpp.UserService(this);
},onTransportTerminate:function(_37,_38,_39){
this.setState(_3.xmpp.xmpp.TERMINATE,_39);
},onConnected:function(){
},onTerminate:function(_3a,_3b,_3c){
},onActive:function(){
},onRegisterChatInstance:function(_3d,_3e){
},onRosterAdded:function(ri){
},onRosterRemoved:function(ri){
},onRosterChanged:function(ri,_3f){
},processXmppError:function(msg){
var err={stanzaType:msg.nodeName,id:msg.getAttribute("id")};
for(var i=0;i<msg.childNodes.length;i++){
var n=msg.childNodes[i];
switch(n.nodeName){
case "error":
err.errorType=n.getAttribute("type");
for(var x=0;x<n.childNodes.length;x++){
var cn=n.childNodes[x];
if((cn.nodeName=="text")&&(cn.getAttribute("xmlns")==_3.xmpp.xmpp.STANZA_NS)&&cn.hasChildNodes()){
err.message=cn.firstChild.nodeValue;
}else{
if((cn.getAttribute("xmlns")==_3.xmpp.xmpp.STANZA_NS)&&(!cn.hasChildNodes())){
err.condition=cn.nodeName;
}
}
}
break;
default:
break;
}
}
return err;
},sendStanzaError:function(_40,to,id,_41,_42,_43){
var req={type:"error"};
if(to){
req.to=to;
}
if(id){
req.id=id;
}
var _44=new _3.string.Builder(_3.xmpp.util.createElement(_40,req,false));
_44.append(_3.xmpp.util.createElement("error",{type:_41},false));
_44.append(_3.xmpp.util.createElement("condition",{xmlns:_3.xmpp.xmpp.STANZA_NS},true));
if(_43){
var _45={xmlns:_3.xmpp.xmpp.STANZA_NS,"xml:lang":this.lang};
_44.append(_3.xmpp.util.createElement("text",_45,false));
_44.append(_43).append("</text>");
}
_44.append("</error></").append(_40).append(">");
this.dispatchPacket(_44.toString());
},getBareJid:function(jid){
var i=jid.indexOf("/");
if(i!=-1){
return jid.substring(0,i);
}
return jid;
},getResourceFromJid:function(jid){
var i=jid.indexOf("/");
if(i!=-1){
return jid.substring((i+1),jid.length);
}
return "";
}});
});
