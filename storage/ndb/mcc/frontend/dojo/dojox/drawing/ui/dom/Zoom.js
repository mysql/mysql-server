//>>built
define("dojox/drawing/ui/dom/Zoom",["dojo","../../util/oo","../../plugins/_Plugin"],function(_1,oo,_2){
var _3=oo.declare(_2,function(_4){
var _5=_4.node.className;
var _6=_4.node.innerHTML;
this.domNode=_1.create("div",{id:"btnZoom","class":"toolCombo"},_4.node,"replace");
this.makeButton("ZoomIn",this.topClass);
this.makeButton("Zoom100",this.midClass);
this.makeButton("ZoomOut",this.botClass);
},{type:"dojox.drawing.ui.dom.Zoom",zoomInc:0.1,maxZoom:10,minZoom:0.1,zoomFactor:1,baseClass:"drawingButton",topClass:"toolComboTop",midClass:"toolComboMid",botClass:"toolComboBot",makeButton:function(_7,_8){
var _9=_1.create("div",{id:"btn"+_7,"class":this.baseClass+" "+_8,innerHTML:"<div title=\"Zoom In\" class=\"icon icon"+_7+"\"></div>"},this.domNode);
_1.connect(document,"mouseup",function(_a){
_1.stopEvent(_a);
_1.removeClass(_9,"active");
});
_1.connect(_9,"mouseup",this,function(_b){
_1.stopEvent(_b);
_1.removeClass(_9,"active");
this["on"+_7]();
});
_1.connect(_9,"mouseover",function(_c){
_1.stopEvent(_c);
_1.addClass(_9,"hover");
});
_1.connect(_9,"mousedown",this,function(_d){
_1.stopEvent(_d);
_1.addClass(_9,"active");
});
_1.connect(_9,"mouseout",this,function(_e){
_1.stopEvent(_e);
_1.removeClass(_9,"hover");
});
},onZoomIn:function(_f){
this.zoomFactor+=this.zoomInc;
this.zoomFactor=Math.min(this.zoomFactor,this.maxZoom);
this.canvas.setZoom(this.zoomFactor);
this.mouse.setZoom(this.zoomFactor);
},onZoom100:function(evt){
this.zoomFactor=1;
this.canvas.setZoom(this.zoomFactor);
this.mouse.setZoom(this.zoomFactor);
},onZoomOut:function(evt){
this.zoomFactor-=this.zoomInc;
this.zoomFactor=Math.max(this.zoomFactor,this.minZoom);
this.canvas.setZoom(this.zoomFactor);
this.mouse.setZoom(this.zoomFactor);
}});
_1.setObject("dojox.drawing.ui.dom.Zoom",_3);
return _3;
});
