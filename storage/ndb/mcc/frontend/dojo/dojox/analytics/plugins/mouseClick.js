//>>built
define("dojox/analytics/plugins/mouseClick",["dojo/_base/lang","../_base","dojo/_base/window","dojo/on"],function(_1,_2,_3,on){
return (_2.plugins.mouseClick=new (function(){
this.addData=_1.hitch(_2,"addData","mouseClick");
this.onClick=function(e){
this.addData(this.trimEvent(e));
};
on(_3.doc,"click",_1.hitch(this,"onClick"));
this.trimEvent=function(e){
var t={};
for(var i in e){
switch(i){
case "target":
case "originalTarget":
case "explicitOriginalTarget":
var _4=["id","className","nodeName","localName","href","spellcheck","lang"];
t[i]={};
for(var j=0;j<_4.length;j++){
if(e[i][_4[j]]){
if(_4[j]=="text"||_4[j]=="textContent"){
if((e[i]["localName"]!="HTML")&&(e[i]["localName"]!="BODY")){
t[i][_4[j]]=e[i][_4[j]].substr(0,50);
}
}else{
t[i][_4[j]]=e[i][_4[j]];
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
