//>>built
define("dojox/charting/plot2d/Scatter",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/has","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/utils","dojox/gfx/fx","dojox/gfx/gradutils"],function(_1,_2,_3,_4,_5,_6,dc,df,du,fx,_7){
return _3("dojox.charting.plot2d.Scatter",[_5,_6],{defaultParams:{shadows:null,animate:null},optionalParams:{markerStroke:{},markerOutline:{},markerShadow:{},markerFill:{},markerFont:"",markerFontColor:"",styleFunc:null},constructor:function(_8,_9){
this.opt=_1.clone(_1.mixin(this.opt,this.defaultParams));
du.updateWithObject(this.opt,_9);
du.updateWithPattern(this.opt,_9,this.optionalParams);
this.animate=this.opt.animate;
},render:function(_a,_b){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_a,_b);
}
this.resetEvents();
this.dirty=this.isDirty();
var s;
if(this.dirty){
_2.forEach(this.series,dc.purgeGroup);
this._eventSeries={};
this.cleanGroup();
s=this.getGroup();
df.forEachRev(this.series,function(_c){
_c.cleanGroup(s);
});
}
var t=this.chart.theme,_d=this.events();
for(var i=0;i<this.series.length;i++){
var _e=this.series[i];
if(!this.dirty&&!_e.dirty){
t.skip();
this._reconnectEvents(_e.name);
continue;
}
_e.cleanGroup();
if(!_e.data.length){
_e.dirty=false;
t.skip();
continue;
}
var _f=t.next("marker",[this.opt,_e]),_10,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler);
if(_e.hidden){
_e.dyn.marker=_f.symbol;
_e.dyn.markerFill=_f.marker.fill;
_e.dyn.markerStroke=_f.marker.stroke;
continue;
}
s=_e.group;
if(typeof _e.data[0]=="number"){
_10=_2.map(_e.data,function(v,i){
return {x:ht(i+1)+_b.l,y:_a.height-_b.b-vt(v)};
},this);
}else{
_10=_2.map(_e.data,function(v,i){
return {x:ht(v.x)+_b.l,y:_a.height-_b.b-vt(v.y)};
},this);
}
var _11=new Array(_10.length),_12=new Array(_10.length),_13=new Array(_10.length);
_2.forEach(_10,function(c,i){
var _14=_e.data[i],_15;
if(this.opt.styleFunc||typeof _14!="number"){
var _16=typeof _14!="number"?[_14]:[];
if(this.opt.styleFunc){
_16.push(this.opt.styleFunc(_14));
}
_15=t.addMixin(_f,"marker",_16,true);
}else{
_15=t.post(_f,"marker");
}
var _17="M"+c.x+" "+c.y+" "+_15.symbol;
if(_15.marker.shadow){
_11[i]=s.createPath("M"+(c.x+_15.marker.shadow.dx)+" "+(c.y+_15.marker.shadow.dy)+" "+_15.symbol).setStroke(_15.marker.shadow).setFill(_15.marker.shadow.color);
if(this.animate){
this._animateScatter(_11[i],_a.height-_b.b);
}
}
if(_15.marker.outline){
var _18=dc.makeStroke(_15.marker.outline);
_18.width=2*_18.width+(_15.marker.stroke&&_15.marker.stroke.width||0);
_13[i]=s.createPath(_17).setStroke(_18);
if(this.animate){
this._animateScatter(_13[i],_a.height-_b.b);
}
}
var _19=dc.makeStroke(_15.marker.stroke),_1a=this._plotFill(_15.marker.fill,_a,_b);
if(_1a&&(_1a.type==="linear"||_1a.type=="radial")){
var _1b=_7.getColor(_1a,{x:c.x,y:c.y});
if(_19){
_19.color=_1b;
}
_12[i]=s.createPath(_17).setStroke(_19).setFill(_1b);
}else{
_12[i]=s.createPath(_17).setStroke(_19).setFill(_1a);
}
if(this.opt.labels){
var _1c=_12[i].getBoundingBox();
this.createLabel(s,_14,_1c,_15);
}
if(this.animate){
this._animateScatter(_12[i],_a.height-_b.b);
}
},this);
if(_12.length){
_e.dyn.marker=_f.symbol;
_e.dyn.markerStroke=_12[_12.length-1].getStroke();
_e.dyn.markerFill=_12[_12.length-1].getFill();
}
if(_d){
var _1d=new Array(_12.length);
_2.forEach(_12,function(s,i){
var o={element:"marker",index:i,run:_e,shape:s,outline:_13&&_13[i]||null,shadow:_11&&_11[i]||null,cx:_10[i].x,cy:_10[i].y};
if(typeof _e.data[0]=="number"){
o.x=i+1;
o.y=_e.data[i];
}else{
o.x=_e.data[i].x;
o.y=_e.data[i].y;
}
this._connectEvents(o);
_1d[i]=o;
},this);
this._eventSeries[_e.name]=_1d;
}else{
delete this._eventSeries[_e.name];
}
_e.dirty=false;
}
this.dirty=false;
if(_4("dojo-bidi")){
this._checkOrientation(this.group,_a,_b);
}
return this;
},_animateScatter:function(_1e,_1f){
fx.animateTransform(_1.delegate({shape:_1e,duration:1200,transform:[{name:"translate",start:[0,_1f],end:[0,0]},{name:"scale",start:[0,0],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
