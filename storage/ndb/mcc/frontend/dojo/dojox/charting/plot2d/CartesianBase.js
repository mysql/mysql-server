//>>built
define("dojox/charting/plot2d/CartesianBase",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/has","./Base","../scaler/primitive","dojox/gfx","dojox/gfx/fx","dojox/lang/utils"],function(_1,_2,_3,_4,_5,_6,_7,fx,du){
var _8=function(){
return false;
};
return _2("dojox.charting.plot2d.CartesianBase",_5,{baseParams:{hAxis:"x",vAxis:"y",labels:false,labelOffset:10,fixed:true,precision:1,labelStyle:"inside",htmlLabels:true,omitLabels:true,labelFunc:null},constructor:function(_9,_a){
this.axes=["hAxis","vAxis"];
this.zoom=null;
this.zoomQueue=[];
this.lastWindow={vscale:1,hscale:1,xoffset:0,yoffset:0};
this.hAxis=(_a&&_a.hAxis)||"x";
this.vAxis=(_a&&_a.vAxis)||"y";
this.series=[];
this.opt=_1.clone(this.baseParams);
du.updateWithObject(this.opt,_a);
},clear:function(){
this.inherited(arguments);
this._hAxis=null;
this._vAxis=null;
return this;
},cleanGroup:function(_b,_c){
this.inherited(arguments);
if(!_c&&this.chart._nativeClip){
var _d=this.chart.offsets,_e=this.chart.dim;
var w=Math.max(0,_e.width-_d.l-_d.r),h=Math.max(0,_e.height-_d.t-_d.b);
this.group.setClip({x:_d.l,y:_d.t,width:w,height:h});
if(!this._clippedGroup){
this._clippedGroup=this.group.createGroup();
}
}
},purgeGroup:function(){
this.inherited(arguments);
this._clippedGroup=null;
},getGroup:function(){
return this._clippedGroup||this.group;
},setAxis:function(_f){
if(_f){
this[_f.vertical?"_vAxis":"_hAxis"]=_f;
}
return this;
},toPage:function(_10){
var ah=this._hAxis,av=this._vAxis,sh=ah.getScaler(),sv=av.getScaler(),th=sh.scaler.getTransformerFromModel(sh),tv=sv.scaler.getTransformerFromModel(sv),c=this.chart.getCoords(),o=this.chart.offsets,dim=this.chart.dim;
var t=function(_11){
var r={};
r.x=th(_11[ah.name])+c.x+o.l;
r.y=c.y+dim.height-o.b-tv(_11[av.name]);
return r;
};
return _10?t(_10):t;
},toData:function(_12){
var ah=this._hAxis,av=this._vAxis,sh=ah.getScaler(),sv=av.getScaler(),th=sh.scaler.getTransformerFromPlot(sh),tv=sv.scaler.getTransformerFromPlot(sv),c=this.chart.getCoords(),o=this.chart.offsets,dim=this.chart.dim;
var t=function(_13){
var r={};
r[ah.name]=th(_13.x-c.x-o.l);
r[av.name]=tv(c.y+dim.height-_13.y-o.b);
return r;
};
return _12?t(_12):t;
},isDirty:function(){
return this.dirty||this._hAxis&&this._hAxis.dirty||this._vAxis&&this._vAxis.dirty;
},createLabel:function(_14,_15,_16,_17){
if(this.opt.labels){
var x,y,_18=this.opt.labelFunc?this.opt.labelFunc.apply(this,[_15,this.opt.fixed,this.opt.precision]):this._getLabel(isNaN(_15.y)?_15:_15.y);
if(this.opt.labelStyle=="inside"){
var _19=_7._base._getTextBox(_18,{font:_17.series.font});
x=_16.x+_16.width/2;
y=_16.y+_16.height/2+_19.h/4;
if(_19.w>_16.width||_19.h>_16.height){
return;
}
}else{
x=_16.x+_16.width/2;
y=_16.y-this.opt.labelOffset;
}
this.renderLabel(_14,x,y,_18,_17,this.opt.labelStyle=="inside");
}
},performZoom:function(dim,_1a){
var vs=this._vAxis.scale||1,hs=this._hAxis.scale||1,_1b=dim.height-_1a.b,_1c=this._hScaler.bounds,_1d=(_1c.from-_1c.lower)*_1c.scale,_1e=this._vScaler.bounds,_1f=(_1e.from-_1e.lower)*_1e.scale,_20=vs/this.lastWindow.vscale,_21=hs/this.lastWindow.hscale,_22=(this.lastWindow.xoffset-_1d)/((this.lastWindow.hscale==1)?hs:this.lastWindow.hscale),_23=(_1f-this.lastWindow.yoffset)/((this.lastWindow.vscale==1)?vs:this.lastWindow.vscale),_24=this.getGroup(),_25=fx.animateTransform(_1.delegate({shape:_24,duration:1200,transform:[{name:"translate",start:[0,0],end:[_1a.l*(1-_21),_1b*(1-_20)]},{name:"scale",start:[1,1],end:[_21,_20]},{name:"original"},{name:"translate",start:[0,0],end:[_22,_23]}]},this.zoom));
_1.mixin(this.lastWindow,{vscale:vs,hscale:hs,xoffset:_1d,yoffset:_1f});
this.zoomQueue.push(_25);
_3.connect(_25,"onEnd",this,function(){
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
},initializeScalers:function(dim,_26){
if(this._hAxis){
if(!this._hAxis.initialized()){
this._hAxis.calculate(_26.hmin,_26.hmax,dim.width);
}
this._hScaler=this._hAxis.getScaler();
}else{
this._hScaler=_6.buildScaler(_26.hmin,_26.hmax,dim.width);
}
if(this._vAxis){
if(!this._vAxis.initialized()){
this._vAxis.calculate(_26.vmin,_26.vmax,dim.height);
}
this._vScaler=this._vAxis.getScaler();
}else{
this._vScaler=_6.buildScaler(_26.vmin,_26.vmax,dim.height);
}
return this;
},isNullValue:function(_27){
if(_27===null||typeof _27=="undefined"){
return true;
}
var h=this._hAxis?this._hAxis.isNullValue:_8,v=this._vAxis?this._vAxis.isNullValue:_8;
if(typeof _27=="number"){
return h(1)||v(_27);
}
return h(isNaN(_27.x)?1:_27.x)||_27.y===null||v(_27.y);
}});
});
