//>>built
define("dojox/charting/plot2d/Scatter",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./Base","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx","dojox/gfx/gradutils"],function(_1,_2,_3,_4,dc,df,_5,du,fx,_6){
var _7=_5.lambda("item.purgeGroup()");
return _3("dojox.charting.plot2d.Scatter",_4,{defaultParams:{hAxis:"x",vAxis:"y",shadows:null,animate:null},optionalParams:{markerStroke:{},markerOutline:{},markerShadow:{},markerFill:{},markerFont:"",markerFontColor:""},constructor:function(_8,_9){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_9);
du.updateWithPattern(this.opt,_9,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.animate=this.opt.animate;
},render:function(_a,_b){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_a,_b);
}
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_2.forEach(this.series,_7);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_c){
_c.cleanGroup(s);
});
}
var t=this.chart.theme,_d=this.events();
for(var i=this.series.length-1;i>=0;--i){
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
var _f=t.next("marker",[this.opt,_e]),s=_e.group,_10,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler);
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
var _14=typeof _e.data[i]=="number"?t.post(_f,"marker"):t.addMixin(_f,"marker",_e.data[i],true),_15="M"+c.x+" "+c.y+" "+_14.symbol;
if(_14.marker.shadow){
_11[i]=s.createPath("M"+(c.x+_14.marker.shadow.dx)+" "+(c.y+_14.marker.shadow.dy)+" "+_14.symbol).setStroke(_14.marker.shadow).setFill(_14.marker.shadow.color);
if(this.animate){
this._animateScatter(_11[i],_a.height-_b.b);
}
}
if(_14.marker.outline){
var _16=dc.makeStroke(_14.marker.outline);
_16.width=2*_16.width+_14.marker.stroke.width;
_13[i]=s.createPath(_15).setStroke(_16);
if(this.animate){
this._animateScatter(_13[i],_a.height-_b.b);
}
}
var _17=dc.makeStroke(_14.marker.stroke),_18=this._plotFill(_14.marker.fill,_a,_b);
if(_18&&(_18.type==="linear"||_18.type=="radial")){
var _19=_6.getColor(_18,{x:c.x,y:c.y});
if(_17){
_17.color=_19;
}
_12[i]=s.createPath(_15).setStroke(_17).setFill(_19);
}else{
_12[i]=s.createPath(_15).setStroke(_17).setFill(_18);
}
if(this.animate){
this._animateScatter(_12[i],_a.height-_b.b);
}
},this);
if(_12.length){
_e.dyn.stroke=_12[_12.length-1].getStroke();
_e.dyn.fill=_12[_12.length-1].getFill();
}
if(_d){
var _1a=new Array(_12.length);
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
_1a[i]=o;
},this);
this._eventSeries[_e.name]=_1a;
}else{
delete this._eventSeries[_e.name];
}
_e.dirty=false;
}
this.dirty=false;
return this;
},_animateScatter:function(_1b,_1c){
fx.animateTransform(_1.delegate({shape:_1b,duration:1200,transform:[{name:"translate",start:[0,_1c],end:[0,0]},{name:"scale",start:[0,0],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
