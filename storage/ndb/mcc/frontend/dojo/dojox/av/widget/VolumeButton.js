//>>built
define("dojox/av/widget/VolumeButton",["dojo","dijit","dijit/_Widget","dijit/_TemplatedMixin","dijit/form/Button"],function(_1,_2,_3,_4,_5){
return _1.declare("dojox.av.widget.VolumeButton",[_3,_4],{templateString:_1.cache("dojox.av.widget","resources/VolumeButton.html"),postCreate:function(){
this.handleWidth=_1.marginBox(this.handle).w;
this.width=_1.marginBox(this.volumeSlider).w;
this.slotWidth=100;
_1.setSelectable(this.handle,false);
this.volumeSlider=this.domNode.removeChild(this.volumeSlider);
},setMedia:function(_6){
this.media=_6;
this.updateIcon();
},updateIcon:function(_7){
_7=(_7===undefined)?this.media.volume():_7;
if(_7===0){
_1.attr(this.domNode,"class","Volume mute");
}else{
if(_7<0.334){
_1.attr(this.domNode,"class","Volume low");
}else{
if(_7<0.667){
_1.attr(this.domNode,"class","Volume med");
}else{
_1.attr(this.domNode,"class","Volume high");
}
}
}
},onShowVolume:function(_8){
if(this.showing==undefined){
_1.body().appendChild(this.volumeSlider);
this.showing=false;
}
if(!this.showing){
var _9=2;
var _a=7;
var _b=this.media.volume();
var _c=this._getVolumeDim();
var _d=this._getHandleDim();
this.x=_c.x-this.width;
_1.style(this.volumeSlider,"display","");
_1.style(this.volumeSlider,"top",_c.y+"px");
_1.style(this.volumeSlider,"left",(this.x)+"px");
var x=(this.slotWidth*_b);
_1.style(this.handle,"top",(_9+(_d.w/2))+"px");
_1.style(this.handle,"left",(x+_a+(_d.h/2))+"px");
this.showing=true;
this.clickOff=_1.connect(_1.doc,"onmousedown",this,"onDocClick");
}else{
this.onHideVolume();
}
},onDocClick:function(_e){
if(!_1.isDescendant(_e.target,this.domNode)&&!_1.isDescendant(_e.target,this.volumeSlider)){
this.onHideVolume();
}
},onHideVolume:function(){
this.endDrag();
_1.style(this.volumeSlider,"display","none");
this.showing=false;
},onDrag:function(_f){
var beg=this.handleWidth/2;
var end=beg+this.slotWidth;
var x=_f.clientX-this.x;
if(x<beg){
x=beg;
}
if(x>end){
x=end;
}
_1.style(this.handle,"left",(x)+"px");
var p=(x-beg)/(end-beg);
this.media.volume(p);
this.updateIcon(p);
},startDrag:function(){
this.isDragging=true;
this.cmove=_1.connect(_1.doc,"mousemove",this,"onDrag");
this.cup=_1.connect(_1.doc,"mouseup",this,"endDrag");
},endDrag:function(){
this.isDragging=false;
if(this.cmove){
_1.disconnect(this.cmove);
}
if(this.cup){
_1.disconnect(this.cup);
}
this.handleOut();
},handleOver:function(){
_1.addClass(this.handle,"over");
},handleOut:function(){
if(!this.isDragging){
_1.removeClass(this.handle,"over");
}
},_getVolumeDim:function(){
if(this._domCoords){
return this._domCoords;
}
this._domCoords=_1.coords(this.domNode);
return this._domCoords;
},_getHandleDim:function(){
if(this._handleCoords){
return this._handleCoords;
}
this._handleCoords=_1.marginBox(this.handle);
return this._handleCoords;
},onResize:function(_10){
this.onHideVolume();
this._domCoords=null;
}});
});
