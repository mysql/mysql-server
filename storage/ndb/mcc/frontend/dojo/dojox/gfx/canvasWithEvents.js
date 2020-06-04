//>>built
define("dojox/gfx/canvasWithEvents",["dojo/_base/lang","dojo/_base/declare","dojo/has","dojo/on","dojo/aspect","dojo/touch","dojo/_base/Color","dojo/dom","dojo/dom-geometry","dojo/_base/window","./_base","./canvas","./shape","./matrix"],function(_1,_2,_3,on,_4,_5,_6,_7,_8,_9,g,_a,_b,m){
function _c(_d){
var _e={};
for(var k in _d){
if(typeof _d[k]==="function"){
_e[k]=_1.hitch(_d,k);
}else{
_e[k]=_d[k];
}
}
return _e;
};
_3.add("dom-mutableEvents",function(){
var _f=document.createEvent("UIEvents");
try{
if(Object.defineProperty){
Object.defineProperty(_f,"type",{value:"foo"});
}else{
_f.type="foo";
}
return _f.type==="foo";
}
catch(e){
return false;
}
});
var _10=g.canvasWithEvents={};
_10.Shape=_2("dojox.gfx.canvasWithEvents.Shape",_a.Shape,{_testInputs:function(ctx,pos){
if(this.clip||(!this.canvasFill&&this.strokeStyle)){
this._hitTestPixel(ctx,pos);
}else{
this._renderShape(ctx);
var _11=pos.length,t=this.getTransform();
for(var i=0;i<_11;++i){
var _12=pos[i];
if(_12.target){
continue;
}
var x=_12.x,y=_12.y,p=t?m.multiplyPoint(m.invert(t),x,y):{x:x,y:y};
_12.target=this._hitTestGeometry(ctx,p.x,p.y);
}
}
},_hitTestPixel:function(ctx,pos){
for(var i=0;i<pos.length;++i){
var _13=pos[i];
if(_13.target){
continue;
}
var x=_13.x,y=_13.y;
ctx.clearRect(0,0,1,1);
ctx.save();
ctx.translate(-x,-y);
this._render(ctx,true);
_13.target=ctx.getImageData(0,0,1,1).data[0]?this:null;
ctx.restore();
}
},_hitTestGeometry:function(ctx,x,y){
return ctx.isPointInPath(x,y)?this:null;
},_renderFill:function(ctx,_14){
if(ctx.pickingMode){
if("canvasFill" in this&&_14){
ctx.fill();
}
return;
}
this.inherited(arguments);
},_renderStroke:function(ctx){
if(this.strokeStyle&&ctx.pickingMode){
var c=this.strokeStyle.color;
try{
this.strokeStyle.color=new _6(ctx.strokeStyle);
this.inherited(arguments);
}
finally{
this.strokeStyle.color=c;
}
}else{
this.inherited(arguments);
}
},getEventSource:function(){
return this.surface.rawNode;
},on:function(_15,_16){
var _17=this.rawNode;
return on(this.getEventSource(),_15,function(_18){
if(_7.isDescendant(_18.target,_17)){
_16.apply(_17,arguments);
}
});
},connect:function(_19,_1a,_1b){
if(_19.substring(0,2)=="on"){
_19=_19.substring(2);
}
return this.on(_19,_1b?_1.hitch(_1a,_1b):_1.hitch(null,_1a));
},disconnect:function(_1c){
_1c.remove();
}});
_10.Group=_2("dojox.gfx.canvasWithEvents.Group",[_10.Shape,_a.Group],{_testInputs:function(ctx,pos){
var _1d=this.children,t=this.getTransform(),i,j,_1e;
if(_1d.length===0){
return;
}
var _1f=[];
for(i=0;i<pos.length;++i){
_1e=pos[i];
_1f[i]={x:_1e.x,y:_1e.y};
if(_1e.target){
continue;
}
var x=_1e.x,y=_1e.y;
var p=t?m.multiplyPoint(m.invert(t),x,y):{x:x,y:y};
_1e.x=p.x;
_1e.y=p.y;
}
for(i=_1d.length-1;i>=0;--i){
_1d[i]._testInputs(ctx,pos);
var _20=true;
for(j=0;j<pos.length;++j){
if(pos[j].target==null){
_20=false;
break;
}
}
if(_20){
break;
}
}
if(this.clip){
for(i=0;i<pos.length;++i){
_1e=pos[i];
_1e.x=_1f[i].x;
_1e.y=_1f[i].y;
if(_1e.target){
ctx.clearRect(0,0,1,1);
ctx.save();
ctx.translate(-_1e.x,-_1e.y);
this._render(ctx,true);
if(!ctx.getImageData(0,0,1,1).data[0]){
_1e.target=null;
}
ctx.restore();
}
}
}else{
for(i=0;i<pos.length;++i){
pos[i].x=_1f[i].x;
pos[i].y=_1f[i].y;
}
}
}});
_10.Image=_2("dojox.gfx.canvasWithEvents.Image",[_10.Shape,_a.Image],{_renderShape:function(ctx){
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
_10.Text=_2("dojox.gfx.canvasWithEvents.Text",[_10.Shape,_a.Text],{_testInputs:function(ctx,pos){
return this._hitTestPixel(ctx,pos);
}});
_10.Rect=_2("dojox.gfx.canvasWithEvents.Rect",[_10.Shape,_a.Rect],{});
_10.Circle=_2("dojox.gfx.canvasWithEvents.Circle",[_10.Shape,_a.Circle],{});
_10.Ellipse=_2("dojox.gfx.canvasWithEvents.Ellipse",[_10.Shape,_a.Ellipse],{});
_10.Line=_2("dojox.gfx.canvasWithEvents.Line",[_10.Shape,_a.Line],{});
_10.Polyline=_2("dojox.gfx.canvasWithEvents.Polyline",[_10.Shape,_a.Polyline],{});
_10.Path=_2("dojox.gfx.canvasWithEvents.Path",[_10.Shape,_a.Path],{});
_10.TextPath=_2("dojox.gfx.canvasWithEvents.TextPath",[_10.Shape,_a.TextPath],{});
var _21=null;
_10.Surface=_2("dojox.gfx.canvasWithEvents.Surface",_a.Surface,{constructor:function(){
this._elementUnderPointer=null;
},fixTarget:function(_22){
var _23=this;
return function(_24){
var k;
if(_21){
if(_3("dom-mutableEvents")){
Object.defineProperties(_24,_21);
}else{
_24=_c(_24);
for(k in _21){
_24[k]=_21[k].value;
}
}
}else{
var _25=_23.getEventSource(),_26=_25._dojoElementFromPoint((_24.changedTouches?_24.changedTouches[0]:_24).pageX,(_24.changedTouches?_24.changedTouches[0]:_24).pageY);
if(_3("dom-mutableEvents")){
Object.defineProperties(_24,{target:{value:_26,configurable:true,enumerable:true},gfxTarget:{value:_26.shape,configurable:true,enumerable:true}});
}else{
_24=_c(_24);
_24.target=_26;
_24.gfxTarget=_26.shape;
}
}
if(_3("touch")){
if(_24.changedTouches&&_24.changedTouches[0]){
var _27=_24.changedTouches[0];
for(k in _27){
if(!_24[k]){
if(_3("dom-mutableEvents")){
Object.defineProperty(_24,k,{value:_27[k],configurable:true,enumerable:true});
}else{
_24[k]=_27[k];
}
}
}
}
_24.corrected=_24;
}
return _22.call(this,_24);
};
},_checkPointer:function(_28){
function _29(_2a,_2b,_2c){
var _2d=_28.bubbles;
for(var i=0,_2e;(_2e=_2a[i]);++i){
_21={target:{value:_2b,configurable:true,enumerable:true},gfxTarget:{value:_2b.shape,configurable:true,enumerable:true},relatedTarget:{value:_2c,configurable:true,enumerable:true}};
Object.defineProperty(_28,"bubbles",{value:_2e.bubbles,configurable:true,enumerable:true});
on.emit(_a,_2e.type,_28);
_21=null;
}
Object.defineProperty(_28,"bubbles",{value:_2d,configurable:true,enumerable:true});
};
var _2f={out:[{type:"mouseout",bubbles:true},{type:"MSPointerOut",bubbles:true},{type:"pointerout",bubbles:true},{type:"mouseleave",bubbles:false},{type:"dojotouchout",bubbles:true}],over:[{type:"mouseover",bubbles:true},{type:"MSPointerOver",bubbles:true},{type:"pointerover",bubbles:true},{type:"mouseenter",bubbles:false},{type:"dojotouchover",bubbles:true}]},_30=_28.target,_31=this._elementUnderPointer,_a=this.getEventSource();
if(_31!==_30){
if(_31&&_31!==_a){
_29(_2f.out,_31,_30);
}
this._elementUnderPointer=_30;
if(_30&&_30!==_a){
_29(_2f.over,_30,_31);
}
}
},getEventSource:function(){
return this.rawNode;
},on:function(_32,_33){
return on(this.getEventSource(),_32,_33);
},connect:function(_34,_35,_36){
if(_34.substring(0,2)=="on"){
_34=_34.substring(2);
}
return this.on(_34,_36?_1.hitch(_35,_36):_35);
},disconnect:function(_37){
_37.remove();
},_initMirrorCanvas:function(){
this._initMirrorCanvas=function(){
};
var _38=this.getEventSource(),_39=this.mirrorCanvas=_38.ownerDocument.createElement("canvas");
_39.width=1;
_39.height=1;
_39.style.position="absolute";
_39.style.left=_39.style.top="-99999px";
_38.parentNode.appendChild(_39);
var _3a="mousemove";
if(_3("pointer-events")){
_3a="pointermove";
}else{
if(_3("MSPointer")){
_3a="MSPointerMove";
}else{
if(_3("touch-events")){
_3a="touchmove";
}
}
}
on(_38,_3a,_1.hitch(this,"_checkPointer"));
},destroy:function(){
if(this.mirrorCanvas){
this.mirrorCanvas.parentNode.removeChild(this.mirrorCanvas);
this.mirrorCanvas=null;
}
this.inherited(arguments);
}});
_10.createSurface=function(_3b,_3c,_3d){
if(!_3c&&!_3d){
var pos=_8.position(_3b);
_3c=_3c||pos.w;
_3d=_3d||pos.h;
}
if(typeof _3c==="number"){
_3c=_3c+"px";
}
if(typeof _3d==="number"){
_3d=_3d+"px";
}
var _3e=new _10.Surface(),_3f=_7.byId(_3b),_a=_3f.ownerDocument.createElement("canvas");
_a.width=g.normalizedLength(_3c);
_a.height=g.normalizedLength(_3d);
_3f.appendChild(_a);
_3e.rawNode=_a;
_3e._parent=_3f;
_3e.surface=_3e;
g._base._fixMsTouchAction(_3e);
var _40=_a.addEventListener,_41=_a.removeEventListener,_42=[];
var _43=function(_44,_45,_46){
_3e._initMirrorCanvas();
var _47=_3e.fixTarget(_45);
_42.push({original:_45,actual:_47});
_40.call(this,_44,_47,_46);
};
var _48=function(_49,_4a,_4b){
for(var i=0,_4c;(_4c=_42[i]);++i){
if(_4c.original===_4a){
_41.call(this,_49,_4c.actual,_4b);
_42.splice(i,1);
break;
}
}
};
try{
Object.defineProperties(_a,{addEventListener:{value:_43,enumerable:true,configurable:true},removeEventListener:{value:_48}});
}
catch(e){
_a.addEventListener=_43;
_a.removeEventListener=_48;
}
_a._dojoElementFromPoint=function(x,y){
if(!_3e.mirrorCanvas){
return this;
}
var _4d=_8.position(this,true);
x-=_4d.x;
y-=_4d.y;
var _4e=_3e.mirrorCanvas,ctx=_4e.getContext("2d"),_4f=_3e.children;
ctx.clearRect(0,0,_4e.width,_4e.height);
ctx.save();
ctx.strokeStyle="rgba(127,127,127,1.0)";
ctx.fillStyle="rgba(127,127,127,1.0)";
ctx.pickingMode=true;
var _50=[{x:x,y:y}];
for(var i=_4f.length-1;i>=0;i--){
_4f[i]._testInputs(ctx,_50);
if(_50[0].target){
break;
}
}
ctx.restore();
return _50[0]&&_50[0].target?_50[0].target.rawNode:this;
};
return _3e;
};
var _51={createObject:function(){
var _52=this.inherited(arguments),_53={};
_52.rawNode={shape:_52,ownerDocument:_52.surface.rawNode.ownerDocument,parentNode:_52.parent?_52.parent.rawNode:null,addEventListener:function(_54,_55){
var _56=_53[_54]=(_53[_54]||[]);
for(var i=0,_57;(_57=_56[i]);++i){
if(_57.listener===_55){
return;
}
}
_56.push({listener:_55,handle:_4.after(this,"on"+_54,_52.surface.fixTarget(_55),true)});
},removeEventListener:function(_58,_59){
var _5a=_53[_58];
if(!_5a){
return;
}
for(var i=0,_5b;(_5b=_5a[i]);++i){
if(_5b.listener===_59){
_5b.handle.remove();
_5a.splice(i,1);
return;
}
}
}};
return _52;
}};
_10.Group.extend(_51);
_10.Surface.extend(_51);
return _10;
});
