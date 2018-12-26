//>>built
define("dojox/sketch/Annotation",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/json","./Anchor","./_Plugin"],function(_1){
_1.declare("dojox.sketch.AnnotationTool",dojox.sketch._Plugin,{onMouseDown:function(e){
this._omd=true;
},onMouseMove:function(e,_2){
if(!this._omd){
return;
}
if(this._cshape){
this._cshape.setShape(_2);
}else{
this._cshape=this.figure.surface.createRect(_2).setStroke({color:"#999",width:1,style:"ShortDot"}).setFill([255,255,255,0.7]);
this._cshape.getEventSource().setAttribute("shape-rendering","crispEdges");
}
},onMouseUp:function(e){
if(!this._omd){
return;
}
this._omd=false;
var f=this.figure;
if(this._cshape){
f.surface.remove(this._cshape);
delete this._cshape;
}
if(!(f._startPoint.x==e.pageX&&f._startPoint.y==e.pageY)){
var _3=10;
if(Math.max(_3,Math.abs(f._absEnd.x-f._start.x),Math.abs(f._absEnd.y-f._start.y))>_3){
this._create(f._start,f._end);
}
}
},_create:function(_4,_5){
var f=this.figure;
var _6=f.nextKey();
var a=new (this.annotation)(f,_6);
a.transform={dx:f._calCol(_4.x/f.zoomFactor),dy:f._calCol(_4.y/f.zoomFactor)};
a.end={x:f._calCol(_5.x/f.zoomFactor),y:f._calCol(_5.y/f.zoomFactor)};
if(a.control){
a.control={x:f._calCol((_5.x/2)/f.zoomFactor),y:f._calCol((_5.y/2)/f.zoomFactor)};
}
f.onBeforeCreateShape(a);
a.initialize();
f.select(a);
f.onCreateShape(a);
f.history.add(dojox.sketch.CommandTypes.Create,a);
}});
dojox.sketch.Annotation=function(_7,id){
this.id=this._key=id;
this.figure=_7;
this.mode=dojox.sketch.Annotation.Modes.View;
this.shape=null;
this.boundingBox=null;
this.hasAnchors=true;
this.anchors={};
this._properties={"stroke":{color:"blue",width:2},"font":{family:"Arial",size:16,weight:"bold"},"fill":"blue","label":""};
if(this.figure){
this.figure.add(this);
}
};
var p=dojox.sketch.Annotation.prototype;
p.constructor=dojox.sketch.Annotation;
p.type=function(){
return "";
};
p.getType=function(){
return dojox.sketch.Annotation;
};
p.onRemove=function(_8){
this.figure.history.add(dojox.sketch.CommandTypes.Delete,this,this.serialize());
};
p.property=function(_9,_a){
var r;
_9=_9.toLowerCase();
if(this._properties[_9]!==undefined){
r=this._properties[_9];
}
if(arguments.length>1){
this._properties[_9]=_a;
if(r!=_a){
this.onPropertyChange(_9,r);
}
}
return r;
};
p.onPropertyChange=function(_b,_c){
};
p.onCreate=function(){
this.figure.history.add(dojox.sketch.CommandTypes.Create,this);
};
p.onDblClick=function(e){
var l=prompt("Set new text:",this.property("label"));
if(l!==false){
this.beginEdit(dojox.sketch.CommandTypes.Modify);
this.property("label",l);
this.draw();
this.endEdit();
}
};
p.initialize=function(){
};
p.destroy=function(){
};
p.draw=function(){
};
p.apply=function(_d){
};
p.serialize=function(){
};
p.getBBox=function(){
};
p.beginEdit=function(_e){
if(!this._type){
this._type=_e||dojox.sketch.CommandTypes.Move;
this._prevState=this.serialize();
}
};
p.endEdit=function(){
if(this._prevState!=this.serialize()){
this.figure.history.add(this._type,this,this._prevState);
}
this._type=this._prevState="";
};
p.calculate={slope:function(p1,p2){
if(!(p1.x-p2.x)){
return 0;
}
return ((p1.y-p2.y)/(p1.x-p2.x));
},dx:function(p1,p2,dy){
var s=this.slope(p1,p2);
if(s==0){
return s;
}
return dy/s;
},dy:function(p1,p2,dx){
return this.slope(p1,p2)*dx;
}};
p.drawBBox=function(){
var r=this.getBBox();
if(!this.boundingBox){
this.boundingBox=this.shape.createRect(r).moveToBack().setStroke({color:"#999",width:1,style:"Dash"}).setFill([238,238,238,0.3]);
this.boundingBox.getEventSource().setAttribute("id",this.id+"-boundingBox");
this.boundingBox.getEventSource().setAttribute("shape-rendering","crispEdges");
this.figure._add(this);
}else{
this.boundingBox.setShape(r);
}
};
p.setBinding=function(pt){
this.transform.dx+=pt.dx;
this.transform.dy+=pt.dy;
this.draw();
};
p.getTextBox=function(_f){
var fp=this.property("font");
var f={fontFamily:fp.family,fontSize:fp.size,fontWeight:fp.weight};
if(_f){
f.fontSize=Math.floor(f.fontSize/_f);
}
return dojox.gfx._base._getTextBox(this.property("label"),f);
};
p.setMode=function(m){
if(this.mode==m){
return;
}
this.mode=m;
var _10="disable";
if(m==dojox.sketch.Annotation.Modes.Edit){
_10="enable";
}
if(_10=="enable"){
this.drawBBox();
this.figure._add(this);
}else{
if(this.boundingBox){
if(this.shape){
this.shape.remove(this.boundingBox);
}
this.boundingBox=null;
}
}
for(var p in this.anchors){
this.anchors[p][_10]();
}
};
p.zoom=function(pct){
pct=pct||this.figure.zoomFactor;
if(this.labelShape){
var f=_1.clone(this.property("font"));
f.size=Math.ceil(f.size/pct)+"px";
this.labelShape.setFont(f);
}
for(var n in this.anchors){
this.anchors[n].zoom(pct);
}
if(dojox.gfx.renderer=="vml"){
pct=1;
}
if(this.pathShape){
var s=_1.clone(this.property("stroke"));
s.width=pct>1?s.width:Math.ceil(s.width/pct)+"px";
this.pathShape.setStroke(s);
}
};
p.writeCommonAttrs=function(){
return "id=\""+this.id+"\" dojoxsketch:type=\""+this.type()+"\""+" transform=\"translate("+this.transform.dx+","+this.transform.dy+")\""+(this.data?(" ><![CDATA[data:"+_1.toJson(this.data)+"]]"):"");
};
p.readCommonAttrs=function(obj){
var i=0,cs=obj.childNodes,c;
while((c=cs[i++])){
if(c.nodeType==4){
if(c.nodeValue.substr(0,11)=="properties:"){
this._properties=_1.fromJson(c.nodeValue.substr(11));
}else{
if(c.nodeValue.substr(0,5)=="data:"){
this.data=_1.fromJson(c.nodeValue.substr(5));
}else{
console.error("unknown CDATA node in node ",obj);
}
}
}
}
if(obj.getAttribute("transform")){
var t=obj.getAttribute("transform").replace("translate(","");
var pt=t.split(",");
this.transform.dx=parseFloat(pt[0],10);
this.transform.dy=parseFloat(pt[1],10);
}
};
dojox.sketch.Annotation.Modes={View:0,Edit:1};
dojox.sketch.Annotation.register=function(_11,_12){
var cls=dojox.sketch[_11+"Annotation"];
dojox.sketch.registerTool(_11,function(p){
_1.mixin(p,{shape:_11,annotation:cls});
return new (_12||dojox.sketch.AnnotationTool)(p);
});
};
return dojox.sketch.Annotation;
});
