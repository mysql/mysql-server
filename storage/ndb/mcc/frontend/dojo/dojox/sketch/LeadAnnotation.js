//>>built
define("dojox/sketch/LeadAnnotation",["dojo/_base/kernel","dojo/_base/lang","./Annotation","./Anchor"],function(_1){
_1.getObject("sketch",true,dojox);
var ta=dojox.sketch;
ta.LeadAnnotation=function(_2,id){
ta.Annotation.call(this,_2,id);
this.transform={dx:0,dy:0};
this.start={x:0,y:0};
this.control={x:100,y:-50};
this.end={x:200,y:0};
this.textPosition={x:0,y:0};
this.textOffset=4;
this.textYOffset=10;
this.pathShape=null;
this.labelShape=null;
this.anchors.start=new ta.Anchor(this,"start");
this.anchors.control=new ta.Anchor(this,"control");
this.anchors.end=new ta.Anchor(this,"end");
};
ta.LeadAnnotation.prototype=new ta.Annotation;
var p=ta.LeadAnnotation.prototype;
p.constructor=ta.LeadAnnotation;
p.type=function(){
return "Lead";
};
p.getType=function(){
return ta.LeadAnnotation;
};
p._pos=function(){
var _3=this.textOffset,x=0,y=0;
var _4=this.calculate.slope(this.control,this.end);
this.textAlign="middle";
if(Math.abs(_4)>=1){
x=this.end.x+this.calculate.dx(this.control,this.end,_3);
if(this.control.y>this.end.y){
y=this.end.y-_3;
}else{
y=this.end.y+_3+this.textYOffset;
}
}else{
if(_4==0){
x=this.end.x+_3;
y=this.end.y+this.textYOffset;
}else{
if(this.start.x>this.end.x){
x=this.end.x-_3;
this.textAlign="end";
}else{
x=this.end.x+_3;
this.textAlign="start";
}
if(this.start.y<this.end.y){
y=this.end.y+this.calculate.dy(this.control,this.end,_3)+this.textYOffset;
}else{
y=this.end.y+this.calculate.dy(this.control,this.end,-_3);
}
}
}
this.textPosition={x:x,y:y};
};
p.apply=function(_5){
if(!_5){
return;
}
if(_5.documentElement){
_5=_5.documentElement;
}
this.readCommonAttrs(_5);
for(var i=0;i<_5.childNodes.length;i++){
var c=_5.childNodes[i];
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
var _6=this.property("stroke");
var _7=c.getAttribute("style");
var m=_7.match(/stroke:([^;]+);/);
if(m){
_6.color=m[1];
this.property("fill",m[1]);
}
m=_7.match(/stroke-width:([^;]+);/);
if(m){
_6.width=m[1];
}
this.property("stroke",_6);
}
}
}
};
p.initialize=function(_8){
this.apply(_8);
this._pos();
this.shape=this.figure.group.createGroup();
this.shape.getEventSource().setAttribute("id",this.id);
this.pathShape=this.shape.createPath("M"+this.start.x+","+this.start.y+" Q"+this.control.x+","+this.control.y+" "+this.end.x+","+this.end.y+" l0,0");
this.labelShape=this.shape.createText({x:this.textPosition.x,y:this.textPosition.y,text:this.property("label"),align:this.textAlign});
this.labelShape.getEventSource().setAttribute("id",this.id+"-labelShape");
this.draw();
};
p.destroy=function(){
if(!this.shape){
return;
}
this.shape.remove(this.pathShape);
this.shape.remove(this.labelShape);
this.figure.group.remove(this.shape);
this.shape=this.pathShape=this.labelShape=null;
};
p.getBBox=function(){
var x=Math.min(this.start.x,this.control.x,this.end.x);
var y=Math.min(this.start.y,this.control.y,this.end.y);
var w=Math.max(this.start.x,this.control.x,this.end.x)-x;
var h=Math.max(this.start.y,this.control.y,this.end.y)-y;
return {x:x,y:y,width:w,height:h};
};
p.draw=function(_9){
this.apply(_9);
this._pos();
this.shape.setTransform(this.transform);
this.pathShape.setShape("M"+this.start.x+","+this.start.y+" Q"+this.control.x+","+this.control.y+" "+this.end.x+","+this.end.y+" l0,0");
this.labelShape.setShape({x:this.textPosition.x,y:this.textPosition.y,text:this.property("label")}).setFill(this.property("fill"));
this.zoom();
};
p.serialize=function(){
var _a=this.property("stroke");
return "<g "+this.writeCommonAttrs()+">"+"<path style=\"stroke:"+_a.color+";stroke-width:"+_a.width+";fill:none;\" d=\""+"M"+this.start.x+","+this.start.y+" "+"Q"+this.control.x+","+this.control.y+" "+this.end.x+","+this.end.y+"\" />"+"<text style=\"fill:"+_a.color+";text-anchor:"+this.textAlign+"\" font-weight=\"bold\" "+"x=\""+this.textPosition.x+"\" "+"y=\""+this.textPosition.y+"\">"+this.property("label")+"</text>"+"</g>";
};
ta.Annotation.register("Lead");
return dojox.sketch.LeadAnnotation;
});
