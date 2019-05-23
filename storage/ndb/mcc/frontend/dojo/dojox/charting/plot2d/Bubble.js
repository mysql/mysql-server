//>>built
define("dojox/charting/plot2d/Bubble",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","./CartesianBase","./_PlotEvents","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,dc,df,_6,du,fx){
var _7=_6.lambda("item.purgeGroup()");
return _2("dojox.charting.plot2d.Bubble",[_4,_5],{defaultParams:{hAxis:"x",vAxis:"y",animate:null},optionalParams:{stroke:{},outline:{},shadow:{},fill:{},styleFunc:null,font:"",fontColor:""},constructor:function(_8,_9){
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
_3.forEach(this.series,_7);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_c){
_c.cleanGroup(s);
});
}
var t=this.chart.theme,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_d=this.events();
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
if(typeof _e.data[0]=="number"){
console.warn("dojox.charting.plot2d.Bubble: the data in the following series cannot be rendered as a bubble chart; ",_e);
continue;
}
var _f=t.next("circle",[this.opt,_e]),s=_e.group,_10=_3.map(_e.data,function(v,i){
return v?{x:ht(v.x)+_b.l,y:_a.height-_b.b-vt(v.y),radius:this._vScaler.bounds.scale*(v.size/2)}:null;
},this);
var _11=null,_12=null,_13=null,_14=this.opt.styleFunc;
var _15=function(_16){
if(_14){
return t.addMixin(_f,"circle",[_16,_14(_16)],true);
}
return t.addMixin(_f,"circle",_16,true);
};
if(_f.series.shadow){
_13=_3.map(_10,function(_17,i){
if(_17!==null){
var _18=_15(_e.data[i]),_19=_18.series.shadow;
var _1a=s.createCircle({cx:_17.x+_19.dx,cy:_17.y+_19.dy,r:_17.radius}).setStroke(_19).setFill(_19.color);
if(this.animate){
this._animateBubble(_1a,_a.height-_b.b,_17.radius);
}
return _1a;
}
return null;
},this);
if(_13.length){
_e.dyn.shadow=_13[_13.length-1].getStroke();
}
}
if(_f.series.outline){
_12=_3.map(_10,function(_1b,i){
if(_1b!==null){
var _1c=_15(_e.data[i]),_1d=dc.makeStroke(_1c.series.outline);
_1d.width=2*_1d.width+_f.series.stroke.width;
var _1e=s.createCircle({cx:_1b.x,cy:_1b.y,r:_1b.radius}).setStroke(_1d);
if(this.animate){
this._animateBubble(_1e,_a.height-_b.b,_1b.radius);
}
return _1e;
}
return null;
},this);
if(_12.length){
_e.dyn.outline=_12[_12.length-1].getStroke();
}
}
_11=_3.map(_10,function(_1f,i){
if(_1f!==null){
var _20=_15(_e.data[i]),_21={x:_1f.x-_1f.radius,y:_1f.y-_1f.radius,width:2*_1f.radius,height:2*_1f.radius};
var _22=this._plotFill(_20.series.fill,_a,_b);
_22=this._shapeFill(_22,_21);
var _23=s.createCircle({cx:_1f.x,cy:_1f.y,r:_1f.radius}).setFill(_22).setStroke(_20.series.stroke);
if(this.animate){
this._animateBubble(_23,_a.height-_b.b,_1f.radius);
}
return _23;
}
return null;
},this);
if(_11.length){
_e.dyn.fill=_11[_11.length-1].getFill();
_e.dyn.stroke=_11[_11.length-1].getStroke();
}
if(_d){
var _24=new Array(_11.length);
_3.forEach(_11,function(s,i){
if(s!==null){
var o={element:"circle",index:i,run:_e,shape:s,outline:_12&&_12[i]||null,shadow:_13&&_13[i]||null,x:_e.data[i].x,y:_e.data[i].y,r:_e.data[i].size/2,cx:_10[i].x,cy:_10[i].y,cr:_10[i].radius};
this._connectEvents(o);
_24[i]=o;
}
},this);
this._eventSeries[_e.name]=_24;
}else{
delete this._eventSeries[_e.name];
}
_e.dirty=false;
}
this.dirty=false;
return this;
},_animateBubble:function(_25,_26,_27){
fx.animateTransform(_1.delegate({shape:_25,duration:1200,transform:[{name:"translate",start:[0,_26],end:[0,0]},{name:"scale",start:[0,1/_27],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
