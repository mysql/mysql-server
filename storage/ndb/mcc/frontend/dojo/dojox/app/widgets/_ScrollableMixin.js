//>>built
define("dojox/app/widgets/_ScrollableMixin",["dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","dojox/mobile/scrollable"],function(_1,_2,_3,_4,_5){
var _6=_1("dojox.app.widgets._ScrollableMixin",null,{scrollableParams:null,allowNestedScrolls:true,constructor:function(){
this.scrollableParams={noResize:true};
},destroy:function(){
this.cleanup();
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
var _7=this.scrollableParams;
this.init(_7);
this.inherited(arguments);
this.reparent();
},buildRendering:function(){
this.inherited(arguments);
_3.add(this.domNode,"mblScrollableView");
this.domNode.style.overflow="hidden";
this.domNode.style.top="0px";
this.containerNode=_4.create("div",{className:"mblScrollableViewContainer"},this.domNode);
this.containerNode.style.position="absolute";
this.containerNode.style.top="0px";
if(this.scrollDir==="v"){
this.containerNode.style.width="100%";
}
},reparent:function(){
var i,_8,_9,c;
for(i=0,_8=0,_9=this.domNode.childNodes.length;i<_9;i++){
c=this.domNode.childNodes[_8];
if(c===this.containerNode){
_8++;
continue;
}
this.containerNode.appendChild(this.domNode.removeChild(c));
}
},resize:function(){
this.inherited(arguments);
array.forEach(this.getChildren(),function(_a){
if(_a.resize){
_a.resize();
}
});
}});
_2.extend(_6,new _5());
return _6;
});
