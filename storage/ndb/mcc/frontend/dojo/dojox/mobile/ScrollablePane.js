//>>built
define("dojox/mobile/ScrollablePane",["dojo/_base/array","dojo/_base/declare","dojo/_base/sniff","dojo/_base/window","dojo/dom","dojo/dom-construct","dojo/dom-style","./_ScrollableMixin","./Pane"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mobile.ScrollablePane",[_9,_8],{roundCornerMask:false,radius:0,baseClass:"mblScrollablePane",buildRendering:function(){
var c=this.containerNode=_6.create("div",{className:"mblScrollableViewContainer",style:{width:this.scrollDir==="v"?"100%":""}});
this.inherited(arguments);
if(this.srcNodeRef){
for(var i=0,_a=this.srcNodeRef.childNodes.length;i<_a;i++){
this.containerNode.appendChild(this.srcNodeRef.firstChild);
}
}
if(this.roundCornerMask&&_3("webkit")){
var _b=this.containerNode;
var _c=this.maskNode=_6.create("div",{className:"mblScrollablePaneMask",style:{webkitMaskImage:"-webkit-canvas("+this.id+"_mask)"}});
_c.appendChild(_b);
c=_c;
}
this.domNode.appendChild(c);
_5.setSelectable(this.containerNode,false);
},resize:function(){
this.inherited(arguments);
if(this.roundCornerMask){
this.createRoundMask();
}
_1.forEach(this.getChildren(),function(_d){
if(_d.resize){
_d.resize();
}
});
},isTopLevel:function(e){
var _e=this.getParent&&this.getParent();
return (!_e||!_e.resize);
},createRoundMask:function(){
if(_3("webkit")){
if(this.domNode.offsetHeight==0){
return;
}
this.maskNode.style.height=this.domNode.offsetHeight+"px";
var _f=this.getChildren()[0],c=this.containerNode,_10=_f?_f.domNode:(c.childNodes.length>0&&(c.childNodes[0].nodeType===1?c.childNodes[0]:c.childNodes[1]));
var r=this.radius;
if(!r){
var _11=function(n){
return parseInt(_7.get(n,"borderTopLeftRadius"));
};
if(_f){
r=_11(_f.domNode);
if(!r){
var _12=_f.getChildren()[0];
r=_12?_11(_12.domNode):0;
}
}else{
r=_11(_10);
}
}
var pw=this.domNode.offsetWidth,w=_10.offsetWidth,h=this.domNode.offsetHeight,t=_7.get(_10,"marginTop"),b=_7.get(_10,"marginBottom"),l=_7.get(_10,"marginLeft");
var ctx=_4.doc.getCSSCanvasContext("2d",this.id+"_mask",pw,h);
ctx.fillStyle="#000000";
ctx.beginPath();
ctx.moveTo(l+r,t);
ctx.arcTo(l+w,t,l+w,h-b-r,r);
ctx.arcTo(l+w,h-b,l+r,h-b,r);
ctx.arcTo(l,h-b,l,t+r,r);
ctx.arcTo(l,t,l+r,t,r);
ctx.closePath();
ctx.fill();
}
}});
});
