//>>built
define("dojox/socket",["dojo/_base/array","dojo/_base/lang","dojo/_base/xhr","dojo/aspect","dojo/on","dojo/Evented","dojo/_base/url"],function(_1,_2,_3,_4,on,_5,_6){
var _7=window.WebSocket;
var _8=function(_9){
if(typeof _9=="string"){
_9={url:_9};
}
return _7?_8.WebSocket(_9,true):_8.LongPoll(_9);
};
_8.WebSocket=function(_a,_b){
var _c=document.baseURI||window.location.href;
var ws=new _7(new _6(_c.replace(/^http/i,"ws"),_a.url));
ws.on=function(_d,_e){
ws.addEventListener(_d,_e,true);
};
var _f;
_4.after(ws,"onopen",function(_10){
_f=true;
},true);
_4.after(ws,"onclose",function(_11){
if(_f){
return;
}
if(_b){
_8.replace(ws,_8.LongPoll(_a),true);
}
},true);
return ws;
};
_8.replace=function(_12,_13,_14){
_12.send=_2.hitch(_13,"send");
_12.close=_2.hitch(_13,"close");
var _15=function(_16){
(_13.addEventListener||_13.on).call(_13,_16,function(_17){
on.emit(_12,_17.type,_17);
},true);
};
if(_14){
_15("open");
}
_1.forEach(["message","close","error"],_15);
};
_8.LongPoll=function(_18){
var _19=false,_1a=true,_1b,_1c=[];
var _1d,_1e;
var _1f={send:function(_20){
var _21=_2.delegate(_18);
_21.rawBody=_20;
clearTimeout(_1b);
var _22=_1a?(_1a=false)||_1f.firstRequest(_21):_1f.transport(_21);
_1c.push(_22);
_22.then(function(_23){
_1f.readyState=1;
_1c.splice(_1.indexOf(_1c,_22),1);
if(!_1c.length){
_1b=setTimeout(_1e,_18.interval);
}
if(_23){
_1d("message",{data:_23},_22);
}
},function(_24){
_1c.splice(_1.indexOf(_1c,_22),1);
if(!_19){
_1d("error",{error:_24},_22);
if(!_1c.length){
clearTimeout(_1b);
_1f.readyState=3;
_1d("close",{wasClean:false},_22);
}
}
});
return _22;
},close:function(){
_1f.readyState=2;
_19=true;
var i;
for(i=0;i<_1c.length;i++){
_1c[i].cancel();
}
_1f.readyState=3;
_1d("close",{wasClean:true});
},transport:_18.transport||_3.post,args:_18,url:_18.url,readyState:0,CONNECTING:0,OPEN:1,CLOSING:2,CLOSED:3,on:_5.prototype.on,firstRequest:function(_25){
var _26=(_25.headers||(_25.headers={}));
_26.Pragma="start-long-poll";
try{
return this.transport(_25);
}
finally{
delete _26.Pragma;
}
}};
_1d=function(_27,_28,_29){
if(_1f["on"+_27]){
_28.ioArgs=_29&&_29.ioArgs;
_28.type=_27;
on.emit(_1f,_27,_28);
}
};
_1e=function(){
if(_1f.readyState===0){
_1d("open",{});
}
if(!_1c.length){
_1f.send();
}
};
_1f.connect=_1f.on;
setTimeout(_1e);
return _1f;
};
return _8;
});
