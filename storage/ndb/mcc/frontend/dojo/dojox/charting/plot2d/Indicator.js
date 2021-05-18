//>>built
define("dojox/charting/plot2d/Indicator",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./CartesianBase","./_PlotEvents","./common","../axis2d/common","dojox/gfx","dojox/lang/utils","dojox/gfx/fx","dojo/has"],function(_1,_2,_3,_4,_5,_6,_7,_8,du,fx,_9){
var _a=function(_b){
return _c(_b,_b.getShape().text);
};
var _c=function(s,t){
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
var _d=s.rawNode,_e=_d.style.display;
_d.style.display="inline";
w=_8.pt2px(parseFloat(_d.currentStyle.width));
h=_8.pt2px(parseFloat(_d.currentStyle.height));
var sz={x:0,y:0,width:w,height:h};
_f(s,sz);
_d.style.display=_e;
return sz;
}else{
if(c.indexOf("silverlight")!=-1){
var bb={width:s.rawNode.actualWidth,height:s.rawNode.actualHeight};
return _f(s,bb,0.75);
}else{
if(s.getTextWidth){
w=s.getTextWidth();
var _10=s.getFont();
var fz=_10?_10.size:_8.defaultFont.size;
h=_8.normalizedLength(fz);
sz={width:w,height:h};
_f(s,sz,0.75);
return sz;
}
}
}
}
return null;
};
var _f=function(s,sz,_11){
var _12=sz.width,_13=sz.height,sh=s.getShape(),_14=sh.align;
switch(_14){
case "end":
sz.x=sh.x-_12;
break;
case "middle":
sz.x=sh.x-_12/2;
break;
case "start":
default:
sz.x=sh.x;
break;
}
_11=_11||1;
sz.y=sh.y-_13*_11;
return sz;
};
var _15=_3("dojox.charting.plot2d.Indicator",[_4,_5],{defaultParams:{vertical:true,fixed:true,precision:0,lines:true,labels:"line",markers:true},optionalParams:{lineStroke:{},outlineStroke:{},shadowStroke:{},lineFill:{},stroke:{},outline:{},shadow:{},fill:{},fillFunc:null,labelFunc:null,font:"",fontColor:"",markerStroke:{},markerOutline:{},markerShadow:{},markerFill:{},markerSymbol:"",values:[],offset:{},start:false,animate:false},constructor:function(_16,_17){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_17);
if(typeof _17.values=="number"){
_17.values=[_17.values];
}
du.updateWithPattern(this.opt,_17,this.optionalParams);
this.animate=this.opt.animate;
},render:function(dim,_18){
if(this.zoom){
return this.performZoom(dim,_18);
}
if(!this.isDirty()){
return this;
}
this.cleanGroup(null,true);
if(!this.opt.values){
return this;
}
this._updateIndicator();
return this;
},_updateIndicator:function(){
var t=this.chart.theme;
var hn=this._hAxis.name,vn=this._vAxis.name,hb=this._hAxis.getScaler().bounds,vb=this._vAxis.getScaler().bounds;
var o={};
o[hn]=hb.from;
o[vn]=vb.from;
var min=this.toPage(o);
o[hn]=hb.to;
o[vn]=vb.to;
var max=this.toPage(o);
var _19=this.events();
var _1a=_2.map(this.opt.values,function(_1b,_1c){
return this._renderIndicator(_1b,_1c,hn,vn,min,max,_19,this.animate);
},this);
var _1d=_1a.length;
if(this.opt.labels=="trend"){
var v=this.opt.vertical;
var _1e=this._data[0][0];
var _1f=this._data[_1d-1][0];
var _20=_1f-_1e;
var _21=this.opt.labelFunc?this.opt.labelFunc(-1,this.values,this._data,this.opt.fixed,this.opt.precision):(_6.getLabel(_20,this.opt.fixed,this.opt.precision)+" ("+_6.getLabel(100*_20/_1e,true,2)+"%)");
this._renderText(this.getGroup(),_21,this.chart.theme,v?(_1a[0].x+_1a[_1d-1].x)/2:_1a[1].x,v?_1a[0].y:(_1a[1].y+_1a[_1d-1].y)/2,-1,this.opt.values,this._data);
}
var _22=this.opt.lineFill!=undefined?this.opt.lineFill:t.indicator.lineFill;
if(_22&&_1d>1){
var x0=Math.min(_1a[0].x1,_1a[_1d-1].x1);
var y0=Math.min(_1a[0].y1,_1a[_1d-1].y1);
var r=this.getGroup().createRect({x:x0,y:y0,width:Math.max(_1a[0].x2,_1a[_1d-1].x2)-x0,height:Math.max(_1a[0].y2,_1a[_1d-1].y2)-y0}).setFill(_22);
r.moveToBack();
}
},_renderIndicator:function(_23,_24,hn,vn,min,max,_25,_26){
var t=this.chart.theme,c=this.chart.getCoords(),v=this.opt.vertical;
var g=this.getGroup().createGroup();
var _27={};
_27[hn]=v?_23:0;
_27[vn]=v?0:_23;
if(_9("dojo-bidi")){
_27.x=this._getMarkX(_27.x);
}
_27=this.toPage(_27);
var _28=v?_27.x>=min.x&&_27.x<=max.x:_27.y>=max.y&&_27.y<=min.y;
var cx=_27.x-c.x,cy=_27.y-c.y;
var x1=v?cx:min.x-c.x,y1=v?min.y-c.y:cy,x2=v?x1:max.x-c.x,y2=v?max.y-c.y:y1;
if(this.opt.lines&&_28){
var sh=this.opt.hasOwnProperty("lineShadow")?this.opt.lineShadow:t.indicator.lineShadow,ls=this.opt.hasOwnProperty("lineStroke")?this.opt.lineStroke:t.indicator.lineStroke,ol=this.opt.hasOwnProperty("lineOutline")?this.opt.lineOutline:t.indicator.lineOutline;
if(sh){
g.createLine({x1:x1+sh.dx,y1:y1+sh.dy,x2:x2+sh.dx,y2:y2+sh.dy}).setStroke(sh);
}
if(ol){
ol=_6.makeStroke(ol);
ol.width=2*ol.width+(ls?ls.width:0);
g.createLine({x1:x1,y1:y1,x2:x2,y2:y2}).setStroke(ol);
}
g.createLine({x1:x1,y1:y1,x2:x2,y2:y2}).setStroke(ls);
}
var _29;
if(this.opt.markers&&_28){
var d=this._data[_24];
var _2a=this;
if(d){
_29=_2.map(d,function(_2b,_2c){
_27[hn]=v?_23:_2b;
_27[vn]=v?_2b:_23;
if(_9("dojo-bidi")){
_27.x=_2a._getMarkX(_27.x);
}
_27=this.toPage(_27);
if(v?_27.y<=min.y&&_27.y>=max.y:_27.x>=min.x&&_27.x<=max.x){
cx=_27.x-c.x;
cy=_27.y-c.y;
var ms=this.opt.markerSymbol?this.opt.markerSymbol:t.indicator.markerSymbol,_2d="M"+cx+" "+cy+" "+ms;
sh=this.opt.markerShadow!=undefined?this.opt.markerShadow:t.indicator.markerShadow;
ls=this.opt.markerStroke!=undefined?this.opt.markerStroke:t.indicator.markerStroke;
ol=this.opt.markerOutline!=undefined?this.opt.markerOutline:t.indicator.markerOutline;
if(sh){
var sp="M"+(cx+sh.dx)+" "+(cy+sh.dy)+" "+ms;
g.createPath(sp).setFill(sh.color).setStroke(sh);
}
if(ol){
ol=_6.makeStroke(ol);
ol.width=2*ol.width+(ls?ls.width:0);
g.createPath(_2d).setStroke(ol);
}
var _2e=g.createPath(_2d);
var sf=this._shapeFill(this.opt.markerFill!=undefined?this.opt.markerFill:t.indicator.markerFill,_2e.getBoundingBox());
_2e.setFill(sf).setStroke(ls);
}
return _2b;
},this);
}
}
var _2f;
if(this.opt.start){
_2f={x:v?x1:x1,y:v?y1:y2};
}else{
_2f={x:v?x1:x2,y:v?y2:y1};
}
if(this.opt.labels&&this.opt.labels!="trend"&&_28){
var _30;
if(this.opt.labelFunc){
_30=this.opt.labelFunc(_24,this.opt.values,this._data,this.opt.fixed,this.opt.precision,this.opt.labels);
}else{
if(this.opt.labels=="markers"){
_30=_2.map(_29,function(_31){
return _6.getLabel(_31,this.opt.fixed,this.opt.precision);
},this);
_30=_30.length!=1?"[ "+_30.join(", ")+" ]":_30[0];
}else{
_30=_6.getLabel(_23,this.opt.fixed,this.opt.precision);
}
}
this._renderText(g,_30,t,_2f.x,_2f.y,_24,this.opt.values,this._data);
}
if(_25){
this._connectEvents({element:"indicator",run:this.run?this.run[_24]:undefined,shape:g,value:_23});
}
if(_26){
this._animateIndicator(g,v,v?y1:x1,v?(y1+y2):(x1+x2),_26);
}
return _1.mixin(_2f,{x1:x1,y1:y1,x2:x2,y2:y2});
},_animateIndicator:function(_32,_33,_34,_35,_36){
var _37=_33?[0,_34]:[_34,0];
var _38=_33?[1,1/_35]:[1/_35,1];
fx.animateTransform(_1.delegate({shape:_32,duration:1200,transform:[{name:"translate",start:_37,end:[0,0]},{name:"scale",start:_38,end:[1,1]},{name:"original"}]},_36)).play();
},clear:function(){
this.inherited(arguments);
this._data=[];
},addSeries:function(run){
this.inherited(arguments);
this._data.push(run.data);
},_renderText:function(g,_39,t,x,y,_3a,_3b,_3c){
if(this.opt.offset){
x+=this.opt.offset.x;
y+=this.opt.offset.y;
}
var _3d=_7.createText.gfx(this.chart,g,x,y,this.opt.vertical?"middle":(this.opt.start?"start":"end"),_39,this.opt.font?this.opt.font:t.indicator.font,this.opt.fontColor?this.opt.fontColor:t.indicator.fontColor);
var b=_a(_3d);
if(this.opt.vertical&&!this.opt.start){
b.y+=b.height/2;
_3d.setShape({y:y+b.height/2});
}
b.x-=2;
b.y-=1;
b.width+=4;
b.height+=2;
b.r=this.opt.radius?this.opt.radius:t.indicator.radius;
var sh=this.opt.shadow!=undefined?this.opt.shadow:t.indicator.shadow,ls=this.opt.stroke!=undefined?this.opt.stroke:t.indicator.stroke,ol=this.opt.outline!=undefined?this.opt.outline:t.indicator.outline;
if(sh){
g.createRect(b).setFill(sh.color).setStroke(sh);
}
if(ol){
ol=_6.makeStroke(ol);
ol.width=2*ol.width+(ls?ls.width:0);
g.createRect(b).setStroke(ol);
}
var f=this.opt.fillFunc?this.opt.fillFunc(_3a,_3b,_3c):(this.opt.fill!=undefined?this.opt.fill:t.indicator.fill);
g.createRect(b).setFill(this._shapeFill(f,b)).setStroke(ls);
_3d.moveToFront();
},getSeriesStats:function(){
return _1.delegate(_6.defaultStats);
}});
if(_9("dojo-bidi")){
_15.extend({_getMarkX:function(x){
if(this.chart.isRightToLeft()){
return this.chart.axes.x.scaler.bounds.to+this.chart.axes.x.scaler.bounds.from-x;
}
return x;
}});
}
return _15;
});
