//>>built
define("dojox/sketch/PreexistingAnnotation",["dojo/_base/kernel","dojo/_base/lang","./Annotation","./Anchor"],function(_1){
_1.getObject("sketch",true,dojox);
var ta=dojox.sketch;
ta.PreexistingAnnotation=function(_2,id){
ta.Annotation.call(this,_2,id);
this.transform={dx:0,dy:0};
this.start={x:0,y:0};
this.end={x:200,y:200};
this.radius=8;
this.textPosition={x:196,y:196};
this.textOffset=4;
this.textAlign="end";
this.rectShape=null;
this.labelShape=null;
this.anchors.start=new ta.Anchor(this,"start");
this.anchors.end=new ta.Anchor(this,"end");
};
ta.PreexistingAnnotation.prototype=new ta.Annotation;
var p=ta.PreexistingAnnotation.prototype;
p.constructor=ta.PreexistingAnnotation;
p.type=function(){
return "Preexisting";
};
p.getType=function(){
return ta.PreexistingAnnotation;
};
p._pos=function(){
var x=Math.min(this.start.x,this.end.x);
var y=Math.min(this.start.y,this.end.y);
var w=Math.max(this.start.x,this.end.x);
var h=Math.max(this.start.y,this.end.y);
this.start={x:x,y:y};
this.end={x:w,y:h};
this.textPosition={x:this.end.x-this.textOffset,y:this.end.y-this.textOffset};
};
p.apply=function(_3){
if(!_3){
return;
}
if(_3.documentElement){
_3=_3.documentElement;
}
this.readCommonAttrs(_3);
for(var i=0;i<_3.childNodes.length;i++){
var c=_3.childNodes[i];
if(c.localName=="text"){
this.property("label",c.childNodes.length?c.childNodes[0].nodeValue:"");
}else{
if(c.localName=="rect"){
if(c.getAttribute("x")!==null){
this.start.x=parseFloat(c.getAttribute("x"),10);
}
if(c.getAttribute("width")!==null){
this.end.x=parseFloat(c.getAttribute("width"),10)+parseFloat(c.getAttribute("x"),10);
}
if(c.getAttribute("y")!==null){
this.start.y=parseFloat(c.getAttribute("y"),10);
}
if(c.getAttribute("height")!==null){
this.end.y=parseFloat(c.getAttribute("height"),10)+parseFloat(c.getAttribute("y"),10);
}
if(c.getAttribute("r")!==null){
this.radius=parseFloat(c.getAttribute("r"),10);
}
var _4=this.property("stroke");
var _5=c.getAttribute("style");
var m=_5.match(/stroke:([^;]+);/);
if(m){
_4.color=m[1];
this.property("fill",m[1]);
}
m=_5.match(/stroke-width:([^;]+);/);
if(m){
_4.width=m[1];
}
this.property("stroke",_4);
}
}
}
};
p.initialize=function(_6){
this.apply(_6);
this._pos();
this.shape=this.figure.group.createGroup();
this.shape.getEventSource().setAttribute("id",this.id);
this.rectShape=this.shape.createRect({x:this.start.x,y:this.start.y,width:this.end.x-this.start.x,height:this.end.y-this.start.y,r:this.radius}).setFill([255,255,255,0.1]);
this.rectShape.getEventSource().setAttribute("shape-rendering","crispEdges");
this.labelShape=this.shape.createText({x:this.textPosition.x,y:this.textPosition.y,text:this.property("label"),align:this.textAlign}).setFill(this.property("fill"));
this.labelShape.getEventSource().setAttribute("id",this.id+"-labelShape");
this.draw();
};
p.destroy=function(){
if(!this.shape){
return;
}
this.shape.remove(this.rectShape);
this.shape.remove(this.labelShape);
this.figure.group.remove(this.shape);
this.shape=this.rectShape=this.labelShape=null;
};
p.getBBox=function(){
var x=Math.min(this.start.x,this.end.x);
var y=Math.min(this.start.y,this.end.y);
var w=Math.max(this.start.x,this.end.x)-x;
var h=Math.max(this.start.y,this.end.y)-y;
return {x:x-2,y:y-2,width:w+4,height:h+4};
};
p.draw=function(_7){
this.apply(_7);
this._pos();
this.shape.setTransform(this.transform);
this.rectShape.setShape({x:this.start.x,y:this.start.y,width:this.end.x-this.start.x,height:this.end.y-this.start.y,r:this.radius}).setFill([255,255,255,0.1]);
this.labelShape.setShape({x:this.textPosition.x,y:this.textPosition.y,text:this.property("label")}).setFill(this.property("fill"));
this.zoom();
};
p.zoom=function(_8){
if(this.rectShape){
_8=_8||this.figure.zoomFactor;
ta.Annotation.prototype.zoom.call(this,_8);
_8=dojox.gfx.renderer=="vml"?1:_8;
this.rectShape.setStroke({color:this.property("fill"),width:1/_8});
}
};
p.serialize=function(){
var s=this.property("stroke");
return "<g "+this.writeCommonAttrs()+">"+"<rect style=\"stroke:"+s.color+";stroke-width:1;fill:none;\" "+"x=\""+this.start.x+"\" "+"width=\""+(this.end.x-this.start.x)+"\" "+"y=\""+this.start.y+"\" "+"height=\""+(this.end.y-this.start.y)+"\" "+"rx=\""+this.radius+"\" "+"ry=\""+this.radius+"\" "+" />"+"<text style=\"fill:"+s.color+";text-anchor:"+this.textAlign+"\" font-weight=\"bold\" "+"x=\""+this.textPosition.x+"\" "+"y=\""+this.textPosition.y+"\">"+this.property("label")+"</text>"+"</g>";
};
ta.Annotation.register("Preexisting");
return dojox.sketch.PreexistingAnnotation;
});
