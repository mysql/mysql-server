//>>built
define("dojox/charting/plot2d/Default",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","./Base","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,dc,df,_5,du,fx){
var _6=_5.lambda("item.purgeGroup()");
var _7=1200;
return _2("dojox.charting.plot2d.Default",_4,{defaultParams:{hAxis:"x",vAxis:"y",lines:true,areas:false,markers:false,tension:"",animate:false,enableCache:false},optionalParams:{stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:"",markerStroke:{},markerOutline:{},markerShadow:{},markerFill:{},markerFont:"",markerFontColor:""},constructor:function(_8,_9){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_9);
du.updateWithPattern(this.opt,_9,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
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
},render:function(_e,_f){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_e,_f);
}
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_3.forEach(this.series,_6);
this._eventSeries={};
this.cleanGroup();
this.group.setTransform(null);
var s=this.group;
df.forEachRev(this.series,function(_10){
_10.cleanGroup(s);
});
}
var t=this.chart.theme,_11,_12,_13,_14=this.events();
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
var _15=t.next(this.opt.areas?"area":"line",[this.opt,run],true),s=run.group,_16=[],_17=[],_18=null,_19,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_1a=this._eventSeries[run.name]=new Array(run.data.length);
var _1b=typeof run.data[0]=="number";
var min=_1b?Math.max(0,Math.floor(this._hScaler.bounds.from-1)):0,max=_1b?Math.min(run.data.length,Math.ceil(this._hScaler.bounds.to)):run.data.length;
for(var j=min;j<max;j++){
if(run.data[j]!=null){
if(!_18){
_18=[];
_17.push(j);
_16.push(_18);
}
_18.push(run.data[j]);
}else{
_18=null;
}
}
for(var seg=0;seg<_16.length;seg++){
if(typeof _16[seg][0]=="number"){
_19=_3.map(_16[seg],function(v,i){
return {x:ht(i+_17[seg]+1)+_f.l,y:_e.height-_f.b-vt(v)};
},this);
}else{
_19=_3.map(_16[seg],function(v,i){
return {x:ht(v.x)+_f.l,y:_e.height-_f.b-vt(v.y)};
},this);
}
var _1c=this.opt.tension?dc.curve(_19,this.opt.tension):"";
if(this.opt.areas&&_19.length>1){
var _1d=_15.series.fill;
var _1e=_1.clone(_19);
if(this.opt.tension){
var _1f="L"+_1e[_1e.length-1].x+","+(_e.height-_f.b)+" L"+_1e[0].x+","+(_e.height-_f.b)+" L"+_1e[0].x+","+_1e[0].y;
run.dyn.fill=s.createPath(_1c+" "+_1f).setFill(_1d).getFill();
}else{
_1e.push({x:_19[_19.length-1].x,y:_e.height-_f.b});
_1e.push({x:_19[0].x,y:_e.height-_f.b});
_1e.push(_19[0]);
run.dyn.fill=s.createPolyline(_1e).setFill(_1d).getFill();
}
}
if(this.opt.lines||this.opt.markers){
_11=_15.series.stroke;
if(_15.series.outline){
_12=run.dyn.outline=dc.makeStroke(_15.series.outline);
_12.width=2*_12.width+_11.width;
}
}
if(this.opt.markers){
run.dyn.marker=_15.symbol;
}
var _20=null,_21=null,_22=null;
if(_11&&_15.series.shadow&&_19.length>1){
var _23=_15.series.shadow,_24=_3.map(_19,function(c){
return {x:c.x+_23.dx,y:c.y+_23.dy};
});
if(this.opt.lines){
if(this.opt.tension){
run.dyn.shadow=s.createPath(dc.curve(_24,this.opt.tension)).setStroke(_23).getStroke();
}else{
run.dyn.shadow=s.createPolyline(_24).setStroke(_23).getStroke();
}
}
if(this.opt.markers&&_15.marker.shadow){
_23=_15.marker.shadow;
_22=_3.map(_24,function(c){
return this.createPath(run,s,"M"+c.x+" "+c.y+" "+_15.symbol).setStroke(_23).setFill(_23.color);
},this);
}
}
if(this.opt.lines&&_19.length>1){
if(_12){
if(this.opt.tension){
run.dyn.outline=s.createPath(_1c).setStroke(_12).getStroke();
}else{
run.dyn.outline=s.createPolyline(_19).setStroke(_12).getStroke();
}
}
if(this.opt.tension){
run.dyn.stroke=s.createPath(_1c).setStroke(_11).getStroke();
}else{
run.dyn.stroke=s.createPolyline(_19).setStroke(_11).getStroke();
}
}
if(this.opt.markers){
_20=new Array(_19.length);
_21=new Array(_19.length);
_12=null;
if(_15.marker.outline){
_12=dc.makeStroke(_15.marker.outline);
_12.width=2*_12.width+(_15.marker.stroke?_15.marker.stroke.width:0);
}
_3.forEach(_19,function(c,i){
var _25="M"+c.x+" "+c.y+" "+_15.symbol;
if(_12){
_21[i]=this.createPath(run,s,_25).setStroke(_12);
}
_20[i]=this.createPath(run,s,_25).setStroke(_15.marker.stroke).setFill(_15.marker.fill);
},this);
run.dyn.markerFill=_15.marker.fill;
run.dyn.markerStroke=_15.marker.stroke;
if(_14){
_3.forEach(_20,function(s,i){
var o={element:"marker",index:i+_17[seg],run:run,shape:s,outline:_21[i]||null,shadow:_22&&_22[i]||null,cx:_19[i].x,cy:_19[i].y};
if(typeof _16[seg][0]=="number"){
o.x=i+_17[seg]+1;
o.y=_16[seg][i];
}else{
o.x=_16[seg][i].x;
o.y=_16[seg][i].y;
}
this._connectEvents(o);
_1a[i+_17[seg]]=o;
},this);
}else{
delete this._eventSeries[run.name];
}
}
}
run.dirty=false;
}
if(this.animate){
var _26=this.group;
fx.animateTransform(_1.delegate({shape:_26,duration:_7,transform:[{name:"translate",start:[0,_e.height-_f.b],end:[0,0]},{name:"scale",start:[1,0],end:[1,1]},{name:"original"}]},this.animate)).play();
}
this.dirty=false;
return this;
}});
});
