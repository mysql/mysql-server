//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/xmpp/util,dojo/AdapterRegistry,dojox/encoding/digests/MD5"],function(_1,_2,_3){
_2.provide("dojox.xmpp.sasl");
_2.require("dojox.xmpp.util");
_2.require("dojo.AdapterRegistry");
_2.require("dojox.encoding.digests.MD5");
_3.xmpp.sasl.saslNS="urn:ietf:params:xml:ns:xmpp-sasl";
_2.declare("dojox.xmpp.sasl._Base",null,{mechanism:null,closeAuthTag:true,constructor:function(_4){
this.session=_4;
this.startAuth();
},startAuth:function(){
var _5=new _3.string.Builder(_3.xmpp.util.createElement("auth",{xmlns:_3.xmpp.sasl.saslNS,mechanism:this.mechanism},this.closeAuthTag));
this.appendToAuth(_5);
this.session.dispatchPacket(_5.toString());
},appendToAuth:function(_6){
},onChallenge:function(_7){
if(!this.first_challenge){
this.first_challenge=true;
this.onFirstChallenge(_7);
}else{
this.onSecondChallenge(_7);
}
},onFirstChallenge:function(){
},onSecondChallenge:function(){
},onSuccess:function(){
this.session.sendRestart();
}});
_2.declare("dojox.xmpp.sasl.SunWebClientAuth",_3.xmpp.sasl._Base,{mechanism:"SUN-COMMS-CLIENT-PROXY-AUTH"});
_2.declare("dojox.xmpp.sasl.Plain",_3.xmpp.sasl._Base,{mechanism:"PLAIN",closeAuthTag:false,appendToAuth:function(_8){
var id=this.session.jid;
var _9=this.session.jid.indexOf("@");
if(_9!=-1){
id=this.session.jid.substring(0,_9);
}
var _a=this.session.jid+"\x00"+id+"\x00"+this.session.password;
_a=_3.xmpp.util.Base64.encode(_a);
_8.append(_a);
_8.append("</auth>");
delete this.session.password;
}});
_2.declare("dojox.xmpp.sasl.DigestMD5",_3.xmpp.sasl._Base,{mechanism:"DIGEST-MD5",onFirstChallenge:function(_b){
var _c=_3.encoding.digests;
var _d=_3.encoding.digests.outputTypes;
var _e=function(n){
return _c.MD5(n,_d.Hex);
};
var H=function(s){
return _c.MD5(s,_d.String);
};
var _f=_3.xmpp.util.Base64.decode(_b.firstChild.nodeValue);
var ch={realm:"",nonce:"",qop:"auth",maxbuf:65536};
_f.replace(/([a-z]+)=([^,]+)/g,function(t,k,v){
v=v.replace(/^"(.+)"$/,"$1");
ch[k]=v;
});
var _10="";
switch(ch.qop){
case "auth-int":
case "auth-conf":
_10=":00000000000000000000000000000000";
case "auth":
break;
default:
return false;
}
var _11=_c.MD5(Math.random()*1234567890,_d.Hex);
var _12="xmpp/"+this.session.domain;
var _13=this.session.jid;
var _14=this.session.jid.indexOf("@");
if(_14!=-1){
_13=this.session.jid.substring(0,_14);
}
_13=_3.xmpp.util.encodeJid(_13);
var A1=new _3.string.Builder();
A1.append(H(_13+":"+ch.realm+":"+this.session.password),":",ch.nonce+":"+_11);
delete this.session.password;
var _15=":"+_12+_10;
var A2="AUTHENTICATE"+_15;
var _16=new _3.string.Builder();
_16.append(_e(A1.toString()),":",ch.nonce,":00000001:",_11,":",ch.qop,":");
var ret=new _3.string.Builder();
ret.append("username=\"",_13,"\",","realm=\"",ch.realm,"\",","nonce=",ch.nonce,",","cnonce=\"",_11,"\",","nc=\"00000001\",qop=\"",ch.qop,"\",digest-uri=\"",_12,"\",","response=\"",_e(_16.toString()+_e(A2)),"\",charset=\"utf-8\"");
var _17=new _3.string.Builder(_3.xmpp.util.createElement("response",{xmlns:_3.xmpp.xmpp.SASL_NS},false));
_17.append(_3.xmpp.util.Base64.encode(ret.toString()));
_17.append("</response>");
this.rspauth=_e(_16.toString()+_e(_15));
this.session.dispatchPacket(_17.toString());
},onSecondChallenge:function(msg){
var _18=_3.xmpp.util.Base64.decode(msg.firstChild.nodeValue);
if(this.rspauth==_18.substring(8)){
var _19=new _3.string.Builder(_3.xmpp.util.createElement("response",{xmlns:_3.xmpp.xmpp.SASL_NS},true));
this.session.dispatchPacket(_19.toString());
}else{
}
}});
_3.xmpp.sasl.registry=new _2.AdapterRegistry();
_3.xmpp.sasl.registry.register("SUN-COMMS-CLIENT-PROXY-AUTH",function(_1a){
return _1a=="SUN-COMMS-CLIENT-PROXY-AUTH";
},function(_1b,_1c){
return new _3.xmpp.sasl.SunWebClientAuth(_1c);
});
_3.xmpp.sasl.registry.register("DIGEST-MD5",function(_1d){
return _1d=="DIGEST-MD5";
},function(_1e,_1f){
return new _3.xmpp.sasl.DigestMD5(_1f);
});
_3.xmpp.sasl.registry.register("PLAIN",function(_20){
return _20=="PLAIN";
},function(_21,_22){
return new _3.xmpp.sasl.Plain(_22);
});
});
