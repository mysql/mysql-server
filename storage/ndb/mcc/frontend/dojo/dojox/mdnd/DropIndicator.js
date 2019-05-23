//>>built
define("dojox/mdnd/DropIndicator",["dojo/_base/kernel","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","./AreaManager"],function(_1,_2,_3,_4){
var di=_2("dojox.mdnd.DropIndicator",null,{node:null,constructor:function(){
var _5=document.createElement("div");
var _6=document.createElement("div");
_5.appendChild(_6);
_3.add(_5,"dropIndicator");
this.node=_5;
},place:function(_7,_8,_9){
if(_9){
this.node.style.height=_9.h+"px";
}
try{
if(_8){
_7.insertBefore(this.node,_8);
}else{
_7.appendChild(this.node);
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
_4.destroy(this.node);
delete this.node;
}
}});
dojox.mdnd.areaManager()._dropIndicator=new dojox.mdnd.DropIndicator();
return di;
});
