//>>built
define("dojox/analytics/plugins/mouseClick",["dojo/_base/lang","../_base","dojo/_base/config","dojo/_base/window","dojo/on"],function(_1,_2,_3,_4,on){
return (_2.plugins.mouseClick=new (function(){
this.addData=_1.hitch(_2,"addData","mouseClick");
this.targetProps=_3["targetProps"]||["id","className","nodeName","localName","href","spellcheck","lang"];
this.textContentMaxChars=_3["textContentMaxChars"]||50;
this.onClick=function(e){
this.addData(this.trimEvent(e));
};
on(_4.doc,"click",_1.hitch(this,"onClick"));
this.trimEvent=function(e){
var t={};
for(var i in e){
switch(i){
case "target":
case "originalTarget":
case "explicitOriginalTarget":
var _5=this.targetProps;
t[i]={};
for(var j=0;j<_5.length;j++){
if(e[i][_5[j]]){
if(_5[j]=="text"||_5[j]=="textContent"){
if((e[i]["localName"]!="HTML")&&(e[i]["localName"]!="BODY")){
t[i][_5[j]]=e[i][_5[j]].substr(0,this.textContentMaxChars);
}
}else{
t[i][_5[j]]=e[i][_5[j]];
}
}
}
break;
case "clientX":
case "clientY":
case "pageX":
case "pageY":
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
