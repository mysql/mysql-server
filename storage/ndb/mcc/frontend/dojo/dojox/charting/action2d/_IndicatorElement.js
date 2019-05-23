//>>built
define("dojox/charting/action2d/_IndicatorElement",["dojo/_base/lang","dojo/_base/declare","../plot2d/Base","../plot2d/common","../axis2d/common","dojox/gfx"],function(_1,_2,_3,_4,_5,_6){
var _7=function(_8){
return _9(_8,_8.getShape().text);
};
var _9=function(s,t){
var c=s.declaredClass;
var w,h;
if(c.indexOf("svg")!=-1){
try{
return _1.mixin({},s.rawNode.getBBox());
}
catch(e){
return null;
}
}else{
if(c.indexOf("vml")!=-1){
var _a=s.rawNode,_b=_a.style.display;
_a.style.display="inline";
w=_6.pt2px(parseFloat(_a.currentStyle.width));
h=_6.pt2px(parseFloat(_a.currentStyle.height));
var sz={x:0,y:0,width:w,height:h};
_c(s,sz);
_a.style.display=_b;
return sz;
}else{
if(c.indexOf("silverlight")!=-1){
var bb={width:s.rawNode.actualWidth,height:s.rawNode.actualHeight};
return _c(s,bb,0.75);
}else{
if(s.getTextWidth){
w=s.getTextWidth();
var _d=s.getFont();
var fz=_d?_d.size:_6.defaultFont.size;
h=_6.normalizedLength(fz);
sz={width:w,height:h};
_c(s,sz,0.75);
return sz;
}
}
}
}
return null;
};
var _c=function(s,sz,_e){
var _f=sz.width,_10=sz.height,sh=s.getShape(),_11=sh.align;
switch(_11){
case "end":
sz.x=sh.x-_f;
break;
case "middle":
sz.x=sh.x-_f/2;
break;
case "start":
default:
sz.x=sh.x;
break;
}
_e=_e||1;
sz.y=sh.y-_10*_e;
return sz;
};
return _2(_3,{constructor:function(_12,_13){
if(!_13){
_13={};
}
this.inter=_13.inter;
},_updateVisibility:function(cp,_14,_15){
var _16=_15=="x"?this.inter.plot._hAxis:this.inter.plot._vAxis;
var _17=_16.getWindowScale();
this.chart.setAxisWindow(_16.name,_17,_16.getWindowOffset()+(cp[_15]-_14[_15])/_17);
this._noDirty=true;
this.chart.render();
this._noDirty=false;
if(!this._tracker){
this.initTrack();
}
},_trackMove:function(){
this._updateIndicator(this.pageCoord);
if(this._initTrackPhase){
this._initTrackPhase=false;
this._tracker=setInterval(_1.hitch(this,this._trackMove),100);
}
},initTrack:function(){
this._initTrackPhase=true;
this._tracker=setTimeout(_1.hitch(this,this._trackMove),500);
},stopTrack:function(){
if(this._tracker){
if(this._initTrackPhase){
clearTimeout(this._tracker);
}else{
clearInterval(this._tracker);
}
this._tracker=null;
}
},render:function(){
if(!this.isDirty()){
return;
}
this.cleanGroup();
if(!this.pageCoord){
return;
}
this._updateIndicator(this.pageCoord,this.secondCoord);
},_updateIndicator:function(cp1,cp2){
var _18=this.inter,_19=_18.plot,v=_18.opt.vertical;
var _1a=this.chart.getAxis(_19.hAxis),_1b=this.chart.getAxis(_19.vAxis);
var hn=_1a.name,vn=_1b.name,hb=_1a.getScaler().bounds,vb=_1b.getScaler().bounds;
var _1c=v?"x":"y",n=v?hn:vn,_1d=v?hb:vb;
if(cp2){
var tmp;
if(v){
if(cp1.x>cp2.x){
tmp=cp2;
cp2=cp1;
cp1=tmp;
}
}else{
if(cp1.y>cp2.y){
tmp=cp2;
cp2=cp1;
cp1=tmp;
}
}
}
var cd1=_19.toData(cp1),cd2;
if(cp2){
cd2=_19.toData(cp2);
}
var o={};
o[hn]=hb.from;
o[vn]=vb.from;
var min=_19.toPage(o);
o[hn]=hb.to;
o[vn]=vb.to;
var max=_19.toPage(o);
if(cd1[n]<_1d.from){
if(!cd2&&_18.opt.autoScroll){
this._updateVisibility(cp1,min,_1c);
return;
}else{
cp1[_1c]=min[_1c];
}
cd1=_19.toData(cp1);
}else{
if(cd1[n]>_1d.to){
if(!cd2&&_18.opt.autoScroll){
this._updateVisibility(cp1,max,_1c);
return;
}else{
cp1[_1c]=max[_1c];
}
cd1=_19.toData(cp1);
}
}
var c1=this._getData(cd1,_1c,v),c2;
if(c1.y==null){
return;
}
if(cp2){
if(cd2[n]<_1d.from){
cp2[_1c]=min[_1c];
cd2=_19.toData(cp2);
}else{
if(cd2[n]>_1d.to){
cp2[_1c]=max[_1c];
cd2=_19.toData(cp2);
}
}
c2=this._getData(cd2,_1c,v);
if(c2.y==null){
cp2=null;
}
}
var t1=this._renderIndicator(c1,cp2?1:0,hn,vn,min,max);
if(cp2){
var t2=this._renderIndicator(c2,2,hn,vn,min,max);
var _1e=v?c2.y-c1.y:c2.x-c1.y;
var _1f=_18.opt.labelFunc?_18.opt.labelFunc(c1,c2,_18.opt.fixed,_18.opt.precision):(_4.getLabel(_1e,_18.opt.fixed,_18.opt.precision)+" ("+_4.getLabel(100*_1e/(v?c1.y:c1.x),true,2)+"%)");
this._renderText(_1f,_18,this.chart.theme,v?(t1.x+t2.x)/2:t1.x,v?t1.y:(t1.y+t2.y)/2,c1,c2);
}
},_renderIndicator:function(_20,_21,hn,vn,min,max){
var t=this.chart.theme,c=this.chart.getCoords(),_22=this.inter,_23=_22.plot,v=_22.opt.vertical;
var _24={};
_24[hn]=_20.x;
_24[vn]=_20.y;
_24=_23.toPage(_24);
var cx=_24.x-c.x,cy=_24.y-c.y;
var x1=v?cx:min.x-c.x,y1=v?min.y-c.y:cy,x2=v?x1:max.x-c.x,y2=v?max.y-c.y:y1;
var sh=_22.opt.lineShadow?_22.opt.lineShadow:t.indicator.lineShadow,ls=_22.opt.lineStroke?_22.opt.lineStroke:t.indicator.lineStroke,ol=_22.opt.lineOutline?_22.opt.lineOutline:t.indicator.lineOutline;
if(sh){
this.group.createLine({x1:x1+sh.dx,y1:y1+sh.dy,x2:x2+sh.dx,y2:y2+sh.dy}).setStroke(sh);
}
if(ol){
ol=_4.makeStroke(ol);
ol.width=2*ol.width+ls.width;
this.group.createLine({x1:x1,y1:y1,x2:x2,y2:y2}).setStroke(ol);
}
this.group.createLine({x1:x1,y1:y1,x2:x2,y2:y2}).setStroke(ls);
var ms=_22.opt.markerSymbol?_22.opt.markerSymbol:t.indicator.markerSymbol,_25="M"+cx+" "+cy+" "+ms;
sh=_22.opt.markerShadow?_22.opt.markerShadow:t.indicator.markerShadow;
ls=_22.opt.markerStroke?_22.opt.markerStroke:t.indicator.markerStroke;
ol=_22.opt.markerOutline?_22.opt.markerOutline:t.indicator.markerOutline;
if(sh){
var sp="M"+(cx+sh.dx)+" "+(cy+sh.dy)+" "+ms;
this.group.createPath(sp).setFill(sh.color).setStroke(sh);
}
if(ol){
ol=_4.makeStroke(ol);
ol.width=2*ol.width+ls.width;
this.group.createPath(_25).setStroke(ol);
}
var _26=this.group.createPath(_25);
var sf=this._shapeFill(_22.opt.markerFill?_22.opt.markerFill:t.indicator.markerFill,_26.getBoundingBox());
_26.setFill(sf).setStroke(ls);
if(_21==0){
var _27=_22.opt.labelFunc?_22.opt.labelFunc(_20,null,_22.opt.fixed,_22.opt.precision):_4.getLabel(v?_20.y:_20.x,_22.opt.fixed,_22.opt.precision);
this._renderText(_27,_22,t,v?x1:x2+5,v?y2+5:y1,_20);
}
return v?{x:x1,y:y2+5}:{x:x2+5,y:y1};
},_renderText:function(_28,_29,t,x,y,c1,c2){
var _2a=_5.createText.gfx(this.chart,this.group,x,y,"middle",_28,_29.opt.font?_29.opt.font:t.indicator.font,_29.opt.fontColor?_29.opt.fontColor:t.indicator.fontColor);
var b=_7(_2a);
b.x-=2;
b.y-=1;
b.width+=4;
b.height+=2;
b.r=_29.opt.radius?_29.opt.radius:t.indicator.radius;
var sh=_29.opt.shadow?_29.opt.shadow:t.indicator.shadow,ls=_29.opt.stroke?_29.opt.stroke:t.indicator.stroke,ol=_29.opt.outline?_29.opt.outline:t.indicator.outline;
if(sh){
this.group.createRect(b).setFill(sh.color).setStroke(sh);
}
if(ol){
ol=_4.makeStroke(ol);
ol.width=2*ol.width+ls.width;
this.group.createRect(b).setStroke(ol);
}
var f=_29.opt.fillFunc?_29.opt.fillFunc(c1,c2):(_29.opt.fill?_29.opt.fill:t.indicator.fill);
this.group.createRect(b).setFill(this._shapeFill(f,b)).setStroke(ls);
_2a.moveToFront();
},_getData:function(cd,_2b,v){
var _2c=this.chart.getSeries(this.inter.opt.series).data;
var i,r,l=_2c.length;
for(i=0;i<l;++i){
r=_2c[i];
if(r==null){
}else{
if(typeof r=="number"){
if(i+1>cd[_2b]){
break;
}
}else{
if(r[_2b]>cd[_2b]){
break;
}
}
}
}
var x,y,px,py;
if(typeof r=="number"){
x=i+1;
y=r;
if(i>0){
px=i;
py=_2c[i-1];
}
}else{
x=r.x;
y=r.y;
if(i>0){
px=_2c[i-1].x;
py=_2c[i-1].y;
}
}
if(i>0){
var m=v?(x+px)/2:(y+py)/2;
if(cd[_2b]<=m){
x=px;
y=py;
}
}
return {x:x,y:y};
},cleanGroup:function(_2d){
this.inherited(arguments);
this.group.moveToFront();
return this;
},getSeriesStats:function(){
return _1.delegate(_4.defaultStats);
},isDirty:function(){
return !this._noDirty&&(this.dirty||this.inter.plot.isDirty());
}});
});
