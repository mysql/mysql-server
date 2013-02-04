//>>built
define("dojox/charting/plot2d/Grid",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","../Element","./common","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,dc,du,fx){
return _2("dojox.charting.plot2d.Grid",_5,{defaultParams:{hAxis:"x",vAxis:"y",hMajorLines:true,hMinorLines:false,vMajorLines:true,vMinorLines:false,hStripes:"none",vStripes:"none",animate:null,enableCache:false},optionalParams:{},constructor:function(_6,_7){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_7);
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.dirty=true;
this.animate=this.opt.animate;
this.zoom=null,this.zoomQueue=[];
this.lastWindow={vscale:1,hscale:1,xoffset:0,yoffset:0};
if(this.opt.enableCache){
this._lineFreePool=[];
this._lineUsePool=[];
}
},clear:function(){
this._hAxis=null;
this._vAxis=null;
this.dirty=true;
return this;
},setAxis:function(_8){
if(_8){
this[_8.vertical?"_vAxis":"_hAxis"]=_8;
}
return this;
},addSeries:function(_9){
return this;
},getSeriesStats:function(){
return _1.delegate(dc.defaultStats);
},initializeScalers:function(){
return this;
},isDirty:function(){
return this.dirty||this._hAxis&&this._hAxis.dirty||this._vAxis&&this._vAxis.dirty;
},performZoom:function(_a,_b){
var vs=this._vAxis.scale||1,hs=this._hAxis.scale||1,_c=_a.height-_b.b,_d=this._hAxis.getScaler().bounds,_e=(_d.from-_d.lower)*_d.scale,_f=this._vAxis.getScaler().bounds,_10=(_f.from-_f.lower)*_f.scale,_11=vs/this.lastWindow.vscale,_12=hs/this.lastWindow.hscale,_13=(this.lastWindow.xoffset-_e)/((this.lastWindow.hscale==1)?hs:this.lastWindow.hscale),_14=(_10-this.lastWindow.yoffset)/((this.lastWindow.vscale==1)?vs:this.lastWindow.vscale),_15=this.group,_16=fx.animateTransform(_1.delegate({shape:_15,duration:1200,transform:[{name:"translate",start:[0,0],end:[_b.l*(1-_12),_c*(1-_11)]},{name:"scale",start:[1,1],end:[_12,_11]},{name:"original"},{name:"translate",start:[0,0],end:[_13,_14]}]},this.zoom));
_1.mixin(this.lastWindow,{vscale:vs,hscale:hs,xoffset:_e,yoffset:_10});
this.zoomQueue.push(_16);
_3.connect(_16,"onEnd",this,function(){
this.zoom=null;
this.zoomQueue.shift();
if(this.zoomQueue.length>0){
this.zoomQueue[0].play();
}
});
if(this.zoomQueue.length==1){
this.zoomQueue[0].play();
}
return this;
},getRequiredColors:function(){
return 0;
},cleanGroup:function(){
this.inherited(arguments);
if(this.opt.enableCache){
this._lineFreePool=this._lineFreePool.concat(this._lineUsePool);
this._lineUsePool=[];
}
},createLine:function(_17,_18){
var _19;
if(this.opt.enableCache&&this._lineFreePool.length>0){
_19=this._lineFreePool.pop();
_19.setShape(_18);
_17.add(_19);
}else{
_19=_17.createLine(_18);
}
if(this.opt.enableCache){
this._lineUsePool.push(_19);
}
return _19;
},render:function(dim,_1a){
if(this.zoom){
return this.performZoom(dim,_1a);
}
this.dirty=this.isDirty();
if(!this.dirty){
return this;
}
this.cleanGroup();
var s=this.group,ta=this.chart.theme.axis;
try{
var _1b=this._vAxis.getScaler(),vt=_1b.scaler.getTransformerFromModel(_1b),_1c=this._vAxis.getTicks();
if(_1c!=null){
if(this.opt.hMinorLines){
_4.forEach(_1c.minor,function(_1d){
var y=dim.height-_1a.b-vt(_1d.value);
var _1e=this.createLine(s,{x1:_1a.l,y1:y,x2:dim.width-_1a.r,y2:y}).setStroke(ta.minorTick);
if(this.animate){
this._animateGrid(_1e,"h",_1a.l,_1a.r+_1a.l-dim.width);
}
},this);
}
if(this.opt.hMajorLines){
_4.forEach(_1c.major,function(_1f){
var y=dim.height-_1a.b-vt(_1f.value);
var _20=this.createLine(s,{x1:_1a.l,y1:y,x2:dim.width-_1a.r,y2:y}).setStroke(ta.majorTick);
if(this.animate){
this._animateGrid(_20,"h",_1a.l,_1a.r+_1a.l-dim.width);
}
},this);
}
}
}
catch(e){
}
try{
var _21=this._hAxis.getScaler(),ht=_21.scaler.getTransformerFromModel(_21),_1c=this._hAxis.getTicks();
if(this!=null){
if(_1c&&this.opt.vMinorLines){
_4.forEach(_1c.minor,function(_22){
var x=_1a.l+ht(_22.value);
var _23=this.createLine(s,{x1:x,y1:_1a.t,x2:x,y2:dim.height-_1a.b}).setStroke(ta.minorTick);
if(this.animate){
this._animateGrid(_23,"v",dim.height-_1a.b,dim.height-_1a.b-_1a.t);
}
},this);
}
if(_1c&&this.opt.vMajorLines){
_4.forEach(_1c.major,function(_24){
var x=_1a.l+ht(_24.value);
var _25=this.createLine(s,{x1:x,y1:_1a.t,x2:x,y2:dim.height-_1a.b}).setStroke(ta.majorTick);
if(this.animate){
this._animateGrid(_25,"v",dim.height-_1a.b,dim.height-_1a.b-_1a.t);
}
},this);
}
}
}
catch(e){
}
this.dirty=false;
return this;
},_animateGrid:function(_26,_27,_28,_29){
var _2a=_27=="h"?[_28,0]:[0,_28];
var _2b=_27=="h"?[1/_29,1]:[1,1/_29];
fx.animateTransform(_1.delegate({shape:_26,duration:1200,transform:[{name:"translate",start:_2a,end:[0,0]},{name:"scale",start:_2b,end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});
