//>>built
define("dojox/socket",["dojo","dojo/Evented","dojo/cookie","dojo/_base/url"],function(_1,_2){
var _3=window.WebSocket;
function _4(_5){
if(typeof _5=="string"){
_5={url:_5};
}
return _3?dojox.socket.WebSocket(_5,true):dojox.socket.LongPoll(_5);
};
dojox.socket=_4;
_4.WebSocket=function(_6,_7){
var ws=new _3(new _1._Url(document.baseURI.replace(/^http/i,"ws"),_6.url));
ws.on=function(_8,_9){
ws.addEventListener(_8,_9,true);
};
var _a;
_1.connect(ws,"onopen",function(_b){
_a=true;
});
_1.connect(ws,"onclose",function(_c){
if(_a){
return;
}
if(_7){
_4.replace(ws,dojox.socket.LongPoll(_6),true);
}
});
return ws;
};
_4.replace=function(_d,_e,_f){
_d.send=_1.hitch(_e,"send");
_d.close=_1.hitch(_e,"close");
if(_f){
_10("open");
}
_1.forEach(["message","close","error"],_10);
function _10(_11){
(_e.addEventListener||_e.on).call(_e,_11,function(_12){
var _13=document.createEvent("MessageEvent");
_13.initMessageEvent(_12.type,false,false,_12.data,_12.origin,_12.lastEventId,_12.source);
_d.dispatchEvent(_13);
},true);
};
};
_4.LongPoll=function(_14){
var _15=false,_16=true,_17,_18=[];
var _19={send:function(_1a){
var _1b=_1.delegate(_14);
_1b.rawBody=_1a;
clearTimeout(_17);
var _1c=_16?(_16=false)||_19.firstRequest(_1b):_19.transport(_1b);
_18.push(_1c);
_1c.then(function(_1d){
_19.readyState=1;
_18.splice(_1.indexOf(_18,_1c),1);
if(!_18.length){
_17=setTimeout(_23,_14.interval);
}
if(_1d){
_1f("message",{data:_1d},_1c);
}
},function(_1e){
_18.splice(_1.indexOf(_18,_1c),1);
if(!_15){
_1f("error",{error:_1e},_1c);
if(!_18.length){
_19.readyState=3;
_1f("close",{wasClean:false},_1c);
}
}
});
return _1c;
},close:function(){
_19.readyState=2;
_15=true;
for(var i=0;i<_18.length;i++){
_18[i].cancel();
}
_19.readyState=3;
_1f("close",{wasClean:true});
},transport:_14.transport||_1.xhrPost,args:_14,url:_14.url,readyState:0,CONNECTING:0,OPEN:1,CLOSING:2,CLOSED:3,dispatchEvent:function(_20){
_1f(_20.type,_20);
},on:_2.prototype.on,firstRequest:function(_21){
var _22=(_21.headers||(_21.headers={}));
_22.Pragma="start-long-poll";
try{
return this.transport(_21);
}
finally{
delete _22.Pragma;
}
}};
function _23(){
if(_19.readyState==0){
_1f("open",{});
}
if(!_18.length){
_19.send();
}
};
function _1f(_24,_25,_26){
if(_19["on"+_24]){
var _27=document.createEvent("HTMLEvents");
_27.initEvent(_24,false,false);
_1.mixin(_27,_25);
_27.ioArgs=_26&&_26.ioArgs;
_19["on"+_24](_27);
}
};
_19.connect=_19.on;
setTimeout(_23);
return _19;
};
return _4;
});
