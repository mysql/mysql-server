//>>built
define("dojox/drawing/tools/Path",["dojo/_base/lang","../util/oo","../manager/_registry","../stencil/Path"],function(_1,oo,_2,_3){
var _4=oo.declare(_3,function(){
this.pathMode="";
this.currentPathMode="";
this._started=false;
this.oddEvenClicks=0;
},{draws:true,onDown:function(_5){
if(!this._started){
this.onStartPath(_5);
}
},makeSubPath:function(_6){
if(_6){
if(this.currentPathMode=="Q"){
this.points.push({x:this.points[0].x,y:this.points[0].y});
}
this.points.push({t:"Z"});
this.render();
}
this.currentPathMode="";
this.pathMode="M";
},onStartPath:function(_7){
this._started=true;
this.revertRenderHit=this.renderHit;
this.renderHit=false;
this.closePath=false;
this.mouse.setEventMode("PathEdit");
this.closePoint={x:_7.x,y:_7.y};
this._kc1=this.connect(this.keys,"onEsc",this,function(){
this.onCompletePath(false);
});
this._kc2=this.connect(this.keys,"onKeyUp",this,function(_8){
switch(_8.letter){
case "c":
this.onCompletePath(true);
break;
case "l":
this.pathMode="L";
break;
case "m":
this.makeSubPath(false);
break;
case "q":
this.pathMode="Q";
break;
case "s":
this.pathMode="S";
break;
case "z":
this.makeSubPath(true);
break;
}
});
},onCompletePath:function(_9){
this.remove(this.closeGuide,this.guide);
var _a=this.getBounds();
if(_a.w<this.minimumSize&&_a.h<this.minimumSize){
this.remove(this.hit,this.shape,this.closeGuide);
this._started=false;
this.mouse.setEventMode("");
this.setPoints([]);
return;
}
if(_9){
if(this.currentPathMode=="Q"){
this.points.push({x:this.points[0].x,y:this.points[0].y});
}
this.closePath=true;
}
this.renderHit=this.revertRenderHit;
this.renderedOnce=true;
this.onRender(this);
this.disconnect([this._kc1,this._kc2]);
this.mouse.setEventMode("");
this.render();
},onUp:function(_b){
if(!this._started||!_b.withinCanvas){
return;
}
if(this.points.length>2&&this.closeRadius>this.util.distance(_b.x,_b.y,this.closePoint.x,this.closePoint.y)){
this.onCompletePath(true);
}else{
var p={x:_b.x,y:_b.y};
this.oddEvenClicks++;
if(this.currentPathMode!=this.pathMode){
if(this.pathMode=="Q"){
p.t="Q";
this.oddEvenClicks=0;
}else{
if(this.pathMode=="L"){
p.t="L";
}else{
if(this.pathMode=="M"){
p.t="M";
this.closePoint={x:_b.x,y:_b.y};
}
}
}
this.currentPathMode=this.pathMode;
}
this.points.push(p);
if(this.points.length>1){
this.remove(this.guide);
this.render();
}
}
},createGuide:function(_c){
if(!this.points.length){
return;
}
var _d=[].concat(this.points);
var pt={x:_c.x,y:_c.y};
if(this.currentPathMode=="Q"&&this.oddEvenClicks%2){
pt.t="L";
}
this.points.push(pt);
this.render();
this.points=_d;
var _e=this.util.distance(_c.x,_c.y,this.closePoint.x,this.closePoint.y);
if(this.points.length>1){
if(_e<this.closeRadius&&!this.closeGuide){
var c={cx:this.closePoint.x,cy:this.closePoint.y,rx:this.closeRadius,ry:this.closeRadius};
this.closeGuide=this.container.createEllipse(c).setFill(this.closeColor);
}else{
if(_e>this.closeRadius&&this.closeGuide){
this.remove(this.closeGuide);
this.closeGuide=null;
}
}
}
},onMove:function(_f){
if(!this._started){
return;
}
this.createGuide(_f);
},onDrag:function(obj){
if(!this._started){
return;
}
this.createGuide(obj);
}});
_1.setObject("dojox.drawing.tools.Path",_4);
_4.setup={name:"dojox.drawing.tools.Path",tooltip:"Path Tool",iconClass:"iconLine"};
_2.register(_4.setup,"tool");
return _4;
});
