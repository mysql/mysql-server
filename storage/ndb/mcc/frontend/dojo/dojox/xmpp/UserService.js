//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.xmpp.UserService");
_2.declare("dojox.xmpp.UserService",null,{constructor:function(_4){
this.session=_4;
},getPersonalProfile:function(){
var _5={id:this.session.getNextIqId(),type:"get"};
var _6=new _3.string.Builder(_3.xmpp.util.createElement("iq",_5,false));
_6.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:private"},false));
_6.append(_3.xmpp.util.createElement("sunmsgr",{xmlsns:"sun:xmpp:properties"},true));
_6.append("</query></iq>");
var _7=this.session.dispatchPacket(_6.toString(),"iq",_5.id);
_7.addCallback(this,"_onGetPersonalProfile");
},setPersonalProfile:function(_8){
var _9={id:this.session.getNextIqId(),type:"set"};
var _a=new _3.string.Builder(_3.xmpp.util.createElement("iq",_9,false));
_a.append(_3.xmpp.util.createElement("query",{xmlns:"jabber:iq:private"},false));
_a.append(_3.xmpp.util.createElement("sunmsgr",{xmlsns:"sun:xmpp:properties"},false));
for(var _b in _8){
_a.append(_3.xmpp.util.createElement("property",{name:_b},false));
_a.append(_3.xmpp.util.createElement("value",{},false));
_a.append(_8[_b]);
_a.append("</value></props>");
}
_a.append("</sunmsgr></query></iq>");
var _c=this.session.dispatchPacket(_a.toString(),"iq",_9.id);
_c.addCallback(this,"_onSetPersonalProfile");
},_onSetPersonalProfile:function(_d){
if(_d.getAttribute("type")=="result"){
this.onSetPersonalProfile(_d.getAttribute("id"));
}else{
if(_d.getAttribute("type")=="error"){
var _e=this.session.processXmppError(_d);
this.onSetPersonalProfileFailure(_e);
}
}
},onSetPersonalProfile:function(id){
},onSetPersonalProfileFailure:function(_f){
},_onGetPersonalProfile:function(_10){
if(_10.getAttribute("type")=="result"){
var _11={};
if(_10.hasChildNodes()){
var _12=_10.firstChild;
if((_12.nodeName=="query")&&(_12.getAttribute("xmlns")=="jabber:iq:private")){
var _13=_12.firstChild;
if((_13.nodeName=="query")&&(_13.getAttributes("xmlns")=="sun:xmpp:properties")){
for(var i=0;i<_13.childNodes.length;i++){
var n=_13.childNodes[i];
if(n.nodeName=="property"){
var _14=n.getAttribute("name");
var val=n.firstChild||"";
_11[_14]=val;
}
}
}
}
this.onGetPersonalProfile(_11);
}
}else{
if(_10.getAttribute("type")=="error"){
var err=this.session.processXmppError(_10);
this.onGetPersonalProfileFailure(err);
}
}
return _10;
},onGetPersonalProfile:function(_15){
},onGetPersonalProfileFailure:function(err){
}});
});
