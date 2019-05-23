//>>built
define("dojox/charting/plot2d/OHLC",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,dc,df,_6,du,fx){
var _7=_6.lambda("item.purgeGroup()");
return _3("dojox.charting.plot2d.OHLC",[_4,_5],{defaultParams:{hAxis:"x",vAxis:"y",gap:2,animate:null},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:""},constructor:function(_8,_9){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_9);
du.updateWithPattern(this.opt,_9,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.animate=this.opt.animate;
},collectStats:function(_a){
var _b=_1.delegate(dc.defaultStats);
for(var i=0;i<_a.length;i++){
var _c=_a[i];
if(!_c.data.length){
continue;
}
var _d=_b.vmin,_e=_b.vmax;
if(!("ymin" in _c)||!("ymax" in _c)){
_2.forEach(_c.data,function(_f,idx){
if(_f!==null){
var x=_f.x||idx+1;
_b.hmin=Math.min(_b.hmin,x);
_b.hmax=Math.max(_b.hmax,x);
_b.vmin=Math.min(_b.vmin,_f.open,_f.close,_f.high,_f.low);
_b.vmax=Math.max(_b.vmax,_f.open,_f.close,_f.high,_f.low);
}
});
}
if("ymin" in _c){
_b.vmin=Math.min(_d,_c.ymin);
}
if("ymax" in _c){
_b.vmax=Math.max(_e,_c.ymax);
}
}
return _b;
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
_2.forEach(this.series,_7);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_12){
_12.cleanGroup(s);
});
}
var t=this.chart.theme,f,gap,_13,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_14=Math.max(0,this._vScaler.bounds.lower),_15=vt(_14),_16=this.events();
f=dc.calculateBarSize(this._hScaler.bounds.scale,this.opt);
gap=f.gap;
_13=f.size;
for(var i=this.series.length-1;i>=0;--i){
var run=this.series[i];
if(!this.dirty&&!run.dirty){
t.skip();
this._reconnectEvents(run.name);
continue;
}
run.cleanGroup();
var _17=t.next("candlestick",[this.opt,run]),s=run.group,_18=new Array(run.data.length);
for(var j=0;j<run.data.length;++j){
var v=run.data[j];
if(v!==null){
var _19=t.addMixin(_17,"candlestick",v,true);
var x=ht(v.x||(j+0.5))+_11.l+gap,y=dim.height-_11.b,_1a=vt(v.open),_1b=vt(v.close),_1c=vt(v.high),low=vt(v.low);
if(low>_1c){
var tmp=_1c;
_1c=low;
low=tmp;
}
if(_13>=1){
var hl={x1:_13/2,x2:_13/2,y1:y-_1c,y2:y-low},op={x1:0,x2:((_13/2)+((_19.series.stroke.width||1)/2)),y1:y-_1a,y2:y-_1a},cl={x1:((_13/2)-((_19.series.stroke.width||1)/2)),x2:_13,y1:y-_1b,y2:y-_1b};
var _1d=s.createGroup();
_1d.setTransform({dx:x,dy:0});
var _1e=_1d.createGroup();
_1e.createLine(hl).setStroke(_19.series.stroke);
_1e.createLine(op).setStroke(_19.series.stroke);
_1e.createLine(cl).setStroke(_19.series.stroke);
run.dyn.stroke=_19.series.stroke;
if(_16){
var o={element:"candlestick",index:j,run:run,shape:_1e,x:x,y:y-Math.max(_1a,_1b),cx:_13/2,cy:(y-Math.max(_1a,_1b))+(Math.max(_1a>_1b?_1a-_1b:_1b-_1a,1)/2),width:_13,height:Math.max(_1a>_1b?_1a-_1b:_1b-_1a,1),data:v};
this._connectEvents(o);
_18[j]=o;
}
}
if(this.animate){
this._animateOHLC(_1d,y-low,_1c-low);
}
}
}
this._eventSeries[run.name]=_18;
run.dirty=false;
}
this.dirty=false;
return this;
},_animateOHLC:function(_1f,_20,_21){
fx.animateTransform(_1.delegate({shape:_1f,duration:1200,transform:[{name:"translate",start:[0,_20-(_20/_21)],end:[0,0]},{name:"scale",start:[1,1/_21],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
