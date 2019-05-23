//>>built
define("dojox/charting/plot2d/Columns",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,dc,df,_6,du,fx){
var _7=_6.lambda("item.purgeGroup()");
return _3("dojox.charting.plot2d.Columns",[_4,_5],{defaultParams:{hAxis:"x",vAxis:"y",gap:0,animate:null,enableCache:false},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},styleFunc:null,font:"",fontColor:""},constructor:function(_8,_9){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_9);
du.updateWithPattern(this.opt,_9,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.animate=this.opt.animate;
},getSeriesStats:function(){
var _a=dc.collectSimpleStats(this.series);
_a.hmin-=0.5;
_a.hmax+=0.5;
return _a;
},createRect:function(_b,_c,_d){
var _e;
if(this.opt.enableCache&&_b._rectFreePool.length>0){
_e=_b._rectFreePool.pop();
_e.setShape(_d);
_c.add(_e);
}else{
_e=_c.createRect(_d);
}
if(this.opt.enableCache){
_b._rectUsePool.push(_e);
}
return _e;
},render:function(_f,_10){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_f,_10);
}
this.getSeriesStats();
this.resetEvents();
this.dirty=this.isDirty();
var s;
if(this.dirty){
_2.forEach(this.series,_7);
this._eventSeries={};
this.cleanGroup();
s=this.group;
df.forEachRev(this.series,function(_11){
_11.cleanGroup(s);
});
}
var t=this.chart.theme,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_12=Math.max(0,this._vScaler.bounds.lower),_13=vt(_12),_14=this.events();
var bar=this.getBarProperties();
for(var i=this.series.length-1;i>=0;--i){
var run=this.series[i];
if(!this.dirty&&!run.dirty){
t.skip();
this._reconnectEvents(run.name);
continue;
}
run.cleanGroup();
if(this.opt.enableCache){
run._rectFreePool=(run._rectFreePool?run._rectFreePool:[]).concat(run._rectUsePool?run._rectUsePool:[]);
run._rectUsePool=[];
}
var _15=t.next("column",[this.opt,run]),_16=new Array(run.data.length);
s=run.group;
var _17=_2.some(run.data,function(_18){
return typeof _18=="number"||(_18&&!_18.hasOwnProperty("x"));
});
var min=_17?Math.max(0,Math.floor(this._hScaler.bounds.from-1)):0;
var max=_17?Math.min(run.data.length,Math.ceil(this._hScaler.bounds.to)):run.data.length;
for(var j=min;j<max;++j){
var _19=run.data[j];
if(_19!=null){
var val=this.getValue(_19,j,i,_17),vv=vt(val.y),h=Math.abs(vv-_13),_1a,_1b;
if(this.opt.styleFunc||typeof _19!="number"){
var _1c=typeof _19!="number"?[_19]:[];
if(this.opt.styleFunc){
_1c.push(this.opt.styleFunc(_19));
}
_1a=t.addMixin(_15,"column",_1c,true);
}else{
_1a=t.post(_15,"column");
}
if(bar.width>=1&&h>=0){
var _1d={x:_10.l+ht(val.x+0.5)+bar.gap+bar.thickness*i,y:_f.height-_10.b-(val.y>_12?vv:_13),width:bar.width,height:h};
if(_1a.series.shadow){
var _1e=_1.clone(_1d);
_1e.x+=_1a.series.shadow.dx;
_1e.y+=_1a.series.shadow.dy;
_1b=this.createRect(run,s,_1e).setFill(_1a.series.shadow.color).setStroke(_1a.series.shadow);
if(this.animate){
this._animateColumn(_1b,_f.height-_10.b+_13,h);
}
}
var _1f=this._plotFill(_1a.series.fill,_f,_10);
_1f=this._shapeFill(_1f,_1d);
var _20=this.createRect(run,s,_1d).setFill(_1f).setStroke(_1a.series.stroke);
run.dyn.fill=_20.getFill();
run.dyn.stroke=_20.getStroke();
if(_14){
var o={element:"column",index:j,run:run,shape:_20,shadow:_1b,x:val.x+0.5,y:val.y};
this._connectEvents(o);
_16[j]=o;
}
if(this.animate){
this._animateColumn(_20,_f.height-_10.b-_13,h);
}
}
}
}
this._eventSeries[run.name]=_16;
run.dirty=false;
}
this.dirty=false;
return this;
},getValue:function(_21,j,_22,_23){
var y,x;
if(_23){
if(typeof _21=="number"){
y=_21;
}else{
y=_21.y;
}
x=j;
}else{
y=_21.y;
x=_21.x-1;
}
return {y:y,x:x};
},getBarProperties:function(){
var f=dc.calculateBarSize(this._hScaler.bounds.scale,this.opt);
return {gap:f.gap,width:f.size,thickness:0};
},_animateColumn:function(_24,_25,_26){
if(_26==0){
_26=1;
}
fx.animateTransform(_1.delegate({shape:_24,duration:1200,transform:[{name:"translate",start:[0,_25-(_25/_26)],end:[0,0]},{name:"scale",start:[1,1/_26],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
