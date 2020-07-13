//>>built
define("dojox/charting/plot2d/Candlesticks",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/has","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,_6,dc,df,du,fx){
return _2("dojox.charting.plot2d.Candlesticks",[_5,_6],{defaultParams:{gap:2,animate:null},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:""},constructor:function(_7,_8){
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
_3.forEach(_b.data,function(_e,_f){
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
var s;
if(this.dirty){
_3.forEach(this.series,dc.purgeGroup);
this._eventSeries={};
this.cleanGroup();
s=this.getGroup();
df.forEachRev(this.series,function(_12){
_12.cleanGroup(s);
});
}
var t=this.chart.theme,f,gap,_13,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_14=this.events();
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
var _15=t.next("candlestick",[this.opt,run]),_16=new Array(run.data.length);
if(run.hidden){
run.dyn.fill=_15.series.fill;
run.dyn.stroke=_15.series.stroke;
continue;
}
s=run.group;
for(var j=0;j<run.data.length;++j){
var v=run.data[j];
if(!this.isNullValue(v)){
var _17=t.addMixin(_15,"candlestick",v,true);
var x=ht(v.x||(j+0.5))+_11.l+gap,y=dim.height-_11.b,_18=vt(v.open),_19=vt(v.close),_1a=vt(v.high),low=vt(v.low);
if("mid" in v){
var mid=vt(v.mid);
}
if(low>_1a){
var tmp=_1a;
_1a=low;
low=tmp;
}
if(_13>=1){
var _1b=_18>_19;
var _1c={x1:_13/2,x2:_13/2,y1:y-_1a,y2:y-low},_1d={x:0,y:y-Math.max(_18,_19),width:_13,height:Math.max(_1b?_18-_19:_19-_18,1)};
var _1e=s.createGroup();
_1e.setTransform({dx:x,dy:0});
var _1f=_1e.createGroup();
_1f.createLine(_1c).setStroke(_17.series.stroke);
_1f.createRect(_1d).setStroke(_17.series.stroke).setFill(_1b?_17.series.fill:"white");
if("mid" in v){
_1f.createLine({x1:(_17.series.stroke?_17.series.stroke.width||1:1),x2:_13-(_17.series.stroke?_17.series.stroke.width||1:1),y1:y-mid,y2:y-mid}).setStroke(_1b?"white":_17.series.stroke);
}
run.dyn.fill=_17.series.fill;
run.dyn.stroke=_17.series.stroke;
if(_14){
var o={element:"candlestick",index:j,run:run,shape:_1f,x:x,y:y-Math.max(_18,_19),cx:_13/2,cy:(y-Math.max(_18,_19))+(Math.max(_1b?_18-_19:_19-_18,1)/2),width:_13,height:Math.max(_1b?_18-_19:_19-_18,1),data:v};
this._connectEvents(o);
_16[j]=o;
}
}
if(this.animate){
this._animateCandlesticks(_1e,y-low,_1a-low);
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
},tooltipFunc:function(o){
return "<table cellpadding=\"1\" cellspacing=\"0\" border=\"0\" style=\"font-size:0.9em;\">"+"<tr><td>Open:</td><td align=\"right\"><strong>"+o.data.open+"</strong></td></tr>"+"<tr><td>High:</td><td align=\"right\"><strong>"+o.data.high+"</strong></td></tr>"+"<tr><td>Low:</td><td align=\"right\"><strong>"+o.data.low+"</strong></td></tr>"+"<tr><td>Close:</td><td align=\"right\"><strong>"+o.data.close+"</strong></td></tr>"+(o.data.mid!==undefined?"<tr><td>Mid:</td><td align=\"right\"><strong>"+o.data.mid+"</strong></td></tr>":"")+"</table>";
},_animateCandlesticks:function(_20,_21,_22){
fx.animateTransform(_1.delegate({shape:_20,duration:1200,transform:[{name:"translate",start:[0,_21-(_21/_22)],end:[0,0]},{name:"scale",start:[1,1/_22],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
