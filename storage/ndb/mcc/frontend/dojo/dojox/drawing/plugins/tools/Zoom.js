//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/plugins/_Plugin"],function(_1,_2,_3){
_2.provide("dojox.drawing.plugins.tools.Zoom");
_2.require("dojox.drawing.plugins._Plugin");
(function(){
var _4=Math.pow(2,0.25),_5=10,_6=0.1,_7=1,dt=_3.drawing.plugins.tools;
dt.ZoomIn=_3.drawing.util.oo.declare(function(_8){
},{});
dt.ZoomIn=_3.drawing.util.oo.declare(_3.drawing.plugins._Plugin,function(_9){
},{type:"dojox.drawing.plugins.tools.ZoomIn",onZoomIn:function(){
_7*=_4;
_7=Math.min(_7,_5);
this.canvas.setZoom(_7);
this.mouse.setZoom(_7);
},onClick:function(){
this.onZoomIn();
}});
dt.Zoom100=_3.drawing.util.oo.declare(_3.drawing.plugins._Plugin,function(_a){
},{type:"dojox.drawing.plugins.tools.Zoom100",onZoom100:function(){
_7=1;
this.canvas.setZoom(_7);
this.mouse.setZoom(_7);
},onClick:function(){
this.onZoom100();
}});
dt.ZoomOut=_3.drawing.util.oo.declare(_3.drawing.plugins._Plugin,function(_b){
},{type:"dojox.drawing.plugins.tools.ZoomOut",onZoomOut:function(){
_7/=_4;
_7=Math.max(_7,_6);
this.canvas.setZoom(_7);
this.mouse.setZoom(_7);
},onClick:function(){
this.onZoomOut();
}});
dt.ZoomIn.setup={name:"dojox.drawing.plugins.tools.ZoomIn",tooltip:"Zoom In"};
_3.drawing.register(dt.ZoomIn.setup,"plugin");
dt.Zoom100.setup={name:"dojox.drawing.plugins.tools.Zoom100",tooltip:"Zoom to 100%"};
_3.drawing.register(dt.Zoom100.setup,"plugin");
dt.ZoomOut.setup={name:"dojox.drawing.plugins.tools.ZoomOut",tooltip:"Zoom In"};
_3.drawing.register(dt.ZoomOut.setup,"plugin");
})();
});
