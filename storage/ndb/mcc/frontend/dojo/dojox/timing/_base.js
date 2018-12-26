//>>built
define("dojox/timing/_base",["dojo/_base/kernel","dojo/_base/lang"],function(_1){
_1.experimental("dojox.timing");
_1.getObject("timing",true,dojox);
dojox.timing.Timer=function(_2){
this.timer=null;
this.isRunning=false;
this.interval=_2;
this.onStart=null;
this.onStop=null;
};
_1.extend(dojox.timing.Timer,{onTick:function(){
},setInterval:function(_3){
if(this.isRunning){
window.clearInterval(this.timer);
}
this.interval=_3;
if(this.isRunning){
this.timer=window.setInterval(_1.hitch(this,"onTick"),this.interval);
}
},start:function(){
if(typeof this.onStart=="function"){
this.onStart();
}
this.isRunning=true;
this.timer=window.setInterval(_1.hitch(this,"onTick"),this.interval);
},stop:function(){
if(typeof this.onStop=="function"){
this.onStop();
}
this.isRunning=false;
window.clearInterval(this.timer);
}});
return dojox.timing;
});
