//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/plugins/_Plugin"],function(_1,_2,_3){
_2.provide("dojox.drawing.plugins.tools.Pan");
_2.require("dojox.drawing.plugins._Plugin");
_3.drawing.plugins.tools.Pan=_3.drawing.util.oo.declare(_3.drawing.plugins._Plugin,function(_4){
this.domNode=_4.node;
var _5;
this.toolbar=_4.scope;
this.connect(this.toolbar,"onToolClick",this,function(){
this.onSetPan(false);
});
this.connect(this.keys,"onKeyUp",this,"onKeyUp");
this.connect(this.keys,"onKeyDown",this,"onKeyDown");
this.connect(this.keys,"onArrow",this,"onArrow");
this.connect(this.anchors,"onAnchorUp",this,"checkBounds");
this.connect(this.stencils,"register",this,"checkBounds");
this.connect(this.canvas,"resize",this,"checkBounds");
this.connect(this.canvas,"setZoom",this,"checkBounds");
this.connect(this.canvas,"onScroll",this,function(){
if(this._blockScroll){
this._blockScroll=false;
return;
}
_5&&clearTimeout(_5);
_5=setTimeout(_2.hitch(this,"checkBounds"),200);
});
this._mouseHandle=this.mouse.register(this);
},{selected:false,keyScroll:false,type:"dojox.drawing.plugins.tools.Pan",onPanUp:function(_6){
if(_6.id==this.button.id){
this.onSetPan(false);
}
},onKeyUp:function(_7){
switch(_7.keyCode){
case 32:
this.onSetPan(false);
break;
case 39:
case 37:
case 38:
case 40:
clearInterval(this._timer);
break;
}
},onKeyDown:function(_8){
if(_8.keyCode==32){
this.onSetPan(true);
}
},interval:20,onArrow:function(_9){
if(this._timer){
clearInterval(this._timer);
}
this._timer=setInterval(_2.hitch(this,function(_a){
this.canvas.domNode.parentNode.scrollLeft+=_a.x*10;
this.canvas.domNode.parentNode.scrollTop+=_a.y*10;
},_9),this.interval);
},onSetPan:function(_b){
if(_b===true||_b===false){
this.selected=!_b;
}
if(this.selected){
this.selected=false;
this.button.deselect();
}else{
this.selected=true;
this.button.select();
}
this.mouse.setEventMode(this.selected?"pan":"");
},onPanDrag:function(_c){
var x=_c.x-_c.last.x;
var y=_c.y-_c.last.y;
this.canvas.domNode.parentNode.scrollTop-=_c.move.y;
this.canvas.domNode.parentNode.scrollLeft-=_c.move.x;
this.canvas.onScroll();
},onUp:function(_d){
if(_d.withinCanvas){
this.keyScroll=true;
}else{
this.keyScroll=false;
}
},onStencilUp:function(_e){
this.checkBounds();
},onStencilDrag:function(_f){
},checkBounds:function(){
var log=function(){
};
var _10=function(){
};
var t=Infinity,r=-Infinity,b=-10000,l=10000,sx=0,sy=0,dy=0,dx=0,mx=this.stencils.group?this.stencils.group.getTransform():{dx:0,dy:0},sc=this.mouse.scrollOffset(),scY=sc.left?10:0,scX=sc.top?10:0,ch=this.canvas.height,cw=this.canvas.width,z=this.canvas.zoom,pch=this.canvas.parentHeight,pcw=this.canvas.parentWidth;
this.stencils.withSelected(function(m){
var o=m.getBounds();
_10("SEL BOUNDS:",o);
t=Math.min(o.y1+mx.dy,t);
r=Math.max(o.x2+mx.dx,r);
b=Math.max(o.y2+mx.dy,b);
l=Math.min(o.x1+mx.dx,l);
});
this.stencils.withUnselected(function(m){
var o=m.getBounds();
_10("UN BOUNDS:",o);
t=Math.min(o.y1,t);
r=Math.max(o.x2,r);
b=Math.max(o.y2,b);
l=Math.min(o.x1,l);
log("----------- B:",b,o.y2);
});
b*=z;
var _11=0,_12=0;
log("Bottom test","b:",b,"z:",z,"ch:",ch,"pch:",pch,"top:",sc.top,"sy:",sy,"mx.dy:",mx.dy);
if(b>pch||sc.top){
log("*bottom scroll*");
ch=Math.max(b,pch+sc.top);
sy=sc.top;
_11+=this.canvas.getScrollWidth();
}else{
if(!sy&&ch>pch){
log("*bottom remove*");
ch=pch;
}
}
r*=z;
if(r>pcw||sc.left){
cw=Math.max(r,pcw+sc.left);
sx=sc.left;
_12+=this.canvas.getScrollWidth();
}else{
if(!sx&&cw>pcw){
cw=pcw;
}
}
cw+=_11*2;
ch+=_12*2;
this._blockScroll=true;
this.stencils.group&&this.stencils.group.applyTransform({dx:dx,dy:dy});
this.stencils.withUnselected(function(m){
m.transformPoints({dx:dx,dy:dy});
});
this.canvas.setDimensions(cw,ch,sx,sy);
}});
_3.drawing.plugins.tools.Pan.setup={name:"dojox.drawing.plugins.tools.Pan",tooltip:"Pan Tool",iconClass:"iconPan",button:false};
_3.drawing.register(_3.drawing.plugins.tools.Pan.setup,"plugin");
});
