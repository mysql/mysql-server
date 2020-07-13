//>>built
define("dojox/charting/Chart3D",["dojo/_base/array","dojo/dom","dojo/_base/declare","dojox/gfx","dojox/gfx3d","dojo/has","dojo/has!dojo-bidi?./bidi/Chart3D"],function(_1,_2,_3,_4,_5,_6,_7){
var _8={x:0,y:0,z:1},v=_5.vector,n=_4.normalizedLength;
var _9=_3(_6("dojo-bidi")?"dojox.charting.NonBidiChart3D":"dojox.charting.Chart3D",null,{constructor:function(_a,_b,_c,_d){
this.node=_2.byId(_a);
this.surface=_4.createSurface(this.node,n(this.node.style.width),n(this.node.style.height));
this.view=this.surface.createViewport();
this.view.setLights(_b.lights,_b.ambient,_b.specular);
this.view.setCameraTransform(_c);
this.theme=_d;
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
},addPlot:function(_e){
return this._add(this.plots,_e);
},removePlot:function(_f){
return this._remove(this.plots,_f);
},addWall:function(_10){
return this._add(this.walls,_10);
},removeWall:function(_11){
return this._remove(this.walls,_11);
},_add:function(_12,_13){
if(!_1.some(_12,function(i){
return i==_13;
})){
_12.push(_13);
this.view.invalidate();
}
return this;
},_remove:function(_14,_15){
var a=_1.filter(_14,function(i){
return i!=_15;
});
return a.length<_14.length?(_14=a,this.invalidate()):this;
},_generateWalls:function(){
for(var i=0;i<this.walls.length;++i){
if(v.dotProduct(_8,this.walls[i].normal)>0){
this.walls[i].generate(this);
}
}
return this;
},_generatePlots:function(){
var _16=0,m=_5.matrix,i=0;
for(;i<this.plots.length;++i){
_16+=this.plots[i].getDepth();
}
for(--i;i>=0;--i){
var _17=this.view.createScene();
_17.setTransform(m.translate(0,0,-_16));
this.plots[i].generate(this,_17);
_16-=this.plots[i].getDepth();
}
return this;
},setDir:function(dir){
return this;
}});
return _6("dojo-bidi")?_3("dojox.charting.Chart3D",[_9,_7]):_9;
});
