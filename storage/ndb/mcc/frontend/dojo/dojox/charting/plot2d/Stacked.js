//>>built
define("dojox/charting/plot2d/Stacked",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","./Default","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/functional/sequence"],function(_1,_2,_3,_4,dc,df,_5,_6){
var _7=_5.lambda("item.purgeGroup()");
return _2("dojox.charting.plot2d.Stacked",_4,{getSeriesStats:function(){
var _8=dc.collectStackedStats(this.series);
this._maxRunLength=_8.hmax;
return _8;
},render:function(_9,_a){
if(this._maxRunLength<=0){
return this;
}
var _b=df.repeat(this._maxRunLength,"-> 0",0);
for(var i=0;i<this.series.length;++i){
var _c=this.series[i];
for(var j=0;j<_c.data.length;++j){
var v=_c.data[j];
if(v!==null){
if(isNaN(v)){
v=0;
}
_b[j]+=v;
}
}
}
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_9,_a);
}
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_3.forEach(this.series,_7);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_d){
_d.cleanGroup(s);
});
}
var t=this.chart.theme,_e=this.events(),ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler);
for(var i=this.series.length-1;i>=0;--i){
var _c=this.series[i];
if(!this.dirty&&!_c.dirty){
t.skip();
this._reconnectEvents(_c.name);
continue;
}
_c.cleanGroup();
var _f=t.next(this.opt.areas?"area":"line",[this.opt,_c],true),s=_c.group,_10,_11=_3.map(_b,function(v,i){
return {x:ht(i+1)+_a.l,y:_9.height-_a.b-vt(v)};
},this);
var _12=this.opt.tension?dc.curve(_11,this.opt.tension):"";
if(this.opt.areas){
var _13=_1.clone(_11);
if(this.opt.tension){
var p=dc.curve(_13,this.opt.tension);
p+=" L"+_11[_11.length-1].x+","+(_9.height-_a.b)+" L"+_11[0].x+","+(_9.height-_a.b)+" L"+_11[0].x+","+_11[0].y;
_c.dyn.fill=s.createPath(p).setFill(_f.series.fill).getFill();
}else{
_13.push({x:_11[_11.length-1].x,y:_9.height-_a.b});
_13.push({x:_11[0].x,y:_9.height-_a.b});
_13.push(_11[0]);
_c.dyn.fill=s.createPolyline(_13).setFill(_f.series.fill).getFill();
}
}
if(this.opt.lines||this.opt.markers){
if(_f.series.outline){
_10=dc.makeStroke(_f.series.outline);
_10.width=2*_10.width+_f.series.stroke.width;
}
}
if(this.opt.markers){
_c.dyn.marker=_f.symbol;
}
var _14,_15,_16;
if(_f.series.shadow&&_f.series.stroke){
var _17=_f.series.shadow,_18=_3.map(_11,function(c){
return {x:c.x+_17.dx,y:c.y+_17.dy};
});
if(this.opt.lines){
if(this.opt.tension){
_c.dyn.shadow=s.createPath(dc.curve(_18,this.opt.tension)).setStroke(_17).getStroke();
}else{
_c.dyn.shadow=s.createPolyline(_18).setStroke(_17).getStroke();
}
}
if(this.opt.markers){
_17=_f.marker.shadow;
_16=_3.map(_18,function(c){
return s.createPath("M"+c.x+" "+c.y+" "+_f.symbol).setStroke(_17).setFill(_17.color);
},this);
}
}
if(this.opt.lines){
if(_10){
if(this.opt.tension){
_c.dyn.outline=s.createPath(_12).setStroke(_10).getStroke();
}else{
_c.dyn.outline=s.createPolyline(_11).setStroke(_10).getStroke();
}
}
if(this.opt.tension){
_c.dyn.stroke=s.createPath(_12).setStroke(_f.series.stroke).getStroke();
}else{
_c.dyn.stroke=s.createPolyline(_11).setStroke(_f.series.stroke).getStroke();
}
}
if(this.opt.markers){
_14=new Array(_11.length);
_15=new Array(_11.length);
_10=null;
if(_f.marker.outline){
_10=dc.makeStroke(_f.marker.outline);
_10.width=2*_10.width+(_f.marker.stroke?_f.marker.stroke.width:0);
}
_3.forEach(_11,function(c,i){
var _19="M"+c.x+" "+c.y+" "+_f.symbol;
if(_10){
_15[i]=s.createPath(_19).setStroke(_10);
}
_14[i]=s.createPath(_19).setStroke(_f.marker.stroke).setFill(_f.marker.fill);
},this);
if(_e){
var _1a=new Array(_14.length);
_3.forEach(_14,function(s,i){
var o={element:"marker",index:i,run:_c,shape:s,outline:_15[i]||null,shadow:_16&&_16[i]||null,cx:_11[i].x,cy:_11[i].y,x:i+1,y:_c.data[i]};
this._connectEvents(o);
_1a[i]=o;
},this);
this._eventSeries[_c.name]=_1a;
}else{
delete this._eventSeries[_c.name];
}
}
_c.dirty=false;
for(var j=0;j<_c.data.length;++j){
var v=_c.data[j];
if(v!==null){
if(isNaN(v)){
v=0;
}
_b[j]-=v;
}
}
}
this.dirty=false;
return this;
}});
});
