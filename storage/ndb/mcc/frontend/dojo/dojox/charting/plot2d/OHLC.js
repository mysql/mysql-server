//>>built
define("dojox/charting/plot2d/OHLC",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/has","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,_6,dc,df,du,fx){
return _3("dojox.charting.plot2d.OHLC",[_5,_6],{defaultParams:{gap:2,animate:null},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:""},constructor:function(_7,_8){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_8);
du.updateWithPattern(this.opt,_8,this.optionalParams);
this.animate=this.opt.animate;
},collectStats:function(_9){
var _a=_1.delegate(dc.defaultStats);
for(var i=0;i<_9.length;i++){
var _b=_9[i];
if(!_b.data.length){
continue;
}
var _c=_a.vmin,_d=_a.vmax;
if(!("ymin" in _b)||!("ymax" in _b)){
_2.forEach(_b.data,function(_e,_f){
if(!this.isNullValue(_e)){
var x=_e.x||_f+1;
_a.hmin=Math.min(_a.hmin,x);
_a.hmax=Math.max(_a.hmax,x);
_a.vmin=Math.min(_a.vmin,_e.open,_e.close,_e.high,_e.low);
_a.vmax=Math.max(_a.vmax,_e.open,_e.close,_e.high,_e.low);
}
},this);
}
if("ymin" in _b){
_a.vmin=Math.min(_c,_b.ymin);
}
if("ymax" in _b){
_a.vmax=Math.max(_d,_b.ymax);
}
}
return _a;
},getSeriesStats:function(){
var _10=this.collectStats(this.series);
_10.hmin-=0.5;
_10.hmax+=0.5;
return _10;
},render:function(dim,_11){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(dim,_11);
}
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_2.forEach(this.series,dc.purgeGroup);
this._eventSeries={};
this.cleanGroup();
var s=this.getGroup();
df.forEachRev(this.series,function(_12){
_12.cleanGroup(s);
});
}
var t=this.chart.theme,f,gap,_13,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_14=this.events();
f=dc.calculateBarSize(this._hScaler.bounds.scale,this.opt);
gap=f.gap;
_13=f.size;
for(var i=0;i<this.series.length;i++){
var run=this.series[i];
if(!this.dirty&&!run.dirty){
t.skip();
this._reconnectEvents(run.name);
continue;
}
run.cleanGroup();
var _15=t.next("candlestick",[this.opt,run]),s=run.group,_16=new Array(run.data.length);
for(var j=0;j<run.data.length;++j){
var v=run.data[j];
if(!this.isNullValue(v)){
var _17=t.addMixin(_15,"candlestick",v,true);
var x=ht(v.x||(j+0.5))+_11.l+gap,y=dim.height-_11.b,_18=vt(v.open),_19=vt(v.close),_1a=vt(v.high),low=vt(v.low);
if(low>_1a){
var tmp=_1a;
_1a=low;
low=tmp;
}
if(_13>=1){
var hl={x1:_13/2,x2:_13/2,y1:y-_1a,y2:y-low},op={x1:0,x2:((_13/2)+((_17.series.stroke?_17.series.stroke.width||1:1)/2)),y1:y-_18,y2:y-_18},cl={x1:((_13/2)-((_17.series.stroke?_17.series.stroke.width||1:1)/2)),x2:_13,y1:y-_19,y2:y-_19};
var _1b=s.createGroup();
_1b.setTransform({dx:x,dy:0});
var _1c=_1b.createGroup();
_1c.createLine(hl).setStroke(_17.series.stroke);
_1c.createLine(op).setStroke(_17.series.stroke);
_1c.createLine(cl).setStroke(_17.series.stroke);
run.dyn.stroke=_17.series.stroke;
if(_14){
var o={element:"candlestick",index:j,run:run,shape:_1c,x:x,y:y-Math.max(_18,_19),cx:_13/2,cy:(y-Math.max(_18,_19))+(Math.max(_18>_19?_18-_19:_19-_18,1)/2),width:_13,height:Math.max(_18>_19?_18-_19:_19-_18,1),data:v};
this._connectEvents(o);
_16[j]=o;
}
}
if(this.animate){
this._animateOHLC(_1b,y-low,_1a-low);
}
}
}
this._eventSeries[run.name]=_16;
run.dirty=false;
}
this.dirty=false;
if(_4("dojo-bidi")){
this._checkOrientation(this.group,dim,_11);
}
return this;
},_animateOHLC:function(_1d,_1e,_1f){
fx.animateTransform(_1.delegate({shape:_1d,duration:1200,transform:[{name:"translate",start:[0,_1e-(_1e/_1f)],end:[0,0]},{name:"scale",start:[1,1/_1f],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
