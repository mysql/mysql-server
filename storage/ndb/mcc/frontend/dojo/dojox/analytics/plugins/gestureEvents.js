//>>built
define("dojox/analytics/plugins/gestureEvents",["dojo/_base/lang","../_base","dojo/_base/window","dojo/on","dojo/_base/config","dojo/touch","dojox/gesture/tap","dojox/gesture/swipe"],function(_1,_2,_3,on,_4,_5,_6,_7){
return (_2.plugins.gestureEvents=new (function(){
if(_4["watchSwipe"]!==undefined&&!_4["watchSwipe"]){
this.watchSwipe=false;
}else{
this.watchSwipe=true;
}
this.swipeSampleDelay=_4["swipeSampleDelay"]||1000;
this.targetProps=_4["targetProps"]||["id","className","localName","href","spellcheck","lang","textContent","value"];
this.textContentMaxChars=_4["textContentMaxChars"]||50;
this.addDataSwipe=_1.hitch(_2,"addData","gesture.swipe");
this.sampleSwipe=function(e){
if(!this._rateLimited){
this.addDataSwipe(this.trimEvent(e));
this._rateLimited=true;
setTimeout(_1.hitch(this,function(){
if(this._rateLimited){
this.trimEvent(this._lastSwipeEvent);
delete this._lastSwipeEvent;
delete this._rateLimited;
}
}),this.swipeSampleDelay);
}
this._lastSwipeEvent=e;
return e;
};
if(this.watchSwipe){
on(_3.doc,_7,_1.hitch(this,"sampleSwipe"));
}
this.addData=_1.hitch(_2,"addData","gesture.tap");
this.onGestureTap=function(e){
this.addData(this.trimEvent(e));
};
on(_3.doc,_6,_1.hitch(this,"onGestureTap"));
this.addDataDoubleTap=_1.hitch(_2,"addData","gesture.tap.doubletap");
this.onGestureDoubleTap=function(e){
this.addDataDoubleTap(this.trimEvent(e));
};
on(_3.doc,_6.doubletap,_1.hitch(this,"onGestureDoubleTap"));
this.addDataTapHold=_1.hitch(_2,"addData","gesture.tap.taphold");
this.onGestureTapHold=function(e){
this.addDataTapHold(this.trimEvent(e));
};
on(_3.doc,_6.hold,_1.hitch(this,"onGestureTapHold"));
this.trimEvent=function(e){
var t={};
for(var i in e){
switch(i){
case "target":
var _8=this.targetProps;
t[i]={};
for(var j=0;j<_8.length;j++){
if(e[i][_8[j]]){
if(_8[j]=="text"||_8[j]=="textContent"){
if((e[i]["localName"]!="HTML")&&(e[i]["localName"]!="BODY")){
t[i][_8[j]]=e[i][_8[j]].substr(0,this.textContentMaxChars);
}
}else{
t[i][_8[j]]=e[i][_8[j]];
}
}
}
break;
case "clientX":
case "clientY":
case "screenX":
case "screenY":
case "dx":
case "dy":
case "time":
t[i]=e[i];
break;
}
}
return t;
};
})());
});
