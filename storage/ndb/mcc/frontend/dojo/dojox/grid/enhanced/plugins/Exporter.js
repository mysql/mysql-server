//>>built
define("dojox/grid/enhanced/plugins/Exporter",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","../_Plugin","../../_RowSelector","../../EnhancedGrid","../../cells/_base"],function(_1,_2,_3,_4,_5,_6){
var _7=_3.getObject("dojox.grid.cells");
var _8=_1("dojox.grid.enhanced.plugins.Exporter",_4,{name:"exporter",constructor:function(_9,_a){
this.grid=_9;
this.formatter=(_a&&_3.isObject(_a))&&_a.exportFormatter;
this._mixinGrid();
},_mixinGrid:function(){
var g=this.grid;
g.exportTo=_3.hitch(this,this.exportTo);
g.exportGrid=_3.hitch(this,this.exportGrid);
g.exportSelected=_3.hitch(this,this.exportSelected);
g.setExportFormatter=_3.hitch(this,this.setExportFormatter);
},setExportFormatter:function(_b){
this.formatter=_b;
},exportGrid:function(_c,_d,_e){
if(_3.isFunction(_d)){
_e=_d;
_d={};
}
if(!_3.isString(_c)||!_3.isFunction(_e)){
return;
}
_d=_d||{};
var g=this.grid,_f=this,_10=this._getExportWriter(_c,_d.writerArgs),_11=(_d.fetchArgs&&_3.isObject(_d.fetchArgs))?_d.fetchArgs:{},_12=_11.onComplete;
if(g.store){
_11.onComplete=function(_13,_14){
if(_12){
_12(_13,_14);
}
_e(_f._goThroughGridData(_13,_10));
};
_11.sort=_11.sort||g.getSortProps();
g._storeLayerFetch(_11);
}else{
var _15=_11.start||0,_16=_11.count||-1,_17=[];
for(var i=_15;i!=_15+_16&&i<g.rowCount;++i){
_17.push(g.getItem(i));
}
_e(this._goThroughGridData(_17,_10));
}
},exportSelected:function(_18,_19,_1a){
if(!_3.isString(_18)){
return "";
}
var _1b=this._getExportWriter(_18,_19);
return _1a(this._goThroughGridData(this.grid.selection.getSelected(),_1b));
},_buildRow:function(_1c,_1d){
var _1e=this;
_2.forEach(_1c._views,function(_1f,_20){
_1c.view=_1f;
_1c.viewIdx=_20;
if(_1d.beforeView(_1c)){
_2.forEach(_1f.structure.cells,function(_21,_22){
_1c.subrow=_21;
_1c.subrowIdx=_22;
if(_1d.beforeSubrow(_1c)){
_2.forEach(_21,function(_23,_24){
if(_1c.isHeader&&_1e._isSpecialCol(_23)){
_1c.spCols.push(_23.index);
}
_1c.cell=_23;
_1c.cellIdx=_24;
_1d.handleCell(_1c);
});
_1d.afterSubrow(_1c);
}
});
_1d.afterView(_1c);
}
});
},_goThroughGridData:function(_25,_26){
var _27=this.grid,_28=_2.filter(_27.views.views,function(_29){
return !(_29 instanceof _5);
}),_2a={"grid":_27,"isHeader":true,"spCols":[],"_views":_28,"colOffset":(_28.length<_27.views.views.length?-1:0)};
if(_26.beforeHeader(_27)){
this._buildRow(_2a,_26);
_26.afterHeader();
}
_2a.isHeader=false;
if(_26.beforeContent(_25)){
_2.forEach(_25,function(_2b,_2c){
_2a.row=_2b;
_2a.rowIdx=_2c;
if(_26.beforeContentRow(_2a)){
this._buildRow(_2a,_26);
_26.afterContentRow(_2a);
}
},this);
_26.afterContent();
}
return _26.toString();
},_isSpecialCol:function(_2d){
return _2d.isRowSelector||_2d instanceof _7.RowIndex;
},_getExportWriter:function(_2e,_2f){
var _30,cls,_31=_8;
if(_31.writerNames){
_30=_31.writerNames[_2e.toLowerCase()];
cls=_3.getObject(_30);
if(cls){
var _32=new cls(_2f);
_32.formatter=this.formatter;
return _32;
}else{
throw new Error("Please make sure class \""+_30+"\" is required.");
}
}
throw new Error("The writer for \""+_2e+"\" has not been registered.");
}});
_8.registerWriter=function(_33,_34){
_8.writerNames=_8.writerNames||{};
_8.writerNames[_33]=_34;
};
_6.registerPlugin(_8);
return _8;
});
