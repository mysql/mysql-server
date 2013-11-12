//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.xmpp.PresenceService");
_3.xmpp.presence={UPDATE:201,SUBSCRIPTION_REQUEST:202,SUBSCRIPTION_SUBSTATUS_NONE:204,SUBSCRIPTION_NONE:"none",SUBSCRIPTION_FROM:"from",SUBSCRIPTION_TO:"to",SUBSCRIPTION_BOTH:"both",SUBSCRIPTION_REQUEST_PENDING:"pending",STATUS_ONLINE:"online",STATUS_AWAY:"away",STATUS_CHAT:"chat",STATUS_DND:"dnd",STATUS_EXTENDED_AWAY:"xa",STATUS_OFFLINE:"offline",STATUS_INVISIBLE:"invisible"};
_2.declare("dojox.xmpp.PresenceService",null,{constructor:function(_4){
this.session=_4;
this.isInvisible=false;
this.avatarHash=null;
this.presence=null;
this.restrictedContactjids={};
},publish:function(_5){
this.presence=_5;
this._setPresence();
},sendAvatarHash:function(_6){
this.avatarHash=_6;
this._setPresence();
},_setPresence:function(){
var _7=this.presence;
var p={xmlns:"jabber:client"};
if(_7&&_7.to){
p.to=_7.to;
}
if(_7.show&&_7.show==_3.xmpp.presence.STATUS_OFFLINE){
p.type="unavailable";
}
if(_7.show&&_7.show==_3.xmpp.presence.STATUS_INVISIBLE){
this._setInvisible();
this.isInvisible=true;
return;
}
if(this.isInvisible){
this._setVisible();
}
var _8=new _3.string.Builder(_3.xmpp.util.createElement("presence",p,false));
if(_7.show&&_7.show!=_3.xmpp.presence.STATUS_OFFLINE){
_8.append(_3.xmpp.util.createElement("show",{},false));
_8.append(_7.show);
_8.append("</show>");
}
if(_7.status){
_8.append(_3.xmpp.util.createElement("status",{},false));
_8.append(_7.status);
_8.append("</status>");
}
if(this.avatarHash){
_8.append(_3.xmpp.util.createElement("x",{xmlns:"vcard-temp:x:update"},false));
_8.append(_3.xmpp.util.createElement("photo",{},false));
_8.append(this.avatarHash);
_8.append("</photo>");
_8.append("</x>");
}
if(_7.priority&&_7.show!=_3.xmpp.presence.STATUS_OFFLINE){
if(_7.priority>127||_7.priority<-128){
_7.priority=5;
}
_8.append(_3.xmpp.util.createElement("priority",{},false));
_8.append(_7.priority);
_8.append("</priority>");
}
_8.append("</presence>");
this.session.dispatchPacket(_8.toString());
},toggleBlockContact:function(_9){
if(!this.restrictedContactjids[_9]){
this.restrictedContactjids[_9]=this._createRestrictedJid();
}
this.restrictedContactjids[_9].blocked=!this.restrictedContactjids[_9].blocked;
this._updateRestricted();
return this.restrictedContactjids;
},toggleContactInvisiblity:function(_a){
if(!this.restrictedContactjids[_a]){
this.restrictedContactjids[_a]=this._createRestrictedJid();
}
this.restrictedContactjids[_a].invisible=!this.restrictedContactjids[_a].invisible;
this._updateRestricted();
return this.restrictedContactjids;
},_createRestrictedJid:function(){
return {invisible:false,blocked:false};
},_updateRestricted:function(){
var _b={id:this.session.getNextIqId(),from:this.session.jid+"/"+this.session.resource,type:"set"};
var _c=new _3.string.Builder(_3.xmpp.util.createElement("iq",_b,false));
_c.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:privacy"},false));
_c.append(_3.xmpp.util.createElement("list",{name:"iwcRestrictedContacts"},false));
var _d=1;
for(var _e in this.restrictedContactjids){
var _f=this.restrictedContactjids[_e];
if(_f.blocked||_f.invisible){
_c.append(_3.xmpp.util.createElement("item",{value:_3.xmpp.util.encodeJid(_e),action:"deny",order:_d++},false));
if(_f.blocked){
_c.append(_3.xmpp.util.createElement("message",{},true));
}
if(_f.invisible){
_c.append(_3.xmpp.util.createElement("presence-out",{},true));
}
_c.append("</item>");
}else{
delete this.restrictedContactjids[_e];
}
}
_c.append("</list>");
_c.append("</query>");
_c.append("</iq>");
var _10=new _3.string.Builder(_3.xmpp.util.createElement("iq",_b,false));
_10.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:privacy"},false));
_10.append(_3.xmpp.util.createElement("active",{name:"iwcRestrictedContacts"},true));
_10.append("</query>");
_10.append("</iq>");
this.session.dispatchPacket(_c.toString());
this.session.dispatchPacket(_10.toString());
},_setVisible:function(){
var _11={id:this.session.getNextIqId(),from:this.session.jid+"/"+this.session.resource,type:"set"};
var req=new _3.string.Builder(_3.xmpp.util.createElement("iq",_11,false));
req.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:privacy"},false));
req.append(_3.xmpp.util.createElement("active",{},true));
req.append("</query>");
req.append("</iq>");
this.session.dispatchPacket(req.toString());
},_setInvisible:function(){
var _12={id:this.session.getNextIqId(),from:this.session.jid+"/"+this.session.resource,type:"set"};
var req=new _3.string.Builder(_3.xmpp.util.createElement("iq",_12,false));
req.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:privacy"},false));
req.append(_3.xmpp.util.createElement("list",{name:"invisible"},false));
req.append(_3.xmpp.util.createElement("item",{action:"deny",order:"1"},false));
req.append(_3.xmpp.util.createElement("presence-out",{},true));
req.append("</item>");
req.append("</list>");
req.append("</query>");
req.append("</iq>");
_12={id:this.session.getNextIqId(),from:this.session.jid+"/"+this.session.resource,type:"set"};
var _13=new _3.string.Builder(_3.xmpp.util.createElement("iq",_12,false));
_13.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:privacy"},false));
_13.append(_3.xmpp.util.createElement("active",{name:"invisible"},true));
_13.append("</query>");
_13.append("</iq>");
this.session.dispatchPacket(req.toString());
this.session.dispatchPacket(_13.toString());
},_manageSubscriptions:function(_14,_15){
if(!_14){
return;
}
if(_14.indexOf("@")==-1){
_14+="@"+this.session.domain;
}
var req=_3.xmpp.util.createElement("presence",{to:_14,type:_15},true);
this.session.dispatchPacket(req);
},subscribe:function(_16){
this._manageSubscriptions(_16,"subscribe");
},approveSubscription:function(_17){
this._manageSubscriptions(_17,"subscribed");
},unsubscribe:function(_18){
this._manageSubscriptions(_18,"unsubscribe");
},declineSubscription:function(_19){
this._manageSubscriptions(_19,"unsubscribed");
},cancelSubscription:function(_1a){
this._manageSubscriptions(_1a,"unsubscribed");
}});
});
