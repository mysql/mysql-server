//>>built
define("dojox/image/Magnifier",["dojo/_base/declare","dojo/dom-construct","dojo/_base/window","dojox/gfx","dojox/gfx/canvas","./MagnifierLite"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.image.Magnifier",_6,{_createGlass:function(){
this.glassNode=_2.create("div",{style:{height:this.glassSize+"px",width:this.glassSize+"px"},className:"glassNode"},_3.body());
this.surfaceNode=_2.create("div",null,this.glassNode);
_4.switchTo("canvas");
this.surface=_5.createSurface(this.surfaceNode,this.glassSize,this.glassSize);
this.img=this.surface.createImage({src:this.domNode.src,width:this._zoomSize.w,height:this._zoomSize.h});
},_placeGlass:function(e){
var x=e.pageX-2,y=e.pageY-2,_7=this.offset.x+this.offset.w+2,_8=this.offset.y+this.offset.h+2;
if(x<this.offset.x||y<this.offset.y||x>_7||y>_8){
this._hideGlass();
}else{
this.inherited(arguments);
}
},_setImage:function(e){
var _9=(e.pageX-this.offset.x)/this.offset.w,_a=(e.pageY-this.offset.y)/this.offset.h,x=(this._zoomSize.w*_9*-1)+(this.glassSize*_9),y=(this._zoomSize.h*_a*-1)+(this.glassSize*_a);
this.img.setShape({x:x,y:y});
}});
});
