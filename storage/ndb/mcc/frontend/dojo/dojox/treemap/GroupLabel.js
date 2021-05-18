//>>built
define("dojox/treemap/GroupLabel",["dojo/_base/declare","dojo/dom-construct","dojo/dom-style"],function(_1,_2,_3){
return _1("dojox.treemap.GroupLabel",null,{createRenderer:function(_4,_5,_6){
var _7=this.inherited(arguments);
if(_6=="content"||_6=="leaf"){
var p=_2.create("div");
_3.set(p,{"zIndex":30,"position":"relative","height":"100%","textAlign":"center","top":"50%","marginTop":"-.5em"});
_2.place(p,_7);
}
return _7;
},styleRenderer:function(_8,_9,_a,_b){
switch(_b){
case "leaf":
_3.set(_8,"background",this.getColorForItem(_9).toHex());
case "content":
if(_a==0){
_8.firstChild.innerHTML=this.getLabelForItem(_9);
}else{
_8.firstChild.innerHTML=null;
}
break;
case "header":
_3.set(_8,"display","none");
}
}});
});
