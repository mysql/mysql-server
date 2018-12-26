//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.socket.Reconnect");
_3.socket.Reconnect=function(_4,_5){
_5=_5||{};
var _6=_5.reconnectTime||10000;
var _7=_2.connect(_4,"onclose",function(_8){
clearTimeout(_9);
if(!_8.wasClean){
_4.disconnected(function(){
_3.socket.replace(_4,_a=_4.reconnect());
});
}
});
var _9,_a;
if(!_4.disconnected){
_4.disconnected=function(_b){
setTimeout(function(){
_b();
_9=setTimeout(function(){
if(_a.readyState<2){
_6=_5.reconnectTime||10000;
}
},10000);
},_6);
_6*=_5.backoffRate||2;
};
}
if(!_4.reconnect){
_4.reconnect=function(){
return _4.args?_3.socket.LongPoll(_4.args):_3.socket.WebSocket({url:_4.URL||_4.url});
};
}
return _4;
};
});
