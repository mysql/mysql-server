//>>built
define("dojox/drawing/stencil/Path",["dojo","dojo/_base/array","../util/oo","./_Base","../manager/_registry"],function(_1,_2,oo,_3,_4){
var _5=oo.declare(_3,function(_6){
},{type:"dojox.drawing.stencil.Path",closePath:true,baseRender:true,closeRadius:10,closeColor:{r:255,g:255,b:0,a:0.5},_create:function(_7,_8){
this.remove(this[_7]);
if(!this.points.length){
return;
}
if(dojox.gfx.renderer=="svg"){
var _9=[];
_2.forEach(this.points,function(o,i){
if(!o.skip){
if(i==0){
_9.push("M "+o.x+" "+o.y);
}else{
var _a=(o.t||"")+" ";
if(o.x===undefined){
_9.push(_a);
}else{
_9.push(_a+o.x+" "+o.y);
}
}
}
},this);
if(this.closePath){
_9.push("Z");
}
this.stringPath=_9.join(" ");
this[_7]=this.container.createPath(_9.join(" ")).setStroke(_8);
this.closePath&&this[_7].setFill(_8.fill);
}else{
this[_7]=this.container.createPath({}).setStroke(_8);
this.closePath&&this[_7].setFill(_8.fill);
_2.forEach(this.points,function(o,i){
if(!o.skip){
if(i==0||o.t=="M"){
this[_7].moveTo(o.x,o.y);
}else{
if(o.t=="Z"){
this.closePath&&this[_7].closePath();
}else{
this[_7].lineTo(o.x,o.y);
}
}
}
},this);
this.closePath&&this[_7].closePath();
}
this._setNodeAtts(this[_7]);
},render:function(){
this.onBeforeRender(this);
this.renderHit&&this._create("hit",this.style.currentHit);
this._create("shape",this.style.current);
},getBounds:function(_b){
var _c=10000,_d=10000,_e=0,_f=0;
_2.forEach(this.points,function(p){
if(p.x!==undefined&&!isNaN(p.x)){
_c=Math.min(_c,p.x);
_d=Math.min(_d,p.y);
_e=Math.max(_e,p.x);
_f=Math.max(_f,p.y);
}
});
return {x1:_c,y1:_d,x2:_e,y2:_f,x:_c,y:_d,w:_e-_c,h:_f-_d};
},checkClosePoint:function(_10,_11,_12){
var _13=this.util.distance(_10.x,_10.y,_11.x,_11.y);
if(this.points.length>1){
if(_13<this.closeRadius&&!this.closeGuide&&!_12){
var c={cx:_10.x,cy:_10.y,rx:this.closeRadius,ry:this.closeRadius};
this.closeGuide=this.container.createEllipse(c).setFill(this.closeColor);
}else{
if(_12||_13>this.closeRadius&&this.closeGuide){
this.remove(this.closeGuide);
this.closeGuide=null;
}
}
}
return _13<this.closeRadius;
}});
_1.setObject("dojox.drawing.stencil.Path",_5);
_4.register({name:"dojox.drawing.stencil.Path"},"stencil");
return _5;
});
