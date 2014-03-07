//>>built
define("dojox/grid/enhanced/plugins/exporter/CSVWriter",["dojo/_base/declare","dojo/_base/array","./_ExportWriter","../Exporter"],function(_1,_2,_3,_4){
_4.registerWriter("csv","dojox.grid.enhanced.plugins.exporter.CSVWriter");
return _1("dojox.grid.enhanced.plugins.exporter.CSVWriter",_3,{_separator:",",_newline:"\r\n",constructor:function(_5){
if(_5){
this._separator=_5.separator?_5.separator:this._separator;
this._newline=_5.newline?_5.newline:this._newline;
}
this._headers=[];
this._dataRows=[];
},_formatCSVCell:function(_6){
if(_6===null||_6===undefined){
return "";
}
var _7=String(_6).replace(/"/g,"\"\"");
if(_7.indexOf(this._separator)>=0||_7.search(/[" \t\r\n]/)>=0){
_7="\""+_7+"\"";
}
return _7;
},beforeContentRow:function(_8){
var _9=[],_a=this._formatCSVCell;
_2.forEach(_8.grid.layout.cells,function(_b){
if(!_b.hidden&&_2.indexOf(_8.spCols,_b.index)<0){
_9.push(_a(this._getExportDataForCell(_8.rowIndex,_8.row,_b,_8.grid)));
}
},this);
this._dataRows.push(_9);
return false;
},handleCell:function(_c){
var _d=_c.cell;
if(_c.isHeader&&!_d.hidden&&_2.indexOf(_c.spCols,_d.index)<0){
this._headers.push(_d.name||_d.field);
}
},toString:function(){
var _e=this._headers.join(this._separator);
for(var i=this._dataRows.length-1;i>=0;--i){
this._dataRows[i]=this._dataRows[i].join(this._separator);
}
return _e+this._newline+this._dataRows.join(this._newline);
}});
});
