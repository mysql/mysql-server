//>>built
define("dojox/charting/plot2d/Grid",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","./CartesianBase","./common","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,dc,du,fx){
return _2("dojox.charting.plot2d.Grid",_5,{defaultParams:{hAxis:"x",vAxis:"y",hMajorLines:true,hMinorLines:false,vMajorLines:true,vMinorLines:false,hStripes:false,vStripes:false,animate:null,enableCache:false,renderOnAxis:true},optionalParams:{majorHLine:{},minorHLine:{},majorVLine:{},minorVLine:{}},constructor:function(_6,_7){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_7);
du.updateWithPattern(this.opt,_7,this.optionalParams);
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.animate=this.opt.animate;
if(this.opt.enableCache){
this._lineFreePool=[];
this._lineUsePool=[];
}
},addSeries:function(_8){
return this;
},getSeriesStats:function(){
return _1.delegate(dc.defaultStats);
},cleanGroup:function(){
this.inherited(arguments);
if(this.opt.enableCache){
this._lineFreePool=this._lineFreePool.concat(this._lineUsePool);
this._lineUsePool=[];
}
},createLine:function(_9,_a){
var _b;
if(this.opt.enableCache&&this._lineFreePool.length>0){
_b=this._lineFreePool.pop();
_b.setShape(_a);
_9.add(_b);
}else{
_b=_9.createLine(_a);
}
if(this.opt.enableCache){
this._lineUsePool.push(_b);
}
return _b;
},render:function(_c,_d){
if(this.zoom){
return this.performZoom(_c,_d);
}
this.dirty=this.isDirty();
if(!this.dirty){
return this;
}
this.cleanGroup();
var s=this.group,ta=this.chart.theme,_e;
var _f=this.opt.renderOnAxis;
if(this._vAxis){
var _10=this._vAxis.getScaler();
if(_10){
var vt=_10.scaler.getTransformerFromModel(_10);
var _11;
_11=this._vAxis.getTicks();
if(_11!=null){
if(this.opt.hMinorLines){
_e=this.opt.minorHLine||(ta.grid&&ta.grid.minorLine)||ta.axis.minorTick;
_4.forEach(_11.minor,function(_12){
if(!_f&&_12.value==(this._vAxis.opt.leftBottom?_10.bounds.from:_10.bounds.to)){
return;
}
var y=_c.height-_d.b-vt(_12.value);
var _13=this.createLine(s,{x1:_d.l,y1:y,x2:_c.width-_d.r,y2:y}).setStroke(_e);
if(this.animate){
this._animateGrid(_13,"h",_d.l,_d.r+_d.l-_c.width);
}
},this);
}
if(this.opt.hMajorLines){
_e=this.opt.majorHLine||(ta.grid&&ta.grid.majorLine)||ta.axis.majorTick;
_4.forEach(_11.major,function(_14){
if(!_f&&_14.value==(this._vAxis.opt.leftBottom?_10.bounds.from:_10.bounds.to)){
return;
}
var y=_c.height-_d.b-vt(_14.value);
var _15=this.createLine(s,{x1:_d.l,y1:y,x2:_c.width-_d.r,y2:y}).setStroke(_e);
if(this.animate){
this._animateGrid(_15,"h",_d.l,_d.r+_d.l-_c.width);
}
},this);
}
}
}
}
if(this._hAxis){
var _16=this._hAxis.getScaler();
if(_16){
var ht=_16.scaler.getTransformerFromModel(_16);
_11=this._hAxis.getTicks();
if(this!=null){
if(_11&&this.opt.vMinorLines){
_e=this.opt.minorVLine||(ta.grid&&ta.grid.minorLine)||ta.axis.minorTick;
_4.forEach(_11.minor,function(_17){
if(!_f&&_17.value==(this._hAxis.opt.leftBottom?_16.bounds.from:_16.bounds.to)){
return;
}
var x=_d.l+ht(_17.value);
var _18=this.createLine(s,{x1:x,y1:_d.t,x2:x,y2:_c.height-_d.b}).setStroke(_e);
if(this.animate){
this._animateGrid(_18,"v",_c.height-_d.b,_c.height-_d.b-_d.t);
}
},this);
}
if(_11&&this.opt.vMajorLines){
_e=this.opt.majorVLine||(ta.grid&&ta.grid.majorLine)||ta.axis.majorTick;
_4.forEach(_11.major,function(_19){
if(!_f&&_19.value==(this._hAxis.opt.leftBottom?_16.bounds.from:_16.bounds.to)){
return;
}
var x=_d.l+ht(_19.value);
var _1a=this.createLine(s,{x1:x,y1:_d.t,x2:x,y2:_c.height-_d.b}).setStroke(_e);
if(this.animate){
this._animateGrid(_1a,"v",_c.height-_d.b,_c.height-_d.b-_d.t);
}
},this);
}
}
}
}
this.dirty=false;
return this;
},_animateGrid:function(_1b,_1c,_1d,_1e){
var _1f=_1c=="h"?[_1d,0]:[0,_1d];
var _20=_1c=="h"?[1/_1e,1]:[1,1/_1e];
fx.animateTransform(_1.delegate({shape:_1b,duration:1200,transform:[{name:"translate",start:_1f,end:[0,0]},{name:"scale",start:_20,end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
