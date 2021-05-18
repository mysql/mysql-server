//>>built
define("dojox/charting/Chart",["../main","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/dom-style","dojo/dom","dojo/dom-geometry","dojo/dom-construct","dojo/_base/Color","dojo/sniff","./Element","./SimpleTheme","./Series","./axis2d/common","./plot2d/common","dojox/gfx/shape","dojox/gfx","dojo/has!dojo-bidi?./bidi/Chart","dojox/lang/functional","dojox/lang/functional/fold","dojox/lang/functional/reversed"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,g,_11,_12){
var dc=_2.getObject("charting",true,_1),_13={l:10,t:10,r:10,b:10};
function _14(_15){
return _15.clear();
};
function _16(_17){
return _17.destroy();
};
function _18(_19){
_19.dirty=false;
return false;
};
function _1a(_1b){
_1b.dirty=true;
return true;
};
function _1c(_1d){
return _1d.name;
};
var _1e=_4(_a("dojo-bidi")?"dojox.charting.NonBidiChart":"dojox.charting.Chart",null,{constructor:function(_1f,_20){
if(!_20){
_20={};
}
this.margins=_20.margins||_13;
this._customMargins=!!_20.margins;
this.stroke=_20.stroke;
this.fill=_20.fill;
this.delayInMs=_20.delayInMs||200;
this.title=_20.title;
this.titleGap=_20.titleGap;
this.titlePos=_20.titlePos;
this.titleFont=_20.titleFont;
this.titleFontColor=_20.titleFontColor;
this.titleAlign=_20.titleAlign;
this.chartTitle=null;
this.htmlLabels=true;
if("htmlLabels" in _20){
this.htmlLabels=_20.htmlLabels;
}
this.theme=null;
this.axes={};
this.stack=[];
this.plots={};
this.series=[];
this.runs={};
this.dirty=true;
this.node=_6.byId(_1f);
var box=_7.getMarginBox(_1f);
this.surface=g.createSurface(this.node,box.w||400,box.h||300);
if(this.surface.declaredClass.indexOf("vml")==-1){
this._nativeClip=true;
}
},destroy:function(){
_3.forEach(this.series,_16);
_3.forEach(this.stack,_16);
_12.forIn(this.axes,_16);
this.surface.destroy();
if(this.chartTitle&&this.chartTitle.tagName){
_8.destroy(this.chartTitle);
}
},getCoords:function(){
var _21=this.node;
var s=_5.getComputedStyle(_21),_22=_7.getMarginBox(_21,s);
var abs=_7.position(_21,true);
_22.x=abs.x;
_22.y=abs.y;
return _22;
},setTheme:function(_23){
this.theme=_23.clone();
if(!this._customMargins){
this.margins=this.theme.chart.margins||_13;
}
this.dirty=true;
return this;
},addAxis:function(_24,_25){
var _26,_27=_25&&_25.type||"Default";
if(typeof _27=="string"){
if(!dc.axis2d||!dc.axis2d[_27]){
throw Error("Can't find axis: "+_27+" - Check "+"require() dependencies.");
}
_26=new dc.axis2d[_27](this,_25);
}else{
_26=new _27(this,_25);
}
_26.name=_24;
_26.dirty=true;
if(_24 in this.axes){
this.axes[_24].destroy();
}
this.axes[_24]=_26;
this.dirty=true;
return this;
},getAxis:function(_28){
return this.axes[_28];
},removeAxis:function(_29){
if(_29 in this.axes){
this.axes[_29].destroy();
delete this.axes[_29];
this.dirty=true;
}
return this;
},addPlot:function(_2a,_2b){
var _2c,_2d=_2b&&_2b.type||"Default";
if(typeof _2d=="string"){
if(!dc.plot2d||!dc.plot2d[_2d]){
throw Error("Can't find plot: "+_2d+" - didn't you forget to dojo"+".require() it?");
}
_2c=new dc.plot2d[_2d](this,_2b);
}else{
_2c=new _2d(this,_2b);
}
_2c.name=_2a;
_2c.dirty=true;
if(_2a in this.plots){
this.stack[this.plots[_2a]].destroy();
this.stack[this.plots[_2a]]=_2c;
}else{
this.plots[_2a]=this.stack.length;
this.stack.push(_2c);
}
this.dirty=true;
return this;
},getPlot:function(_2e){
return this.stack[this.plots[_2e]];
},removePlot:function(_2f){
if(_2f in this.plots){
var _30=this.plots[_2f];
delete this.plots[_2f];
this.stack[_30].destroy();
this.stack.splice(_30,1);
_12.forIn(this.plots,function(idx,_31,_32){
if(idx>_30){
_32[_31]=idx-1;
}
});
var ns=_3.filter(this.series,function(run){
return run.plot!=_2f;
});
if(ns.length<this.series.length){
_3.forEach(this.series,function(run){
if(run.plot==_2f){
run.destroy();
}
});
this.runs={};
_3.forEach(ns,function(run,_33){
this.runs[run.plot]=_33;
},this);
this.series=ns;
}
this.dirty=true;
}
return this;
},getPlotOrder:function(){
return _12.map(this.stack,_1c);
},setPlotOrder:function(_34){
var _35={},_36=_12.filter(_34,function(_37){
if(!(_37 in this.plots)||(_37 in _35)){
return false;
}
_35[_37]=1;
return true;
},this);
if(_36.length<this.stack.length){
_12.forEach(this.stack,function(_38){
var _39=_38.name;
if(!(_39 in _35)){
_36.push(_39);
}
});
}
var _3a=_12.map(_36,function(_3b){
return this.stack[this.plots[_3b]];
},this);
_12.forEach(_3a,function(_3c,i){
this.plots[_3c.name]=i;
},this);
this.stack=_3a;
this.dirty=true;
return this;
},movePlotToFront:function(_3d){
if(_3d in this.plots){
var _3e=this.plots[_3d];
if(_3e){
var _3f=this.getPlotOrder();
_3f.splice(_3e,1);
_3f.unshift(_3d);
return this.setPlotOrder(_3f);
}
}
return this;
},movePlotToBack:function(_40){
if(_40 in this.plots){
var _41=this.plots[_40];
if(_41<this.stack.length-1){
var _42=this.getPlotOrder();
_42.splice(_41,1);
_42.push(_40);
return this.setPlotOrder(_42);
}
}
return this;
},addSeries:function(_43,_44,_45){
var run=new _d(this,_44,_45);
run.name=_43;
if(_43 in this.runs){
this.series[this.runs[_43]].destroy();
this.series[this.runs[_43]]=run;
}else{
this.runs[_43]=this.series.length;
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
},getSeries:function(_46){
return this.series[this.runs[_46]];
},removeSeries:function(_47){
if(_47 in this.runs){
var _48=this.runs[_47];
delete this.runs[_47];
this.series[_48].destroy();
this.series.splice(_48,1);
_12.forIn(this.runs,function(idx,_49,_4a){
if(idx>_48){
_4a[_49]=idx-1;
}
});
this.dirty=true;
}
return this;
},updateSeries:function(_4b,_4c,_4d){
if(_4b in this.runs){
var run=this.series[this.runs[_4b]];
run.update(_4c);
if(_4d){
this.dirty=true;
}else{
this._invalidateDependentPlots(run.plot,false);
this._invalidateDependentPlots(run.plot,true);
}
}
return this;
},getSeriesOrder:function(_4e){
return _12.map(_12.filter(this.series,function(run){
return run.plot==_4e;
}),_1c);
},setSeriesOrder:function(_4f){
var _50,_51={},_52=_12.filter(_4f,function(_53){
if(!(_53 in this.runs)||(_53 in _51)){
return false;
}
var run=this.series[this.runs[_53]];
if(_50){
if(run.plot!=_50){
return false;
}
}else{
_50=run.plot;
}
_51[_53]=1;
return true;
},this);
_12.forEach(this.series,function(run){
var _54=run.name;
if(!(_54 in _51)&&run.plot==_50){
_52.push(_54);
}
});
var _55=_12.map(_52,function(_56){
return this.series[this.runs[_56]];
},this);
this.series=_55.concat(_12.filter(this.series,function(run){
return run.plot!=_50;
}));
_12.forEach(this.series,function(run,i){
this.runs[run.name]=i;
},this);
this.dirty=true;
return this;
},moveSeriesToFront:function(_57){
if(_57 in this.runs){
var _58=this.runs[_57],_59=this.getSeriesOrder(this.series[_58].plot);
if(_57!=_59[0]){
_59.splice(_58,1);
_59.unshift(_57);
return this.setSeriesOrder(_59);
}
}
return this;
},moveSeriesToBack:function(_5a){
if(_5a in this.runs){
var _5b=this.runs[_5a],_5c=this.getSeriesOrder(this.series[_5b].plot);
if(_5a!=_5c[_5c.length-1]){
_5c.splice(_5b,1);
_5c.push(_5a);
return this.setSeriesOrder(_5c);
}
}
return this;
},resize:function(_5d,_5e){
switch(arguments.length){
case 1:
_7.setMarginBox(this.node,_5d);
break;
case 2:
_7.setMarginBox(this.node,{w:_5d,h:_5e});
break;
}
var box=_7.getMarginBox(this.node);
var d=this.surface.getDimensions();
if(d.width!=box.w||d.height!=box.h){
this.surface.setDimensions(box.w,box.h);
this.dirty=true;
return this.render();
}else{
return this;
}
},getGeometry:function(){
var ret={};
_12.forIn(this.axes,function(_5f){
if(_5f.initialized()){
ret[_5f.name]={name:_5f.name,vertical:_5f.vertical,scaler:_5f.scaler,ticks:_5f.ticks};
}
});
return ret;
},setAxisWindow:function(_60,_61,_62,_63){
var _64=this.axes[_60];
if(_64){
_64.setWindow(_61,_62);
_3.forEach(this.stack,function(_65){
if(_65.hAxis==_60||_65.vAxis==_60){
_65.zoom=_63;
}
});
}
return this;
},setWindow:function(sx,sy,dx,dy,_66){
if(!("plotArea" in this)){
this.calculateGeometry();
}
_12.forIn(this.axes,function(_67){
var _68,_69,_6a=_67.getScaler().bounds,s=_6a.span/(_6a.upper-_6a.lower);
if(_67.vertical){
_68=sy;
_69=dy/s/_68;
}else{
_68=sx;
_69=dx/s/_68;
}
_67.setWindow(_68,_69);
});
_3.forEach(this.stack,function(_6b){
_6b.zoom=_66;
});
return this;
},zoomIn:function(_6c,_6d,_6e){
var _6f=this.axes[_6c];
if(_6f){
var _70,_71,_72=_6f.getScaler().bounds;
var _73=Math.min(_6d[0],_6d[1]);
var _74=Math.max(_6d[0],_6d[1]);
_73=_6d[0]<_72.lower?_72.lower:_73;
_74=_6d[1]>_72.upper?_72.upper:_74;
_70=(_72.upper-_72.lower)/(_74-_73);
_71=_73-_72.lower;
this.setAxisWindow(_6c,_70,_71);
if(_6e){
this.delayedRender();
}else{
this.render();
}
}
},calculateGeometry:function(){
if(this.dirty){
return this.fullGeometry();
}
var _75=_3.filter(this.stack,function(_76){
return _76.dirty||(_76.hAxis&&this.axes[_76.hAxis].dirty)||(_76.vAxis&&this.axes[_76.vAxis].dirty);
},this);
_77(_75,this.plotArea);
return this;
},fullGeometry:function(){
this._makeDirty();
_3.forEach(this.stack,_14);
if(!this.theme){
this.setTheme(new _c());
}
_3.forEach(this.series,function(run){
if(!(run.plot in this.plots)){
if(!dc.plot2d||!dc.plot2d.Default){
throw Error("Can't find plot: Default - didn't you forget to dojo"+".require() it?");
}
var _78=new dc.plot2d.Default(this,{});
_78.name=run.plot;
this.plots[run.plot]=this.stack.length;
this.stack.push(_78);
}
this.stack[this.plots[run.plot]].addSeries(run);
},this);
_3.forEach(this.stack,function(_79){
if(_79.assignAxes){
_79.assignAxes(this.axes);
}
},this);
var dim=this.dim=this.surface.getDimensions();
dim.width=g.normalizedLength(dim.width);
dim.height=g.normalizedLength(dim.height);
_12.forIn(this.axes,_14);
_77(this.stack,dim);
var _7a=this.offsets={l:0,r:0,t:0,b:0};
var _7b=this;
_12.forIn(this.axes,function(_7c){
if(_a("dojo-bidi")){
_7b._resetLeftBottom(_7c);
}
_12.forIn(_7c.getOffsets(),function(o,i){
_7a[i]=Math.max(o,_7a[i]);
});
});
if(this.title){
this.titleGap=(this.titleGap==0)?0:this.titleGap||this.theme.chart.titleGap||20;
this.titlePos=this.titlePos||this.theme.chart.titlePos||"top";
this.titleFont=this.titleFont||this.theme.chart.titleFont;
this.titleFontColor=this.titleFontColor||this.theme.chart.titleFontColor||"black";
this.titleAlign=this.titleAlign||this.theme&&this.theme.chart&&this.theme.chart.titleAlign||"middle";
var _7d=g.normalizedLength(g.splitFontString(this.titleFont).size);
_7a[this.titlePos=="top"?"t":"b"]+=(_7d+this.titleGap);
}
_12.forIn(this.margins,function(o,i){
_7a[i]+=o;
});
this.plotArea={width:dim.width-_7a.l-_7a.r,height:dim.height-_7a.t-_7a.b};
_12.forIn(this.axes,_14);
_77(this.stack,this.plotArea);
return this;
},render:function(){
if(this._delayedRenderHandle){
clearTimeout(this._delayedRenderHandle);
this._delayedRenderHandle=null;
}
if(this.theme){
this.theme.clear();
}
if(this.dirty){
return this.fullRender();
}
this.calculateGeometry();
_12.forEachRev(this.stack,function(_7e){
_7e.render(this.dim,this.offsets);
},this);
_12.forIn(this.axes,function(_7f){
_7f.render(this.dim,this.offsets);
},this);
this._makeClean();
return this;
},fullRender:function(){
this.fullGeometry();
var _80=this.offsets,dim=this.dim;
var w=Math.max(0,dim.width-_80.l-_80.r),h=Math.max(0,dim.height-_80.t-_80.b);
_3.forEach(this.series,_f.purgeGroup);
_12.forIn(this.axes,_f.purgeGroup);
_3.forEach(this.stack,_f.purgeGroup);
var _81=this.surface.children;
if(_10.dispose){
for(var i=0;i<_81.length;++i){
_10.dispose(_81[i]);
}
}
if(this.chartTitle&&this.chartTitle.tagName){
_8.destroy(this.chartTitle);
}
this.surface.clear();
this.chartTitle=null;
this._renderChartBackground(dim,_80);
if(this._nativeClip){
this._renderPlotBackground(dim,_80,w,h);
}else{
this._renderPlotBackground(dim,_80,w,h);
}
_12.foldr(this.stack,function(z,_82){
return _82.render(dim,_80),0;
},0);
if(!this._nativeClip){
this._renderChartBackground(dim,_80);
}
if(this.title){
this._renderTitle(dim,_80);
}
_12.forIn(this.axes,function(_83){
_83.render(dim,_80);
});
this._makeClean();
return this;
},_renderTitle:function(dim,_84){
var _85=(g.renderer=="canvas")&&this.htmlLabels,_86=_85||!_a("ie")&&!_a("opera")&&this.htmlLabels?"html":"gfx",_87=g.normalizedLength(g.splitFontString(this.titleFont).size),_88=g._base._getTextBox(this.title,{font:this.titleFont});
var _89=this.titleAlign;
var _8a=_a("dojo-bidi")&&this.isRightToLeft();
var _8b=dim.width/2;
if(_89==="edge"){
_89="left";
if(_8a){
_8b=dim.width-(_84.r+_88.w);
}else{
_8b=_84.l;
}
}else{
if(_89!="middle"){
if(_8a){
_89=_89==="left"?"right":"left";
}
if(_89==="left"){
_8b=this.margins.l;
}else{
if(_89==="right"){
_89="left";
_8b=dim.width-(this.margins.l+_88.w);
}
}
}
}
this.chartTitle=_e.createText[_86](this,this.surface,_8b,this.titlePos=="top"?_87+this.margins.t:dim.height-this.margins.b,_89,this.title,this.titleFont,this.titleFontColor);
},_renderChartBackground:function(dim,_8c){
var t=this.theme,_8d;
var _8e=this.fill!==undefined?this.fill:(t.chart&&t.chart.fill);
var _8f=this.stroke!==undefined?this.stroke:(t.chart&&t.chart.stroke);
if(_8e=="inherit"){
var _90=this.node;
_8e=new _9(_5.get(_90,"backgroundColor"));
while(_8e.a==0&&_90!=document.documentElement){
_8e=new _9(_5.get(_90,"backgroundColor"));
_90=_90.parentNode;
}
}
if(_8e){
if(this._nativeClip){
_8e=_b.prototype._shapeFill(_b.prototype._plotFill(_8e,dim),{x:0,y:0,width:dim.width+1,height:dim.height+1});
this.surface.createRect({width:dim.width+1,height:dim.height+1}).setFill(_8e);
}else{
_8e=_b.prototype._plotFill(_8e,dim,_8c);
if(_8c.l){
_8d={x:0,y:0,width:_8c.l,height:dim.height+1};
this.surface.createRect(_8d).setFill(_b.prototype._shapeFill(_8e,_8d));
}
if(_8c.r){
_8d={x:dim.width-_8c.r,y:0,width:_8c.r+1,height:dim.height+2};
this.surface.createRect(_8d).setFill(_b.prototype._shapeFill(_8e,_8d));
}
if(_8c.t){
_8d={x:0,y:0,width:dim.width+1,height:_8c.t};
this.surface.createRect(_8d).setFill(_b.prototype._shapeFill(_8e,_8d));
}
if(_8c.b){
_8d={x:0,y:dim.height-_8c.b,width:dim.width+1,height:_8c.b+2};
this.surface.createRect(_8d).setFill(_b.prototype._shapeFill(_8e,_8d));
}
}
}
if(_8f){
this.surface.createRect({width:dim.width-1,height:dim.height-1}).setStroke(_8f);
}
},_renderPlotBackground:function(dim,_91,w,h){
var t=this.theme;
var _92=t.plotarea&&t.plotarea.fill;
var _93=t.plotarea&&t.plotarea.stroke;
var _94={x:_91.l-1,y:_91.t-1,width:w+2,height:h+2};
if(_92){
_92=_b.prototype._shapeFill(_b.prototype._plotFill(_92,dim,_91),_94);
this.surface.createRect(_94).setFill(_92);
}
if(_93){
this.surface.createRect({x:_91.l,y:_91.t,width:w+1,height:h+1}).setStroke(_93);
}
},delayedRender:function(){
if(!this._delayedRenderHandle){
this._delayedRenderHandle=setTimeout(_2.hitch(this,function(){
this.render();
}),this.delayInMs);
}
return this;
},connectToPlot:function(_95,_96,_97){
return _95 in this.plots?this.stack[this.plots[_95]].connect(_96,_97):null;
},fireEvent:function(_98,_99,_9a){
if(_98 in this.runs){
var _9b=this.series[this.runs[_98]].plot;
if(_9b in this.plots){
var _9c=this.stack[this.plots[_9b]];
if(_9c){
_9c.fireEvent(_98,_99,_9a);
}
}
}
return this;
},_makeClean:function(){
_3.forEach(this.axes,_18);
_3.forEach(this.stack,_18);
_3.forEach(this.series,_18);
this.dirty=false;
},_makeDirty:function(){
_3.forEach(this.axes,_1a);
_3.forEach(this.stack,_1a);
_3.forEach(this.series,_1a);
this.dirty=true;
},_invalidateDependentPlots:function(_9d,_9e){
if(_9d in this.plots){
var _9f=this.stack[this.plots[_9d]],_a0,_a1=_9e?"vAxis":"hAxis";
if(_9f[_a1]){
_a0=this.axes[_9f[_a1]];
if(_a0&&_a0.dependOnData()){
_a0.dirty=true;
_3.forEach(this.stack,function(p){
if(p[_a1]&&p[_a1]==_9f[_a1]){
p.dirty=true;
}
});
}
}else{
_9f.dirty=true;
}
}
},setDir:function(dir){
return this;
},_resetLeftBottom:function(_a2){
},formatTruncatedLabel:function(_a3,_a4,_a5){
}});
function _a6(_a7){
return {min:_a7.hmin,max:_a7.hmax};
};
function _a8(_a9){
return {min:_a9.vmin,max:_a9.vmax};
};
function _aa(_ab,h){
_ab.hmin=h.min;
_ab.hmax=h.max;
};
function _ac(_ad,v){
_ad.vmin=v.min;
_ad.vmax=v.max;
};
function _ae(_af,_b0){
if(_af&&_b0){
_af.min=Math.min(_af.min,_b0.min);
_af.max=Math.max(_af.max,_b0.max);
}
return _af||_b0;
};
function _77(_b1,_b2){
var _b3={},_b4={};
_3.forEach(_b1,function(_b5){
var _b6=_b3[_b5.name]=_b5.getSeriesStats();
if(_b5.hAxis){
_b4[_b5.hAxis]=_ae(_b4[_b5.hAxis],_a6(_b6));
}
if(_b5.vAxis){
_b4[_b5.vAxis]=_ae(_b4[_b5.vAxis],_a8(_b6));
}
});
_3.forEach(_b1,function(_b7){
var _b8=_b3[_b7.name];
if(_b7.hAxis){
_aa(_b8,_b4[_b7.hAxis]);
}
if(_b7.vAxis){
_ac(_b8,_b4[_b7.vAxis]);
}
_b7.initializeScalers(_b2,_b8);
});
};
return _a("dojo-bidi")?_4("dojox.charting.Chart",[_1e,_11]):_1e;
});
