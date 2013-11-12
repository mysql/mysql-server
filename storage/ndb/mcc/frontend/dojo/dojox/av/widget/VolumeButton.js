//>>built
define("dojox/av/widget/VolumeButton",["dojo","dijit","dijit/_Widget","dijit/_TemplatedMixin","dijit/form/Button"],function(_1,_2){
_1.declare("dojox.av.widget.VolumeButton",[_2._Widget,_2._TemplatedMixin],{templateString:_1.cache("dojox.av.widget","resources/VolumeButton.html"),postCreate:function(){
this.handleWidth=_1.marginBox(this.handle).w;
this.width=_1.marginBox(this.volumeSlider).w;
this.slotWidth=100;
_1.setSelectable(this.handle,false);
this.volumeSlider=this.domNode.removeChild(this.volumeSlider);
},setMedia:function(_3){
this.media=_3;
this.updateIcon();
},updateIcon:function(_4){
_4=(_4===undefined)?this.media.volume():_4;
if(_4===0){
_1.attr(this.domNode,"class","Volume mute");
}else{
if(_4<0.334){
_1.attr(this.domNode,"class","Volume low");
}else{
if(_4<0.667){
_1.attr(this.domNode,"class","Volume med");
}else{
_1.attr(this.domNode,"class","Volume high");
}
}
}
},onShowVolume:function(_5){
if(this.showing==undefined){
_1.body().appendChild(this.volumeSlider);
this.showing=false;
}
if(!this.showing){
var _6=2;
var _7=7;
var _8=this.media.volume();
var _9=this._getVolumeDim();
var _a=this._getHandleDim();
this.x=_9.x-this.width;
_1.style(this.volumeSlider,"display","");
_1.style(this.volumeSlider,"top",_9.y+"px");
_1.style(this.volumeSlider,"left",(this.x)+"px");
var x=(this.slotWidth*_8);
_1.style(this.handle,"top",(_6+(_a.w/2))+"px");
_1.style(this.handle,"left",(x+_7+(_a.h/2))+"px");
this.showing=true;
this.clickOff=_1.connect(_1.doc,"onmousedown",this,"onDocClick");
}else{
this.onHideVolume();
}
},onDocClick:function(_b){
if(!_1.isDescendant(_b.target,this.domNode)&&!_1.isDescendant(_b.target,this.volumeSlider)){
this.onHideVolume();
}
},onHideVolume:function(){
this.endDrag();
_1.style(this.volumeSlider,"display","none");
this.showing=false;
},onDrag:function(_c){
var _d=this.handleWidth/2;
var _e=_d+this.slotWidth;
var x=_c.clientX-this.x;
if(x<_d){
x=_d;
}
if(x>_e){
x=_e;
}
_1.style(this.handle,"left",(x)+"px");
var p=(x-_d)/(_e-_d);
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
},onResize:function(_f){
this.onHideVolume();
this._domCoords=null;
}});
return dojox.av.widget.VolumeButton;
});
