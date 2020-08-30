//>>built
define("dojox/analytics/plugins/mouseOver",["dojo/_base/lang","../_base","dojo/_base/config","dojo/_base/window","dojo/on"],function(_1,_2,_3,_4,on){
return (_2.plugins.mouseOver=new (function(){
this.watchMouse=_3["watchMouseOver"]||true;
this.mouseSampleDelay=_3["sampleDelay"]||2500;
this.addData=_1.hitch(_2,"addData","mouseOver");
this.targetProps=_3["targetProps"]||["id","className","localName","href","spellcheck","lang","textContent","value"];
this.textContentMaxChars=_3["textContentMaxChars"]||50;
this.toggleWatchMouse=function(){
if(this._watchingMouse){
this._watchingMouse.remove();
delete this._watchingMouse;
return;
}
on(_4.doc,"mousemove",_1.hitch(this,"sampleMouse"));
};
if(this.watchMouse){
on(_4.doc,"mouseover",_1.hitch(this,"toggleWatchMouse"));
on(_4.doc,"mouseout",_1.hitch(this,"toggleWatchMouse"));
}
this.sampleMouse=function(e){
if(!this._rateLimited){
this.addData("sample",this.trimMouseEvent(e));
this._rateLimited=true;
setTimeout(_1.hitch(this,function(){
if(this._rateLimited){
this.trimMouseEvent(this._lastMouseEvent);
delete this._lastMouseEvent;
delete this._rateLimited;
}
}),this.mouseSampleDelay);
}
this._lastMouseEvent=e;
return e;
};
this.trimMouseEvent=function(e){
var t={};
for(var i in e){
switch(i){
case "target":
var _5=this.targetProps;
t[i]={};
for(var j=0;j<_5.length;j++){
if((typeof e[i]=="object"||typeof e[i]=="function")&&_5[j] in e[i]){
if(_5[j]=="text"||_5[j]=="textContent"){
if(e[i]["localName"]&&(e[i]["localName"]!="HTML")&&(e[i]["localName"]!="BODY")){
t[i][_5[j]]=e[i][_5[j]].substr(0,this.textContentMaxChars);
}
}else{
t[i][_5[j]]=e[i][_5[j]];
}
}
}
break;
case "screenX":
case "screenY":
case "x":
case "y":
if(e[i]){
var _6=e[i];
t[i]=_6+"";
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
