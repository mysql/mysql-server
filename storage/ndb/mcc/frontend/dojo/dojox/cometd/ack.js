//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/cometd/_base"],function(_1,_2,_3){
_2.provide("dojox.cometd.ack");
_2.require("dojox.cometd._base");
_3.cometd._ack=new function(){
var _4=false;
var _5=-1;
this._in=function(_6){
if(_6.channel=="/meta/handshake"){
_4=_6.ext&&_6.ext.ack;
}else{
if(_4&&_6.channel=="/meta/connect"&&_6.ext&&_6.ext.ack&&_6.successful){
var _7=parseInt(_6.ext.ack);
_5=_7;
}
}
return _6;
};
this._out=function(_8){
if(_8.channel=="/meta/handshake"){
if(!_8.ext){
_8.ext={};
}
_8.ext.ack=_3.cometd.ackEnabled;
_5=-1;
}
if(_4&&_8.channel=="/meta/connect"){
if(!_8.ext){
_8.ext={};
}
_8.ext.ack=_5;
}
return _8;
};
};
_3.cometd._extendInList.push(_2.hitch(_3.cometd._ack,"_in"));
_3.cometd._extendOutList.push(_2.hitch(_3.cometd._ack,"_out"));
_3.cometd.ackEnabled=true;
});
