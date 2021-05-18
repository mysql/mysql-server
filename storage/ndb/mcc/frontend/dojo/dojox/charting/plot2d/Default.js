//>>built
define("dojox/charting/plot2d/Default",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/has","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,_6,dc,df,du,fx){
var _7=1200;
return _2("dojox.charting.plot2d.Default",[_5,_6],{defaultParams:{lines:true,areas:false,markers:false,tension:"",animate:false,enableCache:false,interpolate:false},optionalParams:{stroke:{},outline:{},shadow:{},fill:{},filter:{},styleFunc:null,font:"",fontColor:"",marker:"",markerStroke:{},markerOutline:{},markerShadow:{},markerFill:{},markerFont:"",markerFontColor:"",zeroLine:0},constructor:function(_8,_9){
this.opt=_1.clone(_1.mixin(this.opt,this.defaultParams));
du.updateWithObject(this.opt,_9);
du.updateWithPattern(this.opt,_9,this.optionalParams);
this.animate=this.opt.animate;
},createPath:function(_a,_b,_c){
var _d;
if(this.opt.enableCache&&_a._pathFreePool.length>0){
_d=_a._pathFreePool.pop();
_d.setShape(_c);
_b.add(_d);
}else{
_d=_b.createPath(_c);
}
if(this.opt.enableCache){
_a._pathUsePool.push(_d);
}
return _d;
},buildSegments:function(i,_e){
var _f=this.series[i],min=_e?Math.max(0,Math.floor(this._hScaler.bounds.from-1)):0,max=_e?Math.min(_f.data.length,Math.ceil(this._hScaler.bounds.to)):_f.data.length,_10=null,_11=[];
for(var j=min;j<max;j++){
if(!this.isNullValue(_f.data[j])){
if(!_10){
_10=[];
_11.push({index:j,rseg:_10});
}
_10.push((_e&&_f.data[j].hasOwnProperty("y"))?_f.data[j].y:_f.data[j]);
}else{
if(!this.opt.interpolate||_e){
_10=null;
}
}
}
return _11;
},render:function(dim,_12){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(dim,_12);
}
this.resetEvents();
this.dirty=this.isDirty();
var s;
if(this.dirty){
_3.forEach(this.series,dc.purgeGroup);
this._eventSeries={};
this.cleanGroup();
this.getGroup().setTransform(null);
s=this.getGroup();
df.forEachRev(this.series,function(_13){
_13.cleanGroup(s);
});
}
var t=this.chart.theme,_14,_15,_16=this.events();
for(var i=0;i<this.series.length;i++){
var run=this.series[i];
if(!this.dirty&&!run.dirty){
t.skip();
this._reconnectEvents(run.name);
continue;
}
run.cleanGroup();
if(this.opt.enableCache){
run._pathFreePool=(run._pathFreePool?run._pathFreePool:[]).concat(run._pathUsePool?run._pathUsePool:[]);
run._pathUsePool=[];
}
if(!run.data.length){
run.dirty=false;
t.skip();
continue;
}
var _17=t.next(this.opt.areas?"area":"line",[this.opt,run],true),_18,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_19=this._eventSeries[run.name]=new Array(run.data.length);
s=run.group;
if(run.hidden){
if(this.opt.lines){
run.dyn.stroke=_17.series.stroke;
}
if(run.markers||(run.markers===undefined&&this.opt.markers)){
run.dyn.markerFill=_17.marker.fill;
run.dyn.markerStroke=_17.marker.stroke;
run.dyn.marker=_17.symbol;
}
if(this.opt.areas){
run.dyn.fill=_17.series.fill;
}
continue;
}
var _1a=_3.some(run.data,function(_1b){
return typeof _1b=="number"||(_1b&&!_1b.hasOwnProperty("x"));
});
var _1c=this.buildSegments(i,_1a);
for(var seg=0;seg<_1c.length;seg++){
var _1d=_1c[seg];
if(_1a){
_18=_3.map(_1d.rseg,function(v,i){
return {x:ht(i+_1d.index+1)+_12.l,y:dim.height-_12.b-vt(v),data:v};
},this);
}else{
_18=_3.map(_1d.rseg,function(v){
return {x:ht(v.x)+_12.l,y:dim.height-_12.b-vt(v.y),data:v};
},this);
}
if(_1a&&this.opt.interpolate){
while(seg<_1c.length){
seg++;
_1d=_1c[seg];
if(_1d){
_18=_18.concat(_3.map(_1d.rseg,function(v,i){
return {x:ht(i+_1d.index+1)+_12.l,y:dim.height-_12.b-vt(v),data:v};
},this));
}
}
}
var _1e=this.opt.tension?dc.curve(_18,this.opt.tension):"";
if(this.opt.areas&&_18.length>1){
var _1f=this._plotFill(_17.series.fill,dim,_12),_20=_1.clone(_18),_21=dim.height-_12.b;
if(!isNaN(this.opt.zeroLine)){
_21=Math.max(_12.t,Math.min(dim.height-_12.b-vt(this.opt.zeroLine),_21));
}
if(this.opt.tension){
var _22="L"+_20[_20.length-1].x+","+_21+" L"+_20[0].x+","+_21+" L"+_20[0].x+","+_20[0].y;
run.dyn.fill=s.createPath(_1e+" "+_22).setFill(_1f).getFill();
}else{
_20.push({x:_18[_18.length-1].x,y:_21});
_20.push({x:_18[0].x,y:_21});
_20.push(_18[0]);
run.dyn.fill=s.createPolyline(_20).setFill(_1f).getFill();
}
}
if(this.opt.lines||this.opt.markers){
_14=_17.series.stroke;
if(_17.series.outline){
_15=run.dyn.outline=dc.makeStroke(_17.series.outline);
_15.width=2*_15.width+(_14&&_14.width||0);
}
}
if(this.opt.markers){
run.dyn.marker=_17.symbol;
}
var _23=null,_24=null,_25=null;
if(_14&&_17.series.shadow&&_18.length>1){
var _26=_17.series.shadow,_27=_3.map(_18,function(c){
return {x:c.x+_26.dx,y:c.y+_26.dy};
});
if(this.opt.lines){
if(this.opt.tension){
run.dyn.shadow=s.createPath(dc.curve(_27,this.opt.tension)).setStroke(_26).getStroke();
}else{
run.dyn.shadow=s.createPolyline(_27).setStroke(_26).getStroke();
}
}
if(this.opt.markers&&_17.marker.shadow){
_26=_17.marker.shadow;
_25=_3.map(_27,function(c){
return this.createPath(run,s,"M"+c.x+" "+c.y+" "+_17.symbol).setStroke(_26).setFill(_26.color);
},this);
}
}
if(this.opt.lines&&_18.length>1){
var _28;
if(_15){
if(this.opt.tension){
run.dyn.outline=s.createPath(_1e).setStroke(_15).getStroke();
}else{
run.dyn.outline=s.createPolyline(_18).setStroke(_15).getStroke();
}
}
if(this.opt.tension){
run.dyn.stroke=(_28=s.createPath(_1e)).setStroke(_14).getStroke();
}else{
run.dyn.stroke=(_28=s.createPolyline(_18)).setStroke(_14).getStroke();
}
if(_28.setFilter&&_17.series.filter){
_28.setFilter(_17.series.filter);
}
}
var _29=null;
if(this.opt.markers){
var _2a=_17;
_23=new Array(_18.length);
_24=new Array(_18.length);
_15=null;
if(_2a.marker.outline){
_15=dc.makeStroke(_2a.marker.outline);
_15.width=2*_15.width+(_2a.marker.stroke?_2a.marker.stroke.width:0);
}
_3.forEach(_18,function(c,i){
if(this.opt.styleFunc||typeof c.data!="number"){
var _2b=typeof c.data!="number"?[c.data]:[];
if(this.opt.styleFunc){
_2b.push(this.opt.styleFunc(c.data));
}
_2a=t.addMixin(_17,"marker",_2b,true);
}else{
_2a=t.post(_17,"marker");
}
var _2c="M"+c.x+" "+c.y+" "+_2a.symbol;
if(_15){
_24[i]=this.createPath(run,s,_2c).setStroke(_15);
}
_23[i]=this.createPath(run,s,_2c).setStroke(_2a.marker.stroke).setFill(_2a.marker.fill);
},this);
run.dyn.markerFill=_2a.marker.fill;
run.dyn.markerStroke=_2a.marker.stroke;
if(!_29&&this.opt.labels){
_29=_23[0].getBoundingBox();
}
if(_16){
_3.forEach(_23,function(s,i){
var o={element:"marker",index:i+_1d.index,run:run,shape:s,outline:_24[i]||null,shadow:_25&&_25[i]||null,cx:_18[i].x,cy:_18[i].y};
if(_1a){
o.x=i+_1d.index+1;
o.y=run.data[i+_1d.index];
}else{
o.x=_1d.rseg[i].x;
o.y=run.data[i+_1d.index].y;
}
this._connectEvents(o);
_19[i+_1d.index]=o;
},this);
}else{
delete this._eventSeries[run.name];
}
}
if(this.opt.labels){
var _2d=_29?_29.width:2;
var _2e=_29?_29.height:2;
_3.forEach(_18,function(c,i){
if(this.opt.styleFunc||typeof c.data!="number"){
var _2f=typeof c.data!="number"?[c.data]:[];
if(this.opt.styleFunc){
_2f.push(this.opt.styleFunc(c.data));
}
_2a=t.addMixin(_17,"marker",_2f,true);
}else{
_2a=t.post(_17,"marker");
}
this.createLabel(s,_1d.rseg[i],{x:c.x-_2d/2,y:c.y-_2e/2,width:_2d,height:_2e},_2a);
},this);
}
}
run.dirty=false;
}
if(_4("dojo-bidi")){
this._checkOrientation(this.group,dim,_12);
}
if(this.animate){
var _30=this.getGroup();
fx.animateTransform(_1.delegate({shape:_30,duration:_7,transform:[{name:"translate",start:[0,dim.height-_12.b],end:[0,0]},{name:"scale",start:[1,0],end:[1,1]},{name:"original"}]},this.animate)).play();
}
this.dirty=false;
return this;
}});
});
