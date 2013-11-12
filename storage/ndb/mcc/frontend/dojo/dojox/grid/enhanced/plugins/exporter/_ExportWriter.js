//>>built
define("dojox/grid/enhanced/plugins/exporter/_ExportWriter",["dojo/_base/declare"],function(_1){
return _1("dojox.grid.enhanced.plugins.exporter._ExportWriter",null,{constructor:function(_2){
},_getExportDataForCell:function(_3,_4,_5,_6){
var _7=(_5.get||_6.get).call(_5,_3,_4);
if(this.formatter){
return this.formatter(_7,_5,_3,_4);
}else{
return _7;
}
},beforeHeader:function(_8){
return true;
},afterHeader:function(){
},beforeContent:function(_9){
return true;
},afterContent:function(){
},beforeContentRow:function(_a){
return true;
},afterContentRow:function(_b){
},beforeView:function(_c){
return true;
},afterView:function(_d){
},beforeSubrow:function(_e){
return true;
},afterSubrow:function(_f){
},handleCell:function(_10){
},toString:function(){
return "";
}});
});
