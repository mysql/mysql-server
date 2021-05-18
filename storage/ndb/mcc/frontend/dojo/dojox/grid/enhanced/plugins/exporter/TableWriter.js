//>>built
define("dojox/grid/enhanced/plugins/exporter/TableWriter",["dojo/_base/declare","dojo/_base/array","dojo/dom-geometry","./_ExportWriter","../Exporter"],function(_1,_2,_3,_4,_5){
_5.registerWriter("table","dojox.grid.enhanced.plugins.exporter.TableWriter");
return _1("dojox.grid.enhanced.plugins.exporter.TableWriter",_4,{constructor:function(_6){
this._viewTables=[];
this._tableAttrs=_6||{};
},_getTableAttrs:function(_7){
var _8=this._tableAttrs[_7]||"";
if(_8&&_8[0]!=" "){
_8=" "+_8;
}
return _8;
},_getRowClass:function(_9){
return _9.isHeader?" grid_header":[" grid_row grid_row_",_9.rowIdx+1,_9.rowIdx%2?" grid_even_row":" grid_odd_row"].join("");
},_getColumnClass:function(_a){
var _b=_a.cell.index+_a.colOffset+1;
return [" grid_column grid_column_",_b,_b%2?" grid_odd_column":" grid_even_column"].join("");
},beforeView:function(_c){
var _d=_c.viewIdx,_e=this._viewTables[_d],_f,_10=_3.getMarginBox(_c.view.contentNode).w;
if(!_e){
var _11=0;
for(var i=0;i<_d;++i){
_11+=this._viewTables[i]._width;
}
_e=this._viewTables[_d]=["<div class=\"grid_view\" style=\"position: absolute; top: 0; ",_3.isBodyLtr()?"left":"right",":",_11,"px;\">"];
}
_e._width=_10;
if(_c.isHeader){
_f=_3.getContentBox(_c.view.headerContentNode).h;
}else{
var _12=_c.grid.getRowNode(_c.rowIdx);
if(_12){
_f=_3.getContentBox(_12).h;
}else{
_f=_c.grid.scroller.averageRowHeight;
}
}
_e.push("<table class=\"",this._getRowClass(_c),"\" style=\"table-layout:fixed; height:",_f,"px; width:",_10,"px;\" ","border=\"0\" cellspacing=\"0\" cellpadding=\"0\" ",this._getTableAttrs("table"),"><tbody ",this._getTableAttrs("tbody"),">");
return true;
},afterView:function(_13){
this._viewTables[_13.viewIdx].push("</tbody></table>");
},beforeSubrow:function(_14){
this._viewTables[_14.viewIdx].push("<tr",this._getTableAttrs("tr"),">");
return true;
},afterSubrow:function(_15){
this._viewTables[_15.viewIdx].push("</tr>");
},handleCell:function(_16){
var _17=_16.cell;
if(_17.hidden||_2.indexOf(_16.spCols,_17.index)>=0){
return;
}
var _18=_16.isHeader?"th":"td",_19=[_17.colSpan?" colspan=\""+_17.colSpan+"\"":"",_17.rowSpan?" rowspan=\""+_17.rowSpan+"\"":""," style=\"width: ",_3.getContentBox(_17.getHeaderNode()).w,"px;\"",this._getTableAttrs(_18)," class=\"",this._getColumnClass(_16),"\""].join(""),_1a=this._viewTables[_16.viewIdx];
_1a.push("<",_18,_19,">");
if(_16.isHeader){
_1a.push(_17.name||_17.field);
}else{
_1a.push(this._getExportDataForCell(_16.rowIdx,_16.row,_17,_16.grid));
}
_1a.push("</",_18,">");
},afterContent:function(){
_2.forEach(this._viewTables,function(_1b){
_1b.push("</div>");
});
},toString:function(){
var _1c=_2.map(this._viewTables,function(_1d){
return _1d.join("");
}).join("");
return ["<div style=\"position: relative;\">",_1c,"</div>"].join("");
}});
});
