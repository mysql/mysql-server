//>>built
define("dojox/charting/plot2d/Scatter",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx","dojox/gfx/gradutils"],function(_1,_2,_3,_4,_5,dc,df,_6,du,fx,_7){
var _8=_6.lambda("item.purgeGroup()");
return _3("dojox.charting.plot2d.Scatter",[_4,_5],{defaultParams:{hAxis:"x",vAxis:"y",shadows:null,animate:null},optionalParams:{markerStroke:{},markerOutline:{},markerShadow:{},markerFill:{},markerFont:"",markerFontColor:"",styleFunc:null},constructor:function(_9,_a){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_a);
du.updateWithPattern(this.opt,_a,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.animate=this.opt.animate;
},render:function(_b,_c){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_b,_c);
}
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_2.forEach(this.series,_8);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_d){
_d.cleanGroup(s);
});
}
var t=this.chart.theme,_e=this.events();
for(var i=this.series.length-1;i>=0;--i){
var _f=this.series[i];
if(!this.dirty&&!_f.dirty){
t.skip();
this._reconnectEvents(_f.name);
continue;
}
_f.cleanGroup();
if(!_f.data.length){
_f.dirty=false;
t.skip();
continue;
}
var _10=t.next("marker",[this.opt,_f]),s=_f.group,_11,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler);
if(typeof _f.data[0]=="number"){
_11=_2.map(_f.data,function(v,i){
return {x:ht(i+1)+_c.l,y:_b.height-_c.b-vt(v)};
},this);
}else{
_11=_2.map(_f.data,function(v,i){
return {x:ht(v.x)+_c.l,y:_b.height-_c.b-vt(v.y)};
},this);
}
var _12=new Array(_11.length),_13=new Array(_11.length),_14=new Array(_11.length);
_2.forEach(_11,function(c,i){
var _15=_f.data[i],_16;
if(this.opt.styleFunc||typeof _15!="number"){
var _17=typeof _15!="number"?[_15]:[];
if(this.opt.styleFunc){
_17.push(this.opt.styleFunc(_15));
}
_16=t.addMixin(_10,"marker",_17,true);
}else{
_16=t.post(_10,"marker");
}
var _18="M"+c.x+" "+c.y+" "+_16.symbol;
if(_16.marker.shadow){
_12[i]=s.createPath("M"+(c.x+_16.marker.shadow.dx)+" "+(c.y+_16.marker.shadow.dy)+" "+_16.symbol).setStroke(_16.marker.shadow).setFill(_16.marker.shadow.color);
if(this.animate){
this._animateScatter(_12[i],_b.height-_c.b);
}
}
if(_16.marker.outline){
var _19=dc.makeStroke(_16.marker.outline);
_19.width=2*_19.width+_16.marker.stroke.width;
_14[i]=s.createPath(_18).setStroke(_19);
if(this.animate){
this._animateScatter(_14[i],_b.height-_c.b);
}
}
var _1a=dc.makeStroke(_16.marker.stroke),_1b=this._plotFill(_16.marker.fill,_b,_c);
if(_1b&&(_1b.type==="linear"||_1b.type=="radial")){
var _1c=_7.getColor(_1b,{x:c.x,y:c.y});
if(_1a){
_1a.color=_1c;
}
_13[i]=s.createPath(_18).setStroke(_1a).setFill(_1c);
}else{
_13[i]=s.createPath(_18).setStroke(_1a).setFill(_1b);
}
if(this.animate){
this._animateScatter(_13[i],_b.height-_c.b);
}
},this);
if(_13.length){
_f.dyn.marker=_10.symbol;
_f.dyn.markerStroke=_13[_13.length-1].getStroke();
_f.dyn.markerFill=_13[_13.length-1].getFill();
}
if(_e){
var _1d=new Array(_13.length);
_2.forEach(_13,function(s,i){
var o={element:"marker",index:i,run:_f,shape:s,outline:_14&&_14[i]||null,shadow:_12&&_12[i]||null,cx:_11[i].x,cy:_11[i].y};
if(typeof _f.data[0]=="number"){
o.x=i+1;
o.y=_f.data[i];
}else{
o.x=_f.data[i].x;
o.y=_f.data[i].y;
}
this._connectEvents(o);
_1d[i]=o;
},this);
this._eventSeries[_f.name]=_1d;
}else{
delete this._eventSeries[_f.name];
}
_f.dirty=false;
}
this.dirty=false;
return this;
},_animateScatter:function(_1e,_1f){
fx.animateTransform(_1.delegate({shape:_1e,duration:1200,transform:[{name:"translate",start:[0,_1f],end:[0,0]},{name:"scale",start:[0,0],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
