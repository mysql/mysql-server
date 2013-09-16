//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/mobile/app/_Widget,dojo/fx/easing"],function(_1,_2,_3){
_2.provide("dojox.mobile.app.ImageView");
_2.experimental("dojox.mobile.app.ImageView");
_2.require("dojox.mobile.app._Widget");
_2.require("dojo.fx.easing");
_2.declare("dojox.mobile.app.ImageView",_3.mobile.app._Widget,{zoom:1,zoomCenterX:0,zoomCenterY:0,maxZoom:5,autoZoomLevel:3,disableAutoZoom:false,disableSwipe:false,autoZoomEvent:null,_leftImg:null,_centerImg:null,_rightImg:null,_leftSmallImg:null,_centerSmallImg:null,_rightSmallImg:null,constructor:function(){
this.panX=0;
this.panY=0;
this.handleLoad=_2.hitch(this,this.handleLoad);
this._updateAnimatedZoom=_2.hitch(this,this._updateAnimatedZoom);
this._updateAnimatedPan=_2.hitch(this,this._updateAnimatedPan);
this._onAnimPanEnd=_2.hitch(this,this._onAnimPanEnd);
},buildRendering:function(){
this.inherited(arguments);
this.canvas=_2.create("canvas",{},this.domNode);
_2.addClass(this.domNode,"mblImageView");
},postCreate:function(){
this.inherited(arguments);
this.size=_2.marginBox(this.domNode);
_2.style(this.canvas,{width:this.size.w+"px",height:this.size.h+"px"});
this.canvas.height=this.size.h;
this.canvas.width=this.size.w;
var _4=this;
this.connect(this.domNode,"onmousedown",function(_5){
if(_4.isAnimating()){
return;
}
if(_4.panX){
_4.handleDragEnd();
}
_4.downX=_5.targetTouches?_5.targetTouches[0].clientX:_5.clientX;
_4.downY=_5.targetTouches?_5.targetTouches[0].clientY:_5.clientY;
});
this.connect(this.domNode,"onmousemove",function(_6){
if(_4.isAnimating()){
return;
}
if((!_4.downX&&_4.downX!==0)||(!_4.downY&&_4.downY!==0)){
return;
}
if((!_4.disableSwipe&&_4.zoom==1)||(!_4.disableAutoZoom&&_4.zoom!=1)){
var x=_6.targetTouches?_6.targetTouches[0].clientX:_6.pageX;
var y=_6.targetTouches?_6.targetTouches[0].clientY:_6.pageY;
_4.panX=x-_4.downX;
_4.panY=y-_4.downY;
if(_4.zoom==1){
if(Math.abs(_4.panX)>10){
_4.render();
}
}else{
if(Math.abs(_4.panX)>10||Math.abs(_4.panY)>10){
_4.render();
}
}
}
});
this.connect(this.domNode,"onmouseout",function(_7){
if(!_4.isAnimating()&&_4.panX){
_4.handleDragEnd();
}
});
this.connect(this.domNode,"onmouseover",function(_8){
_4.downX=_4.downY=null;
});
this.connect(this.domNode,"onclick",function(_9){
if(_4.isAnimating()){
return;
}
if(_4.downX==null||_4.downY==null){
return;
}
var x=(_9.targetTouches?_9.targetTouches[0].clientX:_9.pageX);
var y=(_9.targetTouches?_9.targetTouches[0].clientY:_9.pageY);
if(Math.abs(_4.panX)>14||Math.abs(_4.panY)>14){
_4.downX=_4.downY=null;
_4.handleDragEnd();
return;
}
_4.downX=_4.downY=null;
if(!_4.disableAutoZoom){
if(!_4._centerImg||!_4._centerImg._loaded){
return;
}
if(_4.zoom!=1){
_4.set("animatedZoom",1);
return;
}
var _a=_2._abs(_4.domNode);
var _b=_4.size.w/_4._centerImg.width;
var _c=_4.size.h/_4._centerImg.height;
_4.zoomTo(((x-_a.x)/_b)-_4.panX,((y-_a.y)/_c)-_4.panY,_4.autoZoomLevel);
}
});
_2.connect(this.domNode,"flick",this,"handleFlick");
},isAnimating:function(){
return this._anim&&this._anim.status()=="playing";
},handleDragEnd:function(){
this.downX=this.downY=null;
if(this.zoom==1){
if(!this.panX){
return;
}
var _d=(this._leftImg&&this._leftImg._loaded)||(this._leftSmallImg&&this._leftSmallImg._loaded);
var _e=(this._rightImg&&this._rightImg._loaded)||(this._rightSmallImg&&this._rightSmallImg._loaded);
var _f=!(Math.abs(this.panX)<this._centerImg._baseWidth/2)&&((this.panX>0&&_d?1:0)||(this.panX<0&&_e?1:0));
if(!_f){
this._animPanTo(0,_2.fx.easing.expoOut,700);
}else{
this.moveTo(this.panX);
}
}else{
if(!this.panX&&!this.panY){
return;
}
this.zoomCenterX-=(this.panX/this.zoom);
this.zoomCenterY-=(this.panY/this.zoom);
this.panX=this.panY=0;
}
},handleFlick:function(_10){
if(this.zoom==1&&_10.duration<500){
if(_10.direction=="ltr"){
this.moveTo(1);
}else{
if(_10.direction=="rtl"){
this.moveTo(-1);
}
}
this.downX=this.downY=null;
}
},moveTo:function(_11){
_11=_11>0?1:-1;
var _12;
if(_11<1){
if(this._rightImg&&this._rightImg._loaded){
_12=this._rightImg;
}else{
if(this._rightSmallImg&&this._rightSmallImg._loaded){
_12=this._rightSmallImg;
}
}
}else{
if(this._leftImg&&this._leftImg._loaded){
_12=this._leftImg;
}else{
if(this._leftSmallImg&&this._leftSmallImg._loaded){
_12=this._leftSmallImg;
}
}
}
this._moveDir=_11;
var _13=this;
if(_12&&_12._loaded){
this._animPanTo(this.size.w*_11,null,500,function(){
_13.panX=0;
_13.panY=0;
if(_11<0){
_13._switchImage("left","right");
}else{
_13._switchImage("right","left");
}
_13.render();
_13.onChange(_11*-1);
});
}else{
this._animPanTo(0,_2.fx.easing.expoOut,700);
}
},_switchImage:function(_14,_15){
var _16="_"+_14+"SmallImg";
var _17="_"+_14+"Img";
var _18="_"+_15+"SmallImg";
var _19="_"+_15+"Img";
this[_17]=this._centerImg;
this[_16]=this._centerSmallImg;
this[_17]._type=_14;
if(this[_16]){
this[_16]._type=_14;
}
this._centerImg=this[_19];
this._centerSmallImg=this[_18];
this._centerImg._type="center";
if(this._centerSmallImg){
this._centerSmallImg._type="center";
}
this[_19]=this[_18]=null;
},_animPanTo:function(to,_1a,_1b,_1c){
this._animCallback=_1c;
this._anim=new _2.Animation({curve:[this.panX,to],onAnimate:this._updateAnimatedPan,duration:_1b||500,easing:_1a,onEnd:this._onAnimPanEnd});
this._anim.play();
return this._anim;
},onChange:function(_1d){
},_updateAnimatedPan:function(_1e){
this.panX=_1e;
this.render();
},_onAnimPanEnd:function(){
this.panX=this.panY=0;
if(this._animCallback){
this._animCallback();
}
},zoomTo:function(_1f,_20,_21){
this.set("zoomCenterX",_1f);
this.set("zoomCenterY",_20);
this.set("animatedZoom",_21);
},render:function(){
var cxt=this.canvas.getContext("2d");
cxt.clearRect(0,0,this.canvas.width,this.canvas.height);
this._renderImg(this._centerSmallImg,this._centerImg,this.zoom==1?(this.panX<0?1:this.panX>0?-1:0):0);
if(this.zoom==1&&this.panX!=0){
if(this.panX>0){
this._renderImg(this._leftSmallImg,this._leftImg,1);
}else{
this._renderImg(this._rightSmallImg,this._rightImg,-1);
}
}
},_renderImg:function(_22,_23,_24){
var img=(_23&&_23._loaded)?_23:_22;
if(!img||!img._loaded){
return;
}
var cxt=this.canvas.getContext("2d");
var _25=img._baseWidth;
var _26=img._baseHeight;
var _27=_25*this.zoom;
var _28=_26*this.zoom;
var _29=Math.min(this.size.w,_27);
var _2a=Math.min(this.size.h,_28);
var _2b=this.dispWidth=img.width*(_29/_27);
var _2c=this.dispHeight=img.height*(_2a/_28);
var _2d=this.zoomCenterX-(this.panX/this.zoom);
var _2e=this.zoomCenterY-(this.panY/this.zoom);
var _2f=Math.floor(Math.max(_2b/2,Math.min(img.width-_2b/2,_2d)));
var _30=Math.floor(Math.max(_2c/2,Math.min(img.height-_2c/2,_2e)));
var _31=Math.max(0,Math.round((img.width-_2b)/2+(_2f-img._centerX)));
var _32=Math.max(0,Math.round((img.height-_2c)/2+(_30-img._centerY)));
var _33=Math.round(Math.max(0,this.canvas.width-_29)/2);
var _34=Math.round(Math.max(0,this.canvas.height-_2a)/2);
var _35=_29;
var _36=_2b;
if(this.zoom==1&&_24&&this.panX){
if(this.panX<0){
if(_24>0){
_29-=Math.abs(this.panX);
_33=0;
}else{
if(_24<0){
_29=Math.max(1,Math.abs(this.panX)-5);
_33=this.size.w-_29;
}
}
}else{
if(_24>0){
_29=Math.max(1,Math.abs(this.panX)-5);
_33=0;
}else{
if(_24<0){
_29-=Math.abs(this.panX);
_33=this.size.w-_29;
}
}
}
_2b=Math.max(1,Math.floor(_2b*(_29/_35)));
if(_24>0){
_31=(_31+_36)-(_2b);
}
_31=Math.floor(_31);
}
try{
cxt.drawImage(img,Math.max(0,_31),_32,Math.min(_36,_2b),_2c,_33,_34,Math.min(_35,_29),_2a);
}
catch(e){
}
},_setZoomAttr:function(_37){
this.zoom=Math.min(this.maxZoom,Math.max(1,_37));
if(this.zoom==1&&this._centerImg&&this._centerImg._loaded){
if(!this.isAnimating()){
this.zoomCenterX=this._centerImg.width/2;
this.zoomCenterY=this._centerImg.height/2;
}
this.panX=this.panY=0;
}
this.render();
},_setZoomCenterXAttr:function(_38){
if(_38!=this.zoomCenterX){
if(this._centerImg&&this._centerImg._loaded){
_38=Math.min(this._centerImg.width,_38);
}
this.zoomCenterX=Math.max(0,Math.round(_38));
}
},_setZoomCenterYAttr:function(_39){
if(_39!=this.zoomCenterY){
if(this._centerImg&&this._centerImg._loaded){
_39=Math.min(this._centerImg.height,_39);
}
this.zoomCenterY=Math.max(0,Math.round(_39));
}
},_setZoomCenterAttr:function(_3a){
if(_3a.x!=this.zoomCenterX||_3a.y!=this.zoomCenterY){
this.set("zoomCenterX",_3a.x);
this.set("zoomCenterY",_3a.y);
this.render();
}
},_setAnimatedZoomAttr:function(_3b){
if(this._anim&&this._anim.status()=="playing"){
return;
}
this._anim=new _2.Animation({curve:[this.zoom,_3b],onAnimate:this._updateAnimatedZoom,onEnd:this._onAnimEnd});
this._anim.play();
},_updateAnimatedZoom:function(_3c){
this._setZoomAttr(_3c);
},_setCenterUrlAttr:function(_3d){
this._setImage("center",_3d);
},_setLeftUrlAttr:function(_3e){
this._setImage("left",_3e);
},_setRightUrlAttr:function(_3f){
this._setImage("right",_3f);
},_setImage:function(_40,_41){
var _42=null;
var _43=null;
if(_2.isString(_41)){
_43=_41;
}else{
_43=_41.large;
_42=_41.small;
}
if(this["_"+_40+"Img"]&&this["_"+_40+"Img"]._src==_43){
return;
}
var _44=this["_"+_40+"Img"]=new Image();
_44._type=_40;
_44._loaded=false;
_44._src=_43;
_44._conn=_2.connect(_44,"onload",this.handleLoad);
if(_42){
var _45=this["_"+_40+"SmallImg"]=new Image();
_45._type=_40;
_45._loaded=false;
_45._conn=_2.connect(_45,"onload",this.handleLoad);
_45._isSmall=true;
_45._src=_42;
_45.src=_42;
}
_44.src=_43;
},handleLoad:function(evt){
var img=evt.target;
img._loaded=true;
_2.disconnect(img._conn);
var _46=img._type;
switch(_46){
case "center":
this.zoomCenterX=img.width/2;
this.zoomCenterY=img.height/2;
break;
}
var _47=img.height;
var _48=img.width;
if(_48/this.size.w<_47/this.size.h){
img._baseHeight=this.canvas.height;
img._baseWidth=_48/(_47/this.size.h);
}else{
img._baseWidth=this.canvas.width;
img._baseHeight=_47/(_48/this.size.w);
}
img._centerX=_48/2;
img._centerY=_47/2;
this.render();
this.onLoad(img._type,img._src,img._isSmall);
},onLoad:function(_49,url,_4a){
}});
});
