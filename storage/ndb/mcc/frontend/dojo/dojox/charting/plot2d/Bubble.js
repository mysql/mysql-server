//>>built
define("dojox/charting/plot2d/Bubble",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","./Base","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,dc,df,_5,du,fx){
var _6=_5.lambda("item.purgeGroup()");
return _2("dojox.charting.plot2d.Bubble",_4,{defaultParams:{hAxis:"x",vAxis:"y",animate:null},optionalParams:{stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:""},constructor:function(_7,_8){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_8);
du.updateWithPattern(this.opt,_8,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.animate=this.opt.animate;
},render:function(_9,_a){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_9,_a);
}
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_3.forEach(this.series,_6);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_b){
_b.cleanGroup(s);
});
}
var t=this.chart.theme,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_c=this.events();
for(var i=this.series.length-1;i>=0;--i){
var _d=this.series[i];
if(!this.dirty&&!_d.dirty){
t.skip();
this._reconnectEvents(_d.name);
continue;
}
_d.cleanGroup();
if(!_d.data.length){
_d.dirty=false;
t.skip();
continue;
}
if(typeof _d.data[0]=="number"){
console.warn("dojox.charting.plot2d.Bubble: the data in the following series cannot be rendered as a bubble chart; ",_d);
continue;
}
var _e=t.next("circle",[this.opt,_d]),s=_d.group,_f=_3.map(_d.data,function(v,i){
return v?{x:ht(v.x)+_a.l,y:_9.height-_a.b-vt(v.y),radius:this._vScaler.bounds.scale*(v.size/2)}:null;
},this);
var _10=null,_11=null,_12=null;
if(_e.series.shadow){
_12=_3.map(_f,function(_13){
if(_13!==null){
var _14=t.addMixin(_e,"circle",_13,true),_15=_14.series.shadow;
var _16=s.createCircle({cx:_13.x+_15.dx,cy:_13.y+_15.dy,r:_13.radius}).setStroke(_15).setFill(_15.color);
if(this.animate){
this._animateBubble(_16,_9.height-_a.b,_13.radius);
}
return _16;
}
return null;
},this);
if(_12.length){
_d.dyn.shadow=_12[_12.length-1].getStroke();
}
}
if(_e.series.outline){
_11=_3.map(_f,function(_17){
if(_17!==null){
var _18=t.addMixin(_e,"circle",_17,true),_19=dc.makeStroke(_18.series.outline);
_19.width=2*_19.width+_e.series.stroke.width;
var _1a=s.createCircle({cx:_17.x,cy:_17.y,r:_17.radius}).setStroke(_19);
if(this.animate){
this._animateBubble(_1a,_9.height-_a.b,_17.radius);
}
return _1a;
}
return null;
},this);
if(_11.length){
_d.dyn.outline=_11[_11.length-1].getStroke();
}
}
_10=_3.map(_f,function(_1b){
if(_1b!==null){
var _1c=t.addMixin(_e,"circle",_1b,true),_1d={x:_1b.x-_1b.radius,y:_1b.y-_1b.radius,width:2*_1b.radius,height:2*_1b.radius};
var _1e=this._plotFill(_1c.series.fill,_9,_a);
_1e=this._shapeFill(_1e,_1d);
var _1f=s.createCircle({cx:_1b.x,cy:_1b.y,r:_1b.radius}).setFill(_1e).setStroke(_1c.series.stroke);
if(this.animate){
this._animateBubble(_1f,_9.height-_a.b,_1b.radius);
}
return _1f;
}
return null;
},this);
if(_10.length){
_d.dyn.fill=_10[_10.length-1].getFill();
_d.dyn.stroke=_10[_10.length-1].getStroke();
}
if(_c){
var _20=new Array(_10.length);
_3.forEach(_10,function(s,i){
if(s!==null){
var o={element:"circle",index:i,run:_d,shape:s,outline:_11&&_11[i]||null,shadow:_12&&_12[i]||null,x:_d.data[i].x,y:_d.data[i].y,r:_d.data[i].size/2,cx:_f[i].x,cy:_f[i].y,cr:_f[i].radius};
this._connectEvents(o);
_20[i]=o;
}
},this);
this._eventSeries[_d.name]=_20;
}else{
delete this._eventSeries[_d.name];
}
_d.dirty=false;
}
this.dirty=false;
return this;
},_animateBubble:function(_21,_22,_23){
fx.animateTransform(_1.delegate({shape:_21,duration:1200,transform:[{name:"translate",start:[0,_22],end:[0,0]},{name:"scale",start:[0,1/_23],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
