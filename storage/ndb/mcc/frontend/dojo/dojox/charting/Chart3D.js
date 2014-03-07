//>>built
define("dojox/charting/Chart3D",["dojo/_base/array","dojo/dom","dojo/_base/declare","dojo/_base/html","dojox/gfx","dojox/gfx3d"],function(_1,_2,_3,_4,_5,_6){
var _7={x:0,y:0,z:1},v=_6.vector,n=_5.normalizedLength;
return _3("dojox.charting.Chart3D",null,{constructor:function(_8,_9,_a,_b){
this.node=_2.byId(_8);
this.surface=_5.createSurface(this.node,n(this.node.style.width),n(this.node.style.height));
this.view=this.surface.createViewport();
this.view.setLights(_9.lights,_9.ambient,_9.specular);
this.view.setCameraTransform(_a);
this.theme=_b;
this.walls=[];
this.plots=[];
},generate:function(){
return this._generateWalls()._generatePlots();
},invalidate:function(){
this.view.invalidate();
return this;
},render:function(){
this.view.render();
return this;
},addPlot:function(_c){
return this._add(this.plots,_c);
},removePlot:function(_d){
return this._remove(this.plots,_d);
},addWall:function(_e){
return this._add(this.walls,_e);
},removeWall:function(_f){
return this._remove(this.walls,_f);
},_add:function(_10,_11){
if(!_1.some(_10,function(i){
return i==_11;
})){
_10.push(_11);
this.view.invalidate();
}
return this;
},_remove:function(_12,_13){
var a=_1.filter(_12,function(i){
return i!=_13;
});
return a.length<_12.length?(_12=a,this.invalidate()):this;
},_generateWalls:function(){
for(var i=0;i<this.walls.length;++i){
if(v.dotProduct(_7,this.walls[i].normal)>0){
this.walls[i].generate(this);
}
}
return this;
},_generatePlots:function(){
var _14=0,m=_6.matrix,i=0;
for(;i<this.plots.length;++i){
_14+=this.plots[i].getDepth();
}
for(--i;i>=0;--i){
var _15=this.view.createScene();
_15.setTransform(m.translate(0,0,-_14));
this.plots[i].generate(this,_15);
_14-=this.plots[i].getDepth();
}
return this;
}});
});
