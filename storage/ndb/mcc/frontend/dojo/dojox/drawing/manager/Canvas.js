//>>built
define("dojox/drawing/manager/Canvas",["dojo","../util/oo","dojox/gfx"],function(_1,oo,_2){
return oo.declare(function(_3){
_1.mixin(this,_3);
var _4=_1.contentBox(this.srcRefNode);
this.height=this.parentHeight=_3.height||_4.h;
this.width=this.parentWidth=_3.width||_4.w;
this.domNode=_1.create("div",{id:"canvasNode"},this.srcRefNode);
_1.style(this.domNode,{width:this.width,height:"auto"});
_1.setSelectable(this.domNode,false);
this.id=this.id||this.util.uid("surface");
this.gfxSurface=_2.createSurface(this.domNode,this.width,this.height);
this.gfxSurface.whenLoaded(this,function(){
setTimeout(_1.hitch(this,function(){
this.surfaceReady=true;
if(_1.isIE){
}else{
if(_2.renderer=="silverlight"){
this.id=this.domNode.firstChild.id;
}else{
}
}
this.underlay=this.gfxSurface.createGroup();
this.surface=this.gfxSurface.createGroup();
this.overlay=this.gfxSurface.createGroup();
this.surface.setTransform({dx:0,dy:0,xx:1,yy:1});
this.gfxSurface.getDimensions=_1.hitch(this.gfxSurface,"getDimensions");
if(_3.callback){
_3.callback(this.domNode);
}
}),500);
});
this._mouseHandle=this.mouse.register(this);
},{zoom:1,useScrollbars:true,baseClass:"drawingCanvas",resize:function(_5,_6){
this.parentWidth=_5;
this.parentHeight=_6;
this.setDimensions(_5,_6);
},setDimensions:function(_7,_8,_9,_a){
var sw=this.getScrollWidth();
this.width=Math.max(_7,this.parentWidth);
this.height=Math.max(_8,this.parentHeight);
if(this.height>this.parentHeight){
this.width-=sw;
}
if(this.width>this.parentWidth){
this.height-=sw;
}
this.mouse.resize(this.width,this.height);
this.gfxSurface.setDimensions(this.width,this.height);
this.domNode.parentNode.scrollTop=_a||0;
this.domNode.parentNode.scrollLeft=_9||0;
if(this.useScrollbars){
_1.style(this.domNode.parentNode,{overflowY:this.height>this.parentHeight?"scroll":"hidden",overflowX:this.width>this.parentWidth?"scroll":"hidden"});
}else{
_1.style(this.domNode.parentNode,{overflowY:"hidden",overflowX:"hidden"});
}
},setZoom:function(_b){
this.zoom=_b;
this.surface.setTransform({xx:_b,yy:_b});
this.setDimensions(this.width*_b,this.height*_b);
},onScroll:function(){
},getScrollOffset:function(){
return {top:this.domNode.parentNode.scrollTop,left:this.domNode.parentNode.scrollLeft};
},getScrollWidth:function(){
var p=_1.create("div");
p.innerHTML="<div style=\"width:50px;height:50px;overflow:hidden;position:absolute;top:0;left:-1000px;\"><div style=\"height:100px;\"></div>";
var _c=p.firstChild;
_1.body().appendChild(_c);
var _d=_1.contentBox(_c).h;
_1.style(_c,"overflow","scroll");
var _e=_d-_1.contentBox(_c).h;
_1.destroy(_c);
this.getScrollWidth=function(){
return _e;
};
return _e;
}});
});
