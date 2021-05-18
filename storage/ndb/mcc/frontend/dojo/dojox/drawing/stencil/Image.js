//>>built
define("dojox/drawing/stencil/Image",["dojo","../util/oo","./_Base","../manager/_registry"],function(_1,oo,_2,_3){
var _4=oo.declare(_2,function(_5){
},{type:"dojox.drawing.stencil.Image",anchorType:"group",baseRender:true,dataToPoints:function(o){
o=o||this.data;
this.points=[{x:o.x,y:o.y},{x:o.x+o.width,y:o.y},{x:o.x+o.width,y:o.y+o.height},{x:o.x,y:o.y+o.height}];
return this.points;
},pointsToData:function(p){
p=p||this.points;
var s=p[0];
var e=p[2];
this.data={x:s.x,y:s.y,width:e.x-s.x,height:e.y-s.y,src:this.src||this.data.src};
return this.data;
},_createHilite:function(){
this.remove(this.hit);
this.hit=this.container.createRect(this.data).setStroke(this.style.current).setFill(this.style.current.fill);
this._setNodeAtts(this.hit);
},_create:function(_6,d,_7){
this.remove(this[_6]);
var s=this.container.getParent();
this[_6]=s.createImage(d);
this.container.add(this[_6]);
this._setNodeAtts(this[_6]);
},render:function(_8){
if(this.data.width=="auto"||isNaN(this.data.width)){
this.getImageSize(true);
console.warn("Image size not provided. Acquiring...");
return;
}
this.onBeforeRender(this);
this.renderHit&&this._createHilite();
this._create("shape",this.data,this.style.current);
},getImageSize:function(_9){
if(this._gettingSize){
return;
}
this._gettingSize=true;
var _a=_1.create("img",{src:this.data.src},_1.body());
var _b=_1.connect(_a,"error",this,function(){
_1.disconnect(c);
_1.disconnect(_b);
console.error("Error loading image:",this.data.src);
console.warn("Error image:",this.data);
});
var c=_1.connect(_a,"load",this,function(){
var _c=_1.marginBox(_a);
this.setData({x:this.data.x,y:this.data.y,src:this.data.src,width:_c.w,height:_c.h});
_1.disconnect(c);
_1.destroy(_a);
_9&&this.render(true);
});
}});
_1.setObject("dojox.drawing.stencil.Image",_4);
_3.register({name:"dojox.drawing.stencil.Image"},"stencil");
return _4;
});
