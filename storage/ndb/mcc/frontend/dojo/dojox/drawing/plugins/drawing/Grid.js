//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/plugins/_Plugin"],function(_1,_2,_3){
_2.provide("dojox.drawing.plugins.drawing.Grid");
_2.require("dojox.drawing.plugins._Plugin");
_3.drawing.plugins.drawing.Grid=_3.drawing.util.oo.declare(_3.drawing.plugins._Plugin,function(_4){
if(_4.gap){
this.major=_4.gap;
}
this.majorColor=_4.majorColor||this.majorColor;
this.minorColor=_4.minorColor||this.minorColor;
this.setGrid();
_2.connect(this.canvas,"setZoom",this,"setZoom");
},{type:"dojox.drawing.plugins.drawing.Grid",gap:100,major:100,minor:0,majorColor:"#00ffff",minorColor:"#d7ffff",zoom:1,setZoom:function(_5){
this.zoom=_5;
this.setGrid();
},setGrid:function(_6){
var _7=Math.floor(this.major*this.zoom);
var _8=this.minor?Math.floor(this.minor*this.zoom):_7;
this.grid&&this.grid.removeShape();
var x1,x2,y1,y2,i,_9,_a;
var s=this.canvas.underlay.createGroup();
var w=2000;
var h=1000;
var b=1;
var mj=this.majorColor;
var mn=this.minorColor;
var _b=function(x1,y1,x2,y2,c){
s.createLine({x1:x1,y1:y1,x2:x2,y2:y2}).setStroke({style:"Solid",width:b,cap:"round",color:c});
};
for(i=1,_a=h/_8;i<_a;i++){
x1=0,x2=w;
y1=_8*i,y2=y1;
_9=y1%_7?mn:mj;
_b(x1,y1,x2,y2,_9);
}
for(i=1,_a=w/_8;i<_a;i++){
y1=0,y2=h;
x1=_8*i,x2=x1;
_9=x1%_7?mn:mj;
_b(x1,y1,x2,y2,_9);
}
s.moveToBack();
this.grid=s;
this.util.attr(s,"id","grid");
return s;
}});
});
