//>>built
define("dojox/analytics/_base",["dojo/_base/lang","dojo/_base/config","dojo/ready","dojo/_base/unload","dojo/_base/sniff","dojo/_base/xhr","dojo/_base/json","dojo/io-query","dojo/io/script"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=function(){
this._data=[];
this._id=1;
this.sendInterval=_2["sendInterval"]||5000;
this.inTransitRetry=_2["inTransitRetry"]||200;
this.dataUrl=_2["analyticsUrl"]||require.toUrl("dojox/analytics/logger/dojoxAnalytics.php");
this.sendMethod=_2["sendMethod"]||"xhrPost";
this.maxRequestSize=_5("ie")?2000:_2["maxRequestSize"]||4000;
_3(this,"schedulePusher");
_4.addOnUnload(this,function(){
this.pushData();
});
};
_1.extend(_a,{schedulePusher:function(_b){
setTimeout(_1.hitch(this,"checkData"),_b||this.sendInterval);
},addData:function(_c,_d){
if(arguments.length>2){
_d=Array.prototype.slice.call(arguments,1);
}
this._data.push({plugin:_c,data:_d});
},checkData:function(){
if(this._inTransit){
this.schedulePusher(this.inTransitRetry);
return;
}
if(this.pushData()){
return;
}
this.schedulePusher();
},pushData:function(){
if(this._data.length){
this._inTransit=this._data;
this._data=[];
var _e;
switch(this.sendMethod){
case "script":
_e=_9.get({url:this.getQueryPacket(),preventCache:1,callbackParamName:"callback"});
break;
case "xhrPost":
default:
_e=_6.post({url:this.dataUrl,content:{id:this._id++,data:_7.toJson(this._inTransit)}});
break;
}
_e.addCallback(this,"onPushComplete");
return _e;
}
return false;
},getQueryPacket:function(){
while(true){
var _f={id:this._id++,data:_7.toJson(this._inTransit)};
var _10=this.dataUrl+"?"+_8.objectToQuery(_f);
if(_10.length>this.maxRequestSize){
this._data.unshift(this._inTransit.pop());
this._split=1;
}else{
return _10;
}
}
},onPushComplete:function(_11){
if(this._inTransit){
delete this._inTransit;
}
if(this._data.length>0){
this.schedulePusher(this.inTransitRetry);
}else{
this.schedulePusher();
}
}});
return _1.setObject("dojox.analytics",new _a());
});
