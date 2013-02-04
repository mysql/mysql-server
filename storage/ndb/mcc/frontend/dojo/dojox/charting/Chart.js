//>>built
define("dojox/charting/Chart",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/html","dojo/dom","dojo/dom-geometry","dojo/dom-construct","dojo/_base/Color","dojo/_base/sniff","./Element","./Theme","./Series","./axis2d/common","dojox/gfx","dojox/lang/functional","dojox/lang/functional/fold","dojox/lang/functional/reversed"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,g,_e,_f,_10){
var dc=dojox.charting,_11=_e.lambda("item.clear()"),_12=_e.lambda("item.purgeGroup()"),_13=_e.lambda("item.destroy()"),_14=_e.lambda("item.dirty = false"),_15=_e.lambda("item.dirty = true"),_16=_e.lambda("item.name");
_3("dojox.charting.Chart",null,{constructor:function(_17,_18){
if(!_18){
_18={};
}
this.margins=_18.margins?_18.margins:{l:10,t:10,r:10,b:10};
this.stroke=_18.stroke;
this.fill=_18.fill;
this.delayInMs=_18.delayInMs||200;
this.title=_18.title;
this.titleGap=_18.titleGap;
this.titlePos=_18.titlePos;
this.titleFont=_18.titleFont;
this.titleFontColor=_18.titleFontColor;
this.chartTitle=null;
this.theme=null;
this.axes={};
this.stack=[];
this.plots={};
this.series=[];
this.runs={};
this.dirty=true;
this.coords=null;
this.node=_5.byId(_17);
var box=_6.getMarginBox(_17);
this.surface=g.createSurface(this.node,box.w||400,box.h||300);
},destroy:function(){
_2.forEach(this.series,_13);
_2.forEach(this.stack,_13);
_e.forIn(this.axes,_13);
if(this.chartTitle&&this.chartTitle.tagName){
_7.destroy(this.chartTitle);
}
this.surface.destroy();
},getCoords:function(){
if(!this.coords){
this.coords=_4.coords(this.node,true);
}
return this.coords;
},setTheme:function(_19){
this.theme=_19.clone();
this.dirty=true;
return this;
},addAxis:function(_1a,_1b){
var _1c,_1d=_1b&&_1b.type||"Default";
if(typeof _1d=="string"){
if(!dc.axis2d||!dc.axis2d[_1d]){
throw Error("Can't find axis: "+_1d+" - Check "+"require() dependencies.");
}
_1c=new dc.axis2d[_1d](this,_1b);
}else{
_1c=new _1d(this,_1b);
}
_1c.name=_1a;
_1c.dirty=true;
if(_1a in this.axes){
this.axes[_1a].destroy();
}
this.axes[_1a]=_1c;
this.dirty=true;
return this;
},getAxis:function(_1e){
return this.axes[_1e];
},removeAxis:function(_1f){
if(_1f in this.axes){
this.axes[_1f].destroy();
delete this.axes[_1f];
this.dirty=true;
}
return this;
},addPlot:function(_20,_21){
var _22,_23=_21&&_21.type||"Default";
if(typeof _23=="string"){
if(!dc.plot2d||!dc.plot2d[_23]){
throw Error("Can't find plot: "+_23+" - didn't you forget to dojo"+".require() it?");
}
_22=new dc.plot2d[_23](this,_21);
}else{
_22=new _23(this,_21);
}
_22.name=_20;
_22.dirty=true;
if(_20 in this.plots){
this.stack[this.plots[_20]].destroy();
this.stack[this.plots[_20]]=_22;
}else{
this.plots[_20]=this.stack.length;
this.stack.push(_22);
}
this.dirty=true;
return this;
},getPlot:function(_24){
return this.stack[this.plots[_24]];
},removePlot:function(_25){
if(_25 in this.plots){
var _26=this.plots[_25];
delete this.plots[_25];
this.stack[_26].destroy();
this.stack.splice(_26,1);
_e.forIn(this.plots,function(idx,_27,_28){
if(idx>_26){
_28[_27]=idx-1;
}
});
var ns=_2.filter(this.series,function(run){
return run.plot!=_25;
});
if(ns.length<this.series.length){
_2.forEach(this.series,function(run){
if(run.plot==_25){
run.destroy();
}
});
this.runs={};
_2.forEach(ns,function(run,_29){
this.runs[run.plot]=_29;
},this);
this.series=ns;
}
this.dirty=true;
}
return this;
},getPlotOrder:function(){
return _e.map(this.stack,_16);
},setPlotOrder:function(_2a){
var _2b={},_2c=_e.filter(_2a,function(_2d){
if(!(_2d in this.plots)||(_2d in _2b)){
return false;
}
_2b[_2d]=1;
return true;
},this);
if(_2c.length<this.stack.length){
_e.forEach(this.stack,function(_2e){
var _2f=_2e.name;
if(!(_2f in _2b)){
_2c.push(_2f);
}
});
}
var _30=_e.map(_2c,function(_31){
return this.stack[this.plots[_31]];
},this);
_e.forEach(_30,function(_32,i){
this.plots[_32.name]=i;
},this);
this.stack=_30;
this.dirty=true;
return this;
},movePlotToFront:function(_33){
if(_33 in this.plots){
var _34=this.plots[_33];
if(_34){
var _35=this.getPlotOrder();
_35.splice(_34,1);
_35.unshift(_33);
return this.setPlotOrder(_35);
}
}
return this;
},movePlotToBack:function(_36){
if(_36 in this.plots){
var _37=this.plots[_36];
if(_37<this.stack.length-1){
var _38=this.getPlotOrder();
_38.splice(_37,1);
_38.push(_36);
return this.setPlotOrder(_38);
}
}
return this;
},addSeries:function(_39,_3a,_3b){
var run=new _c(this,_3a,_3b);
run.name=_39;
if(_39 in this.runs){
this.series[this.runs[_39]].destroy();
this.series[this.runs[_39]]=run;
}else{
this.runs[_39]=this.series.length;
this.series.push(run);
}
this.dirty=true;
if(!("ymin" in run)&&"min" in run){
run.ymin=run.min;
}
if(!("ymax" in run)&&"max" in run){
run.ymax=run.max;
}
return this;
},getSeries:function(_3c){
return this.series[this.runs[_3c]];
},removeSeries:function(_3d){
if(_3d in this.runs){
var _3e=this.runs[_3d];
delete this.runs[_3d];
this.series[_3e].destroy();
this.series.splice(_3e,1);
_e.forIn(this.runs,function(idx,_3f,_40){
if(idx>_3e){
_40[_3f]=idx-1;
}
});
this.dirty=true;
}
return this;
},updateSeries:function(_41,_42){
if(_41 in this.runs){
var run=this.series[this.runs[_41]];
run.update(_42);
this._invalidateDependentPlots(run.plot,false);
this._invalidateDependentPlots(run.plot,true);
}
return this;
},getSeriesOrder:function(_43){
return _e.map(_e.filter(this.series,function(run){
return run.plot==_43;
}),_16);
},setSeriesOrder:function(_44){
var _45,_46={},_47=_e.filter(_44,function(_48){
if(!(_48 in this.runs)||(_48 in _46)){
return false;
}
var run=this.series[this.runs[_48]];
if(_45){
if(run.plot!=_45){
return false;
}
}else{
_45=run.plot;
}
_46[_48]=1;
return true;
},this);
_e.forEach(this.series,function(run){
var _49=run.name;
if(!(_49 in _46)&&run.plot==_45){
_47.push(_49);
}
});
var _4a=_e.map(_47,function(_4b){
return this.series[this.runs[_4b]];
},this);
this.series=_4a.concat(_e.filter(this.series,function(run){
return run.plot!=_45;
}));
_e.forEach(this.series,function(run,i){
this.runs[run.name]=i;
},this);
this.dirty=true;
return this;
},moveSeriesToFront:function(_4c){
if(_4c in this.runs){
var _4d=this.runs[_4c],_4e=this.getSeriesOrder(this.series[_4d].plot);
if(_4c!=_4e[0]){
_4e.splice(_4d,1);
_4e.unshift(_4c);
return this.setSeriesOrder(_4e);
}
}
return this;
},moveSeriesToBack:function(_4f){
if(_4f in this.runs){
var _50=this.runs[_4f],_51=this.getSeriesOrder(this.series[_50].plot);
if(_4f!=_51[_51.length-1]){
_51.splice(_50,1);
_51.push(_4f);
return this.setSeriesOrder(_51);
}
}
return this;
},resize:function(_52,_53){
var box;
switch(arguments.length){
case 1:
box=_1.mixin({},_52);
_6.setMarginBox(this.node,box);
break;
case 2:
box={w:_52,h:_53};
_6.setMarginBox(this.node,box);
break;
}
box=_6.getMarginBox(this.node);
var d=this.surface.getDimensions();
if(d.width!=box.w||d.height!=box.h){
this.surface.setDimensions(box.w,box.h);
this.dirty=true;
this.coords=null;
return this.render();
}else{
return this;
}
},getGeometry:function(){
var ret={};
_e.forIn(this.axes,function(_54){
if(_54.initialized()){
ret[_54.name]={name:_54.name,vertical:_54.vertical,scaler:_54.scaler,ticks:_54.ticks};
}
});
return ret;
},setAxisWindow:function(_55,_56,_57,_58){
var _59=this.axes[_55];
if(_59){
_59.setWindow(_56,_57);
_2.forEach(this.stack,function(_5a){
if(_5a.hAxis==_55||_5a.vAxis==_55){
_5a.zoom=_58;
}
});
}
return this;
},setWindow:function(sx,sy,dx,dy,_5b){
if(!("plotArea" in this)){
this.calculateGeometry();
}
_e.forIn(this.axes,function(_5c){
var _5d,_5e,_5f=_5c.getScaler().bounds,s=_5f.span/(_5f.upper-_5f.lower);
if(_5c.vertical){
_5d=sy;
_5e=dy/s/_5d;
}else{
_5d=sx;
_5e=dx/s/_5d;
}
_5c.setWindow(_5d,_5e);
});
_2.forEach(this.stack,function(_60){
_60.zoom=_5b;
});
return this;
},zoomIn:function(_61,_62){
var _63=this.axes[_61];
if(_63){
var _64,_65,_66=_63.getScaler().bounds;
var _67=Math.min(_62[0],_62[1]);
var _68=Math.max(_62[0],_62[1]);
_67=_62[0]<_66.lower?_66.lower:_67;
_68=_62[1]>_66.upper?_66.upper:_68;
_64=(_66.upper-_66.lower)/(_68-_67);
_65=_67-_66.lower;
this.setAxisWindow(_61,_64,_65);
this.render();
}
},calculateGeometry:function(){
if(this.dirty){
return this.fullGeometry();
}
var _69=_2.filter(this.stack,function(_6a){
return _6a.dirty||(_6a.hAxis&&this.axes[_6a.hAxis].dirty)||(_6a.vAxis&&this.axes[_6a.vAxis].dirty);
},this);
_6b(_69,this.plotArea);
return this;
},fullGeometry:function(){
this._makeDirty();
_2.forEach(this.stack,_11);
if(!this.theme){
this.setTheme(new _b(dojox.charting._def));
}
_2.forEach(this.series,function(run){
if(!(run.plot in this.plots)){
if(!dc.plot2d||!dc.plot2d.Default){
throw Error("Can't find plot: Default - didn't you forget to dojo"+".require() it?");
}
var _6c=new dc.plot2d.Default(this,{});
_6c.name=run.plot;
this.plots[run.plot]=this.stack.length;
this.stack.push(_6c);
}
this.stack[this.plots[run.plot]].addSeries(run);
},this);
_2.forEach(this.stack,function(_6d){
if(_6d.hAxis){
_6d.setAxis(this.axes[_6d.hAxis]);
}
if(_6d.vAxis){
_6d.setAxis(this.axes[_6d.vAxis]);
}
},this);
var dim=this.dim=this.surface.getDimensions();
dim.width=g.normalizedLength(dim.width);
dim.height=g.normalizedLength(dim.height);
_e.forIn(this.axes,_11);
_6b(this.stack,dim);
var _6e=this.offsets={l:0,r:0,t:0,b:0};
_e.forIn(this.axes,function(_6f){
_e.forIn(_6f.getOffsets(),function(o,i){
_6e[i]+=o;
});
});
if(this.title){
this.titleGap=(this.titleGap==0)?0:this.titleGap||this.theme.chart.titleGap||20;
this.titlePos=this.titlePos||this.theme.chart.titlePos||"top";
this.titleFont=this.titleFont||this.theme.chart.titleFont;
this.titleFontColor=this.titleFontColor||this.theme.chart.titleFontColor||"black";
var _70=g.normalizedLength(g.splitFontString(this.titleFont).size);
_6e[this.titlePos=="top"?"t":"b"]+=(_70+this.titleGap);
}
_e.forIn(this.margins,function(o,i){
_6e[i]+=o;
});
this.plotArea={width:dim.width-_6e.l-_6e.r,height:dim.height-_6e.t-_6e.b};
_e.forIn(this.axes,_11);
_6b(this.stack,this.plotArea);
return this;
},render:function(){
if(this.theme){
this.theme.clear();
}
if(this.dirty){
return this.fullRender();
}
this.calculateGeometry();
_e.forEachRev(this.stack,function(_71){
_71.render(this.dim,this.offsets);
},this);
_e.forIn(this.axes,function(_72){
_72.render(this.dim,this.offsets);
},this);
this._makeClean();
if(this.surface.render){
this.surface.render();
}
return this;
},fullRender:function(){
this.fullGeometry();
var _73=this.offsets,dim=this.dim,_74;
_2.forEach(this.series,_12);
_e.forIn(this.axes,_12);
_2.forEach(this.stack,_12);
if(this.chartTitle&&this.chartTitle.tagName){
_7.destroy(this.chartTitle);
}
this.surface.clear();
this.chartTitle=null;
var t=this.theme,_75=t.plotarea&&t.plotarea.fill,_76=t.plotarea&&t.plotarea.stroke,w=Math.max(0,dim.width-_73.l-_73.r),h=Math.max(0,dim.height-_73.t-_73.b),_74={x:_73.l-1,y:_73.t-1,width:w+2,height:h+2};
if(_75){
_75=_a.prototype._shapeFill(_a.prototype._plotFill(_75,dim,_73),_74);
this.surface.createRect(_74).setFill(_75);
}
if(_76){
this.surface.createRect({x:_73.l,y:_73.t,width:w+1,height:h+1}).setStroke(_76);
}
_e.foldr(this.stack,function(z,_77){
return _77.render(dim,_73),0;
},0);
_75=this.fill!==undefined?this.fill:(t.chart&&t.chart.fill);
_76=this.stroke!==undefined?this.stroke:(t.chart&&t.chart.stroke);
if(_75=="inherit"){
var _78=this.node,_75=new _8(_4.style(_78,"backgroundColor"));
while(_75.a==0&&_78!=document.documentElement){
_75=new _8(_4.style(_78,"backgroundColor"));
_78=_78.parentNode;
}
}
if(_75){
_75=_a.prototype._plotFill(_75,dim,_73);
if(_73.l){
_74={width:_73.l,height:dim.height+1};
this.surface.createRect(_74).setFill(_a.prototype._shapeFill(_75,_74));
}
if(_73.r){
_74={x:dim.width-_73.r,width:_73.r+1,height:dim.height+2};
this.surface.createRect(_74).setFill(_a.prototype._shapeFill(_75,_74));
}
if(_73.t){
_74={width:dim.width+1,height:_73.t};
this.surface.createRect(_74).setFill(_a.prototype._shapeFill(_75,_74));
}
if(_73.b){
_74={y:dim.height-_73.b,width:dim.width+1,height:_73.b+2};
this.surface.createRect(_74).setFill(_a.prototype._shapeFill(_75,_74));
}
}
if(_76){
this.surface.createRect({width:dim.width-1,height:dim.height-1}).setStroke(_76);
}
if(this.title){
var _79=(g.renderer=="canvas"),_7a=_79||!_9("ie")&&!_9("opera")?"html":"gfx",_7b=g.normalizedLength(g.splitFontString(this.titleFont).size);
this.chartTitle=_d.createText[_7a](this,this.surface,dim.width/2,this.titlePos=="top"?_7b+this.margins.t:dim.height-this.margins.b,"middle",this.title,this.titleFont,this.titleFontColor);
}
_e.forIn(this.axes,function(_7c){
_7c.render(dim,_73);
});
this._makeClean();
if(this.surface.render){
this.surface.render();
}
return this;
},delayedRender:function(){
if(!this._delayedRenderHandle){
this._delayedRenderHandle=setTimeout(_1.hitch(this,function(){
clearTimeout(this._delayedRenderHandle);
this._delayedRenderHandle=null;
this.render();
}),this.delayInMs);
}
return this;
},connectToPlot:function(_7d,_7e,_7f){
return _7d in this.plots?this.stack[this.plots[_7d]].connect(_7e,_7f):null;
},fireEvent:function(_80,_81,_82){
if(_80 in this.runs){
var _83=this.series[this.runs[_80]].plot;
if(_83 in this.plots){
var _84=this.stack[this.plots[_83]];
if(_84){
_84.fireEvent(_80,_81,_82);
}
}
}
return this;
},_makeClean:function(){
_2.forEach(this.axes,_14);
_2.forEach(this.stack,_14);
_2.forEach(this.series,_14);
this.dirty=false;
},_makeDirty:function(){
_2.forEach(this.axes,_15);
_2.forEach(this.stack,_15);
_2.forEach(this.series,_15);
this.dirty=true;
},_invalidateDependentPlots:function(_85,_86){
if(_85 in this.plots){
var _87=this.stack[this.plots[_85]],_88,_89=_86?"vAxis":"hAxis";
if(_87[_89]){
_88=this.axes[_87[_89]];
if(_88&&_88.dependOnData()){
_88.dirty=true;
_2.forEach(this.stack,function(p){
if(p[_89]&&p[_89]==_87[_89]){
p.dirty=true;
}
});
}
}else{
_87.dirty=true;
}
}
}});
function _8a(_8b){
return {min:_8b.hmin,max:_8b.hmax};
};
function _8c(_8d){
return {min:_8d.vmin,max:_8d.vmax};
};
function _8e(_8f,h){
_8f.hmin=h.min;
_8f.hmax=h.max;
};
function _90(_91,v){
_91.vmin=v.min;
_91.vmax=v.max;
};
function _92(_93,_94){
if(_93&&_94){
_93.min=Math.min(_93.min,_94.min);
_93.max=Math.max(_93.max,_94.max);
}
return _93||_94;
};
function _6b(_95,_96){
var _97={},_98={};
_2.forEach(_95,function(_99){
var _9a=_97[_99.name]=_99.getSeriesStats();
if(_99.hAxis){
_98[_99.hAxis]=_92(_98[_99.hAxis],_8a(_9a));
}
if(_99.vAxis){
_98[_99.vAxis]=_92(_98[_99.vAxis],_8c(_9a));
}
});
_2.forEach(_95,function(_9b){
var _9c=_97[_9b.name];
if(_9b.hAxis){
_8e(_9c,_98[_9b.hAxis]);
}
if(_9b.vAxis){
_90(_9c,_98[_9b.vAxis]);
}
_9b.initializeScalers(_96,_9c);
});
};
return dojox.charting.Chart;
});
