//>>built
define("dojox/charting/plot2d/Bubble",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/has","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,_6,dc,df,du,fx){
return _2("dojox.charting.plot2d.Bubble",[_5,_6],{defaultParams:{animate:null},optionalParams:{stroke:{},outline:{},shadow:{},fill:{},filter:{},styleFunc:null,font:"",fontColor:"",labelFunc:null},constructor:function(_7,_8){
this.opt=_1.clone(_1.mixin(this.opt,this.defaultParams));
du.updateWithObject(this.opt,_8);
du.updateWithPattern(this.opt,_8,this.optionalParams);
if(!this.opt.labelFunc){
this.opt.labelFunc=function(_9,_a,_b){
return this._getLabel(_9.size,_a,_b);
};
}
this.animate=this.opt.animate;
},render:function(_c,_d){
var s;
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_c,_d);
}
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_3.forEach(this.series,dc.purgeGroup);
this._eventSeries={};
this.cleanGroup();
s=this.getGroup();
df.forEachRev(this.series,function(_e){
_e.cleanGroup(s);
});
}
var t=this.chart.theme,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_f=this.events();
for(var i=0;i<this.series.length;i++){
var run=this.series[i];
if(!this.dirty&&!run.dirty){
t.skip();
this._reconnectEvents(run.name);
continue;
}
run.cleanGroup();
if(!run.data.length){
run.dirty=false;
t.skip();
continue;
}
if(typeof run.data[0]=="number"){
console.warn("dojox.charting.plot2d.Bubble: the data in the following series cannot be rendered as a bubble chart; ",run);
continue;
}
var _10=t.next("circle",[this.opt,run]),_11=_3.map(run.data,function(v){
return v?{x:ht(v.x)+_d.l,y:_c.height-_d.b-vt(v.y),radius:this._vScaler.bounds.scale*(v.size/2)}:null;
},this);
if(run.hidden){
run.dyn.fill=_10.series.fill;
run.dyn.stroke=_10.series.stroke;
continue;
}
s=run.group;
var _12=null,_13=null,_14=null,_15=this.opt.styleFunc;
var _16=function(_17){
if(_15){
return t.addMixin(_10,"circle",[_17,_15(_17)],true);
}
return t.addMixin(_10,"circle",_17,true);
};
if(_10.series.shadow){
_14=_3.map(_11,function(_18,i){
if(!this.isNullValue(_18)){
var _19=_16(run.data[i]),_1a=_19.series.shadow;
var _1b=s.createCircle({cx:_18.x+_1a.dx,cy:_18.y+_1a.dy,r:_18.radius}).setStroke(_1a).setFill(_1a.color);
if(this.animate){
this._animateBubble(_1b,_c.height-_d.b,_18.radius);
}
return _1b;
}
return null;
},this);
if(_14.length){
run.dyn.shadow=_14[_14.length-1].getStroke();
}
}
if(_10.series.outline){
_13=_3.map(_11,function(_1c,i){
if(!this.isNullValue(_1c)){
var _1d=_16(run.data[i]),_1e=dc.makeStroke(_1d.series.outline);
_1e.width=2*_1e.width+(_10.series.stroke&&_10.series.stroke.width||0);
var _1f=s.createCircle({cx:_1c.x,cy:_1c.y,r:_1c.radius}).setStroke(_1e);
if(this.animate){
this._animateBubble(_1f,_c.height-_d.b,_1c.radius);
}
return _1f;
}
return null;
},this);
if(_13.length){
run.dyn.outline=_13[_13.length-1].getStroke();
}
}
_12=_3.map(_11,function(_20,i){
if(!this.isNullValue(_20)){
var _21=_16(run.data[i]),_22={x:_20.x-_20.radius,y:_20.y-_20.radius,width:2*_20.radius,height:2*_20.radius};
var _23=this._plotFill(_21.series.fill,_c,_d);
_23=this._shapeFill(_23,_22);
var _24=s.createCircle({cx:_20.x,cy:_20.y,r:_20.radius}).setFill(_23).setStroke(_21.series.stroke);
if(_24.setFilter&&_21.series.filter){
_24.setFilter(_21.series.filter);
}
if(this.animate){
this._animateBubble(_24,_c.height-_d.b,_20.radius);
}
this.createLabel(s,run.data[i],_22,_21);
return _24;
}
return null;
},this);
if(_12.length){
run.dyn.fill=_12[_12.length-1].getFill();
run.dyn.stroke=_12[_12.length-1].getStroke();
}
if(_f){
var _25=new Array(_12.length);
_3.forEach(_12,function(s,i){
if(s!==null){
var o={element:"circle",index:i,run:run,shape:s,outline:_13&&_13[i]||null,shadow:_14&&_14[i]||null,x:run.data[i].x,y:run.data[i].y,r:run.data[i].size/2,cx:_11[i].x,cy:_11[i].y,cr:_11[i].radius};
this._connectEvents(o);
_25[i]=o;
}
},this);
this._eventSeries[run.name]=_25;
}else{
delete this._eventSeries[run.name];
}
run.dirty=false;
}
this.dirty=false;
if(_4("dojo-bidi")){
this._checkOrientation(this.group,_c,_d);
}
return this;
},_animateBubble:function(_26,_27,_28){
fx.animateTransform(_1.delegate({shape:_26,duration:1200,transform:[{name:"translate",start:[0,_27],end:[0,0]},{name:"scale",start:[0,1/_28],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
