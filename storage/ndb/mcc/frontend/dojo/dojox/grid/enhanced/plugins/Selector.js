//>>built
define("dojox/grid/enhanced/plugins/Selector",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/event","dojo/keys","dojo/query","dojo/_base/html","dojo/_base/window","dijit/focus","../../_RowSelector","../_Plugin","../../EnhancedGrid","../../cells/_base","./AutoScroll"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var _e=0,_f=1,_10=2,_11={col:"row",row:"col"},_12=function(_13,_14,_15,end,_16){
if(_13!=="cell"){
_14=_14[_13];
_15=_15[_13];
end=end[_13];
if(typeof _14!=="number"||typeof _15!=="number"||typeof end!=="number"){
return false;
}
return _16?((_14>=_15&&_14<end)||(_14>end&&_14<=_15)):((_14>=_15&&_14<=end)||(_14>=end&&_14<=_15));
}else{
return _12("col",_14,_15,end,_16)&&_12("row",_14,_15,end,_16);
}
},_17=function(_18,v1,v2){
try{
if(v1&&v2){
switch(_18){
case "col":
case "row":
return v1[_18]==v2[_18]&&typeof v1[_18]=="number"&&!(_11[_18] in v1)&&!(_11[_18] in v2);
case "cell":
return v1.col==v2.col&&v1.row==v2.row&&typeof v1.col=="number"&&typeof v1.row=="number";
}
}
}
catch(e){
}
return false;
},_19=function(evt){
try{
if(evt&&evt.preventDefault){
_5.stop(evt);
}
}
catch(e){
}
},_1a=function(_1b,_1c,_1d){
switch(_1b){
case "col":
return {"col":typeof _1d=="undefined"?_1c:_1d,"except":[]};
case "row":
return {"row":_1c,"except":[]};
case "cell":
return {"row":_1c,"col":_1d};
}
return null;
};
var _1e=_3("dojox.grid.enhanced.plugins.Selector",_c,{name:"selector",constructor:function(_1f,_20){
this.grid=_1f;
this._config={row:_10,col:_10,cell:_10};
this.noClear=_20&&_20.noClear;
this.setupConfig(_20);
if(_1f.selectionMode==="single"){
this._config.row=_f;
}
this._enabled=true;
this._selecting={};
this._selected={"col":[],"row":[],"cell":[]};
this._startPoint={};
this._currentPoint={};
this._lastAnchorPoint={};
this._lastEndPoint={};
this._lastSelectedAnchorPoint={};
this._lastSelectedEndPoint={};
this._keyboardSelect={};
this._lastType=null;
this._selectedRowModified={};
this._hacks();
this._initEvents();
this._initAreas();
this._mixinGrid();
},destroy:function(){
this.inherited(arguments);
},setupConfig:function(_21){
if(!_21||!_2.isObject(_21)){
return;
}
var _22=["row","col","cell"];
for(var _23 in _21){
if(_4.indexOf(_22,_23)>=0){
if(!_21[_23]||_21[_23]=="disabled"){
this._config[_23]=_e;
}else{
if(_21[_23]=="single"){
this._config[_23]=_f;
}else{
this._config[_23]=_10;
}
}
}
}
var _24=["none","single","extended"][this._config.row];
this.grid.selection.setMode(_24);
},isSelected:function(_25,_26,_27){
return this._isSelected(_25,_1a(_25,_26,_27));
},toggleSelect:function(_28,_29,_2a){
this._startSelect(_28,_1a(_28,_29,_2a),this._config[_28]===_10,false,false,!this.isSelected(_28,_29,_2a));
this._endSelect(_28);
},select:function(_2b,_2c,_2d){
if(!this.isSelected(_2b,_2c,_2d)){
this.toggleSelect(_2b,_2c,_2d);
}
},deselect:function(_2e,_2f,_30){
if(this.isSelected(_2e,_2f,_30)){
this.toggleSelect(_2e,_2f,_30);
}
},selectRange:function(_31,_32,end,_33){
this.grid._selectingRange=true;
var _34=_31=="cell"?_1a(_31,_32.row,_32.col):_1a(_31,_32),_35=_31=="cell"?_1a(_31,end.row,end.col):_1a(_31,end);
this._startSelect(_31,_34,false,false,false,_33);
this._highlight(_31,_35,_33===undefined?true:_33);
this._endSelect(_31);
this.grid._selectingRange=false;
},clear:function(_36){
this._clearSelection(_36||"all");
},isSelecting:function(_37){
if(typeof _37=="undefined"){
return this._selecting.col||this._selecting.row||this._selecting.cell;
}
return this._selecting[_37];
},selectEnabled:function(_38){
if(typeof _38!="undefined"&&!this.isSelecting()){
this._enabled=!!_38;
}
return this._enabled;
},getSelected:function(_39,_3a){
switch(_39){
case "cell":
return _4.map(this._selected[_39],function(_3b){
return _3b;
});
case "col":
case "row":
return _4.map(_3a?this._selected[_39]:_4.filter(this._selected[_39],function(_3c){
return _3c.except.length===0;
}),function(_3d){
return _3a?_3d:_3d[_39];
});
}
return [];
},getSelectedCount:function(_3e,_3f){
switch(_3e){
case "cell":
return this._selected[_3e].length;
case "col":
case "row":
return (_3f?this._selected[_3e]:_4.filter(this._selected[_3e],function(_40){
return _40.except.length===0;
})).length;
}
return 0;
},getSelectedType:function(){
var s=this._selected;
return ["","cell","row","row|cell","col","col|cell","col|row","col|row|cell"][(!!s.cell.length)|(!!s.row.length<<1)|(!!s.col.length<<2)];
},getLastSelectedRange:function(_41){
return this._lastAnchorPoint[_41]?{"start":this._lastAnchorPoint[_41],"end":this._lastEndPoint[_41]}:null;
},_hacks:function(){
var g=this.grid;
var _42=function(e){
if(e.cellNode){
g.onMouseUp(e);
}
g.onMouseUpRow(e);
};
var _43=_2.hitch(g,"onMouseUp");
var _44=_2.hitch(g,"onMouseDown");
var _45=function(e){
e.cellNode.style.border="solid 1px";
};
_4.forEach(g.views.views,function(_46){
_46.content.domouseup=_42;
_46.header.domouseup=_43;
if(_46.declaredClass=="dojox.grid._RowSelector"){
_46.domousedown=_44;
_46.domouseup=_43;
_46.dofocus=_45;
}
});
g.selection.clickSelect=function(){
};
this._oldDeselectAll=g.selection.deselectAll;
var _47=this;
g.selection.selectRange=function(_48,to){
_47.selectRange("row",_48,to,true);
if(g.selection.preserver){
g.selection.preserver._updateMapping(true,true,false,_48,to);
}
g.selection.onChanged();
};
g.selection.deselectRange=function(_49,to){
_47.selectRange("row",_49,to,false);
if(g.selection.preserver){
g.selection.preserver._updateMapping(true,false,false,_49,to);
}
g.selection.onChanged();
};
g.selection.deselectAll=function(){
g._selectingRange=true;
_47._oldDeselectAll.apply(g.selection,arguments);
_47._clearSelection("all");
g._selectingRange=false;
if(g.selection.preserver){
g.selection.preserver._updateMapping(true,false,true);
}
g.selection.onChanged();
};
var _4a=g.views.views[0];
if(_4a instanceof _b){
_4a.doStyleRowNode=function(_4b,_4c){
_8.removeClass(_4c,"dojoxGridRow");
_8.addClass(_4c,"dojoxGridRowbar");
_8.addClass(_4c,"dojoxGridNonNormalizedCell");
_8.toggleClass(_4c,"dojoxGridRowbarOver",g.rows.isOver(_4b));
_8.toggleClass(_4c,"dojoxGridRowbarSelected",!!g.selection.isSelected(_4b));
};
}
this.connect(g,"updateRow",function(_4d){
_4.forEach(g.layout.cells,function(_4e){
if(this.isSelected("cell",_4d,_4e.index)){
this._highlightNode(_4e.getNode(_4d),true);
}
},this);
});
},_mixinGrid:function(){
var g=this.grid;
g.setupSelectorConfig=_2.hitch(this,this.setupConfig);
g.onStartSelect=function(){
};
g.onEndSelect=function(){
};
g.onStartDeselect=function(){
};
g.onEndDeselect=function(){
};
g.onSelectCleared=function(){
};
},_initEvents:function(){
var g=this.grid,_4f=this,dp=_2.partial,_50=function(_51,e){
if(_51==="row"){
_4f._isUsingRowSelector=true;
}
if(_4f.selectEnabled()&&_4f._config[_51]&&e.button!=2){
if(_4f._keyboardSelect.col||_4f._keyboardSelect.row||_4f._keyboardSelect.cell){
_4f._endSelect("all");
_4f._keyboardSelect.col=_4f._keyboardSelect.row=_4f._keyboardSelect.cell=0;
}
if(_4f._usingKeyboard){
_4f._usingKeyboard=false;
}
var _52=_1a(_51,e.rowIndex,e.cell&&e.cell.index);
_4f._startSelect(_51,_52,e.ctrlKey,e.shiftKey);
}
},_53=_2.hitch(this,"_endSelect");
this.connect(g,"onHeaderCellMouseDown",dp(_50,"col"));
this.connect(g,"onHeaderCellMouseUp",dp(_53,"col"));
this.connect(g,"onRowSelectorMouseDown",dp(_50,"row"));
this.connect(g,"onRowSelectorMouseUp",dp(_53,"row"));
this.connect(g,"onCellMouseDown",function(e){
if(e.cell&&e.cell.isRowSelector){
return;
}
if(g.singleClickEdit){
_4f._singleClickEdit=true;
g.singleClickEdit=false;
}
_50(_4f._config["cell"]==_e?"row":"cell",e);
});
this.connect(g,"onCellMouseUp",function(e){
if(_4f._singleClickEdit){
delete _4f._singleClickEdit;
g.singleClickEdit=true;
}
_53("all",e);
});
this.connect(g,"onCellMouseOver",function(e){
if(_4f._curType!="row"&&_4f._selecting[_4f._curType]&&_4f._config[_4f._curType]==_10){
_4f._highlight("col",_1a("col",e.cell.index),_4f._toSelect);
if(!_4f._keyboardSelect.cell){
_4f._highlight("cell",_1a("cell",e.rowIndex,e.cell.index),_4f._toSelect);
}
}
});
this.connect(g,"onHeaderCellMouseOver",function(e){
if(_4f._selecting.col&&_4f._config.col==_10){
_4f._highlight("col",_1a("col",e.cell.index),_4f._toSelect);
}
});
this.connect(g,"onRowMouseOver",function(e){
if(_4f._selecting.row&&_4f._config.row==_10){
_4f._highlight("row",_1a("row",e.rowIndex),_4f._toSelect);
}
});
this.connect(g,"onSelectedById","_onSelectedById");
this.connect(g,"_onFetchComplete",function(){
if(!g._notRefreshSelection){
this._refreshSelected(true);
}
});
this.connect(g.scroller,"buildPage",function(){
if(!g._notRefreshSelection){
this._refreshSelected(true);
}
});
this.connect(_9.doc,"onmouseup",dp(_53,"all"));
this.connect(g,"onEndAutoScroll",function(_54,_55,_56,_57){
var _58=_4f._selecting.cell,_59,_5a,dir=_55?1:-1;
if(_54&&(_58||_4f._selecting.row)){
_59=_58?"cell":"row";
_5a=_4f._currentPoint[_59];
_4f._highlight(_59,_1a(_59,_5a.row+dir,_5a.col),_4f._toSelect);
}else{
if(!_54&&(_58||_4f._selecting.col)){
_59=_58?"cell":"col";
_5a=_4f._currentPoint[_59];
_4f._highlight(_59,_1a(_59,_5a.row,_57),_4f._toSelect);
}
}
});
this.subscribe("dojox/grid/rearrange/move/"+g.id,"_onInternalRearrange");
this.subscribe("dojox/grid/rearrange/copy/"+g.id,"_onInternalRearrange");
this.subscribe("dojox/grid/rearrange/change/"+g.id,"_onExternalChange");
this.subscribe("dojox/grid/rearrange/insert/"+g.id,"_onExternalChange");
this.subscribe("dojox/grid/rearrange/remove/"+g.id,"clear");
this.connect(g,"onSelected",function(_5b){
if(this._selectedRowModified&&this._isUsingRowSelector){
delete this._selectedRowModified;
}else{
if(!this.grid._selectingRange){
this.select("row",_5b);
}
}
});
this.connect(g,"onDeselected",function(_5c){
if(this._selectedRowModified&&this._isUsingRowSelector){
delete this._selectedRowModified;
}else{
if(!this.grid._selectingRange){
this.deselect("row",_5c);
}
}
});
},_onSelectedById:function(id,_5d,_5e){
if(this.grid._noInternalMapping){
return;
}
var _5f=[this._lastAnchorPoint.row,this._lastEndPoint.row,this._lastSelectedAnchorPoint.row,this._lastSelectedEndPoint.row];
_5f=_5f.concat(this._selected.row);
var _60=false;
_4.forEach(_5f,function(_61){
if(_61){
if(_61.id===id){
_60=true;
_61.row=_5d;
}else{
if(_61.row===_5d&&_61.id){
_61.row=-1;
}
}
}
});
if(!_60&&_5e){
_4.some(this._selected.row,function(_62){
if(_62&&!_62.id&&!_62.except.length){
_62.id=id;
_62.row=_5d;
return true;
}
return false;
});
}
_60=false;
_5f=[this._lastAnchorPoint.cell,this._lastEndPoint.cell,this._lastSelectedAnchorPoint.cell,this._lastSelectedEndPoint.cell];
_5f=_5f.concat(this._selected.cell);
_4.forEach(_5f,function(_63){
if(_63){
if(_63.id===id){
_60=true;
_63.row=_5d;
}else{
if(_63.row===_5d&&_63.id){
_63.row=-1;
}
}
}
});
},onSetStore:function(){
this._clearSelection("all");
},_onInternalRearrange:function(_64,_65){
try{
this._refresh("col",false);
_4.forEach(this._selected.row,function(_66){
_4.forEach(this.grid.layout.cells,function(_67){
this._highlightNode(_67.getNode(_66.row),false);
},this);
},this);
_7(".dojoxGridRowSelectorSelected").forEach(function(_68){
_8.removeClass(_68,"dojoxGridRowSelectorSelected");
_8.removeClass(_68,"dojoxGridRowSelectorSelectedUp");
_8.removeClass(_68,"dojoxGridRowSelectorSelectedDown");
});
var _69=function(_6a){
if(_6a){
delete _6a.converted;
}
},_6b=[this._lastAnchorPoint[_64],this._lastEndPoint[_64],this._lastSelectedAnchorPoint[_64],this._lastSelectedEndPoint[_64]];
if(_64==="cell"){
this.selectRange("cell",_65.to.min,_65.to.max);
var _6c=this.grid.layout.cells;
_4.forEach(_6b,function(_6d){
if(_6d.converted){
return;
}
for(var r=_65.from.min.row,tr=_65.to.min.row;r<=_65.from.max.row;++r,++tr){
for(var c=_65.from.min.col,tc=_65.to.min.col;c<=_65.from.max.col;++c,++tc){
while(_6c[c].hidden){
++c;
}
while(_6c[tc].hidden){
++tc;
}
if(_6d.row==r&&_6d.col==c){
_6d.row=tr;
_6d.col=tc;
_6d.converted=true;
return;
}
}
}
});
}else{
_6b=this._selected.cell.concat(this._selected[_64]).concat(_6b).concat([this._lastAnchorPoint.cell,this._lastEndPoint.cell,this._lastSelectedAnchorPoint.cell,this._lastSelectedEndPoint.cell]);
_4.forEach(_6b,function(_6e){
if(_6e&&!_6e.converted){
var _6f=_6e[_64];
if(_6f in _65){
_6e[_64]=_65[_6f];
}
_6e.converted=true;
}
});
_4.forEach(this._selected[_11[_64]],function(_70){
for(var i=0,len=_70.except.length;i<len;++i){
var _71=_70.except[i];
if(_71 in _65){
_70.except[i]=_65[_71];
}
}
});
}
_4.forEach(_6b,_69);
this._refreshSelected(true);
this._focusPoint(_64,this._lastEndPoint);
}
catch(e){
console.warn("Selector._onInternalRearrange() error",e);
}
},_onExternalChange:function(_72,_73){
var _74=_72=="cell"?_73.min:_73[0],end=_72=="cell"?_73.max:_73[_73.length-1];
this.selectRange(_72,_74,end);
},_refresh:function(_75,_76){
if(!this._keyboardSelect[_75]){
_4.forEach(this._selected[_75],function(_77){
this._highlightSingle(_75,_76,_77,undefined,true);
},this);
}
},_refreshSelected:function(){
this._refresh("col",true);
this._refresh("row",true);
this._refresh("cell",true);
},_initAreas:function(){
var g=this.grid,f=g.focus,_78=this,_79=1,_7a=2,_7b=function(_7c,_7d,_7e,_7f,evt){
var ks=_78._keyboardSelect;
if(evt.shiftKey&&ks[_7c]){
if(ks[_7c]===_79){
if(_7c==="cell"){
var _80=_78._lastEndPoint[_7c];
if(f.cell!=g.layout.cells[_80.col+_7f]||f.rowIndex!=_80.row+_7e){
ks[_7c]=0;
return;
}
}
_78._startSelect(_7c,_78._lastAnchorPoint[_7c],true,false,true);
_78._highlight(_7c,_78._lastEndPoint[_7c],_78._toSelect);
ks[_7c]=_7a;
}
var _81=_7d(_7c,_7e,_7f,evt);
if(_78._isValid(_7c,_81,g)){
_78._highlight(_7c,_81,_78._toSelect);
}
_19(evt);
}
},_82=function(_83,_84,evt,_85){
if(_85&&_78.selectEnabled()&&_78._config[_83]!=_e){
switch(evt.keyCode){
case _6.SPACE:
_78._startSelect(_83,_84(),evt.ctrlKey,evt.shiftKey);
_78._endSelect(_83);
break;
case _6.SHIFT:
if(_78._config[_83]==_10&&_78._isValid(_83,_78._lastAnchorPoint[_83],g)){
_78._endSelect(_83);
_78._keyboardSelect[_83]=_79;
_78._usingKeyboard=true;
}
}
}
},_86=function(_87,evt,_88){
if(_88&&evt.keyCode==_6.SHIFT&&_78._keyboardSelect[_87]){
_78._endSelect(_87);
_78._keyboardSelect[_87]=0;
}
};
if(g.views.views[0] instanceof _b){
this._lastFocusedRowBarIdx=0;
f.addArea({name:"rowHeader",onFocus:function(evt,_89){
var _8a=g.views.views[0];
if(_8a instanceof _b){
var _8b=_8a.getCellNode(_78._lastFocusedRowBarIdx,0);
if(_8b){
_8.toggleClass(_8b,f.focusClass,false);
}
if(evt&&"rowIndex" in evt){
if(evt.rowIndex>=0){
_78._lastFocusedRowBarIdx=evt.rowIndex;
}else{
if(!_78._lastFocusedRowBarIdx){
_78._lastFocusedRowBarIdx=0;
}
}
}
_8b=_8a.getCellNode(_78._lastFocusedRowBarIdx,0);
if(_8b){
_a.focus(_8b);
_8.toggleClass(_8b,f.focusClass,true);
}
f.rowIndex=_78._lastFocusedRowBarIdx;
_19(evt);
return true;
}
return false;
},onBlur:function(evt,_8c){
var _8d=g.views.views[0];
if(_8d instanceof _b){
var _8e=_8d.getCellNode(_78._lastFocusedRowBarIdx,0);
if(_8e){
_8.toggleClass(_8e,f.focusClass,false);
}
_19(evt);
}
return true;
},onMove:function(_8f,_90,evt){
var _91=g.views.views[0];
if(_8f&&_91 instanceof _b){
var _92=_78._lastFocusedRowBarIdx+_8f;
if(_92>=0&&_92<g.rowCount){
_19(evt);
var _93=_91.getCellNode(_78._lastFocusedRowBarIdx,0);
_8.toggleClass(_93,f.focusClass,false);
var sc=g.scroller;
var _94=sc.getLastPageRow(sc.page);
var rc=g.rowCount-1,row=Math.min(rc,_92);
if(_92>_94){
g.setScrollTop(g.scrollTop+sc.findScrollTop(row)-sc.findScrollTop(_78._lastFocusedRowBarIdx));
}
_93=_91.getCellNode(_92,0);
_a.focus(_93);
_8.toggleClass(_93,f.focusClass,true);
_78._lastFocusedRowBarIdx=_92;
f.cell=_93;
f.cell.view=_91;
f.cell.getNode=function(_95){
return f.cell;
};
f.rowIndex=_78._lastFocusedRowBarIdx;
f.scrollIntoView();
f.cell=null;
}
}
}});
f.placeArea("rowHeader","before","content");
}
f.addArea({name:"cellselect",onMove:_2.partial(_7b,"cell",function(_96,_97,_98,evt){
var _99=_78._currentPoint[_96];
return _1a("cell",_99.row+_97,_99.col+_98);
}),onKeyDown:_2.partial(_82,"cell",function(){
return _1a("cell",f.rowIndex,f.cell.index);
}),onKeyUp:_2.partial(_86,"cell")});
f.placeArea("cellselect","below","content");
f.addArea({name:"colselect",onMove:_2.partial(_7b,"col",function(_9a,_9b,_9c,evt){
var _9d=_78._currentPoint[_9a];
return _1a("col",_9d.col+_9c);
}),onKeyDown:_2.partial(_82,"col",function(){
return _1a("col",f.getHeaderIndex());
}),onKeyUp:_2.partial(_86,"col")});
f.placeArea("colselect","below","header");
f.addArea({name:"rowselect",onMove:_2.partial(_7b,"row",function(_9e,_9f,_a0,evt){
return _1a("row",f.rowIndex);
}),onKeyDown:_2.partial(_82,"row",function(){
return _1a("row",f.rowIndex);
}),onKeyUp:_2.partial(_86,"row")});
f.placeArea("rowselect","below","rowHeader");
},_clearSelection:function(_a1,_a2){
if(_a1=="all"){
this._clearSelection("cell",_a2);
this._clearSelection("col",_a2);
this._clearSelection("row",_a2);
return;
}
this._isUsingRowSelector=true;
_4.forEach(this._selected[_a1],function(_a3){
if(!_17(_a1,_a2,_a3)){
this._highlightSingle(_a1,false,_a3);
}
},this);
this._blurPoint(_a1,this._currentPoint);
this._selecting[_a1]=false;
this._startPoint[_a1]=this._currentPoint[_a1]=null;
this._selected[_a1]=[];
if(_a1=="row"&&!this.grid._selectingRange){
this._oldDeselectAll.call(this.grid.selection);
this.grid.selection._selectedById={};
}
this.grid.onEndDeselect(_a1,null,null,this._selected);
this.grid.onSelectCleared(_a1);
},_startSelect:function(_a4,_a5,_a6,_a7,_a8,_a9){
if(!this._isValid(_a4,_a5)){
return;
}
var _aa=this._isSelected(_a4,this._lastEndPoint[_a4]),_ab=this._isSelected(_a4,_a5);
if(this.noClear&&!_a6){
this._toSelect=_a9===undefined?true:_a9;
}else{
this._toSelect=_a8?_ab:!_ab;
}
if(!_a6||(!_ab&&this._config[_a4]==_f)){
this._clearSelection("col",_a5);
this._clearSelection("cell",_a5);
if(!this.noClear||(_a4==="row"&&this._config[_a4]==_f)){
this._clearSelection("row",_a5);
}
this._toSelect=_a9===undefined?true:_a9;
}
this._selecting[_a4]=true;
this._currentPoint[_a4]=null;
if(_a7&&this._lastType==_a4&&_aa==this._toSelect&&this._config[_a4]==_10){
if(_a4==="row"){
this._isUsingRowSelector=true;
}
this._startPoint[_a4]=this._lastAnchorPoint[_a4];
this._highlight(_a4,this._startPoint[_a4]);
this._isUsingRowSelector=false;
}else{
this._startPoint[_a4]=_a5;
}
this._curType=_a4;
this._fireEvent("start",_a4);
this._isStartFocus=true;
this._isUsingRowSelector=true;
this._highlight(_a4,_a5,this._toSelect);
this._isStartFocus=false;
},_endSelect:function(_ac){
if(_ac==="row"){
delete this._isUsingRowSelector;
}
if(_ac=="all"){
this._endSelect("col");
this._endSelect("row");
this._endSelect("cell");
}else{
if(this._selecting[_ac]){
this._addToSelected(_ac);
this._lastAnchorPoint[_ac]=this._startPoint[_ac];
this._lastEndPoint[_ac]=this._currentPoint[_ac];
if(this._toSelect){
this._lastSelectedAnchorPoint[_ac]=this._lastAnchorPoint[_ac];
this._lastSelectedEndPoint[_ac]=this._lastEndPoint[_ac];
}
this._startPoint[_ac]=this._currentPoint[_ac]=null;
this._selecting[_ac]=false;
this._lastType=_ac;
this._fireEvent("end",_ac);
}
}
},_fireEvent:function(_ad,_ae){
switch(_ad){
case "start":
this.grid[this._toSelect?"onStartSelect":"onStartDeselect"](_ae,this._startPoint[_ae],this._selected);
break;
case "end":
this.grid[this._toSelect?"onEndSelect":"onEndDeselect"](_ae,this._lastAnchorPoint[_ae],this._lastEndPoint[_ae],this._selected);
break;
}
},_calcToHighlight:function(_af,_b0,_b1,_b2){
if(_b2!==undefined){
var _b3;
if(this._usingKeyboard&&!_b1){
var _b4=this._isInLastRange(this._lastType,_b0);
if(_b4){
_b3=this._isSelected(_af,_b0);
if(_b2&&_b3){
return false;
}
if(!_b2&&!_b3&&this._isInLastRange(this._lastType,_b0,true)){
return true;
}
}
}
return _b1?_b2:(_b3||this._isSelected(_af,_b0));
}
return _b1;
},_highlightNode:function(_b5,_b6){
if(_b5){
var _b7="dojoxGridRowSelected";
var _b8="dojoxGridCellSelected";
_8.toggleClass(_b5,_b7,_b6);
_8.toggleClass(_b5,_b8,_b6);
}
},_highlightHeader:function(_b9,_ba){
var _bb=this.grid.layout.cells;
var _bc=_bb[_b9].getHeaderNode();
var _bd="dojoxGridHeaderSelected";
_8.toggleClass(_bc,_bd,_ba);
},_highlightRowSelector:function(_be,_bf){
var _c0=this.grid.views.views[0];
if(_c0 instanceof _b){
var _c1=_c0.getRowNode(_be);
if(_c1){
var _c2="dojoxGridRowSelectorSelected";
_8.toggleClass(_c1,_c2,_bf);
}
}
},_highlightSingle:function(_c3,_c4,_c5,_c6,_c7){
var _c8=this,_c9,g=_c8.grid,_ca=g.layout.cells;
switch(_c3){
case "cell":
_c9=this._calcToHighlight(_c3,_c5,_c4,_c6);
var c=_ca[_c5.col];
if(!c.hidden&&!c.notselectable){
this._highlightNode(_c5.node||c.getNode(_c5.row),_c9);
}
break;
case "col":
_c9=this._calcToHighlight(_c3,_c5,_c4,_c6);
this._highlightHeader(_c5.col,_c9);
_7("td[idx='"+_c5.col+"']",g.domNode).forEach(function(_cb){
var _cc=_ca[_c5.col].view.content.findRowTarget(_cb);
if(_cc){
var _cd=_cc[dojox.grid.util.rowIndexTag];
_c8._highlightSingle("cell",_c9,{"row":_cd,"col":_c5.col,"node":_cb});
}
});
break;
case "row":
_c9=this._calcToHighlight(_c3,_c5,_c4,_c6);
this._highlightRowSelector(_c5.row,_c9);
if(this._config.cell){
_4.forEach(_ca,function(_ce){
_c8._highlightSingle("cell",_c9,{"row":_c5.row,"col":_ce.index,"node":_ce.getNode(_c5.row)});
});
}
this._selectedRowModified=true;
if(!_c7){
g.selection.setSelected(_c5.row,_c9);
}
}
},_highlight:function(_cf,_d0,_d1){
if(this._selecting[_cf]&&_d0!==null){
var _d2=this._startPoint[_cf],_d3=this._currentPoint[_cf],_d4=this,_d5=function(_d6,to,_d7){
_d4._forEach(_cf,_d6,to,function(_d8){
_d4._highlightSingle(_cf,_d7,_d8,_d1);
},true);
};
switch(_cf){
case "col":
case "row":
if(_d3!==null){
if(_12(_cf,_d0,_d2,_d3,true)){
_d5(_d3,_d0,false);
}else{
if(_12(_cf,_d2,_d0,_d3,true)){
_d5(_d3,_d2,false);
_d3=_d2;
}
_d5(_d0,_d3,true);
}
}else{
this._highlightSingle(_cf,true,_d0,_d1);
}
break;
case "cell":
if(_d3!==null){
if(_12("row",_d0,_d2,_d3,true)||_12("col",_d0,_d2,_d3,true)||_12("row",_d2,_d0,_d3,true)||_12("col",_d2,_d0,_d3,true)){
_d5(_d2,_d3,false);
}
}
_d5(_d2,_d0,true);
}
this._currentPoint[_cf]=_d0;
this._focusPoint(_cf,this._currentPoint);
}
},_focusPoint:function(_d9,_da){
if(!this._isStartFocus){
var _db=_da[_d9],f=this.grid.focus;
if(_d9=="col"){
f._colHeadFocusIdx=_db.col;
f.focusArea("header");
}else{
if(_d9=="row"){
f.focusArea("rowHeader",{"rowIndex":_db.row});
}else{
if(_d9=="cell"){
f.setFocusIndex(_db.row,_db.col);
}
}
}
}
},_blurPoint:function(_dc,_dd){
var f=this.grid.focus;
if(_dc=="cell"){
f._blurContent();
}
},_addToSelected:function(_de){
var _df=this._toSelect,_e0=this,_e1=[],_e2=[],_e3=this._startPoint[_de],end=this._currentPoint[_de];
if(this._usingKeyboard){
this._forEach(_de,this._lastAnchorPoint[_de],this._lastEndPoint[_de],function(_e4){
if(!_12(_de,_e4,_e3,end)){
(_df?_e2:_e1).push(_e4);
}
});
}
this._forEach(_de,_e3,end,function(_e5){
var _e6=_e0._isSelected(_de,_e5);
if(_df&&!_e6){
_e1.push(_e5);
}else{
if(!_df){
_e2.push(_e5);
}
}
});
this._add(_de,_e1);
this._remove(_de,_e2);
_4.forEach(this._selected.row,function(_e7){
if(_e7.except.length>0){
this._selectedRowModified=true;
this.grid.selection.setSelected(_e7.row,false);
}
},this);
},_forEach:function(_e8,_e9,end,_ea,_eb){
if(!this._isValid(_e8,_e9,true)||!this._isValid(_e8,end,true)){
return;
}
switch(_e8){
case "col":
case "row":
_e9=_e9[_e8];
end=end[_e8];
var dir=end>_e9?1:-1;
if(!_eb){
end+=dir;
}
for(;_e9!=end;_e9+=dir){
_ea(_1a(_e8,_e9));
}
break;
case "cell":
var _ec=end.col>_e9.col?1:-1,_ed=end.row>_e9.row?1:-1;
for(var i=_e9.row,p=end.row+_ed;i!=p;i+=_ed){
for(var j=_e9.col,q=end.col+_ec;j!=q;j+=_ec){
_ea(_1a(_e8,i,j));
}
}
}
},_makeupForExceptions:function(_ee,_ef){
var _f0=[];
_4.forEach(this._selected[_ee],function(v1){
_4.forEach(_ef,function(v2){
if(v1[_ee]==v2[_ee]){
var pos=_4.indexOf(v1.except,v2[_11[_ee]]);
if(pos>=0){
v1.except.splice(pos,1);
}
_f0.push(v2);
}
});
});
return _f0;
},_makeupForCells:function(_f1,_f2){
var _f3=[];
_4.forEach(this._selected.cell,function(v1){
_4.some(_f2,function(v2){
if(v1[_f1]==v2[_f1]){
_f3.push(v1);
return true;
}
return false;
});
});
this._remove("cell",_f3);
_4.forEach(this._selected[_11[_f1]],function(v1){
_4.forEach(_f2,function(v2){
var pos=_4.indexOf(v1.except,v2[_f1]);
if(pos>=0){
v1.except.splice(pos,1);
}
});
});
},_addException:function(_f4,_f5){
_4.forEach(this._selected[_f4],function(v1){
_4.forEach(_f5,function(v2){
v1.except.push(v2[_11[_f4]]);
});
});
},_addCellException:function(_f6,_f7){
_4.forEach(this._selected[_f6],function(v1){
_4.forEach(_f7,function(v2){
if(v1[_f6]==v2[_f6]){
v1.except.push(v2[_11[_f6]]);
}
});
});
},_add:function(_f8,_f9){
var _fa=this.grid.layout.cells;
if(_f8=="cell"){
var _fb=this._makeupForExceptions("col",_f9);
var _fc=this._makeupForExceptions("row",_f9);
_f9=_4.filter(_f9,function(_fd){
return _4.indexOf(_fb,_fd)<0&&_4.indexOf(_fc,_fd)<0&&!_fa[_fd.col].hidden&&!_fa[_fd.col].notselectable;
});
}else{
if(_f8=="col"){
_f9=_4.filter(_f9,function(_fe){
return !_fa[_fe.col].hidden&&!_fa[_fe.col].notselectable;
});
}
this._makeupForCells(_f8,_f9);
this._selected[_f8]=_4.filter(this._selected[_f8],function(v){
return _4.every(_f9,function(_ff){
return v[_f8]!==_ff[_f8];
});
});
}
if(_f8!="col"&&this.grid._hasIdentity){
_4.forEach(_f9,function(item){
var _100=this.grid._by_idx[item.row];
if(_100){
item.id=_100.idty;
}
},this);
}
this._selected[_f8]=this._selected[_f8].concat(_f9);
},_remove:function(type,_101){
var comp=_2.partial(_17,type);
this._selected[type]=_4.filter(this._selected[type],function(v1){
return !_4.some(_101,function(v2){
return comp(v1,v2);
});
});
if(type=="cell"){
this._addCellException("col",_101);
this._addCellException("row",_101);
}else{
if(this._config.cell){
this._addException(_11[type],_101);
}
}
},_isCellNotInExcept:function(type,item){
var attr=item[type],_102=item[_11[type]];
return _4.some(this._selected[type],function(v){
return v[type]==attr&&_4.indexOf(v.except,_102)<0;
});
},_isSelected:function(type,item){
if(!item){
return false;
}
var res=_4.some(this._selected[type],function(v){
var ret=_17(type,item,v);
if(ret&&type!=="cell"){
return v.except.length===0;
}
return ret;
});
if(!res&&type==="cell"){
res=(this._isCellNotInExcept("col",item)||this._isCellNotInExcept("row",item));
if(type==="cell"){
res=res&&!this.grid.layout.cells[item.col].notselectable;
}
}
return res;
},_isInLastRange:function(type,item,_103){
var _104=this[_103?"_lastSelectedAnchorPoint":"_lastAnchorPoint"][type],end=this[_103?"_lastSelectedEndPoint":"_lastEndPoint"][type];
if(!item||!_104||!end){
return false;
}
return _12(type,item,_104,end);
},_isValid:function(type,item,_105){
if(!item){
return false;
}
try{
var g=this.grid,_106=item[type];
switch(type){
case "col":
return _106>=0&&_106<g.layout.cells.length&&_2.isArray(item.except)&&(_105||!g.layout.cells[_106].notselectable);
case "row":
return _106>=0&&_106<g.rowCount&&_2.isArray(item.except);
case "cell":
return item.col>=0&&item.col<g.layout.cells.length&&item.row>=0&&item.row<g.rowCount&&(_105||!g.layout.cells[item.col].notselectable);
}
}
catch(e){
}
return false;
}});
_d.registerPlugin(_1e,{"dependency":["autoScroll"]});
return _1e;
});
