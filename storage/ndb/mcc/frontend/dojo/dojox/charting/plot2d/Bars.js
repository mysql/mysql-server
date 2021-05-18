//>>built
define("dojox/charting/plot2d/Bars",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/has","./CartesianBase","./_PlotEvents","./common","dojox/gfx/fx","dojox/lang/utils","dojox/lang/functional"],function(_1,_2,_3,_4,_5,_6,dc,fx,du,df){
var _7=function(){
return false;
};
return _3("dojox.charting.plot2d.Bars",[_5,_6],{defaultParams:{gap:0,animate:null,enableCache:false},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},filter:{},styleFunc:null,font:"",fontColor:""},constructor:function(_8,_9){
this.opt=_1.clone(_1.mixin(this.opt,this.defaultParams));
du.updateWithObject(this.opt,_9);
du.updateWithPattern(this.opt,_9,this.optionalParams);
this.animate=this.opt.animate;
this.renderingOptions={"shape-rendering":"crispEdges"};
},getSeriesStats:function(){
var _a=dc.collectSimpleStats(this.series,_1.hitch(this,"isNullValue")),t;
_a.hmin-=0.5;
_a.hmax+=0.5;
t=_a.hmin,_a.hmin=_a.vmin,_a.vmin=t;
t=_a.hmax,_a.hmax=_a.vmax,_a.vmax=t;
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
},createLabel:function(_f,_10,_11,_12){
if(this.opt.labels&&this.opt.labelStyle=="outside"){
var y=_11.y+_11.height/2;
var x=_11.x+_11.width+this.opt.labelOffset;
this.renderLabel(_f,x,y,this._getLabel(isNaN(_10.y)?_10:_10.y),_12,"start");
}else{
this.inherited(arguments);
}
},render:function(dim,_13){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(dim,_13);
}
this.dirty=this.isDirty();
this.resetEvents();
var s;
if(this.dirty){
_2.forEach(this.series,dc.purgeGroup);
this._eventSeries={};
this.cleanGroup();
s=this.getGroup();
df.forEachRev(this.series,function(_14){
_14.cleanGroup(s);
});
}
var t=this.chart.theme,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_15=Math.max(this._hScaler.bounds.lower,this._hAxis?this._hAxis.naturalBaseline:0),_16=ht(_15),_17=this.events();
var bar=this.getBarProperties();
var _18=this.series.length;
_2.forEach(this.series,function(_19){
if(_19.hidden){
_18--;
}
});
var z=_18;
var _1a=this.extractValues(this._vScaler);
_1a=this.rearrangeValues(_1a,ht,_16);
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
var _1b=t.next("bar",[this.opt,run]);
if(run.hidden){
run.dyn.fill=_1b.series.fill;
run.dyn.stroke=_1b.series.stroke;
continue;
}
z--;
var _1c=new Array(run.data.length);
s=run.group;
var _1d=_2.some(run.data,function(_1e){
return typeof _1e=="number"||(_1e&&!_1e.hasOwnProperty("x"));
});
var min=_1d?Math.max(0,Math.floor(this._vScaler.bounds.from-1)):0;
var max=_1d?Math.min(run.data.length,Math.ceil(this._vScaler.bounds.to)):run.data.length;
for(var j=min;j<max;++j){
var _1f=run.data[j];
if(!this.isNullValue(_1f)){
var val=this.getValue(_1f,j,i,_1d),w=_1a[i][j],_20,_21;
if(this.opt.styleFunc||typeof _1f!="number"){
var _22=typeof _1f!="number"?[_1f]:[];
if(this.opt.styleFunc){
_22.push(this.opt.styleFunc(_1f));
}
_20=t.addMixin(_1b,"bar",_22,true);
}else{
_20=t.post(_1b,"bar");
}
if(w&&bar.height>=1){
var _23={x:_13.l+_16+Math.min(w,0),y:dim.height-_13.b-vt(val.x+1.5)+bar.gap+bar.thickness*(_18-z-1),width:Math.abs(w),height:bar.height};
if(_20.series.shadow){
var _24=_1.clone(_23);
_24.x+=_20.series.shadow.dx;
_24.y+=_20.series.shadow.dy;
_21=this.createRect(run,s,_24).setFill(_20.series.shadow.color).setStroke(_20.series.shadow);
if(this.animate){
this._animateBar(_21,_13.l+_16,-w);
}
}
var _25=this._plotFill(_20.series.fill,dim,_13);
_25=this._shapeFill(_25,_23);
var _26=this.createRect(run,s,_23).setFill(_25).setStroke(_20.series.stroke);
if(_26.setFilter&&_20.series.filter){
_26.setFilter(_20.series.filter);
}
run.dyn.fill=_26.getFill();
run.dyn.stroke=_26.getStroke();
if(_17){
var o={element:"bar",index:j,run:run,shape:_26,shadow:_21,cx:val.y,cy:val.x+1.5,x:_1d?j:run.data[j].x,y:_1d?run.data[j]:run.data[j].y};
this._connectEvents(o);
_1c[j]=o;
}
if(!isNaN(val.py)&&val.py>_15){
_23.x+=ht(val.py);
_23.width-=ht(val.py);
}
this.createLabel(s,_1f,_23,_20);
if(this.animate){
this._animateBar(_26,_13.l+_16,-Math.abs(w));
}
}
}
}
this._eventSeries[run.name]=_1c;
run.dirty=false;
}
this.dirty=false;
if(_4("dojo-bidi")){
this._checkOrientation(this.group,dim,_13);
}
return this;
},getValue:function(_27,j,_28,_29){
var y,x;
if(_29){
if(typeof _27=="number"){
y=_27;
}else{
y=_27.y;
}
x=j;
}else{
y=_27.y;
x=_27.x-1;
}
return {y:y,x:x};
},extractValues:function(_2a){
var _2b=[];
for(var i=this.series.length-1;i>=0;--i){
var run=this.series[i];
if(!this.dirty&&!run.dirty){
continue;
}
var _2c=_2.some(run.data,function(_2d){
return typeof _2d=="number"||(_2d&&!_2d.hasOwnProperty("x"));
}),min=_2c?Math.max(0,Math.floor(_2a.bounds.from-1)):0,max=_2c?Math.min(run.data.length,Math.ceil(_2a.bounds.to)):run.data.length,_2e=_2b[i]=[];
_2e.min=min;
_2e.max=max;
for(var j=min;j<max;++j){
var _2f=run.data[j];
_2e[j]=this.isNullValue(_2f)?0:(typeof _2f=="number"?_2f:_2f.y);
}
}
return _2b;
},rearrangeValues:function(_30,_31,_32){
for(var i=0,n=_30.length;i<n;++i){
var _33=_30[i];
if(_33){
for(var j=_33.min,k=_33.max;j<k;++j){
var _34=_33[j];
_33[j]=this.isNullValue(_34)?0:_31(_34)-_32;
}
}
}
return _30;
},isNullValue:function(_35){
if(_35===null||typeof _35=="undefined"){
return true;
}
var h=this._hAxis?this._hAxis.isNullValue:_7,v=this._vAxis?this._vAxis.isNullValue:_7;
if(typeof _35=="number"){
return v(0.5)||h(_35);
}
return v(isNaN(_35.x)?0.5:_35.x+0.5)||_35.y===null||h(_35.y);
},getBarProperties:function(){
var f=dc.calculateBarSize(this._vScaler.bounds.scale,this.opt);
return {gap:f.gap,height:f.size,thickness:0};
},_animateBar:function(_36,_37,_38){
if(_38==0){
_38=1;
}
fx.animateTransform(_1.delegate({shape:_36,duration:1200,transform:[{name:"translate",start:[_37-(_37/_38),0],end:[0,0]},{name:"scale",start:[1/_38,1],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
