//>>built
define("dojox/timing/Streamer",["./_base"],function(){
dojo.experimental("dojox.timing.Streamer");
dojox.timing.Streamer=function(_1,_2,_3,_4,_5){
var _6=this;
var _7=[];
this.interval=_3||1000;
this.minimumSize=_4||10;
this.inputFunction=_1||function(q){
};
this.outputFunction=_2||function(_8){
};
var _9=new dojox.timing.Timer(this.interval);
var _a=function(){
_6.onTick(_6);
if(_7.length<_6.minimumSize){
_6.inputFunction(_7);
}
var _b=_7.shift();
while(typeof (_b)=="undefined"&&_7.length>0){
_b=_7.shift();
}
if(typeof (_b)=="undefined"){
_6.stop();
return;
}
_6.outputFunction(_b);
};
this.setInterval=function(ms){
this.interval=ms;
_9.setInterval(ms);
};
this.onTick=function(_c){
};
this.start=function(){
if(typeof (this.inputFunction)=="function"&&typeof (this.outputFunction)=="function"){
_9.start();
return;
}
throw new Error("You cannot start a Streamer without an input and an output function.");
};
this.onStart=function(){
};
this.stop=function(){
_9.stop();
};
this.onStop=function(){
};
_9.onTick=this.tick;
_9.onStart=this.onStart;
_9.onStop=this.onStop;
if(_5){
_7.concat(_5);
}
};
return dojox.timing.Streamer;
});
