//>>built
define("dojox/drawing/ui/dom/Pan",["dojo","../../util/oo","../../plugins/_Plugin","../../manager/_registry"],function(_1,oo,_2,_3){
_1.deprecated("dojox.drawing.ui.dom.Pan","It may not even make it to the 1.4 release.",1.4);
var _4=oo.declare(_2,function(_5){
this.domNode=_5.node;
var _6;
_1.connect(this.domNode,"click",this,"onSetPan");
_1.connect(this.keys,"onKeyUp",this,"onKeyUp");
_1.connect(this.keys,"onKeyDown",this,"onKeyDown");
_1.connect(this.anchors,"onAnchorUp",this,"checkBounds");
_1.connect(this.stencils,"register",this,"checkBounds");
_1.connect(this.canvas,"resize",this,"checkBounds");
_1.connect(this.canvas,"setZoom",this,"checkBounds");
_1.connect(this.canvas,"onScroll",this,function(){
if(this._blockScroll){
this._blockScroll=false;
return;
}
_6&&clearTimeout(_6);
_6=setTimeout(_1.hitch(this,"checkBounds"),200);
});
this._mouseHandle=this.mouse.register(this);
},{selected:false,type:"dojox.drawing.ui.dom.Pan",onKeyUp:function(_7){
if(_7.keyCode==32){
this.onSetPan(false);
}
},onKeyDown:function(_8){
if(_8.keyCode==32){
this.onSetPan(true);
}
},onSetPan:function(_9){
if(_9===true||_9===false){
this.selected=!_9;
}
if(this.selected){
this.selected=false;
_1.removeClass(this.domNode,"selected");
}else{
this.selected=true;
_1.addClass(this.domNode,"selected");
}
this.mouse.setEventMode(this.selected?"pan":"");
},onPanDrag:function(_a){
var x=_a.x-_a.last.x;
var y=_a.y-_a.last.y;
this.canvas.domNode.parentNode.scrollTop-=_a.move.y;
this.canvas.domNode.parentNode.scrollLeft-=_a.move.x;
this.canvas.onScroll();
},onStencilUp:function(_b){
this.checkBounds();
},onStencilDrag:function(_c){
},checkBounds:function(){
var _d=function(){
};
var _e=function(){
};
var t=Infinity,r=-Infinity,b=-Infinity,l=Infinity,sx=0,sy=0,dy=0,dx=0,mx=this.stencils.group?this.stencils.group.getTransform():{dx:0,dy:0},sc=this.mouse.scrollOffset(),_f=sc.left?10:0,scX=sc.top?10:0,ch=this.canvas.height,cw=this.canvas.width,z=this.canvas.zoom,pch=this.canvas.parentHeight,pcw=this.canvas.parentWidth;
this.stencils.withSelected(function(m){
var o=m.getBounds();
_e("SEL BOUNDS:",o);
t=Math.min(o.y1+mx.dy,t);
r=Math.max(o.x2+mx.dx,r);
b=Math.max(o.y2+mx.dy,b);
l=Math.min(o.x1+mx.dx,l);
});
this.stencils.withUnselected(function(m){
var o=m.getBounds();
_e("UN BOUNDS:",o);
t=Math.min(o.y1,t);
r=Math.max(o.x2,r);
b=Math.max(o.y2,b);
l=Math.min(o.x1,l);
});
b*=z;
var _10=0,_11=0;
_d("Bottom test","b:",b,"z:",z,"ch:",ch,"pch:",pch,"top:",sc.top,"sy:",sy);
if(b>pch||sc.top){
_d("*bottom scroll*");
ch=Math.max(b,pch+sc.top);
sy=sc.top;
_10+=this.canvas.getScrollWidth();
}else{
if(!sy&&ch>pch){
_d("*bottom remove*");
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
_1.setObject("dojox.drawing.ui.dom.Pan",_4);
_4.setup={name:"dojox.drawing.ui.dom.Pan",tooltip:"Pan Tool",iconClass:"iconPan"};
_3.register(_4.setup,"plugin");
return _4;
});
