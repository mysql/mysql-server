//>>built
define("dojox/sketch/Figure",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/connect","dojo/_base/html","../gfx","../xml/DomParser","./UndoStack"],function(_1){
_1.experimental("dojox.sketch");
var ta=dojox.sketch;
ta.tools={};
ta.registerTool=function(_2,fn){
ta.tools[_2]=fn;
};
ta.Figure=function(_3){
var _4=this;
this.annCounter=1;
this.shapes=[];
this.image=null;
this.imageSrc=null;
this.size={w:0,h:0};
this.surface=null;
this.group=null;
this.node=null;
this.zoomFactor=1;
this.tools=null;
this.obj={};
_1.mixin(this,_3);
this.selected=[];
this.hasSelections=function(){
return this.selected.length>0;
};
this.isSelected=function(_5){
for(var i=0;i<_4.selected.length;i++){
if(_4.selected[i]==_5){
return true;
}
}
return false;
};
this.select=function(_6){
if(!_4.isSelected(_6)){
_4.clearSelections();
_4.selected=[_6];
}
_6.setMode(ta.Annotation.Modes.View);
_6.setMode(ta.Annotation.Modes.Edit);
};
this.deselect=function(_7){
var _8=-1;
for(var i=0;i<_4.selected.length;i++){
if(_4.selected[i]==_7){
_8=i;
break;
}
}
if(_8>-1){
_7.setMode(ta.Annotation.Modes.View);
_4.selected.splice(_8,1);
}
return _7;
};
this.clearSelections=function(){
for(var i=0;i<_4.selected.length;i++){
_4.selected[i].setMode(ta.Annotation.Modes.View);
}
_4.selected=[];
};
this.replaceSelection=function(n,o){
if(!_4.isSelected(o)){
_4.select(n);
return;
}
var _9=-1;
for(var i=0;i<_4.selected.length;i++){
if(_4.selected[i]==o){
_9=i;
break;
}
}
if(_9>-1){
_4.selected.splice(_9,1,n);
}
};
this._c=null;
this._ctr=null;
this._lp=null;
this._action=null;
this._prevState=null;
this._startPoint=null;
this._ctool=null;
this._start=null;
this._end=null;
this._absEnd=null;
this._cshape=null;
this._dblclick=function(e){
var o=_4._fromEvt(e);
if(o){
_4.onDblClickShape(o,e);
}
};
this._keydown=function(e){
var _a=false;
if(e.ctrlKey){
if(e.keyCode===90||e.keyCode===122){
_4.undo();
_a=true;
}else{
if(e.keyCode===89||e.keyCode===121){
_4.redo();
_a=true;
}
}
}
if(e.keyCode===46||e.keyCode===8){
_4._delete(_4.selected);
_a=true;
}
if(_a){
_1.stopEvent(e);
}
};
this._md=function(e){
if(dojox.gfx.renderer=="vml"){
_4.node.focus();
}
var o=_4._fromEvt(e);
_4._startPoint={x:e.pageX,y:e.pageY};
_4._ctr=_1.position(_4.node);
var _b={x:_4.node.scrollLeft,y:_4.node.scrollTop};
_4._ctr={x:_4._ctr.x-_b.x,y:_4._ctr.y-_b.y};
var X=e.clientX-_4._ctr.x,Y=e.clientY-_4._ctr.y;
_4._lp={x:X,y:Y};
_4._start={x:X,y:Y};
_4._end={x:X,y:Y};
_4._absEnd={x:X,y:Y};
if(!o){
_4.clearSelections();
_4._ctool.onMouseDown(e);
}else{
if(o.type&&o.type()!="Anchor"){
if(!_4.isSelected(o)){
_4.select(o);
_4._sameShapeSelected=false;
}else{
_4._sameShapeSelected=true;
}
}
o.beginEdit();
_4._c=o;
}
};
this._mm=function(e){
if(!_4._ctr){
return;
}
var x=e.clientX-_4._ctr.x;
var y=e.clientY-_4._ctr.y;
var dx=x-_4._lp.x;
var dy=y-_4._lp.y;
_4._absEnd={x:x,y:y};
if(_4._c){
_4._c.setBinding({dx:dx/_4.zoomFactor,dy:dy/_4.zoomFactor});
_4._lp={x:x,y:y};
}else{
_4._end={x:dx,y:dy};
var _c={x:Math.min(_4._start.x,_4._absEnd.x),y:Math.min(_4._start.y,_4._absEnd.y),width:Math.abs(_4._start.x-_4._absEnd.x),height:Math.abs(_4._start.y-_4._absEnd.y)};
if(_c.width&&_c.height){
_4._ctool.onMouseMove(e,_c);
}
}
};
this._mu=function(e){
if(_4._c){
_4._c.endEdit();
}else{
_4._ctool.onMouseUp(e);
}
_4._c=_4._ctr=_4._lp=_4._action=_4._prevState=_4._startPoint=null;
_4._cshape=_4._start=_4._end=_4._absEnd=null;
};
this.initUndoStack();
};
var p=ta.Figure.prototype;
p.initUndoStack=function(){
this.history=new ta.UndoStack(this);
};
p.setTool=function(t){
this._ctool=t;
};
p.gridSize=0;
p._calCol=function(v){
return this.gridSize?(Math.round(v/this.gridSize)*this.gridSize):v;
};
p._delete=function(_d,_e){
for(var i=0;i<_d.length;i++){
_d[i].setMode(ta.Annotation.Modes.View);
_d[i].destroy(_e);
this.remove(_d[i]);
this._remove(_d[i]);
if(!_e){
_d[i].onRemove();
}
}
_d.splice(0,_d.length);
};
p.onDblClickShape=function(_f,e){
if(_f["onDblClick"]){
_f.onDblClick(e);
}
};
p.onCreateShape=function(_10){
};
p.onBeforeCreateShape=function(_11){
};
p.initialize=function(_12){
this.node=_12;
this.surface=dojox.gfx.createSurface(_12,this.size.w,this.size.h);
this.group=this.surface.createGroup();
this._cons=[];
var es=this.surface.getEventSource();
this._cons.push(_1.connect(es,"ondraggesture",_1.stopEvent),_1.connect(es,"ondragenter",_1.stopEvent),_1.connect(es,"ondragover",_1.stopEvent),_1.connect(es,"ondragexit",_1.stopEvent),_1.connect(es,"ondragstart",_1.stopEvent),_1.connect(es,"onselectstart",_1.stopEvent),_1.connect(es,"onmousedown",this._md),_1.connect(es,"onmousemove",this._mm),_1.connect(es,"onmouseup",this._mu),_1.connect(es,"onclick",this,"onClick"),_1.connect(es,"ondblclick",this._dblclick),_1.connect(_12,"onkeydown",this._keydown));
this.image=this.group.createImage({width:this.imageSize.w,height:this.imageSize.h,src:this.imageSrc});
};
p.destroy=function(_13){
if(!this.node){
return;
}
if(!_13){
if(this.history){
this.history.destroy();
}
if(this._subscribed){
_1.unsubscribe(this._subscribed);
delete this._subscribed;
}
}
_1.forEach(this._cons,_1.disconnect);
this._cons=[];
_1.empty(this.node);
this.group=this.surface=null;
this.obj={};
this.shapes=[];
};
p.nextKey=function(){
return "annotation-"+this.annCounter++;
};
p.draw=function(){
};
p.zoom=function(pct){
this.zoomFactor=pct/100;
var w=this.size.w*this.zoomFactor;
var h=this.size.h*this.zoomFactor;
this.surface.setDimensions(w,h);
this.group.setTransform(dojox.gfx.matrix.scale(this.zoomFactor,this.zoomFactor));
for(var i=0;i<this.shapes.length;i++){
this.shapes[i].zoom(this.zoomFactor);
}
};
p.getFit=function(){
var wF=(this.node.parentNode.offsetWidth-5)/this.size.w;
var hF=(this.node.parentNode.offsetHeight-5)/this.size.h;
return Math.min(wF,hF)*100;
};
p.unzoom=function(){
this.zoomFactor=1;
this.surface.setDimensions(this.size.w,this.size.h);
this.group.setTransform();
};
p._add=function(obj){
this.obj[obj._key]=obj;
};
p._remove=function(obj){
if(this.obj[obj._key]){
delete this.obj[obj._key];
}
};
p._get=function(key){
if(key&&key.indexOf("bounding")>-1){
key=key.replace("-boundingBox","");
}else{
if(key&&key.indexOf("-labelShape")>-1){
key=key.replace("-labelShape","");
}
}
return this.obj[key];
};
p._keyFromEvt=function(e){
var key=e.target.id+"";
if(key.length==0){
var p=e.target.parentNode;
var _14=this.surface.getEventSource();
while(p&&p.id.length==0&&p!=_14){
p=p.parentNode;
}
key=p.id;
}
return key;
};
p._fromEvt=function(e){
return this._get(this._keyFromEvt(e));
};
p.add=function(_15){
for(var i=0;i<this.shapes.length;i++){
if(this.shapes[i]==_15){
return true;
}
}
this.shapes.push(_15);
return true;
};
p.remove=function(_16){
var idx=-1;
for(var i=0;i<this.shapes.length;i++){
if(this.shapes[i]==_16){
idx=i;
break;
}
}
if(idx>-1){
this.shapes.splice(idx,1);
}
return _16;
};
p.getAnnotator=function(id){
for(var i=0;i<this.shapes.length;i++){
if(this.shapes[i].id==id){
return this.shapes[i];
}
}
return null;
};
p.convert=function(ann,t){
var _17=t+"Annotation";
if(!ta[_17]){
return;
}
var _18=ann.type(),id=ann.id,_19=ann.label,_1a=ann.mode,_1b=ann.tokenId;
var _1c,end,_1d,_1e;
switch(_18){
case "Preexisting":
case "Lead":
_1e={dx:ann.transform.dx,dy:ann.transform.dy};
_1c={x:ann.start.x,y:ann.start.y};
end={x:ann.end.x,y:ann.end.y};
var cx=end.x-((end.x-_1c.x)/2);
var cy=end.y-((end.y-_1c.y)/2);
_1d={x:cx,y:cy};
break;
case "SingleArrow":
case "DoubleArrow":
_1e={dx:ann.transform.dx,dy:ann.transform.dy};
_1c={x:ann.start.x,y:ann.start.y};
end={x:ann.end.x,y:ann.end.y};
_1d={x:ann.control.x,y:ann.control.y};
break;
case "Underline":
_1e={dx:ann.transform.dx,dy:ann.transform.dy};
_1c={x:ann.start.x,y:ann.start.y};
_1d={x:_1c.x+50,y:_1c.y+50};
end={x:_1c.x+100,y:_1c.y+100};
break;
case "Brace":
}
var n=new ta[_17](this,id);
if(n.type()=="Underline"){
n.transform={dx:_1e.dx+_1c.x,dy:_1e.dy+_1c.y};
}else{
if(n.transform){
n.transform=_1e;
}
if(n.start){
n.start=_1c;
}
}
if(n.end){
n.end=end;
}
if(n.control){
n.control=_1d;
}
n.label=_19;
n.token=_1.lang.shallowCopy(ann.token);
n.initialize();
this.replaceSelection(n,ann);
this._remove(ann);
this.remove(ann);
ann.destroy();
n.setMode(_1a);
};
p.setValue=function(_1f){
var obj=dojox.xml.DomParser.parse(_1f);
var _20=this.node;
this.load(obj,_20);
};
p.load=function(obj,n){
if(this.surface){
this.destroy(true);
}
var _21=obj.documentElement;
this.size={w:parseFloat(_21.getAttribute("width"),10),h:parseFloat(_21.getAttribute("height"),10)};
var g=_21.childrenByName("g")[0];
var img=g.childrenByName("image")[0];
this.imageSize={w:parseFloat(img.getAttribute("width"),10),h:parseFloat(img.getAttribute("height"),10)};
this.imageSrc=img.getAttribute("xlink:href");
this.initialize(n);
var ann=g.childrenByName("g");
for(var i=0;i<ann.length;i++){
this._loadAnnotation(ann[i]);
}
if(this._loadDeferred){
this._loadDeferred.callback(this);
this._loadDeferred=null;
}
this.onLoad();
};
p.onLoad=function(){
};
p.onClick=function(){
};
p._loadAnnotation=function(obj){
var _22=obj.getAttribute("dojoxsketch:type")+"Annotation";
if(ta[_22]){
var a=new ta[_22](this,obj.id);
a.initialize(obj);
this.nextKey();
a.setMode(ta.Annotation.Modes.View);
this._add(a);
return a;
}
return null;
};
p.onUndo=function(){
};
p.onBeforeUndo=function(){
};
p.onRedo=function(){
};
p.onBeforeRedo=function(){
};
p.undo=function(){
if(this.history){
this.onBeforeUndo();
this.history.undo();
this.onUndo();
}
};
p.redo=function(){
if(this.history){
this.onBeforeRedo();
this.history.redo();
this.onRedo();
}
};
p.serialize=function(){
var s="<svg xmlns=\"http://www.w3.org/2000/svg\" "+"xmlns:xlink=\"http://www.w3.org/1999/xlink\" "+"xmlns:dojoxsketch=\"http://dojotoolkit.org/dojox/sketch\" "+"width=\""+this.size.w+"\" height=\""+this.size.h+"\">"+"<g>"+"<image xlink:href=\""+this.imageSrc+"\" x=\"0\" y=\"0\" width=\""+this.size.w+"\" height=\""+this.size.h+"\" />";
for(var i=0;i<this.shapes.length;i++){
s+=this.shapes[i].serialize();
}
s+="</g></svg>";
return s;
};
p.getValue=p.serialize;
return dojox.sketch.Figure;
});
