//>>built
define("dojox/grid/TreeGrid",["dojo/_base/kernel","../main","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/event","dojo/dom-attr","dojo/dom-class","dojo/query","dojo/keys","dijit/tree/ForestStoreModel","./DataGrid","./_Layout","./_FocusManager","./_RowManager","./_EditManager","./TreeSelection","./cells/tree","./_TreeView"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12){
_1.experimental("dojox.grid.TreeGrid");
var _13=_3("dojox.grid._TreeAggregator",null,{cells:[],grid:null,childFields:[],constructor:function(_14){
this.cells=_14.cells||[];
this.childFields=_14.childFields||[];
this.grid=_14.grid;
this.store=this.grid.store;
},_cacheValue:function(_15,id,_16){
_15[id]=_16;
return _16;
},clearSubtotalCache:function(){
if(this.store){
delete this.store._cachedAggregates;
}
},cnt:function(_17,_18,_19){
var _1a=0;
var _1b=this.store;
var _1c=this.childFields;
if(_1c[_18]){
var _1d=_1b.getValues(_19,_1c[_18]);
if(_17.index<=_18+1){
_1a=_1d.length;
}else{
_4.forEach(_1d,function(c){
_1a+=this.getForCell(_17,_18+1,c,"cnt");
},this);
}
}else{
_1a=1;
}
return _1a;
},sum:function(_1e,_1f,_20){
var _21=0;
var _22=this.store;
var _23=this.childFields;
if(_23[_1f]){
_4.forEach(_22.getValues(_20,_23[_1f]),function(c){
_21+=this.getForCell(_1e,_1f+1,c,"sum");
},this);
}else{
_21+=_22.getValue(_20,_1e.field);
}
return _21;
},value:function(_24,_25,_26){
},getForCell:function(_27,_28,_29,_2a){
var _2b=this.store;
if(!_2b||!_29||!_2b.isItem(_29)){
return "";
}
var _2c=_2b._cachedAggregates=_2b._cachedAggregates||{};
var id=_2b.getIdentity(_29);
var _2d=_2c[id]=_2c[id]||[];
if(!_27.getOpenState){
_27=this.grid.getCell(_27.layoutIndex+_28+1);
}
var idx=_27.index;
var _2e=_2d[idx]=_2d[idx]||{};
_2a=(_2a||(_27.parentCell?_27.parentCell.aggregate:"sum"))||"sum";
var _2f=_27.field;
if(_2f==_2b.getLabelAttributes()[0]){
_2a="cnt";
}
var _30=_2e[_2a]=_2e[_2a]||[];
if(_30[_28]!=undefined){
return _30[_28];
}
var _31=((_27.parentCell&&_27.parentCell.itemAggregates)?_27.parentCell.itemAggregates[_27.idxInParent]:"")||"";
if(_31&&_2b.hasAttribute(_29,_31)){
return this._cacheValue(_30,_28,_2b.getValue(_29,_31));
}else{
if(_31){
return this._cacheValue(_30,_28,0);
}
}
return this._cacheValue(_30,_28,this[_2a](_27,_28,_29));
}});
var _32=_3("dojox.grid._TreeLayout",_d,{_isCollapsable:false,_getInternalStructure:function(_33){
var g=this.grid;
var s=_33;
var _34=s[0].cells[0];
var _35={type:"dojox.grid._TreeView",cells:[[]]};
var _36=[];
var _37=0;
var _38=function(_39,_3a){
var _3b=_39.children;
var _3c=function(_3d,idx){
var k,n={};
for(k in _3d){
n[k]=_3d[k];
}
n=_5.mixin(n,{level:_3a,idxInParent:_3a>0?idx:-1,parentCell:_3a>0?_39:null});
return n;
};
var ret=[];
_4.forEach(_3b,function(c,idx){
if("children" in c){
_36.push(c.field);
var _3e=ret[ret.length-1];
_3e.isCollapsable=true;
c.level=_3a;
ret=ret.concat(_38(c,_3a+1));
}else{
ret.push(_3c(c,idx));
}
});
_37=Math.max(_37,_3a);
return ret;
};
var _3f={children:_34,itemAggregates:[]};
_35.cells[0]=_38(_3f,0);
g.aggregator=new _13({cells:_35.cells[0],grid:g,childFields:_36});
if(g.scroller&&g.defaultOpen){
g.scroller.defaultRowHeight=g.scroller._origDefaultRowHeight*(2*_37+1);
}
return [_35];
},setStructure:function(_40){
var s=_40;
var g=this.grid;
if(g&&g.treeModel&&!_4.every(s,function(i){
return ("cells" in i);
})){
s=arguments[0]=[{cells:[s]}];
}
if(s.length==1&&s[0].cells.length==1){
if(g&&g.treeModel){
s[0].type="dojox.grid._TreeView";
this._isCollapsable=true;
s[0].cells[0][(this.grid.treeModel?this.grid.expandoCell:0)].isCollapsable=true;
}else{
var _41=_4.filter(s[0].cells[0],function(c){
return ("children" in c);
});
if(_41.length===1){
this._isCollapsable=true;
}
}
}
if(this._isCollapsable&&(!g||!g.treeModel)){
arguments[0]=this._getInternalStructure(s);
}
this.inherited(arguments);
},addCellDef:function(_42,_43,_44){
var obj=this.inherited(arguments);
return _5.mixin(obj,_12);
}});
var _45=_3("dojox.grid.TreePath",null,{level:0,_str:"",_arr:null,grid:null,store:null,cell:null,item:null,constructor:function(_46,_47){
if(_5.isString(_46)){
this._str=_46;
this._arr=_4.map(_46.split("/"),function(_48){
return parseInt(_48,10);
});
}else{
if(_5.isArray(_46)){
this._str=_46.join("/");
this._arr=_46.slice(0);
}else{
if(typeof _46=="number"){
this._str=String(_46);
this._arr=[_46];
}else{
this._str=_46._str;
this._arr=_46._arr.slice(0);
}
}
}
this.level=this._arr.length-1;
this.grid=_47;
this.store=this.grid.store;
if(_47.treeModel){
this.cell=_47.layout.cells[_47.expandoCell];
}else{
this.cell=_47.layout.cells[this.level];
}
},item:function(){
if(!this._item){
this._item=this.grid.getItem(this._arr);
}
return this._item;
},compare:function(_49){
if(_5.isString(_49)||_5.isArray(_49)){
if(this._str==_49){
return 0;
}
if(_49.join&&this._str==_49.join("/")){
return 0;
}
_49=new _45(_49,this.grid);
}else{
if(_49 instanceof _45){
if(this._str==_49._str){
return 0;
}
}
}
for(var i=0,l=(this._arr.length<_49._arr.length?this._arr.length:_49._arr.length);i<l;i++){
if(this._arr[i]<_49._arr[i]){
return -1;
}
if(this._arr[i]>_49._arr[i]){
return 1;
}
}
if(this._arr.length<_49._arr.length){
return -1;
}
if(this._arr.length>_49._arr.length){
return 1;
}
return 0;
},isOpen:function(){
return this.cell.openStates&&this.cell.getOpenState(this.item());
},previous:function(){
var _4a=this._arr.slice(0);
if(this._str=="0"){
return null;
}
var _4b=_4a.length-1;
if(_4a[_4b]===0){
_4a.pop();
return new _45(_4a,this.grid);
}
_4a[_4b]--;
var _4c=new _45(_4a,this.grid);
return _4c.lastChild(true);
},next:function(){
var _4d=this._arr.slice(0);
if(this.isOpen()){
_4d.push(0);
}else{
_4d[_4d.length-1]++;
for(var i=this.level;i>=0;i--){
var _4e=this.grid.getItem(_4d.slice(0,i+1));
if(i>0){
if(!_4e){
_4d.pop();
_4d[i-1]++;
}
}else{
if(!_4e){
return null;
}
}
}
}
return new _45(_4d,this.grid);
},children:function(_4f){
if(!this.isOpen()&&!_4f){
return null;
}
var _50=[];
var _51=this.grid.treeModel;
if(_51){
var _52=this.item();
var _53=_51.store;
if(!_51.mayHaveChildren(_52)){
return null;
}
_4.forEach(_51.childrenAttrs,function(_54){
_50=_50.concat(_53.getValues(_52,_54));
});
}else{
_50=this.store.getValues(this.item(),this.grid.layout.cells[this.cell.level+1].parentCell.field);
if(_50.length>1&&this.grid.sortChildItems){
var _55=this.grid.getSortProps();
if(_55&&_55.length){
var _56=_55[0].attribute,_57=this.grid;
if(_56&&_50[0][_56]){
var _58=!!_55[0].descending;
_50=_50.slice(0);
_50.sort(function(a,b){
return _57._childItemSorter(a,b,_56,_58);
});
}
}
}
}
return _50;
},childPaths:function(){
var _59=this.children();
if(!_59){
return [];
}
return _4.map(_59,function(_5a,_5b){
return new _45(this._str+"/"+_5b,this.grid);
},this);
},parent:function(){
if(this.level===0){
return null;
}
return new _45(this._arr.slice(0,this.level),this.grid);
},lastChild:function(_5c){
var _5d=this.children();
if(!_5d||!_5d.length){
return this;
}
var _5e=new _45(this._str+"/"+String(_5d.length-1),this.grid);
if(!_5c){
return _5e;
}
return _5e.lastChild(true);
},toString:function(){
return this._str;
}});
var _5f=_3("dojox.grid._TreeFocusManager",_e,{setFocusCell:function(_60,_61){
if(_60&&_60.getNode(_61)){
this.inherited(arguments);
}
},isLastFocusCell:function(){
if(this.cell&&this.cell.index==this.grid.layout.cellCount-1){
var _62=new _45(this.grid.rowCount-1,this.grid);
_62=_62.lastChild(true);
return this.rowIndex==_62._str;
}
return false;
},next:function(){
if(this.cell){
var row=this.rowIndex,col=this.cell.index+1,cc=this.grid.layout.cellCount-1;
var _63=new _45(this.rowIndex,this.grid);
if(col>cc){
var _64=_63.next();
if(!_64){
col--;
}else{
col=0;
_63=_64;
}
}
if(this.grid.edit.isEditing()){
var _65=this.grid.getCell(col);
if(!this.isLastFocusCell()&&!_65.editable){
this._focusifyCellNode(false);
this.cell=_65;
this.rowIndex=_63._str;
this.next();
return;
}
}
this.setFocusIndex(_63._str,col);
}
},previous:function(){
if(this.cell){
var row=(this.rowIndex||0),col=(this.cell.index||0)-1;
var _66=new _45(row,this.grid);
if(col<0){
var _67=_66.previous();
if(!_67){
col=0;
}else{
col=this.grid.layout.cellCount-1;
_66=_67;
}
}
if(this.grid.edit.isEditing()){
var _68=this.grid.getCell(col);
if(!this.isFirstFocusCell()&&!_68.editable){
this._focusifyCellNode(false);
this.cell=_68;
this.rowIndex=_66._str;
this.previous();
return;
}
}
this.setFocusIndex(_66._str,col);
}
},move:function(_69,_6a){
if(this.isNavHeader()){
this.inherited(arguments);
return;
}
if(!this.cell){
return;
}
var sc=this.grid.scroller,r=this.rowIndex,rc=this.grid.rowCount-1,_6b=new _45(this.rowIndex,this.grid);
if(_69){
var row;
if(_69>0){
_6b=_6b.next();
row=_6b._arr[0];
if(row>sc.getLastPageRow(sc.page)){
this.grid.setScrollTop(this.grid.scrollTop+sc.findScrollTop(row)-sc.findScrollTop(r));
}
}else{
if(_69<0){
_6b=_6b.previous();
row=_6b._arr[0];
if(row<=sc.getPageRow(sc.page)){
this.grid.setScrollTop(this.grid.scrollTop-sc.findScrollTop(r)-sc.findScrollTop(row));
}
}
}
}
var cc=this.grid.layout.cellCount-1,i=this.cell.index,col=Math.min(cc,Math.max(0,i+_6a));
var _6c=this.grid.getCell(col);
var _6d=_6a<0?-1:1;
while(col>=0&&col<cc&&_6c&&_6c.hidden===true){
col+=_6d;
_6c=this.grid.getCell(col);
}
if(!_6c||_6c.hidden===true){
col=i;
}
if(_69){
this.grid.updateRow(r);
}
this.setFocusIndex(_6b._str,col);
}});
var _6e=_3("dojox.grid.TreeGrid",_c,{defaultOpen:true,sortChildItems:false,openAtLevels:[],treeModel:null,expandoCell:0,aggregator:null,_layoutClass:_32,createSelection:function(){
this.selection=new _11(this);
},_childItemSorter:function(a,b,_6f,_70){
var av=this.store.getValue(a,_6f);
var bv=this.store.getValue(b,_6f);
if(av!=bv){
return av<bv==_70?1:-1;
}
return 0;
},_onNew:function(_71,_72){
if(!_72||!_72.item){
this.inherited(arguments);
}else{
var idx=this.getItemIndex(_72.item);
if(typeof idx=="string"){
this.updateRow(idx.split("/")[0]);
}else{
if(idx>-1){
this.updateRow(idx);
}
}
}
},_onSet:function(_73,_74,_75,_76){
this._checkUpdateStatus();
if(this.aggregator){
this.aggregator.clearSubtotalCache();
}
var idx=this.getItemIndex(_73);
if(typeof idx=="string"){
this.updateRow(idx.split("/")[0]);
}else{
if(idx>-1){
this.updateRow(idx);
}
}
},_onDelete:function(_77){
this._cleanupExpandoCache(this._getItemIndex(_77,true),this.store.getIdentity(_77),_77);
this.inherited(arguments);
},_cleanupExpandoCache:function(_78,_79,_7a){
},_addItem:function(_7b,_7c,_7d,_7e){
if(!_7e&&this.model&&_4.indexOf(this.model.root.children,_7b)==-1){
this.model.root.children[_7c]=_7b;
}
this.inherited(arguments);
},getItem:function(idx){
var _7f=_5.isArray(idx);
if(_5.isString(idx)&&idx.indexOf("/")){
idx=idx.split("/");
_7f=true;
}
if(_7f&&idx.length==1){
idx=idx[0];
_7f=false;
}
if(!_7f){
return _c.prototype.getItem.call(this,idx);
}
var s=this.store;
var itm=_c.prototype.getItem.call(this,idx[0]);
var cf,i,j;
if(this.aggregator){
cf=this.aggregator.childFields||[];
if(cf){
for(i=0;i<idx.length-1&&itm;i++){
if(cf[i]){
itm=(s.getValues(itm,cf[i])||[])[idx[i+1]];
}else{
itm=null;
}
}
}
}else{
if(this.treeModel){
cf=this.treeModel.childrenAttrs||[];
if(cf&&itm){
for(i=1,il=idx.length;(i<il)&&itm;i++){
for(j=0,jl=cf.length;j<jl;j++){
if(cf[j]){
itm=(s.getValues(itm,cf[j])||[])[idx[i]];
}else{
itm=null;
}
if(itm){
break;
}
}
}
}
}
}
return itm||null;
},_getItemIndex:function(_80,_81){
if(!_81&&!this.store.isItem(_80)){
return -1;
}
var idx=this.inherited(arguments);
if(idx==-1){
var _82=this.store.getIdentity(_80);
return this._by_idty_paths[_82]||-1;
}
return idx;
},postMixInProperties:function(){
if(this.treeModel&&!("defaultOpen" in this.params)){
this.defaultOpen=false;
}
var def=this.defaultOpen;
this.openAtLevels=_4.map(this.openAtLevels,function(l){
if(typeof l=="string"){
switch(l.toLowerCase()){
case "true":
return true;
break;
case "false":
return false;
break;
default:
var r=parseInt(l,10);
if(isNaN(r)){
return def;
}
return r;
break;
}
}
return l;
});
this._by_idty_paths={};
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
if(this.treeModel){
this._setModel(this.treeModel);
}
},setModel:function(_83){
this._setModel(_83);
this._refresh(true);
},_setModel:function(_84){
if(_84&&(!_b||!(_84 instanceof _b))){
throw new Error("dojox.grid.TreeGrid: treeModel must be an instance of dijit.tree.ForestStoreModel");
}
this.treeModel=_84;
_8.toggle(this.domNode,"dojoxGridTreeModel",this.treeModel?true:false);
this._setQuery(_84?_84.query:null);
this._setStore(_84?_84.store:null);
},createScroller:function(){
this.inherited(arguments);
this.scroller._origDefaultRowHeight=this.scroller.defaultRowHeight;
},createManagers:function(){
this.rows=new _f(this);
this.focus=new _5f(this);
this.edit=new _10(this);
},_setStore:function(_85){
this.inherited(arguments);
if(this.treeModel&&!this.treeModel.root.children){
this.treeModel.root.children=[];
}
if(this.aggregator){
this.aggregator.store=_85;
}
},getDefaultOpenState:function(_86,_87){
var cf;
var _88=this.store;
if(this.treeModel){
return this.defaultOpen;
}
if(!_86||!_88||!_88.isItem(_87)||!(cf=this.aggregator.childFields[_86.level])){
return this.defaultOpen;
}
if(this.openAtLevels.length>_86.level){
var _89=this.openAtLevels[_86.level];
if(typeof _89=="boolean"){
return _89;
}else{
if(typeof _89=="number"){
return (_88.getValues(_87,cf).length<=_89);
}
}
}
return this.defaultOpen;
},onStyleRow:function(row){
if(!this.layout._isCollapsable){
this.inherited(arguments);
return;
}
var _8a=_7.get(row.node,"dojoxTreeGridBaseClasses");
if(_8a){
row.customClasses=_8a;
}
var i=row;
var _8b=i.node.tagName.toLowerCase();
i.customClasses+=(i.odd?" dojoxGridRowOdd":"")+(i.selected&&_8b=="tr"?" dojoxGridRowSelected":"")+(i.over&&_8b=="tr"?" dojoxGridRowOver":"");
this.focus.styleRow(i);
this.edit.styleRow(i);
},styleRowNode:function(_8c,_8d){
if(_8d){
if(_8d.tagName.toLowerCase()=="div"&&this.aggregator){
_9("tr[dojoxTreeGridPath]",_8d).forEach(function(_8e){
this.rows.styleRowNode(_7.get(_8e,"dojoxTreeGridPath"),_8e);
},this);
}
this.rows.styleRowNode(_8c,_8d);
}
},onCanSelect:function(_8f){
var _90=_9("tr[dojoxTreeGridPath='"+_8f+"']",this.domNode);
if(_90.length){
if(_8.contains(_90[0],"dojoxGridSummaryRow")){
return false;
}
}
return this.inherited(arguments);
},onKeyDown:function(e){
if(e.altKey||e.metaKey){
return;
}
switch(e.keyCode){
case _a.UP_ARROW:
if(!this.edit.isEditing()&&this.focus.rowIndex!="0"){
_6.stop(e);
this.focus.move(-1,0);
}
break;
case _a.DOWN_ARROW:
var _91=new _45(this.focus.rowIndex,this);
var _92=new _45(this.rowCount-1,this);
_92=_92.lastChild(true);
if(!this.edit.isEditing()&&_91.toString()!=_92.toString()){
_6.stop(e);
this.focus.move(1,0);
}
break;
default:
this.inherited(arguments);
break;
}
},canEdit:function(_93,_94){
var _95=_93.getNode(_94);
return _95&&this._canEdit;
},doApplyCellEdit:function(_96,_97,_98){
var _99=this.getItem(_97);
var _9a=this.store.getValue(_99,_98);
if(typeof _9a=="number"){
_96=isNaN(_96)?_96:parseFloat(_96);
}else{
if(typeof _9a=="boolean"){
_96=_96=="true"?true:_96=="false"?false:_96;
}else{
if(_9a instanceof Date){
var _9b=new Date(_96);
_96=isNaN(_9b.getTime())?_96:_9b;
}
}
}
this.store.setValue(_99,_98,_96);
this.onApplyCellEdit(_96,_97,_98);
}});
_6e.markupFactory=function(_9c,_9d,_9e,_9f){
var _a0=function(n){
var w=_7.get(n,"width")||"auto";
if((w!="auto")&&(w.slice(-2)!="em")&&(w.slice(-1)!="%")){
w=parseInt(w,10)+"px";
}
return w;
};
var _a1=function(_a2){
var _a3;
if(_a2.nodeName.toLowerCase()=="table"&&_9("> colgroup",_a2).length===0&&(_a3=_9("> thead > tr",_a2)).length==1){
var tr=_a3[0];
return _9("> th",_a3[0]).map(function(th){
var _a4={type:_5.trim(_7.get(th,"cellType")||""),field:_5.trim(_7.get(th,"field")||"")};
if(_a4.type){
_a4.type=_5.getObject(_a4.type);
}
var _a5=_9("> table",th)[0];
if(_a5){
_a4.name="";
_a4.children=_a1(_a5);
if(_7.has(th,"itemAggregates")){
_a4.itemAggregates=_4.map(_7.get(th,"itemAggregates").split(","),function(v){
return _5.trim(v);
});
}else{
_a4.itemAggregates=[];
}
if(_7.has(th,"aggregate")){
_a4.aggregate=_7.get(th,"aggregate");
}
_a4.type=_a4.type||_2.grid.cells.SubtableCell;
}else{
_a4.name=_5.trim(_7.get(th,"name")||th.innerHTML);
if(_7.has(th,"width")){
_a4.width=_a0(th);
}
if(_7.has(th,"relWidth")){
_a4.relWidth=window.parseInt(_7.get(th,"relWidth"),10);
}
if(_7.has(th,"hidden")){
_a4.hidden=_7.get(th,"hidden")=="true";
}
_a4.field=_a4.field||_a4.name;
_c.cell_markupFactory(_9f,th,_a4);
_a4.type=_a4.type||_2.grid.cells.Cell;
}
if(_a4.type&&_a4.type.markupFactory){
_a4.type.markupFactory(th,_a4);
}
return _a4;
});
}
return [];
};
var _a6;
if(!_9c.structure){
var row=_a1(_9d);
if(row.length){
_9c.structure=[{__span:Infinity,cells:[row]}];
}
}
return _c.markupFactory(_9c,_9d,_9e,_9f);
};
return _6e;
});
