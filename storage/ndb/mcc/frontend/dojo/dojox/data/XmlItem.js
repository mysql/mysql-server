//>>built
define("dojox/data/XmlItem",["dojo/_base/declare"],function(_1){
return _1("dojox.data.XmlItem",null,{constructor:function(_2,_3,_4){
this.element=_2;
this.store=_3;
this.q=_4;
},toString:function(){
var _5="";
if(this.element){
for(var i=0;i<this.element.childNodes.length;i++){
var _6=this.element.childNodes[i];
if(_6.nodeType===3||_6.nodeType===4){
_5+=_6.nodeValue;
}
}
}
return _5;
}});
});
