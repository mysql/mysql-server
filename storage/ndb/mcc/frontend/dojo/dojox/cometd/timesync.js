//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/cometd/_base"],function(_1,_2,_3){
_2.provide("dojox.cometd.timesync");
_2.require("dojox.cometd._base");
_3.cometd.timesync=new function(){
this._window=10;
this._lags=[];
this._offsets=[];
this.lag=0;
this.offset=0;
this.samples=0;
this.getServerTime=function(){
return new Date().getTime()+this.offset;
};
this.getServerDate=function(){
return new Date(this.getServerTime());
};
this.setTimeout=function(_4,_5){
var ts=(_5 instanceof Date)?_5.getTime():(0+_5);
var tc=ts-this.offset;
var _6=tc-new Date().getTime();
if(_6<=0){
_6=1;
}
return setTimeout(_4,_6);
};
this._in=function(_7){
var _8=_7.channel;
if(_8&&_8.indexOf("/meta/")==0){
if(_7.ext&&_7.ext.timesync){
var _9=_7.ext.timesync;
var _a=new Date().getTime();
var l=(_a-_9.tc-_9.p)/2-_9.a;
var o=_9.ts-_9.tc-l;
this._lags.push(l);
this._offsets.push(o);
if(this._offsets.length>this._window){
this._offsets.shift();
this._lags.shift();
}
this.samples++;
l=0;
o=0;
for(var i in this._offsets){
l+=this._lags[i];
o+=this._offsets[i];
}
this.offset=parseInt((o/this._offsets.length).toFixed());
this.lag=parseInt((l/this._lags.length).toFixed());
}
}
return _7;
};
this._out=function(_b){
var _c=_b.channel;
if(_c&&_c.indexOf("/meta/")==0){
var _d=new Date().getTime();
if(!_b.ext){
_b.ext={};
}
_b.ext.timesync={tc:_d,l:this.lag,o:this.offset};
}
return _b;
};
};
_3.cometd._extendInList.push(_2.hitch(_3.cometd.timesync,"_in"));
_3.cometd._extendOutList.push(_2.hitch(_3.cometd.timesync,"_out"));
});
