//>>built
define("dojox/charting/plot2d/Columns",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/has","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,_6,dc,df,du,fx){
var _7=function(){
return false;
};
return _3("dojox.charting.plot2d.Columns",[_5,_6],{defaultParams:{gap:0,animate:null,enableCache:false},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},filter:{},styleFunc:null,font:"",fontColor:""},constructor:function(_8,_9){
this.opt=_1.clone(_1.mixin(this.opt,this.defaultParams));
du.updateWithObject(this.opt,_9);
du.updateWithPattern(this.opt,_9,this.optionalParams);
this.animate=this.opt.animate;
this.renderingOptions={"shape-rendering":"crispEdges"};
},getSeriesStats:function(){
var _a=dc.collectSimpleStats(this.series,_1.hitch(this,"isNullValue"));
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
this.resetEvents();
this.dirty=this.isDirty();
var s;
if(this.dirty){
_2.forEach(this.series,dc.purgeGroup);
this._eventSeries={};
this.cleanGroup();
s=this.getGroup();
df.forEachRev(this.series,function(_11){
_11.cleanGroup(s);
});
}
var t=this.chart.theme,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_12=Math.max(this._vScaler.bounds.lower,this._vAxis?this._vAxis.naturalBaseline:0),_13=vt(_12),_14=this.events(),bar=this.getBarProperties();
var z=0;
var _15=this.extractValues(this._hScaler);
_15=this.rearrangeValues(_15,vt,_13);
for(var i=0;i<this.series.length;i++){
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
var _16=t.next("column",[this.opt,run]),_17=new Array(run.data.length);
if(run.hidden){
run.dyn.fill=_16.series.fill;
continue;
}
s=run.group;
var _18=_2.some(run.data,function(_19){
return typeof _19=="number"||(_19&&!_19.hasOwnProperty("x"));
});
var min=_18?Math.max(0,Math.floor(this._hScaler.bounds.from-1)):0;
var max=_18?Math.min(run.data.length,Math.ceil(this._hScaler.bounds.to)):run.data.length;
for(var j=min;j<max;++j){
var _1a=run.data[j];
if(!this.isNullValue(_1a)){
var val=this.getValue(_1a,j,i,_18),vv=vt(val.y),h=_15[i][j],_1b,_1c;
if(this.opt.styleFunc||typeof _1a!="number"){
var _1d=typeof _1a!="number"?[_1a]:[];
if(this.opt.styleFunc){
_1d.push(this.opt.styleFunc(_1a));
}
_1b=t.addMixin(_16,"column",_1d,true);
}else{
_1b=t.post(_16,"column");
}
if(bar.width>=1){
var _1e={x:_10.l+ht(val.x+0.5)+bar.gap+bar.thickness*z,y:_f.height-_10.b-_13-Math.max(h,0),width:bar.width,height:Math.abs(h)};
if(_1b.series.shadow){
var _1f=_1.clone(_1e);
_1f.x+=_1b.series.shadow.dx;
_1f.y+=_1b.series.shadow.dy;
_1c=this.createRect(run,s,_1f).setFill(_1b.series.shadow.color).setStroke(_1b.series.shadow);
if(this.animate){
this._animateColumn(_1c,_f.height-_10.b+_13,h);
}
}
var _20=this._plotFill(_1b.series.fill,_f,_10);
_20=this._shapeFill(_20,_1e);
var _21=this.createRect(run,s,_1e).setFill(_20).setStroke(_1b.series.stroke);
this.overrideShape(_21,{index:j,value:val});
if(_21.setFilter&&_1b.series.filter){
_21.setFilter(_1b.series.filter);
}
run.dyn.fill=_21.getFill();
run.dyn.stroke=_21.getStroke();
if(_14){
var o={element:"column",index:j,run:run,shape:_21,shadow:_1c,cx:val.x+0.5,cy:val.y,x:_18?j:run.data[j].x,y:_18?run.data[j]:run.data[j].y};
this._connectEvents(o);
_17[j]=o;
}
if(!isNaN(val.py)&&val.py>_12){
_1e.height=h-vt(val.py);
}
this.createLabel(s,_1a,_1e,_1b);
if(this.animate){
this._animateColumn(_21,_f.height-_10.b-_13,h);
}
}
}
}
this._eventSeries[run.name]=_17;
run.dirty=false;
z++;
}
this.dirty=false;
if(_4("dojo-bidi")){
this._checkOrientation(this.group,_f,_10);
}
return this;
},getValue:function(_22,j,_23,_24){
var y,x;
if(_24){
if(typeof _22=="number"){
y=_22;
}else{
y=_22.y;
}
x=j;
}else{
y=_22.y;
x=_22.x-1;
}
return {x:x,y:y};
},extractValues:function(_25){
var _26=[];
for(var i=this.series.length-1;i>=0;--i){
var run=this.series[i];
if(!this.dirty&&!run.dirty){
continue;
}
var _27=_2.some(run.data,function(_28){
return typeof _28=="number"||(_28&&!_28.hasOwnProperty("x"));
}),min=_27?Math.max(0,Math.floor(_25.bounds.from-1)):0,max=_27?Math.min(run.data.length,Math.ceil(_25.bounds.to)):run.data.length,_29=_26[i]=[];
_29.min=min;
_29.max=max;
for(var j=min;j<max;++j){
var _2a=run.data[j];
_29[j]=this.isNullValue(_2a)?0:(typeof _2a=="number"?_2a:_2a.y);
}
}
return _26;
},rearrangeValues:function(_2b,_2c,_2d){
for(var i=0,n=_2b.length;i<n;++i){
var _2e=_2b[i];
if(_2e){
for(var j=_2e.min,k=_2e.max;j<k;++j){
var _2f=_2e[j];
_2e[j]=this.isNullValue(_2f)?0:_2c(_2f)-_2d;
}
}
}
return _2b;
},isNullValue:function(_30){
if(_30===null||typeof _30=="undefined"){
return true;
}
var h=this._hAxis?this._hAxis.isNullValue:_7,v=this._vAxis?this._vAxis.isNullValue:_7;
if(typeof _30=="number"){
return v(0.5)||h(_30);
}
return v(isNaN(_30.x)?0.5:_30.x+0.5)||_30.y===null||h(_30.y);
},getBarProperties:function(){
var f=dc.calculateBarSize(this._hScaler.bounds.scale,this.opt);
return {gap:f.gap,width:f.size,thickness:0};
},_animateColumn:function(_31,_32,_33){
if(_33===0){
_33=1;
}
fx.animateTransform(_1.delegate({shape:_31,duration:1200,transform:[{name:"translate",start:[0,_32-(_32/_33)],end:[0,0]},{name:"scale",start:[1,1/_33],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
