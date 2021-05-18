//>>built
define("dojox/grid/enhanced/plugins/exporter/CSVWriter",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","./_ExportWriter","../Exporter"],function(_1,_2,_3,_4,_5){
_5.registerWriter("csv","dojox.grid.enhanced.plugins.exporter.CSVWriter");
return _1("dojox.grid.enhanced.plugins.exporter.CSVWriter",_4,{_separator:",",_newline:"\r\n",constructor:function(_6){
if(_6){
this._separator=_6.separator?_6.separator:this._separator;
this._newline=_6.newline?_6.newline:this._newline;
}
this._headers=[];
this._dataRows=[];
},_formatCSVCell:function(_7){
if(_7===null||_7===undefined){
return "";
}
var _8=String(_7).replace(/"/g,"\"\"");
if(_8.indexOf(this._separator)>=0||_8.search(/[" \t\r\n]/)>=0){
_8="\""+_8+"\"";
}
return _8;
},beforeContentRow:function(_9){
var _a=[],_b=_2.hitch(this,this._formatCSVCell);
_3.forEach(_9.grid.layout.cells,function(_c){
if(!_c.hidden&&_3.indexOf(_9.spCols,_c.index)<0){
_a.push(_b(this._getExportDataForCell(_9.rowIndex,_9.row,_c,_9.grid)));
}
},this);
this._dataRows.push(_a);
return false;
},handleCell:function(_d){
var _e=_d.cell;
if(_d.isHeader&&!_e.hidden&&_3.indexOf(_d.spCols,_e.index)<0){
this._headers.push(_e.name||_e.field);
}
},toString:function(){
var _f=this._headers.join(this._separator);
for(var i=this._dataRows.length-1;i>=0;--i){
this._dataRows[i]=this._dataRows[i].join(this._separator);
}
return _f+this._newline+this._dataRows.join(this._newline);
}});
});
