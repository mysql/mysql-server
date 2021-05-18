//>>built
define("dojox/analytics/plugins/touchPress",["dojo/_base/lang","../_base","dojo/_base/config","dojo/_base/window","dojo/on","dojo/touch"],function(_1,_2,_3,_4,on,_5){
return (_2.plugins.touchPress=new (function(){
if(_3["showTouchesDetails"]!==undefined&&!_3["showTouchesDetails"]){
this.showTouchesDetails=false;
}else{
this.showTouchesDetails=true;
}
this.targetProps=_3["targetProps"]||["id","className","nodeName","localName","href","spellcheck","lang"];
this.textContentMaxChars=_3["textContentMaxChars"]||50;
this.addData=_1.hitch(_2,"addData","touch.press");
this.onTouchPress=function(e){
this.addData(this.trimEvent(e));
};
this.addDataRelease=_1.hitch(_2,"addData","touch.release");
this.onTouchRelease=function(e){
this.addDataRelease(this.trimEvent(e));
};
on(_4.doc,_5.press,_1.hitch(this,"onTouchPress"));
on(_4.doc,_5.release,_1.hitch(this,"onTouchRelease"));
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
this.trimEvent=function(e){
var t={};
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
var _8=e[i][j][s];
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
t[i]=e[i];
break;
}
}
return t;
};
})());
});
