//>>built
define("dojox/charting/plot2d/Default",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,dc,df,_6,du,fx){
var _7=_6.lambda("item.purgeGroup()");
var _8=1200;
return _2("dojox.charting.plot2d.Default",[_4,_5],{defaultParams:{hAxis:"x",vAxis:"y",lines:true,areas:false,markers:false,tension:"",animate:false,enableCache:false,interpolate:false},optionalParams:{stroke:{},outline:{},shadow:{},fill:{},styleFunc:null,font:"",fontColor:"",marker:"",markerStroke:{},markerOutline:{},markerShadow:{},markerFill:{},markerFont:"",markerFontColor:""},constructor:function(_9,_a){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_a);
du.updateWithPattern(this.opt,_a,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.animate=this.opt.animate;
},createPath:function(_b,_c,_d){
var _e;
if(this.opt.enableCache&&_b._pathFreePool.length>0){
_e=_b._pathFreePool.pop();
_e.setShape(_d);
_c.add(_e);
}else{
_e=_c.createPath(_d);
}
if(this.opt.enableCache){
_b._pathUsePool.push(_e);
}
return _e;
},buildSegments:function(i,_f){
var run=this.series[i],min=_f?Math.max(0,Math.floor(this._hScaler.bounds.from-1)):0,max=_f?Math.min(run.data.length,Math.ceil(this._hScaler.bounds.to)):run.data.length,_10=null,_11=[];
for(var j=min;j<max;j++){
if(run.data[j]!=null&&(_f||run.data[j].y!=null)){
if(!_10){
_10=[];
_11.push({index:j,rseg:_10});
}
_10.push((_f&&run.data[j].hasOwnProperty("y"))?run.data[j].y:run.data[j]);
}else{
if(!this.opt.interpolate||_f){
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
_3.forEach(this.series,_7);
this._eventSeries={};
this.cleanGroup();
this.group.setTransform(null);
s=this.group;
df.forEachRev(this.series,function(_13){
_13.cleanGroup(s);
});
}
var t=this.chart.theme,_14,_15,_16,_17=this.events();
for(var i=this.series.length-1;i>=0;--i){
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
var _18=t.next(this.opt.areas?"area":"line",[this.opt,run],true),_19,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_1a=this._eventSeries[run.name]=new Array(run.data.length);
s=run.group;
var _1b=_3.some(run.data,function(_1c){
return typeof _1c=="number"||(_1c&&!_1c.hasOwnProperty("x"));
});
var _1d=this.buildSegments(i,_1b);
for(var seg=0;seg<_1d.length;seg++){
var _1e=_1d[seg];
if(_1b){
_19=_3.map(_1e.rseg,function(v,i){
return {x:ht(i+_1e.index+1)+_12.l,y:dim.height-_12.b-vt(v),data:v};
},this);
}else{
_19=_3.map(_1e.rseg,function(v){
return {x:ht(v.x)+_12.l,y:dim.height-_12.b-vt(v.y),data:v};
},this);
}
if(_1b&&this.opt.interpolate){
while(seg<_1d.length){
seg++;
_1e=_1d[seg];
if(_1e){
_19=_19.concat(_3.map(_1e.rseg,function(v,i){
return {x:ht(i+_1e.index+1)+_12.l,y:dim.height-_12.b-vt(v),data:v};
},this));
}
}
}
var _1f=this.opt.tension?dc.curve(_19,this.opt.tension):"";
if(this.opt.areas&&_19.length>1){
var _20=this._plotFill(_18.series.fill,dim,_12),_21=_1.clone(_19);
if(this.opt.tension){
var _22="L"+_21[_21.length-1].x+","+(dim.height-_12.b)+" L"+_21[0].x+","+(dim.height-_12.b)+" L"+_21[0].x+","+_21[0].y;
run.dyn.fill=s.createPath(_1f+" "+_22).setFill(_20).getFill();
}else{
_21.push({x:_19[_19.length-1].x,y:dim.height-_12.b});
_21.push({x:_19[0].x,y:dim.height-_12.b});
_21.push(_19[0]);
run.dyn.fill=s.createPolyline(_21).setFill(_20).getFill();
}
}
if(this.opt.lines||this.opt.markers){
_14=_18.series.stroke;
if(_18.series.outline){
_15=run.dyn.outline=dc.makeStroke(_18.series.outline);
_15.width=2*_15.width+_14.width;
}
}
if(this.opt.markers){
run.dyn.marker=_18.symbol;
}
var _23=null,_24=null,_25=null;
if(_14&&_18.series.shadow&&_19.length>1){
var _26=_18.series.shadow,_27=_3.map(_19,function(c){
return {x:c.x+_26.dx,y:c.y+_26.dy};
});
if(this.opt.lines){
if(this.opt.tension){
run.dyn.shadow=s.createPath(dc.curve(_27,this.opt.tension)).setStroke(_26).getStroke();
}else{
run.dyn.shadow=s.createPolyline(_27).setStroke(_26).getStroke();
}
}
if(this.opt.markers&&_18.marker.shadow){
_26=_18.marker.shadow;
_25=_3.map(_27,function(c){
return this.createPath(run,s,"M"+c.x+" "+c.y+" "+_18.symbol).setStroke(_26).setFill(_26.color);
},this);
}
}
if(this.opt.lines&&_19.length>1){
if(_15){
if(this.opt.tension){
run.dyn.outline=s.createPath(_1f).setStroke(_15).getStroke();
}else{
run.dyn.outline=s.createPolyline(_19).setStroke(_15).getStroke();
}
}
if(this.opt.tension){
run.dyn.stroke=s.createPath(_1f).setStroke(_14).getStroke();
}else{
run.dyn.stroke=s.createPolyline(_19).setStroke(_14).getStroke();
}
}
if(this.opt.markers){
var _28=_18;
_23=new Array(_19.length);
_24=new Array(_19.length);
_15=null;
if(_28.marker.outline){
_15=dc.makeStroke(_28.marker.outline);
_15.width=2*_15.width+(_28.marker.stroke?_28.marker.stroke.width:0);
}
_3.forEach(_19,function(c,i){
if(this.opt.styleFunc||typeof c.data!="number"){
var _29=typeof c.data!="number"?[c.data]:[];
if(this.opt.styleFunc){
_29.push(this.opt.styleFunc(c.data));
}
_28=t.addMixin(_18,"marker",_29,true);
}else{
_28=t.post(_18,"marker");
}
var _2a="M"+c.x+" "+c.y+" "+_28.symbol;
if(_15){
_24[i]=this.createPath(run,s,_2a).setStroke(_15);
}
_23[i]=this.createPath(run,s,_2a).setStroke(_28.marker.stroke).setFill(_28.marker.fill);
},this);
run.dyn.markerFill=_28.marker.fill;
run.dyn.markerStroke=_28.marker.stroke;
if(_17){
_3.forEach(_23,function(s,i){
var o={element:"marker",index:i+_1e.index,run:run,shape:s,outline:_24[i]||null,shadow:_25&&_25[i]||null,cx:_19[i].x,cy:_19[i].y};
if(_1b){
o.x=i+_1e.index+1;
o.y=_1e.rseg[i];
}else{
o.x=_1e.rseg[i].x;
o.y=_1e.rseg[i].y;
}
this._connectEvents(o);
_1a[i+_1e.index]=o;
},this);
}else{
delete this._eventSeries[run.name];
}
}
}
run.dirty=false;
}
if(this.animate){
var _2b=this.group;
fx.animateTransform(_1.delegate({shape:_2b,duration:_8,transform:[{name:"translate",start:[0,dim.height-_12.b],end:[0,0]},{name:"scale",start:[1,0],end:[1,1]},{name:"original"}]},this.animate)).play();
}
this.dirty=false;
return this;
}});
});
