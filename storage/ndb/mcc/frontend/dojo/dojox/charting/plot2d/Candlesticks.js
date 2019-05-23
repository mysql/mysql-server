//>>built
define("dojox/charting/plot2d/Candlesticks",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,dc,df,_6,du,fx){
var _7=_6.lambda("item.purgeGroup()");
return _2("dojox.charting.plot2d.Candlesticks",[_4,_5],{defaultParams:{hAxis:"x",vAxis:"y",gap:2,animate:null},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:""},constructor:function(_8,_9){
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
_3.forEach(_c.data,function(_f,idx){
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
_3.forEach(this.series,_7);
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
if("mid" in v){
var mid=vt(v.mid);
}
if(low>_1c){
var tmp=_1c;
_1c=low;
low=tmp;
}
if(_13>=1){
var _1d=_1a>_1b;
var _1e={x1:_13/2,x2:_13/2,y1:y-_1c,y2:y-low},_1f={x:0,y:y-Math.max(_1a,_1b),width:_13,height:Math.max(_1d?_1a-_1b:_1b-_1a,1)};
var _20=s.createGroup();
_20.setTransform({dx:x,dy:0});
var _21=_20.createGroup();
_21.createLine(_1e).setStroke(_19.series.stroke);
_21.createRect(_1f).setStroke(_19.series.stroke).setFill(_1d?_19.series.fill:"white");
if("mid" in v){
_21.createLine({x1:(_19.series.stroke.width||1),x2:_13-(_19.series.stroke.width||1),y1:y-mid,y2:y-mid}).setStroke(_1d?"white":_19.series.stroke);
}
run.dyn.fill=_19.series.fill;
run.dyn.stroke=_19.series.stroke;
if(_16){
var o={element:"candlestick",index:j,run:run,shape:_21,x:x,y:y-Math.max(_1a,_1b),cx:_13/2,cy:(y-Math.max(_1a,_1b))+(Math.max(_1d?_1a-_1b:_1b-_1a,1)/2),width:_13,height:Math.max(_1d?_1a-_1b:_1b-_1a,1),data:v};
this._connectEvents(o);
_18[j]=o;
}
}
if(this.animate){
this._animateCandlesticks(_20,y-low,_1c-low);
}
}
}
this._eventSeries[run.name]=_18;
run.dirty=false;
}
this.dirty=false;
return this;
},_animateCandlesticks:function(_22,_23,_24){
fx.animateTransform(_1.delegate({shape:_22,duration:1200,transform:[{name:"translate",start:[0,_23-(_23/_24)],end:[0,0]},{name:"scale",start:[1,1/_24],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
