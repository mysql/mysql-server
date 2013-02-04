//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/layout/StackContainer"],function(_1,_2,_3){
_2.provide("dojox.layout.ext-dijit.layout.StackContainer-touch");
_2.experimental("dojox.layout.ext-dijit.layout.StackContainer-touch");
_2.require("dijit.layout.StackContainer");
_2.connect(_1.layout.StackContainer.prototype,"postCreate",function(){
this.axis=(this.baseClass=="dijitAccordionContainer")?"Y":"X";
_2.forEach(["touchstart","touchmove","touchend","touchcancel"],function(p){
this.connect(this.domNode,p,function(e){
switch(e.type){
case "touchmove":
e.preventDefault();
if(this.touchPosition){
var _4=e.touches[0]["page"+this.axis]-this.touchPosition;
if(Math.abs(_4)>100){
if(this.axis=="Y"){
_4*=-1;
}
delete this.touchPosition;
if(_4>0){
!this.selectedChildWidget.isLastChild&&this.forward();
}else{
!this.selectedChildWidget.isFirstChild&&this.back();
}
}
}
break;
case "touchstart":
if(e.touches.length==1){
this.touchPosition=e.touches[0]["page"+this.axis];
break;
}
case "touchend":
case "touchcancel":
delete this.touchPosition;
}
});
},this);
});
});
