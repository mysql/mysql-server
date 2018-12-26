//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.stencil.Path");
_3.drawing.stencil.Path=_3.drawing.util.oo.declare(_3.drawing.stencil._Base,function(_4){
_2.disconnect(this._postRenderCon);
},{type:"dojox.drawing.stencil.Path",closePath:true,baseRender:true,closeRadius:10,closeColor:{r:255,g:255,b:0,a:0.5},_create:function(_5,_6){
this.remove(this[_5]);
if(!this.points.length){
return;
}
if(_3.gfx.renderer=="svg"){
var _7=[];
_2.forEach(this.points,function(o,i){
if(!o.skip){
if(i==0){
_7.push("M "+o.x+" "+o.y);
}else{
var _8=(o.t||"")+" ";
if(o.x===undefined){
_7.push(_8);
}else{
_7.push(_8+o.x+" "+o.y);
}
}
}
},this);
if(this.closePath){
_7.push("Z");
}
this.stringPath=_7.join(" ");
this[_5]=this.container.createPath(_7.join(" ")).setStroke(_6);
this.closePath&&this[_5].setFill(_6.fill);
}else{
this[_5]=this.container.createPath({}).setStroke(_6);
this.closePath&&this[_5].setFill(_6.fill);
_2.forEach(this.points,function(o,i){
if(!o.skip){
if(i==0||o.t=="M"){
this[_5].moveTo(o.x,o.y);
}else{
if(o.t=="Z"){
this.closePath&&this[_5].closePath();
}else{
this[_5].lineTo(o.x,o.y);
}
}
}
},this);
this.closePath&&this[_5].closePath();
}
this._setNodeAtts(this[_5]);
},render:function(){
this.onBeforeRender(this);
this.renderHit&&this._create("hit",this.style.currentHit);
this._create("shape",this.style.current);
},getBounds:function(_9){
var _a=10000,_b=10000,_c=0,_d=0;
_2.forEach(this.points,function(p){
if(p.x!==undefined&&!isNaN(p.x)){
_a=Math.min(_a,p.x);
_b=Math.min(_b,p.y);
_c=Math.max(_c,p.x);
_d=Math.max(_d,p.y);
}
});
return {x1:_a,y1:_b,x2:_c,y2:_d,x:_a,y:_b,w:_c-_a,h:_d-_b};
},checkClosePoint:function(_e,_f,_10){
var _11=this.util.distance(_e.x,_e.y,_f.x,_f.y);
if(this.points.length>1){
if(_11<this.closeRadius&&!this.closeGuide&&!_10){
var c={cx:_e.x,cy:_e.y,rx:this.closeRadius,ry:this.closeRadius};
this.closeGuide=this.container.createEllipse(c).setFill(this.closeColor);
}else{
if(_10||_11>this.closeRadius&&this.closeGuide){
this.remove(this.closeGuide);
this.closeGuide=null;
}
}
}
return _11<this.closeRadius;
}});
_3.drawing.register({name:"dojox.drawing.stencil.Path"},"stencil");
});
