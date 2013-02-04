//>>built
define("dojox/mdnd/DropIndicator",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/html","./AreaManager"],function(_1){
var di=_1.declare("dojox.mdnd.DropIndicator",null,{node:null,constructor:function(){
var _2=document.createElement("div");
var _3=document.createElement("div");
_2.appendChild(_3);
_1.addClass(_2,"dropIndicator");
this.node=_2;
},place:function(_4,_5,_6){
if(_6){
this.node.style.height=_6.h+"px";
}
try{
if(_5){
_4.insertBefore(this.node,_5);
}else{
_4.appendChild(this.node);
}
return this.node;
}
catch(e){
return null;
}
},remove:function(){
if(this.node){
this.node.style.height="";
if(this.node.parentNode){
this.node.parentNode.removeChild(this.node);
}
}
},destroy:function(){
if(this.node){
if(this.node.parentNode){
this.node.parentNode.removeChild(this.node);
}
_1._destroyElement(this.node);
delete this.node;
}
}});
dojox.mdnd.areaManager()._dropIndicator=new dojox.mdnd.DropIndicator();
return di;
});
