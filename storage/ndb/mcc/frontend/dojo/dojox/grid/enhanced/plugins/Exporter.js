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
},exportSelected:function(_18,_19){
if(!_3.isString(_18)){
return "";
}
var _1a=this._getExportWriter(_18,_19);
return this._goThroughGridData(this.grid.selection.getSelected(),_1a);
},_buildRow:function(_1b,_1c){
var _1d=this;
_2.forEach(_1b._views,function(_1e,_1f){
_1b.view=_1e;
_1b.viewIdx=_1f;
if(_1c.beforeView(_1b)){
_2.forEach(_1e.structure.cells,function(_20,_21){
_1b.subrow=_20;
_1b.subrowIdx=_21;
if(_1c.beforeSubrow(_1b)){
_2.forEach(_20,function(_22,_23){
if(_1b.isHeader&&_1d._isSpecialCol(_22)){
_1b.spCols.push(_22.index);
}
_1b.cell=_22;
_1b.cellIdx=_23;
_1c.handleCell(_1b);
});
_1c.afterSubrow(_1b);
}
});
_1c.afterView(_1b);
}
});
},_goThroughGridData:function(_24,_25){
var _26=this.grid,_27=_2.filter(_26.views.views,function(_28){
return !(_28 instanceof _5);
}),_29={"grid":_26,"isHeader":true,"spCols":[],"_views":_27,"colOffset":(_27.length<_26.views.views.length?-1:0)};
if(_25.beforeHeader(_26)){
this._buildRow(_29,_25);
_25.afterHeader();
}
_29.isHeader=false;
if(_25.beforeContent(_24)){
_2.forEach(_24,function(_2a,_2b){
_29.row=_2a;
_29.rowIdx=_2b;
if(_25.beforeContentRow(_29)){
this._buildRow(_29,_25);
_25.afterContentRow(_29);
}
},this);
_25.afterContent();
}
return _25.toString();
},_isSpecialCol:function(_2c){
return _2c.isRowSelector||_2c instanceof _7.RowIndex;
},_getExportWriter:function(_2d,_2e){
var _2f,cls,_30=_8;
if(_30.writerNames){
_2f=_30.writerNames[_2d.toLowerCase()];
cls=_3.getObject(_2f);
if(cls){
var _31=new cls(_2e);
_31.formatter=this.formatter;
return _31;
}else{
throw new Error("Please make sure class \""+_2f+"\" is required.");
}
}
throw new Error("The writer for \""+_2d+"\" has not been registered.");
}});
_8.registerWriter=function(_32,_33){
_8.writerNames=_8.writerNames||{};
_8.writerNames[_32]=_33;
};
_6.registerPlugin(_8);
return _8;
});
