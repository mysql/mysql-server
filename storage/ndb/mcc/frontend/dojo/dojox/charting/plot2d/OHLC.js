//>>built
define("dojox/charting/plot2d/OHLC",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./Base","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,dc,df,_5,du,fx){
var _6=_5.lambda("item.purgeGroup()");
return _3("dojox.charting.plot2d.OHLC",_4,{defaultParams:{hAxis:"x",vAxis:"y",gap:2,animate:null},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:""},constructor:function(_7,_8){
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
_2.forEach(_b.data,function(_e,_f){
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
_2.forEach(this.series,_6);
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
