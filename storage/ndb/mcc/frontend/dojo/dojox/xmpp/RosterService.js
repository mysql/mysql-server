//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.xmpp.RosterService");
_3.xmpp.roster={ADDED:101,CHANGED:102,REMOVED:103};
_2.declare("dojox.xmpp.RosterService",null,{constructor:function(_4){
this.session=_4;
},addRosterItem:function(_5,_6,_7){
if(!_5){
throw new Error("Roster::addRosterItem() - User ID is null");
}
var _8=this.session.getNextIqId();
var _9={id:_8,from:this.session.jid+"/"+this.session.resource,type:"set"};
var _a=new _3.string.Builder(_3.xmpp.util.createElement("iq",_9,false));
_a.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:roster"},false));
_5=_3.xmpp.util.encodeJid(_5);
if(_5.indexOf("@")==-1){
_5=_5+"@"+this.session.domain;
}
_a.append(_3.xmpp.util.createElement("item",{jid:_5,name:_3.xmpp.util.xmlEncode(_6)},false));
if(_7){
for(var i=0;i<_7.length;i++){
_a.append("<group>");
_a.append(_7[i]);
_a.append("</group>");
}
}
_a.append("</item></query></iq>");
var _b=this.session.dispatchPacket(_a.toString(),"iq",_9.id);
_b.addCallback(this,"verifyRoster");
return _b;
},updateRosterItem:function(_c,_d,_e){
if(_c.indexOf("@")==-1){
_c+=_c+"@"+this.session.domain;
}
var _f={id:this.session.getNextIqId(),from:this.session.jid+"/"+this.session.resource,type:"set"};
var _10=new _3.string.Builder(_3.xmpp.util.createElement("iq",_f,false));
_10.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:roster"},false));
var i=this.session.getRosterIndex(_c);
if(i==-1){
return;
}
var _11={jid:_c};
if(_d){
_11.name=_d;
}else{
if(this.session.roster[i].name){
_11.name=this.session.roster[i].name;
}
}
if(_11.name){
_11.name=_3.xmpp.util.xmlEncode(_11.name);
}
_10.append(_3.xmpp.util.createElement("item",_11,false));
var _12=_e?_e:this.session.roster[i].groups;
if(_12){
for(var x=0;x<_12.length;x++){
_10.append("<group>");
_10.append(_12[x]);
_10.append("</group>");
}
}
_10.append("</item></query></iq>");
var def=this.session.dispatchPacket(_10.toString(),"iq",_f.id);
def.addCallback(this,"verifyRoster");
return def;
},verifyRoster:function(res){
if(res.getAttribute("type")=="result"){
}else{
var err=this.session.processXmppError(res);
this.onAddRosterItemFailed(err);
}
return res;
},addRosterItemToGroup:function(jid,_13){
if(!jid){
throw new Error("Roster::addRosterItemToGroup() JID is null or undefined");
}
if(!_13){
throw new Error("Roster::addRosterItemToGroup() group is null or undefined");
}
var _14=this.session.getRosterIndex(jid);
if(_14==-1){
return;
}
var _15=this.session.roster[_14];
var _16=[];
var _17=false;
for(var i=0;((_15<_15.groups.length)&&(!_17));i++){
if(_15.groups[i]!=_13){
continue;
}
_17=true;
}
if(!_17){
return this.updateRosterItem(jid,_15.name,_15.groups.concat(_13),_14);
}
return _3.xmpp.xmpp.INVALID_ID;
},removeRosterGroup:function(_18){
var _19=this.session.roster;
for(var i=0;i<_19.length;i++){
var _1a=_19[i];
if(_1a.groups.length>0){
for(var j=0;j<_1a.groups.length;j++){
if(_1a.groups[j]==_18){
_1a.groups.splice(j,1);
this.updateRosterItem(_1a.jid,_1a.name,_1a.groups);
}
}
}
}
},renameRosterGroup:function(_1b,_1c){
var _1d=this.session.roster;
for(var i=0;i<_1d.length;i++){
var _1e=_1d[i];
if(_1e.groups.length>0){
for(var j=0;j<_1e.groups.length;j++){
if(_1e.groups[j]==_1b){
_1e.groups[j]=_1c;
this.updateRosterItem(_1e.jid,_1e.name,_1e.groups);
}
}
}
}
},removeRosterItemFromGroup:function(jid,_1f){
if(!jid){
throw new Error("Roster::addRosterItemToGroup() JID is null or undefined");
}
if(!_1f){
throw new Error("Roster::addRosterItemToGroup() group is null or undefined");
}
var _20=this.session.getRosterIndex(jid);
if(_20==-1){
return;
}
var _21=this.session.roster[_20];
var _22=false;
for(var i=0;((i<_21.groups.length)&&(!_22));i++){
if(_21.groups[i]!=_1f){
continue;
}
_22=true;
_20=i;
}
if(_22==true){
_21.groups.splice(_20,1);
return this.updateRosterItem(jid,_21.name,_21.groups);
}
return _3.xmpp.xmpp.INVALID_ID;
},rosterItemRenameGroup:function(jid,_23,_24){
if(!jid){
throw new Error("Roster::rosterItemRenameGroup() JID is null or undefined");
}
if(!_24){
throw new Error("Roster::rosterItemRenameGroup() group is null or undefined");
}
var _25=this.session.getRosterIndex(jid);
if(_25==-1){
return;
}
var _26=this.session.roster[_25];
var _27=false;
for(var i=0;((i<_26.groups.length)&&(!_27));i++){
if(_26.groups[i]==_23){
_26.groups[i]=_24;
_27=true;
}
}
if(_27==true){
return this.updateRosterItem(jid,_26.name,_26.groups);
}
return _3.xmpp.xmpp.INVALID_ID;
},renameRosterItem:function(jid,_28){
if(!jid){
throw new Error("Roster::addRosterItemToGroup() JID is null or undefined");
}
if(!_28){
throw new Error("Roster::addRosterItemToGroup() New Name is null or undefined");
}
var _29=this.session.getRosterIndex(jid);
if(_29==-1){
return;
}
return this.updateRosterItem(jid,_28,this.session.roster.groups,_29);
},removeRosterItem:function(jid){
if(!jid){
throw new Error("Roster::addRosterItemToGroup() JID is null or undefined");
}
var req={id:this.session.getNextIqId(),from:this.session.jid+"/"+this.session.resource,type:"set"};
var _2a=new _3.string.Builder(_3.xmpp.util.createElement("iq",req,false));
_2a.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:roster"},false));
if(jid.indexOf("@")==-1){
jid+=jid+"@"+this.session.domain;
}
_2a.append(_3.xmpp.util.createElement("item",{jid:jid,subscription:"remove"},true));
_2a.append("</query></iq>");
var def=this.session.dispatchPacket(_2a.toString(),"iq",req.id);
def.addCallback(this,"verifyRoster");
return def;
},getAvatar:function(jid){
},publishAvatar:function(_2b,_2c){
},onVerifyRoster:function(id){
},onVerifyRosterFailed:function(err){
}});
});
