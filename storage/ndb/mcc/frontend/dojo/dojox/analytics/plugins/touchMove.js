//>>built
define("dojox/analytics/plugins/touchMove",["dojo/_base/lang","../_base","dojo/_base/config","dojo/_base/window","dojo/on","dojo/touch"],function(_1,_2,_3,_4,on,_5){
return (_2.plugins.touchMove=new (function(){
if(_3["watchTouch"]!==undefined&&!_3["watchTouch"]){
this.watchTouch=false;
}else{
this.watchTouch=true;
}
if(_3["showTouchesDetails"]!==undefined&&!_3["showTouchesDetails"]){
this.showTouchesDetails=false;
}else{
this.showTouchesDetails=true;
}
this.touchSampleDelay=_3["touchSampleDelay"]||1000;
this.targetProps=_3["targetProps"]||["id","className","localName","href","spellcheck","lang","textContent","value"];
this.textContentMaxChars=_3["textContentMaxChars"]||50;
this.addData=_1.hitch(_2,"addData","touch.move");
this.sampleTouchMove=function(e){
if(!this._rateLimited){
this.addData("sample",this.trimTouchEvent(e));
this._rateLimited=true;
setTimeout(_1.hitch(this,function(){
if(this._rateLimited){
this.trimTouchEvent(this._lastTouchEvent);
delete this._lastTouchEvent;
delete this._rateLimited;
}
}),this.touchSampleDelay);
}
this._lastTouchEvent=e;
return e;
};
on(_4.doc,_5.move,_1.hitch(this,"sampleTouchMove"));
this.handleTarget=function(t,_6,i){
var _7=this.targetProps;
t[i]={};
for(var j=0;j<_7.length;j++){
if((typeof _6=="object"||typeof _6=="function")&&_7[j] in _6){
if(_7[j]=="text"||_7[j]=="textContent"){
if(_6["localName"]&&(_6["localName"]!="HTML")&&(_6["localName"]!="BODY")){
t[i][_7[j]]=_6[_7[j]].substr(0,this.textContentMaxChars);
}
}else{
t[i][_7[j]]=_6[_7[j]];
}
}
}
};
this.trimTouchEvent=function(e){
var t={};
var _8;
for(var i in e){
switch(i){
case "target":
this.handleTarget(t,e[i],i);
break;
case "touches":
if(e[i].length!==0){
t["touches.length"]=e[i].length;
}
if(this.showTouchesDetails){
for(var j=0;j<e[i].length;j++){
for(var s in e[i][j]){
switch(s){
case "target":
this.handleTarget(t,e[i][j].target,"touches["+j+"][target]");
break;
case "clientX":
case "clientY":
case "screenX":
case "screenY":
if(e[i][j]){
_8=e[i][j][s];
t["touches["+j+"]["+s+"]"]=_8+"";
}
break;
default:
break;
}
}
}
}
break;
case "clientX":
case "clientY":
case "screenX":
case "screenY":
if(e[i]){
_8=e[i];
t[i]=_8+"";
}
break;
default:
break;
}
}
return t;
};
})());
});
