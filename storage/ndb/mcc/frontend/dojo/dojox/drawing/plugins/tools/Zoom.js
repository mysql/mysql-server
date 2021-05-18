//>>built
define("dojox/drawing/plugins/tools/Zoom",["dojo/_base/lang","../../util/oo","../_Plugin","../../manager/_registry"],function(_1,oo,_2,_3){
var _4=Math.pow(2,0.25),_5=10,_6=0.1,_7=1,dt;
if(!_1.getObject("dojox.drawing.plugins.tools")){
_1.setObject("dojox.drawing.plugins.tools",{});
}
dt=dojox.drawing.plugins.tools;
dt.ZoomIn=oo.declare(_2,function(_8){
},{type:"dojox.drawing.plugins.tools.ZoomIn",onZoomIn:function(){
_7*=_4;
_7=Math.min(_7,_5);
this.canvas.setZoom(_7);
this.mouse.setZoom(_7);
},onClick:function(){
this.onZoomIn();
}});
dt.Zoom100=oo.declare(_2,function(_9){
},{type:"dojox.drawing.plugins.tools.Zoom100",onZoom100:function(){
_7=1;
this.canvas.setZoom(_7);
this.mouse.setZoom(_7);
},onClick:function(){
this.onZoom100();
}});
dt.ZoomOut=oo.declare(_2,function(_a){
},{type:"dojox.drawing.plugins.tools.ZoomOut",onZoomOut:function(){
_7/=_4;
_7=Math.max(_7,_6);
this.canvas.setZoom(_7);
this.mouse.setZoom(_7);
},onClick:function(){
this.onZoomOut();
}});
dt.ZoomIn.setup={name:"dojox.drawing.plugins.tools.ZoomIn",tooltip:"Zoom In"};
_3.register(dt.ZoomIn.setup,"plugin");
dt.Zoom100.setup={name:"dojox.drawing.plugins.tools.Zoom100",tooltip:"Zoom to 100%"};
_3.register(dt.Zoom100.setup,"plugin");
dt.ZoomOut.setup={name:"dojox.drawing.plugins.tools.ZoomOut",tooltip:"Zoom In"};
_3.register(dt.ZoomOut.setup,"plugin");
return dt;
});
