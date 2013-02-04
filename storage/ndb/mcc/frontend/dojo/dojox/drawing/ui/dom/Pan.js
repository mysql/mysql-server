//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/plugins/_Plugin"],function(_1,_2,_3){
_2.provide("dojox.drawing.ui.dom.Pan");
_2.require("dojox.drawing.plugins._Plugin");
_2.deprecated("dojox.drawing.ui.dom.Pan","It may not even make it to the 1.4 release.",1.4);
_3.drawing.ui.dom.Pan=_3.drawing.util.oo.declare(_3.drawing.plugins._Plugin,function(_4){
this.domNode=_4.node;
var _5;
_2.connect(this.domNode,"click",this,"onSetPan");
_2.connect(this.keys,"onKeyUp",this,"onKeyUp");
_2.connect(this.keys,"onKeyDown",this,"onKeyDown");
_2.connect(this.anchors,"onAnchorUp",this,"checkBounds");
_2.connect(this.stencils,"register",this,"checkBounds");
_2.connect(this.canvas,"resize",this,"checkBounds");
_2.connect(this.canvas,"setZoom",this,"checkBounds");
_2.connect(this.canvas,"onScroll",this,function(){
if(this._blockScroll){
this._blockScroll=false;
return;
}
_5&&clearTimeout(_5);
_5=setTimeout(_2.hitch(this,"checkBounds"),200);
});
this._mouseHandle=this.mouse.register(this);
},{selected:false,type:"dojox.drawing.ui.dom.Pan",onKeyUp:function(_6){
if(_6.keyCode==32){
this.onSetPan(false);
}
},onKeyDown:function(_7){
if(_7.keyCode==32){
this.onSetPan(true);
}
},onSetPan:function(_8){
if(_8===true||_8===false){
this.selected=!_8;
}
if(this.selected){
this.selected=false;
_2.removeClass(this.domNode,"selected");
}else{
this.selected=true;
_2.addClass(this.domNode,"selected");
}
this.mouse.setEventMode(this.selected?"pan":"");
},onPanDrag:function(_9){
var x=_9.x-_9.last.x;
var y=_9.y-_9.last.y;
this.canvas.domNode.parentNode.scrollTop-=_9.move.y;
this.canvas.domNode.parentNode.scrollLeft-=_9.move.x;
this.canvas.onScroll();
},onStencilUp:function(_a){
this.checkBounds();
},onStencilDrag:function(_b){
},checkBounds:function(){
var _c=function(){
};
var _d=function(){
};
var t=Infinity,r=-Infinity,b=-Infinity,l=Infinity,sx=0,sy=0,dy=0,dx=0,mx=this.stencils.group?this.stencils.group.getTransform():{dx:0,dy:0},sc=this.mouse.scrollOffset(),_e=sc.left?10:0,_f=sc.top?10:0,ch=this.canvas.height,cw=this.canvas.width,z=this.canvas.zoom,pch=this.canvas.parentHeight,pcw=this.canvas.parentWidth;
this.stencils.withSelected(function(m){
var o=m.getBounds();
_d("SEL BOUNDS:",o);
t=Math.min(o.y1+mx.dy,t);
r=Math.max(o.x2+mx.dx,r);
b=Math.max(o.y2+mx.dy,b);
l=Math.min(o.x1+mx.dx,l);
});
this.stencils.withUnselected(function(m){
var o=m.getBounds();
_d("UN BOUNDS:",o);
t=Math.min(o.y1,t);
r=Math.max(o.x2,r);
b=Math.max(o.y2,b);
l=Math.min(o.x1,l);
});
b*=z;
var _10=0,_11=0;
_c("Bottom test","b:",b,"z:",z,"ch:",ch,"pch:",pch,"top:",sc.top,"sy:",sy);
if(b>pch||sc.top){
_c("*bottom scroll*");
ch=Math.max(b,pch+sc.top);
sy=sc.top;
_10+=this.canvas.getScrollWidth();
}else{
if(!sy&&ch>pch){
_c("*bottom remove*");
ch=pch;
}
}
r*=z;
if(r>pcw||sc.left){
cw=Math.max(r,pcw+sc.left);
sx=sc.left;
_11+=this.canvas.getScrollWidth();
}else{
if(!sx&&cw>pcw){
cw=pcw;
}
}
cw+=_10*2;
ch+=_11*2;
this._blockScroll=true;
this.stencils.group&&this.stencils.group.applyTransform({dx:dx,dy:dy});
this.stencils.withUnselected(function(m){
m.transformPoints({dx:dx,dy:dy});
});
this.canvas.setDimensions(cw,ch,sx,sy);
}});
_3.drawing.ui.dom.Pan.setup={name:"dojox.drawing.ui.dom.Pan",tooltip:"Pan Tool",iconClass:"iconPan"};
_3.drawing.register(_3.drawing.ui.dom.Pan.setup,"plugin");
});
