//>>built
define("dojox/av/widget/ProgressSlider",["dojo","dijit","dijit/_Widget","dijit/_TemplatedMixin"],function(_1,_2,_3,_4){
return _1.declare("dojox.av.widget.ProgressSlider",[_3,_4],{templateString:_1.cache("dojox.av.widget","resources/ProgressSlider.html"),postCreate:function(){
this.seeking=false;
this.handleWidth=_1.marginBox(this.handle).w;
var _5=_1.coords(this.domNode);
this.finalWidth=_5.w;
this.width=_5.w-this.handleWidth;
this.x=_5.x;
_1.setSelectable(this.domNode,false);
_1.setSelectable(this.handle,false);
},setMedia:function(_6,_7){
this.playerWidget=_7;
this.media=_6;
_1.connect(this.media,"onMetaData",this,function(_8){
if(_8&&_8.duration){
this.duration=_8.duration;
}
});
_1.connect(this.media,"onEnd",this,function(){
_1.disconnect(this.posCon);
this.setHandle(this.duration);
});
_1.connect(this.media,"onStart",this,function(){
this.posCon=_1.connect(this.media,"onPosition",this,"setHandle");
});
_1.connect(this.media,"onDownloaded",this,function(_9){
this.setLoadedPosition(_9*0.01);
this.width=this.finalWidth*0.01*_9;
});
},onDrag:function(_a){
var x=_a.clientX-this.x;
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
},setHandle:function(_b){
if(!this.seeking){
var w=this.width-this.handleWidth;
var p=_b/this.duration;
var x=p*w;
_1.style(this.handle,"marginLeft",x+"px");
_1.style(this.progressPosition,"width",x+"px");
}
},setLoadedPosition:function(_c){
_1.style(this.progressLoaded,"width",(this.finalWidth*_c)+"px");
},handleOver:function(){
_1.addClass(this.handle,"over");
},handleOut:function(){
if(!this.seeking){
_1.removeClass(this.handle,"over");
}
},onResize:function(_d){
var _e=_1.coords(this.domNode);
this.finalWidth=_e.w;
}});
});
