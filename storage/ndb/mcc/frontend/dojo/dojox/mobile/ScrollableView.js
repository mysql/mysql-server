//>>built
define("dojox/mobile/ScrollableView",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dijit/registry","./View","./_ScrollableMixin"],function(_1,_2,_3,_4,_5,_6,_7){
return _2("dojox.mobile.ScrollableView",[_6,_7],{scrollableParams:null,keepScrollPos:false,constructor:function(){
this.scrollableParams={noResize:true};
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
},startup:function(){
if(this._started){
return;
}
this.reparent();
this.inherited(arguments);
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
},addFixedBar:function(_a){
var c=_a.domNode;
var _b=this.checkFixedBar(c,true);
if(!_b){
return;
}
this.domNode.appendChild(c);
if(_b==="top"){
this.fixedHeaderHeight=c.offsetHeight;
this.isLocalHeader=true;
}else{
if(_b==="bottom"){
this.fixedFooterHeight=c.offsetHeight;
this.isLocalFooter=true;
c.style.bottom="0px";
}
}
this.resize();
},reparent:function(){
var i,_c,_d,c;
for(i=0,_c=0,_d=this.domNode.childNodes.length;i<_d;i++){
c=this.domNode.childNodes[_c];
if(c===this.containerNode||this.checkFixedBar(c,true)){
_c++;
continue;
}
this.containerNode.appendChild(this.domNode.removeChild(c));
}
},onAfterTransitionIn:function(_e,_f,_10,_11,_12){
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
