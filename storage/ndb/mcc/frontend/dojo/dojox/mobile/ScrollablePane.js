//>>built
define("dojox/mobile/ScrollablePane",["dojo/_base/array","dojo/_base/declare","dojo/sniff","dojo/_base/window","dojo/dom-construct","dojo/dom-style","./common","./_ScrollableMixin","./Pane","./_maskUtils"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.mobile.ScrollablePane",[_9,_8],{roundCornerMask:false,radius:0,baseClass:"mblScrollablePane",buildRendering:function(){
var c=this.containerNode=_5.create("div",{className:"mblScrollableViewContainer",style:{width:this.scrollDir==="v"?"100%":""}});
this.inherited(arguments);
if(this.srcNodeRef){
for(var i=0,_b=this.srcNodeRef.childNodes.length;i<_b;i++){
this.containerNode.appendChild(this.srcNodeRef.firstChild);
}
}
if(this.roundCornerMask&&(_3("mask-image"))){
var _c=this.containerNode;
var _d=this.maskNode=_5.create("div",{className:"mblScrollablePaneMask"});
_d.appendChild(_c);
c=_d;
}
this.domNode.appendChild(c);
_7.setSelectable(this.containerNode,false);
},resize:function(){
this.inherited(arguments);
if(this.roundCornerMask){
this.createRoundMask();
}
_1.forEach(this.getChildren(),function(_e){
if(_e.resize){
_e.resize();
}
});
},isTopLevel:function(e){
var _f=this.getParent&&this.getParent();
return (!_f||!_f.resize);
},createRoundMask:function(){
if(_3("mask-image")){
if(this.domNode.offsetHeight==0){
return;
}
this.maskNode.style.height=this.domNode.offsetHeight+"px";
var _10=this.getChildren()[0],c=this.containerNode,_11=_10?_10.domNode:(c.childNodes.length>0&&(c.childNodes[0].nodeType===1?c.childNodes[0]:c.childNodes[1]));
var r=this.radius;
if(!r){
var _12=function(n){
return parseInt(_6.get(n,"borderTopLeftRadius"));
};
if(_10){
r=_12(_10.domNode);
if(!r){
var _13=_10.getChildren()[0];
r=_13?_12(_13.domNode):0;
}
}else{
r=_12(_11);
}
}
var pw=this.domNode.offsetWidth,w=_11.offsetWidth,h=this.domNode.offsetHeight,t=_6.get(_11,"marginTop"),b=_6.get(_11,"marginBottom"),l=_6.get(_11,"marginLeft");
_a.createRoundMask(this.maskNode,l,t,0,b,w,h-b-t,r,r);
}
}});
});
