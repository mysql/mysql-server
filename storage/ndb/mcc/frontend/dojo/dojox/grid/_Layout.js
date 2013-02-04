//>>built
define("dojox/grid/_Layout",["dojo/_base/kernel","../main","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/dom-geometry","./cells","./_RowSelector"],function(_1,_2,_3,_4,_5,_6){
return _3("dojox.grid._Layout",null,{constructor:function(_7){
this.grid=_7;
},cells:[],structure:null,defaultWidth:"6em",moveColumn:function(_8,_9,_a,_b,_c){
var _d=this.structure[_8].cells[0];
var _e=this.structure[_9].cells[0];
var _f=null;
var _10=0;
var _11=0;
for(var i=0,c;c=_d[i];i++){
if(c.index==_a){
_10=i;
break;
}
}
_f=_d.splice(_10,1)[0];
_f.view=this.grid.views.views[_9];
for(i=0,c=null;c=_e[i];i++){
if(c.index==_b){
_11=i;
break;
}
}
if(!_c){
_11+=1;
}
_e.splice(_11,0,_f);
var _12=this.grid.getCell(this.grid.getSortIndex());
if(_12){
_12._currentlySorted=this.grid.getSortAsc();
}
this.cells=[];
_a=0;
var v;
for(i=0;v=this.structure[i];i++){
for(var j=0,cs;cs=v.cells[j];j++){
for(var k=0;c=cs[k];k++){
c.index=_a;
this.cells.push(c);
if("_currentlySorted" in c){
var si=_a+1;
si*=c._currentlySorted?1:-1;
this.grid.sortInfo=si;
delete c._currentlySorted;
}
_a++;
}
}
}
_4.forEach(this.cells,function(c){
var _13=c.markup[2].split(" ");
var _14=parseInt(_13[1].substring(5));
if(_14!=c.index){
_13[1]="idx=\""+c.index+"\"";
c.markup[2]=_13.join(" ");
}
});
this.grid.setupHeaderMenu();
},setColumnVisibility:function(_15,_16){
var _17=this.cells[_15];
if(_17.hidden==_16){
_17.hidden=!_16;
var v=_17.view,w=v.viewWidth;
if(w&&w!="auto"){
v._togglingColumn=_6.getMarginBox(_17.getHeaderNode()).w||0;
}
v.update();
return true;
}else{
return false;
}
},addCellDef:function(_18,_19,_1a){
var _1b=this;
var _1c=function(_1d){
var w=0;
if(_1d.colSpan>1){
w=0;
}else{
w=_1d.width||_1b._defaultCellProps.width||_1b.defaultWidth;
if(!isNaN(w)){
w=w+"em";
}
}
return w;
};
var _1e={grid:this.grid,subrow:_18,layoutIndex:_19,index:this.cells.length};
if(_1a&&_1a instanceof _2.grid.cells._Base){
var _1f=_5.clone(_1a);
_1e.unitWidth=_1c(_1f._props);
_1f=_5.mixin(_1f,this._defaultCellProps,_1a._props,_1e);
return _1f;
}
var _20=_1a.type||_1a.cellType||this._defaultCellProps.type||this._defaultCellProps.cellType||_2.grid.cells.Cell;
if(_5.isString(_20)){
_20=_5.getObject(_20);
}
_1e.unitWidth=_1c(_1a);
return new _20(_5.mixin({},this._defaultCellProps,_1a,_1e));
},addRowDef:function(_21,_22){
var _23=[];
var _24=0,_25=0,_26=true;
for(var i=0,def,_27;(def=_22[i]);i++){
_27=this.addCellDef(_21,i,def);
_23.push(_27);
this.cells.push(_27);
if(_26&&_27.relWidth){
_24+=_27.relWidth;
}else{
if(_27.width){
var w=_27.width;
if(typeof w=="string"&&w.slice(-1)=="%"){
_25+=window.parseInt(w,10);
}else{
if(w=="auto"){
_26=false;
}
}
}
}
}
if(_24&&_26){
_4.forEach(_23,function(_28){
if(_28.relWidth){
_28.width=_28.unitWidth=((_28.relWidth/_24)*(100-_25))+"%";
}
});
}
return _23;
},addRowsDef:function(_29){
var _2a=[];
if(_5.isArray(_29)){
if(_5.isArray(_29[0])){
for(var i=0,row;_29&&(row=_29[i]);i++){
_2a.push(this.addRowDef(i,row));
}
}else{
_2a.push(this.addRowDef(0,_29));
}
}
return _2a;
},addViewDef:function(_2b){
this._defaultCellProps=_2b.defaultCell||{};
if(_2b.width&&_2b.width=="auto"){
delete _2b.width;
}
return _5.mixin({},_2b,{cells:this.addRowsDef(_2b.rows||_2b.cells)});
},setStructure:function(_2c){
this.fieldIndex=0;
this.cells=[];
var s=this.structure=[];
if(this.grid.rowSelector){
var sel={type:_2._scopeName+".grid._RowSelector"};
if(_5.isString(this.grid.rowSelector)){
var _2d=this.grid.rowSelector;
if(_2d=="false"){
sel=null;
}else{
if(_2d!="true"){
sel["width"]=_2d;
}
}
}else{
if(!this.grid.rowSelector){
sel=null;
}
}
if(sel){
s.push(this.addViewDef(sel));
}
}
var _2e=function(def){
return ("name" in def||"field" in def||"get" in def);
};
var _2f=function(def){
if(_5.isArray(def)){
if(_5.isArray(def[0])||_2e(def[0])){
return true;
}
}
return false;
};
var _30=function(def){
return (def!==null&&_5.isObject(def)&&("cells" in def||"rows" in def||("type" in def&&!_2e(def))));
};
if(_5.isArray(_2c)){
var _31=false;
for(var i=0,st;(st=_2c[i]);i++){
if(_30(st)){
_31=true;
break;
}
}
if(!_31){
s.push(this.addViewDef({cells:_2c}));
}else{
for(i=0;(st=_2c[i]);i++){
if(_2f(st)){
s.push(this.addViewDef({cells:st}));
}else{
if(_30(st)){
s.push(this.addViewDef(st));
}
}
}
}
}else{
if(_30(_2c)){
s.push(this.addViewDef(_2c));
}
}
this.cellCount=this.cells.length;
this.grid.setupHeaderMenu();
}});
});
