//>>built
define("dojox/charting/Chart3D",["dojo/_base/array","dojo/dom","dojo/_base/declare","dojox/gfx","dojox/gfx3d"],function(_1,_2,_3,_4,_5){
var _6={x:0,y:0,z:1},v=_5.vector,n=_4.normalizedLength;
return _3("dojox.charting.Chart3D",null,{constructor:function(_7,_8,_9,_a){
this.node=_2.byId(_7);
this.surface=_4.createSurface(this.node,n(this.node.style.width),n(this.node.style.height));
this.view=this.surface.createViewport();
this.view.setLights(_8.lights,_8.ambient,_8.specular);
this.view.setCameraTransform(_9);
this.theme=_a;
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
},addPlot:function(_b){
return this._add(this.plots,_b);
},removePlot:function(_c){
return this._remove(this.plots,_c);
},addWall:function(_d){
return this._add(this.walls,_d);
},removeWall:function(_e){
return this._remove(this.walls,_e);
},_add:function(_f,_10){
if(!_1.some(_f,function(i){
return i==_10;
})){
_f.push(_10);
this.view.invalidate();
}
return this;
},_remove:function(_11,_12){
var a=_1.filter(_11,function(i){
return i!=_12;
});
return a.length<_11.length?(_11=a,this.invalidate()):this;
},_generateWalls:function(){
for(var i=0;i<this.walls.length;++i){
if(v.dotProduct(_6,this.walls[i].normal)>0){
this.walls[i].generate(this);
}
}
return this;
},_generatePlots:function(){
var _13=0,m=_5.matrix,i=0;
for(;i<this.plots.length;++i){
_13+=this.plots[i].getDepth();
}
for(--i;i>=0;--i){
var _14=this.view.createScene();
_14.setTransform(m.translate(0,0,-_13));
this.plots[i].generate(this,_14);
_13-=this.plots[i].getDepth();
}
return this;
}});
});
