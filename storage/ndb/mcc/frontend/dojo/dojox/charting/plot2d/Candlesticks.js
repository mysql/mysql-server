//>>built
define("dojox/charting/plot2d/Candlesticks",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","./Base","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,dc,df,_5,du,fx){
var _6=_5.lambda("item.purgeGroup()");
return _2("dojox.charting.plot2d.Candlesticks",_4,{defaultParams:{hAxis:"x",vAxis:"y",gap:2,animate:null},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:""},constructor:function(_7,_8){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_8);
du.updateWithPattern(this.opt,_8,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
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
if(_e!==null){
var x=_e.x||_f+1;
_a.hmin=Math.min(_a.hmin,x);
_a.hmax=Math.max(_a.hmax,x);
_a.vmin=Math.min(_a.vmin,_e.open,_e.close,_e.high,_e.low);
_a.vmax=Math.max(_a.vmax,_e.open,_e.close,_e.high,_e.low);
}
});
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
_3.forEach(this.series,_6);
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
