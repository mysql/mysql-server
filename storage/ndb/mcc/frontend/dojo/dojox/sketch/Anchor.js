//>>built
define("dojox/sketch/Anchor",["dojo/_base/kernel","dojo/_base/lang","../gfx"],function(_1){
_1.getObject("sketch",true,dojox);
dojox.sketch.Anchor=function(an,id,_2){
var _3=this;
var _4=4;
var _5=null;
this.type=function(){
return "Anchor";
};
this.annotation=an;
this.id=id;
this._key="anchor-"+dojox.sketch.Anchor.count++;
this.shape=null;
this.isControl=(_2!=null)?_2:true;
this.beginEdit=function(){
this.annotation.beginEdit(dojox.sketch.CommandTypes.Modify);
};
this.endEdit=function(){
this.annotation.endEdit();
};
this.zoom=function(_6){
if(this.shape){
var rs=Math.floor(_4/_6);
var _7=dojox.gfx.renderer=="vml"?1:1/_6;
this.shape.setShape({x:an[id].x-rs,y:an[id].y-rs,width:rs*2,height:rs*2}).setStroke({color:"black",width:_7});
}
};
this.setBinding=function(pt){
an[id]={x:an[id].x+pt.dx,y:an[id].y+pt.dy};
an.draw();
an.drawBBox();
};
this.setUndo=function(){
an.setUndo();
};
this.enable=function(){
if(!an.shape){
return;
}
an.figure._add(this);
_5={x:an[id].x-_4,y:an[id].y-_4,width:_4*2,height:_4*2};
this.shape=an.shape.createRect(_5).setFill([255,255,255,0.35]);
this.shape.getEventSource().setAttribute("id",_3._key);
this.shape.getEventSource().setAttribute("shape-rendering","crispEdges");
this.zoom(an.figure.zoomFactor);
};
this.disable=function(){
an.figure._remove(this);
if(an.shape){
an.shape.remove(this.shape);
}
this.shape=null;
_5=null;
};
};
dojox.sketch.Anchor.count=0;
return dojox.sketch.Anchor;
});
