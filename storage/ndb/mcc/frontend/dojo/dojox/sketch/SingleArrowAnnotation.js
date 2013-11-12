//>>built
define("dojox/sketch/SingleArrowAnnotation",["dojo/_base/kernel","dojo/_base/lang","./Annotation","./Anchor"],function(_1){
_1.getObject("sketch",true,dojox);
var ta=dojox.sketch;
ta.SingleArrowAnnotation=function(_2,id){
ta.Annotation.call(this,_2,id);
this.transform={dx:0,dy:0};
this.start={x:0,y:0};
this.control={x:100,y:-50};
this.end={x:200,y:0};
this.textPosition={x:0,y:0};
this.textOffset=4;
this.textYOffset=10;
this.rotation=0;
this.pathShape=null;
this.arrowhead=null;
this.arrowheadGroup=null;
this.labelShape=null;
this.anchors.start=new ta.Anchor(this,"start");
this.anchors.control=new ta.Anchor(this,"control");
this.anchors.end=new ta.Anchor(this,"end");
};
ta.SingleArrowAnnotation.prototype=new ta.Annotation;
var p=ta.SingleArrowAnnotation.prototype;
p.constructor=ta.SingleArrowAnnotation;
p.type=function(){
return "SingleArrow";
};
p.getType=function(){
return ta.SingleArrowAnnotation;
};
p._rot=function(){
var _3=this.control.y-this.start.y;
var _4=this.control.x-this.start.x;
this.rotation=Math.atan2(_3,_4);
};
p._pos=function(){
var _5=this.textOffset,x=0,y=0;
var _6=this.calculate.slope(this.control,this.end);
this.textAlign="middle";
if(Math.abs(_6)>=1){
x=this.end.x+this.calculate.dx(this.control,this.end,_5);
if(this.control.y>this.end.y){
y=this.end.y-_5;
}else{
y=this.end.y+_5+this.textYOffset;
}
}else{
if(_6==0){
x=this.end.x+_5;
y=this.end.y+this.textYOffset;
}else{
if(this.start.x>this.end.x){
x=this.end.x-_5;
this.textAlign="end";
}else{
x=this.end.x+_5;
this.textAlign="start";
}
if(this.start.y<this.end.y){
y=this.end.y+this.calculate.dy(this.control,this.end,_5)+this.textYOffset;
}else{
y=this.end.y+this.calculate.dy(this.control,this.end,-_5);
}
}
}
this.textPosition={x:x,y:y};
};
p.apply=function(_7){
if(!_7){
return;
}
if(_7.documentElement){
_7=_7.documentElement;
}
this.readCommonAttrs(_7);
for(var i=0;i<_7.childNodes.length;i++){
var c=_7.childNodes[i];
if(c.localName=="text"){
this.property("label",c.childNodes.length?c.childNodes[0].nodeValue:"");
}else{
if(c.localName=="path"){
var d=c.getAttribute("d").split(" ");
var s=d[0].split(",");
this.start.x=parseFloat(s[0].substr(1),10);
this.start.y=parseFloat(s[1],10);
s=d[1].split(",");
this.control.x=parseFloat(s[0].substr(1),10);
this.control.y=parseFloat(s[1],10);
s=d[2].split(",");
this.end.x=parseFloat(s[0],10);
this.end.y=parseFloat(s[1],10);
var _8=this.property("stroke");
var _9=c.getAttribute("style");
var m=_9.match(/stroke:([^;]+);/);
if(m){
_8.color=m[1];
this.property("fill",m[1]);
}
m=_9.match(/stroke-width:([^;]+);/);
if(m){
_8.width=m[1];
}
this.property("stroke",_8);
}
}
}
};
p.initialize=function(_a){
var _b=(ta.Annotation.labelFont)?ta.Annotation.labelFont:{family:"Times",size:"16px"};
this.apply(_a);
this._rot();
this._pos();
var _c=this.rotation;
var _d=dojox.gfx.matrix.rotate(_c);
this.shape=this.figure.group.createGroup();
this.shape.getEventSource().setAttribute("id",this.id);
this.pathShape=this.shape.createPath("M"+this.start.x+","+this.start.y+" Q"+this.control.x+","+this.control.y+" "+this.end.x+","+this.end.y+" l0,0");
this.arrowheadGroup=this.shape.createGroup();
this.arrowhead=this.arrowheadGroup.createPath();
this.labelShape=this.shape.createText({x:this.textPosition.x,y:this.textPosition.y,text:this.property("label"),align:this.textAlign});
this.labelShape.getEventSource().setAttribute("id",this.id+"-labelShape");
this.draw();
};
p.destroy=function(){
if(!this.shape){
return;
}
this.arrowheadGroup.remove(this.arrowhead);
this.shape.remove(this.arrowheadGroup);
this.shape.remove(this.pathShape);
this.shape.remove(this.labelShape);
this.figure.group.remove(this.shape);
this.shape=this.pathShape=this.labelShape=this.arrowheadGroup=this.arrowhead=null;
};
p.draw=function(_e){
this.apply(_e);
this._rot();
this._pos();
var _f=this.rotation;
var _10=dojox.gfx.matrix.rotate(_f);
this.shape.setTransform(this.transform);
this.pathShape.setShape("M"+this.start.x+","+this.start.y+" Q"+this.control.x+","+this.control.y+" "+this.end.x+","+this.end.y+" l0,0");
this.arrowheadGroup.setTransform({dx:this.start.x,dy:this.start.y}).applyTransform(_10);
this.arrowhead.setFill(this.property("fill"));
this.labelShape.setShape({x:this.textPosition.x,y:this.textPosition.y,text:this.property("label"),align:this.textAlign}).setFill(this.property("fill"));
this.zoom();
};
p.zoom=function(pct){
if(this.arrowhead){
pct=pct||this.figure.zoomFactor;
ta.Annotation.prototype.zoom.call(this,pct);
if(this._curPct!==pct){
this._curPct=pct;
var l=pct>1?20:Math.floor(20/pct),w=pct>1?5:Math.floor(5/pct),h=pct>1?3:Math.floor(3/pct);
this.arrowhead.setShape("M0,0 l"+l+",-"+w+" -"+h+","+w+" "+h+","+w+" Z");
}
}
};
p.getBBox=function(){
var x=Math.min(this.start.x,this.control.x,this.end.x);
var y=Math.min(this.start.y,this.control.y,this.end.y);
var w=Math.max(this.start.x,this.control.x,this.end.x)-x;
var h=Math.max(this.start.y,this.control.y,this.end.y)-y;
return {x:x,y:y,width:w,height:h};
};
p.serialize=function(){
var s=this.property("stroke");
var r=this.rotation*(180/Math.PI);
r=Math.round(r*Math.pow(10,4))/Math.pow(10,4);
return "<g "+this.writeCommonAttrs()+">"+"<path style=\"stroke:"+s.color+";stroke-width:"+s.width+";fill:none;\" d=\""+"M"+this.start.x+","+this.start.y+" "+"Q"+this.control.x+","+this.control.y+" "+this.end.x+","+this.end.y+"\" />"+"<g transform=\"translate("+this.start.x+","+this.start.y+") "+"rotate("+r+")\">"+"<path style=\"fill:"+s.color+";\" d=\"M0,0 l20,-5, -3,5, 3,5 Z\" />"+"</g>"+"<text style=\"fill:"+s.color+";text-anchor:"+this.textAlign+"\" font-weight=\"bold\" "+"x=\""+this.textPosition.x+"\" "+"y=\""+this.textPosition.y+"\">"+this.property("label")+"</text>"+"</g>";
};
ta.Annotation.register("SingleArrow");
return dojox.sketch.SingleArrowAnnotation;
});
