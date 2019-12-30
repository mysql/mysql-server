//>>built
define("dojox/gfx/canvasWithEvents",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/Color","dojo/dom","dojo/dom-geometry","./_base","./canvas","./shape","./matrix"],function(_1,_2,_3,_4,_5,_6,g,_7,_8,m){
var _9=g.canvasWithEvents={};
_9.Shape=_2("dojox.gfx.canvasWithEvents.Shape",_7.Shape,{_testInputs:function(_a,_b){
if(this.clip||(!this.canvasFill&&this.strokeStyle)){
this._hitTestPixel(_a,_b);
}else{
this._renderShape(_a);
var _c=_b.length,t=this.getTransform();
for(var i=0;i<_b.length;++i){
var _d=_b[i];
if(_d.target){
continue;
}
var x=_d.x,y=_d.y;
var p=t?m.multiplyPoint(m.invert(t),x,y):{x:x,y:y};
_d.target=this._hitTestGeometry(_a,p.x,p.y);
}
}
},_hitTestPixel:function(_e,_f){
for(var i=0;i<_f.length;++i){
var _10=_f[i];
if(_10.target){
continue;
}
var x=_10.x,y=_10.y;
_e.clearRect(0,0,1,1);
_e.save();
_e.translate(-x,-y);
this._render(_e,true);
_10.target=_e.getImageData(0,0,1,1).data[0]?this:null;
_e.restore();
}
},_hitTestGeometry:function(ctx,x,y){
return ctx.isPointInPath(x,y)?this:null;
},_renderFill:function(ctx,_11){
if(ctx.pickingMode){
if("canvasFill" in this&&_11){
ctx.fill();
}
return;
}
this.inherited(arguments);
},_renderStroke:function(ctx,_12){
if(this.strokeStyle&&ctx.pickingMode){
var c=this.strokeStyle.color;
try{
this.strokeStyle.color=new _4(ctx.strokeStyle);
this.inherited(arguments);
}
finally{
this.strokeStyle.color=c;
}
}else{
this.inherited(arguments);
}
},getEventSource:function(){
return this.surface.getEventSource();
},connect:function(_13,_14,_15){
if(_13.indexOf("mouse")===0){
_13="on"+_13;
}else{
if(_13.indexOf("ontouch")===0){
_13=_13.slice(2);
}
}
this.surface._setupEvents(_13);
return arguments.length>2?_3.connect(this,_13,_14,_15):_3.connect(this,_13,_14);
},disconnect:function(_16){
_3.disconnect(_16);
},oncontextmenu:function(){
},onclick:function(){
},ondblclick:function(){
},onmouseenter:function(){
},onmouseleave:function(){
},onmouseout:function(){
},onmousedown:function(){
},ontouchstart:function(){
},touchstart:function(){
},onmouseup:function(){
},ontouchend:function(){
},touchend:function(){
},onmouseover:function(){
},onmousemove:function(){
},ontouchmove:function(){
},touchmove:function(){
},onkeydown:function(){
},onkeyup:function(){
}});
_9.Group=_2("dojox.gfx.canvasWithEvents.Group",[_9.Shape,_7.Group],{_testInputs:function(ctx,pos){
var _17=this.children,t=this.getTransform(),i,j,_18;
if(_17.length===0){
return;
}
var _19=[];
for(i=0;i<pos.length;++i){
_18=pos[i];
_19[i]={x:_18.x,y:_18.y};
if(_18.target){
continue;
}
var x=_18.x,y=_18.y;
var p=t?m.multiplyPoint(m.invert(t),x,y):{x:x,y:y};
_18.x=p.x;
_18.y=p.y;
}
for(i=_17.length-1;i>=0;--i){
_17[i]._testInputs(ctx,pos);
var _1a=true;
for(j=0;j<pos.length;++j){
if(pos[j].target==null){
_1a=false;
break;
}
}
if(_1a){
break;
}
}
if(this.clip){
for(i=0;i<pos.length;++i){
_18=pos[i];
_18.x=_19[i].x;
_18.y=_19[i].y;
if(_18.target){
ctx.clearRect(0,0,1,1);
ctx.save();
ctx.translate(-_18.x,-_18.y);
this._render(ctx,true);
if(!ctx.getImageData(0,0,1,1).data[0]){
_18.target=null;
}
ctx.restore();
}
}
}else{
for(i=0;i<pos.length;++i){
pos[i].x=_19[i].x;
pos[i].y=_19[i].y;
}
}
}});
_9.Image=_2("dojox.gfx.canvasWithEvents.Image",[_9.Shape,_7.Image],{_renderShape:function(ctx){
var s=this.shape;
if(ctx.pickingMode){
ctx.fillRect(s.x,s.y,s.width,s.height);
}else{
this.inherited(arguments);
}
},_hitTestGeometry:function(ctx,x,y){
var s=this.shape;
return x>=s.x&&x<=s.x+s.width&&y>=s.y&&y<=s.y+s.height?this:null;
}});
_9.Text=_2("dojox.gfx.canvasWithEvents.Text",[_9.Shape,_7.Text],{_testInputs:function(ctx,pos){
return this._hitTestPixel(ctx,pos);
}});
_9.Rect=_2("dojox.gfx.canvasWithEvents.Rect",[_9.Shape,_7.Rect],{});
_9.Circle=_2("dojox.gfx.canvasWithEvents.Circle",[_9.Shape,_7.Circle],{});
_9.Ellipse=_2("dojox.gfx.canvasWithEvents.Ellipse",[_9.Shape,_7.Ellipse],{});
_9.Line=_2("dojox.gfx.canvasWithEvents.Line",[_9.Shape,_7.Line],{});
_9.Polyline=_2("dojox.gfx.canvasWithEvents.Polyline",[_9.Shape,_7.Polyline],{});
_9.Path=_2("dojox.gfx.canvasWithEvents.Path",[_9.Shape,_7.Path],{});
_9.TextPath=_2("dojox.gfx.canvasWithEvents.TextPath",[_9.Shape,_7.TextPath],{});
var _1b={onmouseenter:"onmousemove",onmouseleave:"onmousemove",onmouseout:"onmousemove",onmouseover:"onmousemove",touchstart:"ontouchstart",touchend:"ontouchend",touchmove:"ontouchmove"};
var _1c={ontouchstart:"touchstart",ontouchend:"touchend",ontouchmove:"touchmove"};
var _1d=navigator.userAgent.toLowerCase(),_1e=_1d.search("iphone")>-1||_1d.search("ipad")>-1||_1d.search("ipod")>-1;
_9.Surface=_2("dojox.gfx.canvasWithEvents.Surface",_7.Surface,{constructor:function(){
this._pick={curr:null,last:null};
this._pickOfMouseDown=null;
this._pickOfMouseUp=null;
},connect:function(_1f,_20,_21){
if(_1f.indexOf("touch")!==-1){
this._setupEvents(_1f);
_1f="_on"+_1f+"Impl_";
return _3.connect(this,_1f,_20,_21);
}else{
this._initMirrorCanvas();
return _3.connect(this.getEventSource(),_1f,null,_8.fixCallback(this,g.fixTarget,_20,_21));
}
},_ontouchstartImpl_:function(){
},_ontouchendImpl_:function(){
},_ontouchmoveImpl_:function(){
},_initMirrorCanvas:function(){
if(!this.mirrorCanvas){
var p=this._parent,_22=p.ownerDocument.createElement("canvas");
_22.width=1;
_22.height=1;
_22.style.position="absolute";
_22.style.left="-99999px";
_22.style.top="-99999px";
p.appendChild(_22);
this.mirrorCanvas=_22;
}
},_setupEvents:function(_23){
if(_23 in _1b){
_23=_1b[_23];
}
if(this._eventsH&&this._eventsH[_23]){
return;
}
this._initMirrorCanvas();
if(!this._eventsH){
this._eventsH={};
}
this._eventsH[_23]=_3.connect(this.getEventSource(),_23,_8.fixCallback(this,g.fixTarget,this,"_"+_23));
if(_23==="onclick"||_23==="ondblclick"){
if(!this._eventsH["onmousedown"]){
this._eventsH["onmousedown"]=_3.connect(this.getEventSource(),"onmousedown",_8.fixCallback(this,g.fixTarget,this,"_onmousedown"));
}
if(!this._eventsH["onmouseup"]){
this._eventsH["onmouseup"]=_3.connect(this.getEventSource(),"onmouseup",_8.fixCallback(this,g.fixTarget,this,"_onmouseup"));
}
}
},destroy:function(){
this.inherited(arguments);
for(var i in this._eventsH){
_3.disconnect(this._eventsH[i]);
}
this._eventsH=this.mirrorCanvas=null;
},getEventSource:function(){
return this.rawNode;
},_invokeHandler:function(_24,_25,_26){
var _27=_24[_25];
if(_27&&_27.after){
_27.apply(_24,[_26]);
}else{
if(_25 in _1c){
_27=_24[_1c[_25]];
if(_27&&_27.after){
_27.apply(_24,[_26]);
}
}
}
if(!_27&&_25.indexOf("touch")!==-1){
_25="_"+_25+"Impl_";
_27=_24[_25];
if(_27){
_27.apply(_24,[_26]);
}
}
if(!_28(_26)&&_24.parent){
this._invokeHandler(_24.parent,_25,_26);
}
},_oncontextmenu:function(e){
if(this._pick.curr){
this._invokeHandler(this._pick.curr,"oncontextmenu",e);
}
},_ondblclick:function(e){
if(this._pickOfMouseUp){
this._invokeHandler(this._pickOfMouseUp,"ondblclick",e);
}
},_onclick:function(e){
if(this._pickOfMouseUp&&this._pickOfMouseUp==this._pickOfMouseDown){
this._invokeHandler(this._pickOfMouseUp,"onclick",e);
}
},_onmousedown:function(e){
this._pickOfMouseDown=this._pick.curr;
if(this._pick.curr){
this._invokeHandler(this._pick.curr,"onmousedown",e);
}
},_ontouchstart:function(e){
if(this._pick.curr){
this._fireTouchEvent(e);
}
},_onmouseup:function(e){
this._pickOfMouseUp=this._pick.curr;
if(this._pick.curr){
this._invokeHandler(this._pick.curr,"onmouseup",e);
}
},_ontouchend:function(e){
if(this._pick.curr){
for(var i=0;i<this._pick.curr.length;++i){
if(this._pick.curr[i].target){
e.gfxTarget=this._pick.curr[i].target;
this._invokeHandler(this._pick.curr[i].target,"ontouchend",e);
}
}
}
},_onmousemove:function(e){
if(this._pick.last&&this._pick.last!=this._pick.curr){
this._invokeHandler(this._pick.last,"onmouseleave",e);
this._invokeHandler(this._pick.last,"onmouseout",e);
}
if(this._pick.curr){
if(this._pick.last==this._pick.curr){
this._invokeHandler(this._pick.curr,"onmousemove",e);
}else{
this._invokeHandler(this._pick.curr,"onmouseenter",e);
this._invokeHandler(this._pick.curr,"onmouseover",e);
}
}
},_ontouchmove:function(e){
if(this._pick.curr){
this._fireTouchEvent(e);
}
},_fireTouchEvent:function(e){
var _29=[];
for(var i=0;i<this._pick.curr.length;++i){
var _2a=this._pick.curr[i];
if(_2a.target){
var _2b=_2a.target.__gfxtt;
if(!_2b){
_2b=[];
_2a.target.__gfxtt=_2b;
}
_2b.push(_2a.t);
if(!_2a.target.__inToFire){
_29.push(_2a.target);
_2a.target.__inToFire=true;
}
}
}
if(_29.length===0){
this._invokeHandler(this,"on"+e.type,e);
}else{
for(i=0;i<_29.length;++i){
(function(){
var _2c=_29[i].__gfxtt;
var evt=_1.delegate(e,{gfxTarget:_29[i]});
if(_1e){
evt.preventDefault=function(){
e.preventDefault();
};
evt.stopPropagation=function(){
e.stopPropagation();
};
}
evt.__defineGetter__("targetTouches",function(){
return _2c;
});
delete _29[i].__gfxtt;
delete _29[i].__inToFire;
this._invokeHandler(_29[i],"on"+e.type,evt);
}).call(this);
}
}
},_onkeydown:function(){
},_onkeyup:function(){
},_whatsUnderEvent:function(evt){
var _2d=this,i,pos=_6.position(_2d.rawNode,true),_2e=[],_2f=evt.changedTouches,_30=evt.touches;
if(_2f){
for(i=0;i<_2f.length;++i){
_2e.push({t:_2f[i],x:_2f[i].pageX-pos.x,y:_2f[i].pageY-pos.y});
}
}else{
if(_30){
for(i=0;i<_30.length;++i){
_2e.push({t:_30[i],x:_30[i].pageX-pos.x,y:_30[i].pageY-pos.y});
}
}else{
_2e.push({x:evt.pageX-pos.x,y:evt.pageY-pos.y});
}
}
var _31=_2d.mirrorCanvas,ctx=_31.getContext("2d"),_32=_2d.children;
ctx.clearRect(0,0,_31.width,_31.height);
ctx.save();
ctx.strokeStyle="rgba(127,127,127,1.0)";
ctx.fillStyle="rgba(127,127,127,1.0)";
ctx.pickingMode=true;
var _33=null;
for(i=_32.length-1;i>=0;i--){
_32[i]._testInputs(ctx,_2e);
var _34=true;
for(j=0;j<_2e.length;++j){
if(_2e[j].target==null){
_34=false;
break;
}
}
if(_34){
break;
}
}
ctx.restore();
return (_30||_2f)?_2e:_2e[0].target;
}});
_9.createSurface=function(_35,_36,_37){
if(!_36&&!_37){
var pos=_6.position(_35);
_36=_36||pos.w;
_37=_37||pos.h;
}
if(typeof _36=="number"){
_36=_36+"px";
}
if(typeof _37=="number"){
_37=_37+"px";
}
var s=new _9.Surface(),p=_5.byId(_35),c=p.ownerDocument.createElement("canvas");
c.width=g.normalizedLength(_36);
c.height=g.normalizedLength(_37);
p.appendChild(c);
s.rawNode=c;
s._parent=p;
s.surface=s;
return s;
};
var _28=function(evt){
if(evt.cancelBubble!==undefined){
return evt.cancelBubble;
}
return false;
};
_9.fixTarget=function(_38,_39){
if(_28(_38)){
return false;
}
if(!_38.gfxTarget){
_39._pick.last=_39._pick.curr;
_39._pick.curr=_39._whatsUnderEvent(_38);
if(!_1.isArray(_39._pick.curr)){
_38.gfxTarget=_39._pick.curr;
}
}
return true;
};
return _9;
});
