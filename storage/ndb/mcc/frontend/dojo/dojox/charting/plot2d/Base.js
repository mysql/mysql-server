//>>built
define("dojox/charting/plot2d/Base",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","../Element","./_PlotEvents","dojo/_base/array","../scaler/primitive","./common","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,_6,_7,_8,fx){
return _2("dojox.charting.plot2d.Base",[_4,_5],{constructor:function(_9,_a){
this.zoom=null,this.zoomQueue=[];
this.lastWindow={vscale:1,hscale:1,xoffset:0,yoffset:0};
},clear:function(){
this.series=[];
this._hAxis=null;
this._vAxis=null;
this.dirty=true;
return this;
},setAxis:function(_b){
if(_b){
this[_b.vertical?"_vAxis":"_hAxis"]=_b;
}
return this;
},toPage:function(_c){
var ah=this._hAxis,av=this._vAxis,sh=ah.getScaler(),sv=av.getScaler(),th=sh.scaler.getTransformerFromModel(sh),tv=sv.scaler.getTransformerFromModel(sv),c=this.chart.getCoords(),o=this.chart.offsets,_d=this.chart.dim;
var t=function(_e){
var r={};
r.x=th(_e[ah.name])+c.x+o.l;
r.y=c.y+_d.height-o.b-tv(_e[av.name]);
return r;
};
return _c?t(_c):t;
},toData:function(_f){
var ah=this._hAxis,av=this._vAxis,sh=ah.getScaler(),sv=av.getScaler(),th=sh.scaler.getTransformerFromPlot(sh),tv=sv.scaler.getTransformerFromPlot(sv),c=this.chart.getCoords(),o=this.chart.offsets,dim=this.chart.dim;
var t=function(_10){
var r={};
r[ah.name]=th(_10.x-c.x-o.l);
r[av.name]=tv(c.y+dim.height-_10.y-o.b);
return r;
};
return _f?t(_f):t;
},addSeries:function(run){
this.series.push(run);
return this;
},getSeriesStats:function(){
return _8.collectSimpleStats(this.series);
},calculateAxes:function(dim){
this.initializeScalers(dim,this.getSeriesStats());
return this;
},isDirty:function(){
return this.dirty||this._hAxis&&this._hAxis.dirty||this._vAxis&&this._vAxis.dirty;
},isDataDirty:function(){
return _6.some(this.series,function(_11){
return _11.dirty;
});
},performZoom:function(dim,_12){
var vs=this._vAxis.scale||1,hs=this._hAxis.scale||1,_13=dim.height-_12.b,_14=this._hScaler.bounds,_15=(_14.from-_14.lower)*_14.scale,_16=this._vScaler.bounds,_17=(_16.from-_16.lower)*_16.scale,_18=vs/this.lastWindow.vscale,_19=hs/this.lastWindow.hscale,_1a=(this.lastWindow.xoffset-_15)/((this.lastWindow.hscale==1)?hs:this.lastWindow.hscale),_1b=(_17-this.lastWindow.yoffset)/((this.lastWindow.vscale==1)?vs:this.lastWindow.vscale),_1c=this.group,_1d=fx.animateTransform(_1.delegate({shape:_1c,duration:1200,transform:[{name:"translate",start:[0,0],end:[_12.l*(1-_19),_13*(1-_18)]},{name:"scale",start:[1,1],end:[_19,_18]},{name:"original"},{name:"translate",start:[0,0],end:[_1a,_1b]}]},this.zoom));
_1.mixin(this.lastWindow,{vscale:vs,hscale:hs,xoffset:_15,yoffset:_17});
this.zoomQueue.push(_1d);
_3.connect(_1d,"onEnd",this,function(){
this.zoom=null;
this.zoomQueue.shift();
if(this.zoomQueue.length>0){
this.zoomQueue[0].play();
}
});
if(this.zoomQueue.length==1){
this.zoomQueue[0].play();
}
return this;
},render:function(dim,_1e){
return this;
},getRequiredColors:function(){
return this.series.length;
},initializeScalers:function(dim,_1f){
if(this._hAxis){
if(!this._hAxis.initialized()){
this._hAxis.calculate(_1f.hmin,_1f.hmax,dim.width);
}
this._hScaler=this._hAxis.getScaler();
}else{
this._hScaler=_7.buildScaler(_1f.hmin,_1f.hmax,dim.width);
}
if(this._vAxis){
if(!this._vAxis.initialized()){
this._vAxis.calculate(_1f.vmin,_1f.vmax,dim.height);
}
this._vScaler=this._vAxis.getScaler();
}else{
this._vScaler=_7.buildScaler(_1f.vmin,_1f.vmax,dim.height);
}
return this;
}});
});
