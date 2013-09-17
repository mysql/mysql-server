//>>built
define("dojox/av/widget/ProgressSlider",["dojo","dijit","dijit/_Widget","dijit/_TemplatedMixin"],function(_1,_2){
_1.declare("dojox.av.widget.ProgressSlider",[_2._Widget,_2._TemplatedMixin],{templateString:_1.cache("dojox.av.widget","resources/ProgressSlider.html"),postCreate:function(){
this.seeking=false;
this.handleWidth=_1.marginBox(this.handle).w;
var _3=_1.coords(this.domNode);
this.finalWidth=_3.w;
this.width=_3.w-this.handleWidth;
this.x=_3.x;
_1.setSelectable(this.domNode,false);
_1.setSelectable(this.handle,false);
},setMedia:function(_4,_5){
this.playerWidget=_5;
this.media=_4;
_1.connect(this.media,"onMetaData",this,function(_6){
if(_6&&_6.duration){
this.duration=_6.duration;
}
});
_1.connect(this.media,"onEnd",this,function(){
_1.disconnect(this.posCon);
this.setHandle(this.duration);
});
_1.connect(this.media,"onStart",this,function(){
this.posCon=_1.connect(this.media,"onPosition",this,"setHandle");
});
_1.connect(this.media,"onDownloaded",this,function(_7){
this.setLoadedPosition(_7*0.01);
this.width=this.finalWidth*0.01*_7;
});
},onDrag:function(_8){
var x=_8.clientX-this.x;
if(x<0){
x=0;
}
if(x>this.width-this.handleWidth){
x=this.width-this.handleWidth;
}
var p=x/this.finalWidth;
this.media.seek(this.duration*p);
_1.style(this.handle,"marginLeft",x+"px");
_1.style(this.progressPosition,"width",x+"px");
},startDrag:function(){
_1.setSelectable(this.playerWidget.domNode,false);
this.seeking=true;
this.cmove=_1.connect(_1.doc,"mousemove",this,"onDrag");
this.cup=_1.connect(_1.doc,"mouseup",this,"endDrag");
},endDrag:function(){
_1.setSelectable(this.playerWidget.domNode,true);
this.seeking=false;
if(this.cmove){
_1.disconnect(this.cmove);
}
if(this.cup){
_1.disconnect(this.cup);
}
this.handleOut();
},setHandle:function(_9){
if(!this.seeking){
var w=this.width-this.handleWidth;
var p=_9/this.duration;
var x=p*w;
_1.style(this.handle,"marginLeft",x+"px");
_1.style(this.progressPosition,"width",x+"px");
}
},setLoadedPosition:function(_a){
_1.style(this.progressLoaded,"width",(this.finalWidth*_a)+"px");
},handleOver:function(){
_1.addClass(this.handle,"over");
},handleOut:function(){
if(!this.seeking){
_1.removeClass(this.handle,"over");
}
},onResize:function(_b){
var _c=_1.coords(this.domNode);
this.finalWidth=_c.w;
}});
return dojox.av.widget.ProgressSlider;
});
