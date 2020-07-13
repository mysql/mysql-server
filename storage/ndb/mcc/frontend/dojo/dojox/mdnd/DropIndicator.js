//>>built
define("dojox/mdnd/DropIndicator",["dojo/_base/kernel","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","./AreaManager"],function(_1,_2,_3,_4,_5){
var di=_2("dojox.mdnd.DropIndicator",null,{node:null,constructor:function(){
var _6=document.createElement("div");
var _7=document.createElement("div");
_6.appendChild(_7);
_3.add(_6,"dropIndicator");
this.node=_6;
},place:function(_8,_9,_a){
if(_a){
this.node.style.height=_a.h+"px";
}
try{
if(_9){
_8.insertBefore(this.node,_9);
}else{
_8.appendChild(this.node);
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
_5.areaManager()._dropIndicator=new di();
return di;
});
