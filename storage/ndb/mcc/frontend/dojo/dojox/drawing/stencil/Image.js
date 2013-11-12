//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.stencil.Image");
_3.drawing.stencil.Image=_3.drawing.util.oo.declare(_3.drawing.stencil._Base,function(_4){
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
},_create:function(_5,d,_6){
this.remove(this[_5]);
var s=this.container.getParent();
this[_5]=s.createImage(d);
this.container.add(this[_5]);
this._setNodeAtts(this[_5]);
},render:function(_7){
if(this.data.width=="auto"||isNaN(this.data.width)){
this.getImageSize(true);
console.warn("Image size not provided. Acquiring...");
return;
}
this.onBeforeRender(this);
this.renderHit&&this._createHilite();
this._create("shape",this.data,this.style.current);
},getImageSize:function(_8){
if(this._gettingSize){
return;
}
this._gettingSize=true;
var _9=_2.create("img",{src:this.data.src},_2.body());
var _a=_2.connect(_9,"error",this,function(){
_2.disconnect(c);
_2.disconnect(_a);
console.error("Error loading image:",this.data.src);
console.warn("Error image:",this.data);
});
var c=_2.connect(_9,"load",this,function(){
var _b=_2.marginBox(_9);
this.setData({x:this.data.x,y:this.data.y,src:this.data.src,width:_b.w,height:_b.h});
_2.disconnect(c);
_2.destroy(_9);
_8&&this.render(true);
});
}});
_3.drawing.register({name:"dojox.drawing.stencil.Image"},"stencil");
});
