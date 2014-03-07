//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_Widget,dijit/_Templated,dijit/_Container,dijit/_Contained"],function(_1,_2,_3){
_2.provide("dojox.widget.FisheyeList");
_2.require("dijit._Widget");
_2.require("dijit._Templated");
_2.require("dijit._Container");
_2.require("dijit._Contained");
_2.declare("dojox.widget.FisheyeList",[_1._Widget,_1._Templated,_1._Container],{constructor:function(){
this.pos={"x":-1,"y":-1};
this.timerScale=1;
},EDGE:{CENTER:0,LEFT:1,RIGHT:2,TOP:3,BOTTOM:4},templateString:"<div class=\"dojoxFisheyeListBar\" dojoAttachPoint=\"containerNode\"></div>",snarfChildDomOutput:true,itemWidth:40,itemHeight:40,itemMaxWidth:150,itemMaxHeight:150,imgNode:null,orientation:"horizontal",isFixed:false,conservativeTrigger:false,effectUnits:2,itemPadding:10,attachEdge:"center",labelEdge:"bottom",postCreate:function(){
var e=this.EDGE;
_2.setSelectable(this.domNode,false);
var _4=this.isHorizontal=(this.orientation=="horizontal");
this.selectedNode=-1;
this.isOver=false;
this.hitX1=-1;
this.hitY1=-1;
this.hitX2=-1;
this.hitY2=-1;
this.anchorEdge=this._toEdge(this.attachEdge,e.CENTER);
this.labelEdge=this._toEdge(this.labelEdge,e.TOP);
if(this.labelEdge==e.CENTER){
this.labelEdge=e.TOP;
}
if(_4){
if(this.anchorEdge==e.LEFT){
this.anchorEdge=e.CENTER;
}
if(this.anchorEdge==e.RIGHT){
this.anchorEdge=e.CENTER;
}
if(this.labelEdge==e.LEFT){
this.labelEdge=e.TOP;
}
if(this.labelEdge==e.RIGHT){
this.labelEdge=e.TOP;
}
}else{
if(this.anchorEdge==e.TOP){
this.anchorEdge=e.CENTER;
}
if(this.anchorEdge==e.BOTTOM){
this.anchorEdge=e.CENTER;
}
if(this.labelEdge==e.TOP){
this.labelEdge=e.LEFT;
}
if(this.labelEdge==e.BOTTOM){
this.labelEdge=e.LEFT;
}
}
var _5=this.effectUnits;
this.proximityLeft=this.itemWidth*(_5-0.5);
this.proximityRight=this.itemWidth*(_5-0.5);
this.proximityTop=this.itemHeight*(_5-0.5);
this.proximityBottom=this.itemHeight*(_5-0.5);
if(this.anchorEdge==e.LEFT){
this.proximityLeft=0;
}
if(this.anchorEdge==e.RIGHT){
this.proximityRight=0;
}
if(this.anchorEdge==e.TOP){
this.proximityTop=0;
}
if(this.anchorEdge==e.BOTTOM){
this.proximityBottom=0;
}
if(this.anchorEdge==e.CENTER){
this.proximityLeft/=2;
this.proximityRight/=2;
this.proximityTop/=2;
this.proximityBottom/=2;
}
},startup:function(){
this.children=this.getChildren();
this._initializePositioning();
if(!this.conservativeTrigger){
this._onMouseMoveHandle=_2.connect(document.documentElement,"onmousemove",this,"_onMouseMove");
}
if(this.isFixed){
this._onScrollHandle=_2.connect(document,"onscroll",this,"_onScroll");
}
this._onMouseOutHandle=_2.connect(document.documentElement,"onmouseout",this,"_onBodyOut");
this._addChildHandle=_2.connect(this,"addChild",this,"_initializePositioning");
this._onResizeHandle=_2.connect(window,"onresize",this,"_initializePositioning");
},_initializePositioning:function(){
this.itemCount=this.children.length;
this.barWidth=(this.isHorizontal?this.itemCount:1)*this.itemWidth;
this.barHeight=(this.isHorizontal?1:this.itemCount)*this.itemHeight;
this.totalWidth=this.proximityLeft+this.proximityRight+this.barWidth;
this.totalHeight=this.proximityTop+this.proximityBottom+this.barHeight;
for(var i=0;i<this.children.length;i++){
this.children[i].posX=this.itemWidth*(this.isHorizontal?i:0);
this.children[i].posY=this.itemHeight*(this.isHorizontal?0:i);
this.children[i].cenX=this.children[i].posX+(this.itemWidth/2);
this.children[i].cenY=this.children[i].posY+(this.itemHeight/2);
var _6=this.isHorizontal?this.itemWidth:this.itemHeight;
var r=this.effectUnits*_6;
var c=this.isHorizontal?this.children[i].cenX:this.children[i].cenY;
var _7=this.isHorizontal?this.proximityLeft:this.proximityTop;
var _8=this.isHorizontal?this.proximityRight:this.proximityBottom;
var _9=this.isHorizontal?this.barWidth:this.barHeight;
var _a=r;
var _b=r;
if(_a>c+_7){
_a=c+_7;
}
if(_b>(_9-c+_8)){
_b=_9-c+_8;
}
this.children[i].effectRangeLeft=_a/_6;
this.children[i].effectRangeRght=_b/_6;
}
this.domNode.style.width=this.barWidth+"px";
this.domNode.style.height=this.barHeight+"px";
for(i=0;i<this.children.length;i++){
var _c=this.children[i];
var _d=_c.domNode;
_d.style.left=_c.posX+"px";
_d.style.top=_c.posY+"px";
_d.style.width=this.itemWidth+"px";
_d.style.height=this.itemHeight+"px";
_c.imgNode.style.left=this.itemPadding+"%";
_c.imgNode.style.top=this.itemPadding+"%";
_c.imgNode.style.width=(100-2*this.itemPadding)+"%";
_c.imgNode.style.height=(100-2*this.itemPadding)+"%";
}
this._calcHitGrid();
},_overElement:function(_e,e){
_e=_2.byId(_e);
var _f={x:e.pageX,y:e.pageY};
var _10=_2.position(_e,true);
var top=_10.y;
var _11=top+_10.h;
var _12=_10.x;
var _13=_12+_10.w;
return (_f.x>=_12&&_f.x<=_13&&_f.y>=top&&_f.y<=_11);
},_onBodyOut:function(e){
if(this._overElement(_2.body(),e)){
return;
}
this._setDormant(e);
},_setDormant:function(e){
if(!this.isOver){
return;
}
this.isOver=false;
if(this.conservativeTrigger){
_2.disconnect(this._onMouseMoveHandle);
}
this._onGridMouseMove(-1,-1);
},_setActive:function(e){
if(this.isOver){
return;
}
this.isOver=true;
if(this.conservativeTrigger){
this._onMouseMoveHandle=_2.connect(document.documentElement,"onmousemove",this,"_onMouseMove");
this.timerScale=0;
this._onMouseMove(e);
this._expandSlowly();
}
},_onMouseMove:function(e){
if((e.pageX>=this.hitX1)&&(e.pageX<=this.hitX2)&&(e.pageY>=this.hitY1)&&(e.pageY<=this.hitY2)){
if(!this.isOver){
this._setActive(e);
}
this._onGridMouseMove(e.pageX-this.hitX1,e.pageY-this.hitY1);
}else{
if(this.isOver){
this._setDormant(e);
}
}
},_onScroll:function(){
this._calcHitGrid();
},onResized:function(){
this._calcHitGrid();
},_onGridMouseMove:function(x,y){
this.pos={x:x,y:y};
this._paint();
},_paint:function(){
var x=this.pos.x;
var y=this.pos.y;
if(this.itemCount<=0){
return;
}
var pos=this.isHorizontal?x:y;
var prx=this.isHorizontal?this.proximityLeft:this.proximityTop;
var siz=this.isHorizontal?this.itemWidth:this.itemHeight;
var sim=this.isHorizontal?(1-this.timerScale)*this.itemWidth+this.timerScale*this.itemMaxWidth:(1-this.timerScale)*this.itemHeight+this.timerScale*this.itemMaxHeight;
var cen=((pos-prx)/siz)-0.5;
var _14=(sim/siz)-0.5;
if(_14>this.effectUnits){
_14=this.effectUnits;
}
var _15=0,_16;
if(this.anchorEdge==this.EDGE.BOTTOM){
_16=(y-this.proximityTop)/this.itemHeight;
_15=(_16>0.5)?1:y/(this.proximityTop+(this.itemHeight/2));
}
if(this.anchorEdge==this.EDGE.TOP){
_16=(y-this.proximityTop)/this.itemHeight;
_15=(_16<0.5)?1:(this.totalHeight-y)/(this.proximityBottom+(this.itemHeight/2));
}
if(this.anchorEdge==this.EDGE.RIGHT){
_16=(x-this.proximityLeft)/this.itemWidth;
_15=(_16>0.5)?1:x/(this.proximityLeft+(this.itemWidth/2));
}
if(this.anchorEdge==this.EDGE.LEFT){
_16=(x-this.proximityLeft)/this.itemWidth;
_15=(_16<0.5)?1:(this.totalWidth-x)/(this.proximityRight+(this.itemWidth/2));
}
if(this.anchorEdge==this.EDGE.CENTER){
if(this.isHorizontal){
_15=y/(this.totalHeight);
}else{
_15=x/(this.totalWidth);
}
if(_15>0.5){
_15=1-_15;
}
_15*=2;
}
for(var i=0;i<this.itemCount;i++){
var _17=this._weighAt(cen,i);
if(_17<0){
_17=0;
}
this._setItemSize(i,_17*_15);
}
var _18=Math.round(cen);
var _19=0;
if(cen<0){
_18=0;
}else{
if(cen>this.itemCount-1){
_18=this.itemCount-1;
}else{
_19=(cen-_18)*((this.isHorizontal?this.itemWidth:this.itemHeight)-this.children[_18].sizeMain);
}
}
this._positionElementsFrom(_18,_19);
},_weighAt:function(cen,i){
var _1a=Math.abs(cen-i);
var _1b=((cen-i)>0)?this.children[i].effectRangeRght:this.children[i].effectRangeLeft;
return (_1a>_1b)?0:(1-_1a/_1b);
},_setItemSize:function(p,_1c){
if(this.children[p].scale==_1c){
return;
}
this.children[p].scale=_1c;
_1c*=this.timerScale;
var w=Math.round(this.itemWidth+((this.itemMaxWidth-this.itemWidth)*_1c));
var h=Math.round(this.itemHeight+((this.itemMaxHeight-this.itemHeight)*_1c));
if(this.isHorizontal){
this.children[p].sizeW=w;
this.children[p].sizeH=h;
this.children[p].sizeMain=w;
this.children[p].sizeOff=h;
var y=0;
if(this.anchorEdge==this.EDGE.TOP){
y=(this.children[p].cenY-(this.itemHeight/2));
}else{
if(this.anchorEdge==this.EDGE.BOTTOM){
y=(this.children[p].cenY-(h-(this.itemHeight/2)));
}else{
y=(this.children[p].cenY-(h/2));
}
}
this.children[p].usualX=Math.round(this.children[p].cenX-(w/2));
this.children[p].domNode.style.top=y+"px";
this.children[p].domNode.style.left=this.children[p].usualX+"px";
}else{
this.children[p].sizeW=w;
this.children[p].sizeH=h;
this.children[p].sizeOff=w;
this.children[p].sizeMain=h;
var x=0;
if(this.anchorEdge==this.EDGE.LEFT){
x=this.children[p].cenX-(this.itemWidth/2);
}else{
if(this.anchorEdge==this.EDGE.RIGHT){
x=this.children[p].cenX-(w-(this.itemWidth/2));
}else{
x=this.children[p].cenX-(w/2);
}
}
this.children[p].domNode.style.left=x+"px";
this.children[p].usualY=Math.round(this.children[p].cenY-(h/2));
this.children[p].domNode.style.top=this.children[p].usualY+"px";
}
this.children[p].domNode.style.width=w+"px";
this.children[p].domNode.style.height=h+"px";
if(this.children[p].svgNode){
this.children[p].svgNode.setSize(w,h);
}
},_positionElementsFrom:function(p,_1d){
var pos=0;
var _1e,_1f;
if(this.isHorizontal){
_1e="usualX";
_1f="left";
}else{
_1e="usualY";
_1f="top";
}
pos=Math.round(this.children[p][_1e]+_1d);
if(this.children[p].domNode.style[_1f]!=(pos+"px")){
this.children[p].domNode.style[_1f]=pos+"px";
this._positionLabel(this.children[p]);
}
var _20=pos;
for(var i=p-1;i>=0;i--){
_20-=this.children[i].sizeMain;
if(this.children[p].domNode.style[_1f]!=(_20+"px")){
this.children[i].domNode.style[_1f]=_20+"px";
this._positionLabel(this.children[i]);
}
}
var _21=pos;
for(i=p+1;i<this.itemCount;i++){
_21+=this.children[i-1].sizeMain;
if(this.children[p].domNode.style[_1f]!=(_21+"px")){
this.children[i].domNode.style[_1f]=_21+"px";
this._positionLabel(this.children[i]);
}
}
},_positionLabel:function(itm){
var x=0;
var y=0;
var mb=_2.marginBox(itm.lblNode);
if(this.labelEdge==this.EDGE.TOP){
x=Math.round((itm.sizeW/2)-(mb.w/2));
y=-mb.h;
}
if(this.labelEdge==this.EDGE.BOTTOM){
x=Math.round((itm.sizeW/2)-(mb.w/2));
y=itm.sizeH;
}
if(this.labelEdge==this.EDGE.LEFT){
x=-mb.w;
y=Math.round((itm.sizeH/2)-(mb.h/2));
}
if(this.labelEdge==this.EDGE.RIGHT){
x=itm.sizeW;
y=Math.round((itm.sizeH/2)-(mb.h/2));
}
itm.lblNode.style.left=x+"px";
itm.lblNode.style.top=y+"px";
},_calcHitGrid:function(){
var pos=_2.coords(this.domNode,true);
this.hitX1=pos.x-this.proximityLeft;
this.hitY1=pos.y-this.proximityTop;
this.hitX2=this.hitX1+this.totalWidth;
this.hitY2=this.hitY1+this.totalHeight;
},_toEdge:function(inp,def){
return this.EDGE[inp.toUpperCase()]||def;
},_expandSlowly:function(){
if(!this.isOver){
return;
}
this.timerScale+=0.2;
this._paint();
if(this.timerScale<1){
setTimeout(_2.hitch(this,"_expandSlowly"),10);
}
},destroyRecursive:function(){
_2.disconnect(this._onMouseOutHandle);
_2.disconnect(this._onMouseMoveHandle);
_2.disconnect(this._addChildHandle);
if(this.isFixed){
_2.disconnect(this._onScrollHandle);
}
_2.disconnect(this._onResizeHandle);
this.inherited("destroyRecursive",arguments);
}});
_2.declare("dojox.widget.FisheyeListItem",[_1._Widget,_1._Templated,_1._Contained],{iconSrc:"",label:"",id:"",templateString:"<div class=\"dojoxFisheyeListItem\">"+"  <img class=\"dojoxFisheyeListItemImage\" dojoAttachPoint=\"imgNode\" dojoAttachEvent=\"onmouseover:onMouseOver,onmouseout:onMouseOut,onclick:onClick\">"+"  <div class=\"dojoxFisheyeListItemLabel\" dojoAttachPoint=\"lblNode\"></div>"+"</div>",_isNode:function(wh){
if(typeof Element=="function"){
try{
return wh instanceof Element;
}
catch(e){
}
}else{
return wh&&!isNaN(wh.nodeType);
}
return false;
},_hasParent:function(_22){
return Boolean(_22&&_22.parentNode&&this._isNode(_22.parentNode));
},postCreate:function(){
var _23;
if((this.iconSrc.toLowerCase().substring(this.iconSrc.length-4)==".png")&&_2.isIE<7){
if(this._hasParent(this.imgNode)&&this.id!=""){
_23=this.imgNode.parentNode;
_23.setAttribute("id",this.id);
}
this.imgNode.style.filter="progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+this.iconSrc+"', sizingMethod='scale')";
this.imgNode.src=this._blankGif.toString();
}else{
if(this._hasParent(this.imgNode)&&this.id!=""){
_23=this.imgNode.parentNode;
_23.setAttribute("id",this.id);
}
this.imgNode.src=this.iconSrc;
}
if(this.lblNode){
this.lblNode.appendChild(document.createTextNode(this.label));
}
_2.setSelectable(this.domNode,false);
this.startup();
},startup:function(){
this.parent=this.getParent();
},onMouseOver:function(e){
if(!this.parent.isOver){
this.parent._setActive(e);
}
if(this.label!=""){
_2.addClass(this.lblNode,"dojoxFishSelected");
this.parent._positionLabel(this);
}
},onMouseOut:function(e){
_2.removeClass(this.lblNode,"dojoxFishSelected");
},onClick:function(e){
}});
});
