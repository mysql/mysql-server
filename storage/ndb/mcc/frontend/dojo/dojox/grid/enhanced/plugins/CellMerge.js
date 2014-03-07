//>>built
define("dojox/grid/enhanced/plugins/CellMerge",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/html","../_Plugin","../../EnhancedGrid"],function(_1,_2,_3,_4,_5,_6){
var _7=_1("dojox.grid.enhanced.plugins.CellMerge",_5,{name:"cellMerge",constructor:function(_8,_9){
this.grid=_8;
this._records=[];
this._merged={};
if(_9&&_3.isObject(_9)){
this._setupConfig(_9.mergedCells);
}
this._initEvents();
this._mixinGrid();
},mergeCells:function(_a,_b,_c,_d){
var _e=this._createRecord({"row":_a,"start":_b,"end":_c,"major":_d});
if(_e){
this._updateRows(_e);
}
return _e;
},unmergeCells:function(_f){
var idx;
if(_f&&(idx=_2.indexOf(this._records,_f))>=0){
this._records.splice(idx,1);
this._updateRows(_f);
}
},getMergedCells:function(){
var res=[];
for(var i in this._merged){
res=res.concat(this._merged[i]);
}
return res;
},getMergedCellsByRow:function(_10){
return this._merged[_10]||[];
},_setupConfig:function(_11){
_2.forEach(_11,this._createRecord,this);
},_initEvents:function(){
_2.forEach(this.grid.views.views,function(_12){
this.connect(_12,"onAfterRow",_3.hitch(this,"_onAfterRow",_12.index));
},this);
},_mixinGrid:function(){
var g=this.grid;
g.mergeCells=_3.hitch(this,"mergeCells");
g.unmergeCells=_3.hitch(this,"unmergeCells");
g.getMergedCells=_3.hitch(this,"getMergedCells");
g.getMergedCellsByRow=_3.hitch(this,"getMergedCellsByRow");
},_getWidth:function(_13){
var _14=this.grid.layout.cells[_13].getHeaderNode();
return _4.position(_14).w;
},_onAfterRow:function(_15,_16,_17){
try{
if(_16<0){
return;
}
var _18=[],i,j,len=this._records.length,_19=this.grid.layout.cells;
for(i=0;i<len;++i){
var _1a=this._records[i];
var _1b=this.grid._by_idx[_16];
if(_1a.view==_15&&_1a.row(_16,_1b&&_1b.item,this.grid.store)){
var res={record:_1a,hiddenCells:[],totalWidth:0,majorNode:_19[_1a.major].getNode(_16),majorHeaderNode:_19[_1a.major].getHeaderNode()};
for(j=_1a.start;j<=_1a.end;++j){
var w=this._getWidth(j,_16);
res.totalWidth+=w;
if(j!=_1a.major){
res.hiddenCells.push(_19[j].getNode(_16));
}
}
if(_17.length!=1||res.totalWidth>0){
for(j=_18.length-1;j>=0;--j){
var r=_18[j].record;
if((r.start>=_1a.start&&r.start<=_1a.end)||(r.end>=_1a.start&&r.end<=_1a.end)){
_18.splice(j,1);
}
}
_18.push(res);
}
}
}
this._merged[_16]=[];
_2.forEach(_18,function(res){
_2.forEach(res.hiddenCells,function(_1c){
_4.style(_1c,"display","none");
});
var pbm=_4.marginBox(res.majorHeaderNode).w-_4.contentBox(res.majorHeaderNode).w;
var tw=res.totalWidth;
if(!_4.isWebKit){
tw-=pbm;
}
_4.style(res.majorNode,"width",tw+"px");
res.majorNode.setAttribute("colspan",res.hiddenCells.length+1);
this._merged[_16].push({"row":_16,"start":res.record.start,"end":res.record.end,"major":res.record.major,"handle":res.record});
},this);
}
catch(e){
console.warn("CellMerge._onAfterRow() error: ",_16,e);
}
},_createRecord:function(_1d){
if(this._isValid(_1d)){
_1d={"row":_1d.row,"start":_1d.start,"end":_1d.end,"major":_1d.major};
var _1e=this.grid.layout.cells;
_1d.view=_1e[_1d.start].view.index;
_1d.major=typeof _1d.major=="number"&&!isNaN(_1d.major)?_1d.major:_1d.start;
if(typeof _1d.row=="number"){
var r=_1d.row;
_1d.row=function(_1f){
return _1f===r;
};
}else{
if(typeof _1d.row=="string"){
var id=_1d.row;
_1d.row=function(_20,_21,_22){
try{
if(_22&&_21&&_22.getFeatures()["dojo.data.api.Identity"]){
return _22.getIdentity(_21)==id;
}
}
catch(e){
console.error(e);
}
return false;
};
}
}
if(_3.isFunction(_1d.row)){
this._records.push(_1d);
return _1d;
}
}
return null;
},_isValid:function(_23){
var _24=this.grid.layout.cells,_25=_24.length;
return (_3.isObject(_23)&&("row" in _23)&&("start" in _23)&&("end" in _23)&&_23.start>=0&&_23.start<_25&&_23.end>_23.start&&_23.end<_25&&_24[_23.start].view.index==_24[_23.end].view.index&&_24[_23.start].subrow==_24[_23.end].subrow&&!(typeof _23.major=="number"&&(_23.major<_23.start||_23.major>_23.end)));
},_updateRows:function(_26){
var min=null;
for(var i=0,_27=this.grid.rowCount;i<_27;++i){
var _28=this.grid._by_idx[i];
if(_28&&_26.row(i,_28&&_28.item,this.grid.store)){
this.grid.views.updateRow(i);
if(min===null){
min=i;
}
}
}
if(min>=0){
this.grid.scroller.rowHeightChanged(min);
}
}});
_6.registerPlugin(_7);
return _7;
});
