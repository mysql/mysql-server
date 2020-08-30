//>>built
define("dojox/image/MagnifierLite",["dojo/_base/kernel","dojo/_base/declare","dijit/_Widget","dojo/dom-construct","dojo/dom-style","dojo/dom-geometry","dojo/_base/window","dojo/_base/lang"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.experimental("dojox.image.MagnifierLite");
return _2("dojox.image.MagnifierLite",_3,{glassSize:125,scale:6,postCreate:function(){
this.inherited(arguments);
this._adjustScale();
this._createGlass();
this.connect(this.domNode,"onmouseenter","_showGlass");
this.connect(this.glassNode,"onmousemove","_placeGlass");
this.connect(this.img,"onmouseout","_hideGlass");
this.connect(_7,"onresize","_adjustScale");
},_createGlass:function(){
var _9=this.glassNode=_4.create("div",{style:{height:this.glassSize+"px",width:this.glassSize+"px"},className:"glassNode"},_7.body());
this.surfaceNode=_9.appendChild(_4.create("div"));
this.img=_4.place(_8.clone(this.domNode),_9);
_5.set(this.img,{position:"relative",top:0,left:0,width:this._zoomSize.w+"px",height:this._zoomSize.h+"px"});
},_adjustScale:function(){
this.offset=_6.position(this.domNode,true);
this._imageSize={w:this.offset.w,h:this.offset.h};
this._zoomSize={w:this._imageSize.w*this.scale,h:this._imageSize.h*this.scale};
},_showGlass:function(e){
this._placeGlass(e);
_5.set(this.glassNode,{visibility:"visible",display:""});
},_hideGlass:function(e){
_5.set(this.glassNode,{visibility:"hidden",display:"none"});
},_placeGlass:function(e){
this._setImage(e);
var _a=Math.floor(this.glassSize/2);
_5.set(this.glassNode,{top:Math.floor(e.pageY-_a)+"px",left:Math.floor(e.pageX-_a)+"px"});
},_setImage:function(e){
var _b=(e.pageX-this.offset.x)/this.offset.w,_c=(e.pageY-this.offset.y)/this.offset.h,x=(this._zoomSize.w*_b*-1)+(this.glassSize*_b),y=(this._zoomSize.h*_c*-1)+(this.glassSize*_c);
_5.set(this.img,{top:y+"px",left:x+"px"});
},destroy:function(_d){
_4.destroy(this.glassNode);
this.inherited(arguments);
}});
});
