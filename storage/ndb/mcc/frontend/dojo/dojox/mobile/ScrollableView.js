//>>built
define("dojox/mobile/ScrollableView",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dijit/registry","./View","./_ScrollableMixin"],function(_1,_2,_3,_4,_5,_6,_7){
return _2("dojox.mobile.ScrollableView",[_6,_7],{scrollableParams:null,keepScrollPos:false,constructor:function(){
this.scrollableParams={noResize:true};
},buildRendering:function(){
this.inherited(arguments);
_3.add(this.domNode,"mblScrollableView");
this.domNode.style.overflow="hidden";
this.domNode.style.top="0px";
this.containerNode=_4.create("DIV",{className:"mblScrollableViewContainer"},this.domNode);
this.containerNode.style.position="absolute";
this.containerNode.style.top="0px";
if(this.scrollDir==="v"){
this.containerNode.style.width="100%";
}
this.reparent();
this.findAppBars();
},resize:function(){
this.inherited(arguments);
_1.forEach(this.getChildren(),function(_8){
if(_8.resize){
_8.resize();
}
});
},isTopLevel:function(e){
var _9=this.getParent&&this.getParent();
return (!_9||!_9.resize);
},addChild:function(_a,_b){
var c=_a.domNode;
var _c=this.checkFixedBar(c,true);
if(_c){
this.domNode.appendChild(c);
if(_c==="top"){
this.fixedHeaderHeight=c.offsetHeight;
this.isLocalHeader=true;
}else{
if(_c==="bottom"){
this.fixedFooterHeight=c.offsetHeight;
this.isLocalFooter=true;
c.style.bottom="0px";
}
}
this.resize();
if(this._started&&!_a._started){
_a.startup();
}
}else{
this.inherited(arguments);
}
},reparent:function(){
var i,_d,_e,c;
for(i=0,_d=0,_e=this.domNode.childNodes.length;i<_e;i++){
c=this.domNode.childNodes[_d];
if(c===this.containerNode||this.checkFixedBar(c,true)){
_d++;
continue;
}
this.containerNode.appendChild(this.domNode.removeChild(c));
}
},onAfterTransitionIn:function(_f,dir,_10,_11,_12){
this.flashScrollBar();
},getChildren:function(){
var _13=this.inherited(arguments);
if(this.fixedHeader&&this.fixedHeader.parentNode===this.domNode){
_13.push(_5.byNode(this.fixedHeader));
}
if(this.fixedFooter&&this.fixedFooter.parentNode===this.domNode){
_13.push(_5.byNode(this.fixedFooter));
}
return _13;
}});
});
