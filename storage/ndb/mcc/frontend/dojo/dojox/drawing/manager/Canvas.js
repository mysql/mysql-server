//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.manager.Canvas");
(function(){
_3.drawing.manager.Canvas=_3.drawing.util.oo.declare(function(_4){
_2.mixin(this,_4);
var _5=_2.contentBox(this.srcRefNode);
this.height=this.parentHeight=_5.h;
this.width=this.parentWidth=_5.w;
this.domNode=_2.create("div",{id:"canvasNode"},this.srcRefNode);
_2.style(this.domNode,{width:this.width,height:"auto"});
_2.setSelectable(this.domNode,false);
this.id=this.id||this.util.uid("surface");
this.gfxSurface=_3.gfx.createSurface(this.domNode,this.width,this.height);
this.gfxSurface.whenLoaded(this,function(){
setTimeout(_2.hitch(this,function(){
this.surfaceReady=true;
if(_2.isIE){
}else{
if(_3.gfx.renderer=="silverlight"){
this.id=this.domNode.firstChild.id;
}else{
}
}
this.underlay=this.gfxSurface.createGroup();
this.surface=this.gfxSurface.createGroup();
this.overlay=this.gfxSurface.createGroup();
this.surface.setTransform({dx:0,dy:0,xx:1,yy:1});
this.gfxSurface.getDimensions=_2.hitch(this.gfxSurface,"getDimensions");
if(_4.callback){
_4.callback(this.domNode);
}
}),500);
});
this._mouseHandle=this.mouse.register(this);
},{zoom:1,useScrollbars:true,baseClass:"drawingCanvas",resize:function(_6,_7){
this.parentWidth=_6;
this.parentHeight=_7;
this.setDimensions(_6,_7);
},setDimensions:function(_8,_9,_a,_b){
var sw=this.getScrollWidth();
this.width=Math.max(_8,this.parentWidth);
this.height=Math.max(_9,this.parentHeight);
if(this.height>this.parentHeight){
this.width-=sw;
}
if(this.width>this.parentWidth){
this.height-=sw;
}
this.mouse.resize(this.width,this.height);
this.gfxSurface.setDimensions(this.width,this.height);
this.domNode.parentNode.scrollTop=_b||0;
this.domNode.parentNode.scrollLeft=_a||0;
if(this.useScrollbars){
_2.style(this.domNode.parentNode,{overflowY:this.height>this.parentHeight?"scroll":"hidden",overflowX:this.width>this.parentWidth?"scroll":"hidden"});
}else{
_2.style(this.domNode.parentNode,{overflowY:"hidden",overflowX:"hidden"});
}
},setZoom:function(_c){
this.zoom=_c;
this.surface.setTransform({xx:_c,yy:_c});
this.setDimensions(this.width*_c,this.height*_c);
},onScroll:function(){
},getScrollOffset:function(){
return {top:this.domNode.parentNode.scrollTop,left:this.domNode.parentNode.scrollLeft};
},getScrollWidth:function(){
var p=_2.create("div");
p.innerHTML="<div style=\"width:50px;height:50px;overflow:hidden;position:absolute;top:0;left:-1000px;\"><div style=\"height:100px;\"></div>";
var _d=p.firstChild;
_2.body().appendChild(_d);
var _e=_2.contentBox(_d).h;
_2.style(_d,"overflow","scroll");
var _f=_e-_2.contentBox(_d).h;
_2.destroy(_d);
this.getScrollWidth=function(){
return _f;
};
return _f;
}});
})();
});
