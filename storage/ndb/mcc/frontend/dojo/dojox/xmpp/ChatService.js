//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.xmpp.ChatService");
_3.xmpp.chat={CHAT_STATE_NS:"http://jabber.org/protocol/chatstates",ACTIVE_STATE:"active",COMPOSING_STATE:"composing",INACTIVE_STATE:"inactive",PAUSED_STATE:"paused",GONE_STATE:"gone"};
_2.declare("dojox.xmpp.ChatService",null,{state:"",constructor:function(){
this.state="";
this.chatid=Math.round(Math.random()*1000000000000000);
},recieveMessage:function(_4,_5){
if(_4&&!_5){
this.onNewMessage(_4);
}
},setSession:function(_6){
this.session=_6;
},setState:function(_7){
if(this.state!=_7){
this.state=_7;
}
},invite:function(_8){
if(this.uid){
return;
}
if(!_8||_8==""){
throw new Error("ChatService::invite() contact is NULL");
}
this.uid=_8;
var _9={xmlns:"jabber:client",to:this.uid,from:this.session.jid+"/"+this.session.resource,type:"chat"};
var _a=new _3.string.Builder(_3.xmpp.util.createElement("message",_9,false));
_a.append(_3.xmpp.util.createElement("thread",{},false));
_a.append(this.chatid);
_a.append("</thread>");
_a.append(_3.xmpp.util.createElement("active",{xmlns:_3.xmpp.chat.CHAT_STATE_NS},true));
_a.append("</message>");
this.session.dispatchPacket(_a.toString());
this.onInvite(_8);
this.setState(_3.xmpp.chat.CHAT_STATE_NS);
},sendMessage:function(_b){
if(!this.uid){
return;
}
if((!_b.body||_b.body=="")&&!_b.xhtml){
return;
}
var _c={xmlns:"jabber:client",to:this.uid,from:this.session.jid+"/"+this.session.resource,type:"chat"};
var _d=new _3.string.Builder(_3.xmpp.util.createElement("message",_c,false));
var _e=_3.xmpp.util.createElement("html",{"xmlns":_3.xmpp.xmpp.XHTML_IM_NS},false);
var _f=_3.xmpp.util.createElement("body",{"xml:lang":this.session.lang,"xmlns":_3.xmpp.xmpp.XHTML_BODY_NS},false)+_b.body+"</body>";
var _10=_3.xmpp.util.createElement("body",{},false)+_3.xmpp.util.stripHtml(_b.body)+"</body>";
if(_d.subject&&_d.subject!=""){
_d.append(_3.xmpp.util.createElement("subject",{},false));
_d.append(_d.subject);
_d.append("</subject>");
}
_d.append(_10);
_d.append(_e);
_d.append(_f);
_d.append("</html>");
_d.append(_3.xmpp.util.createElement("thread",{},false));
_d.append(this.chatid);
_d.append("</thread>");
if(this.useChatStates){
_d.append(_3.xmpp.util.createElement("active",{xmlns:_3.xmpp.chat.CHAT_STATE_NS},true));
}
_d.append("</message>");
this.session.dispatchPacket(_d.toString());
},sendChatState:function(_11){
if(!this.useChatState||this.firstMessage){
return;
}
if(_11==this._currentState){
return;
}
var req={xmlns:"jabber:client",to:this.uid,from:this.session.jid+"/"+this.session.resource,type:"chat"};
var _12=new _3.string.Builder(_3.xmpp.util.createElement("message",req,false));
_12.append(_3.xmpp.util.createElement(_11,{xmlns:_3.xmpp.chat.CHAT_STATE_NS},true));
this._currentState=_11;
_12.append("<thread>");
_12.append(this.chatid);
_12.append("</thread></message>");
this.session.dispatchPacket(_12.toString());
},onNewMessage:function(msg){
},onInvite:function(_13){
}});
});
