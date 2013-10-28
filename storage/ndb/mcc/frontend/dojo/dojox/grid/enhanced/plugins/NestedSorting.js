//>>built
define("dojox/grid/enhanced/plugins/NestedSorting",["dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojo/_base/lang","dojo/_base/html","dojo/_base/event","dojo/_base/window","dojo/keys","dojo/query","dojo/string","../_Plugin","../../EnhancedGrid"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_1("dojox.grid.enhanced.plugins.NestedSorting",_b,{name:"nestedSorting",_currMainSort:"none",_currRegionIdx:-1,_a11yText:{"dojoxGridDescending":"&#9662;","dojoxGridAscending":"&#9652;","dojoxGridAscendingTip":"&#1784;","dojoxGridDescendingTip":"&#1783;","dojoxGridUnsortedTip":"x"},constructor:function(){
this._sortDef=[];
this._sortData={};
this._headerNodes={};
this._excludedColIdx=[];
this.nls=this.grid._nls;
this.grid.setSortInfo=function(){
};
this.grid.setSortIndex=_4.hitch(this,"_setGridSortIndex");
this.grid.getSortIndex=function(){
};
this.grid.getSortProps=_4.hitch(this,"getSortProps");
if(this.grid.sortFields){
this._setGridSortIndex(this.grid.sortFields,null,true);
}
this.connect(this.grid.views,"render","_initSort");
this.initCookieHandler();
this.subscribe("dojox/grid/rearrange/move/"+this.grid.id,_4.hitch(this,"_onColumnDnD"));
},onStartUp:function(){
this.inherited(arguments);
this.connect(this.grid,"onHeaderCellClick","_onHeaderCellClick");
this.connect(this.grid,"onHeaderCellMouseOver","_onHeaderCellMouseOver");
this.connect(this.grid,"onHeaderCellMouseOut","_onHeaderCellMouseOut");
},_onColumnDnD:function(_e,_f){
if(_e!=="col"){
return;
}
var m=_f,obj={},d=this._sortData,p;
var cr=this._getCurrentRegion();
this._blurRegion(cr);
var idx=this._getRegionHeader(cr).getAttribute("idx");
for(p in m){
if(d[p]){
obj[m[p]]=d[p];
delete d[p];
}
if(p===idx){
idx=m[p];
}
}
for(p in obj){
d[p]=obj[p];
}
var c=this._headerNodes[idx];
this._currRegionIdx=_2.indexOf(this._getRegions(),c.firstChild);
this._initSort(false);
},_setGridSortIndex:function(_10,_11,_12){
if(_4.isArray(_10)){
var i,d,_13;
for(i=0;i<_10.length;i++){
d=_10[i];
_13=this.grid.getCellByField(d.attribute);
if(!_13){
console.warn("Invalid sorting option, column ",d.attribute," not found.");
return;
}
if(_13["nosort"]||!this.grid.canSort(_13.index,_13.field)){
console.warn("Invalid sorting option, column ",d.attribute," is unsortable.");
return;
}
}
this.clearSort();
_2.forEach(_10,function(d,i){
_13=this.grid.getCellByField(d.attribute);
this.setSortData(_13.index,"index",i);
this.setSortData(_13.index,"order",d.descending?"desc":"asc");
},this);
}else{
if(!isNaN(_10)){
if(_11===undefined){
return;
}
this.setSortData(_10,"order",_11?"asc":"desc");
}else{
return;
}
}
this._updateSortDef();
if(!_12){
this.grid.sort();
}
},getSortProps:function(){
return this._sortDef.length?this._sortDef:null;
},_initSort:function(_14){
var g=this.grid,n=g.domNode,len=this._sortDef.length;
_5.toggleClass(n,"dojoxGridSorted",!!len);
_5.toggleClass(n,"dojoxGridSingleSorted",len===1);
_5.toggleClass(n,"dojoxGridNestSorted",len>1);
if(len>0){
this._currMainSort=this._sortDef[0].descending?"desc":"asc";
}
var idx,_15=this._excludedCoIdx=[];
this._headerNodes=_9("th",g.viewsHeaderNode).forEach(function(n){
idx=parseInt(n.getAttribute("idx"),10);
if(_5.style(n,"display")==="none"||g.layout.cells[idx]["nosort"]||(g.canSort&&!g.canSort(idx,g.layout.cells[idx]["field"]))){
_15.push(idx);
}
});
this._headerNodes.forEach(this._initHeaderNode,this);
this._initFocus();
if(_14){
this._focusHeader();
}
},_initHeaderNode:function(_16){
_5.toggleClass(_16,"dojoxGridSortNoWrap",true);
var _17=_9(".dojoxGridSortNode",_16)[0];
if(_17){
_5.toggleClass(_17,"dojoxGridSortNoWrap",true);
}
if(_2.indexOf(this._excludedCoIdx,_16.getAttribute("idx"))>=0){
_5.addClass(_16,"dojoxGridNoSort");
return;
}
if(!_9(".dojoxGridSortBtn",_16).length){
this._connects=_2.filter(this._connects,function(_18){
if(_18._sort){
_3.disconnect(_18);
return false;
}
return true;
});
var n=_5.create("a",{className:"dojoxGridSortBtn dojoxGridSortBtnNested",title:_a.substitute(this.nls.sortingState,[this.nls.nestedSort,this.nls.ascending]),innerHTML:"1"},_16.firstChild,"last");
n.onmousedown=_6.stop;
n=_5.create("a",{className:"dojoxGridSortBtn dojoxGridSortBtnSingle",title:_a.substitute(this.nls.sortingState,[this.nls.singleSort,this.nls.ascending])},_16.firstChild,"last");
n.onmousedown=_6.stop;
}else{
var a1=_9(".dojoxGridSortBtnSingle",_16)[0];
var a2=_9(".dojoxGridSortBtnNested",_16)[0];
a1.className="dojoxGridSortBtn dojoxGridSortBtnSingle";
a2.className="dojoxGridSortBtn dojoxGridSortBtnNested";
a2.innerHTML="1";
_5.removeClass(_16,"dojoxGridCellShowIndex");
_5.removeClass(_16.firstChild,"dojoxGridSortNodeSorted");
_5.removeClass(_16.firstChild,"dojoxGridSortNodeAsc");
_5.removeClass(_16.firstChild,"dojoxGridSortNodeDesc");
_5.removeClass(_16.firstChild,"dojoxGridSortNodeMain");
_5.removeClass(_16.firstChild,"dojoxGridSortNodeSub");
}
this._updateHeaderNodeUI(_16);
},_onHeaderCellClick:function(e){
this._focusRegion(e.target);
if(_5.hasClass(e.target,"dojoxGridSortBtn")){
this._onSortBtnClick(e);
_6.stop(e);
this._focusRegion(this._getCurrentRegion());
}
},_onHeaderCellMouseOver:function(e){
if(!e.cell){
return;
}
if(this._sortDef.length>1){
return;
}
if(this._sortData[e.cellIndex]&&this._sortData[e.cellIndex].index===0){
return;
}
var p;
for(p in this._sortData){
if(this._sortData[p]&&this._sortData[p].index===0){
_5.addClass(this._headerNodes[p],"dojoxGridCellShowIndex");
break;
}
}
if(!_5.hasClass(_7.body(),"dijit_a11y")){
return;
}
var i=e.cell.index,_19=e.cellNode;
var _1a=_9(".dojoxGridSortBtnSingle",_19)[0];
var _1b=_9(".dojoxGridSortBtnNested",_19)[0];
var _1c="none";
if(_5.hasClass(this.grid.domNode,"dojoxGridSingleSorted")){
_1c="single";
}else{
if(_5.hasClass(this.grid.domNode,"dojoxGridNestSorted")){
_1c="nested";
}
}
var _1d=_1b.getAttribute("orderIndex");
if(_1d===null||_1d===undefined){
_1b.setAttribute("orderIndex",_1b.innerHTML);
_1d=_1b.innerHTML;
}
if(this.isAsc(i)){
_1b.innerHTML=_1d+this._a11yText.dojoxGridDescending;
}else{
if(this.isDesc(i)){
_1b.innerHTML=_1d+this._a11yText.dojoxGridUnsortedTip;
}else{
_1b.innerHTML=_1d+this._a11yText.dojoxGridAscending;
}
}
if(this._currMainSort==="none"){
_1a.innerHTML=this._a11yText.dojoxGridAscending;
}else{
if(this._currMainSort==="asc"){
_1a.innerHTML=this._a11yText.dojoxGridDescending;
}else{
if(this._currMainSort==="desc"){
_1a.innerHTML=this._a11yText.dojoxGridUnsortedTip;
}
}
}
},_onHeaderCellMouseOut:function(e){
var p;
for(p in this._sortData){
if(this._sortData[p]&&this._sortData[p].index===0){
_5.removeClass(this._headerNodes[p],"dojoxGridCellShowIndex");
break;
}
}
},_onSortBtnClick:function(e){
var _1e=e.cell.index;
if(_5.hasClass(e.target,"dojoxGridSortBtnSingle")){
this._prepareSingleSort(_1e);
}else{
if(_5.hasClass(e.target,"dojoxGridSortBtnNested")){
this._prepareNestedSort(_1e);
}else{
return;
}
}
_6.stop(e);
this._doSort(_1e);
},_doSort:function(_1f){
if(!this._sortData[_1f]||!this._sortData[_1f].order){
this.setSortData(_1f,"order","asc");
}else{
if(this.isAsc(_1f)){
this.setSortData(_1f,"order","desc");
}else{
if(this.isDesc(_1f)){
this.removeSortData(_1f);
}
}
}
this._updateSortDef();
this.grid.sort();
this._initSort(true);
},setSortData:function(_20,_21,_22){
var sd=this._sortData[_20];
if(!sd){
sd=this._sortData[_20]={};
}
sd[_21]=_22;
},removeSortData:function(_23){
var d=this._sortData,i=d[_23].index,p;
delete d[_23];
for(p in d){
if(d[p].index>i){
d[p].index--;
}
}
},_prepareSingleSort:function(_24){
var d=this._sortData,p;
for(p in d){
delete d[p];
}
this.setSortData(_24,"index",0);
this.setSortData(_24,"order",this._currMainSort==="none"?null:this._currMainSort);
if(!this._sortData[_24]||!this._sortData[_24].order){
this._currMainSort="asc";
}else{
if(this.isAsc(_24)){
this._currMainSort="desc";
}else{
if(this.isDesc(_24)){
this._currMainSort="none";
}
}
}
},_prepareNestedSort:function(_25){
var i=this._sortData[_25]?this._sortData[_25].index:null;
if(i===0||!!i){
return;
}
this.setSortData(_25,"index",this._sortDef.length);
},_updateSortDef:function(){
this._sortDef.length=0;
var d=this._sortData,p;
for(p in d){
this._sortDef[d[p].index]={attribute:this.grid.layout.cells[p].field,descending:d[p].order==="desc"};
}
},_updateHeaderNodeUI:function(_26){
var _27=this._getCellByNode(_26);
var _28=_27.index;
var _29=this._sortData[_28];
var _2a=_9(".dojoxGridSortNode",_26)[0];
var _2b=_9(".dojoxGridSortBtnSingle",_26)[0];
var _2c=_9(".dojoxGridSortBtnNested",_26)[0];
_5.toggleClass(_2b,"dojoxGridSortBtnAsc",this._currMainSort==="asc");
_5.toggleClass(_2b,"dojoxGridSortBtnDesc",this._currMainSort==="desc");
if(this._currMainSort==="asc"){
_2b.title=_a.substitute(this.nls.sortingState,[this.nls.singleSort,this.nls.descending]);
}else{
if(this._currMainSort==="desc"){
_2b.title=_a.substitute(this.nls.sortingState,[this.nls.singleSort,this.nls.unsorted]);
}else{
_2b.title=_a.substitute(this.nls.sortingState,[this.nls.singleSort,this.nls.ascending]);
}
}
var _2d=this;
function _2e(){
var _2f="Column "+(_27.index+1)+" "+_27.field;
var _30="none";
var _31="ascending";
if(_29){
_30=_29.order==="asc"?"ascending":"descending";
_31=_29.order==="asc"?"descending":"none";
}
var _32=_2f+" - is sorted by "+_30;
var _33=_2f+" - is nested sorted by "+_30;
var _34=_2f+" - choose to sort by "+_31;
var _35=_2f+" - choose to nested sort by "+_31;
_2b.setAttribute("aria-label",_32);
_2c.setAttribute("aria-label",_33);
var _36=[_2d.connect(_2b,"onmouseover",function(){
_2b.setAttribute("aria-label",_34);
}),_2d.connect(_2b,"onmouseout",function(){
_2b.setAttribute("aria-label",_32);
}),_2d.connect(_2c,"onmouseover",function(){
_2c.setAttribute("aria-label",_35);
}),_2d.connect(_2c,"onmouseout",function(){
_2c.setAttribute("aria-label",_33);
})];
_2.forEach(_36,function(_37){
_37._sort=true;
});
};
_2e();
var _38=_5.hasClass(_7.body(),"dijit_a11y");
if(!_29){
_2c.innerHTML=this._sortDef.length+1;
_2c.title=_a.substitute(this.nls.sortingState,[this.nls.nestedSort,this.nls.ascending]);
if(_38){
_2a.innerHTML=this._a11yText.dojoxGridUnsortedTip;
}
return;
}
if(_29.index||(_29.index===0&&this._sortDef.length>1)){
_2c.innerHTML=_29.index+1;
}
_5.addClass(_2a,"dojoxGridSortNodeSorted");
if(this.isAsc(_28)){
_5.addClass(_2a,"dojoxGridSortNodeAsc");
_2c.title=_a.substitute(this.nls.sortingState,[this.nls.nestedSort,this.nls.descending]);
if(_38){
_2a.innerHTML=this._a11yText.dojoxGridAscendingTip;
}
}else{
if(this.isDesc(_28)){
_5.addClass(_2a,"dojoxGridSortNodeDesc");
_2c.title=_a.substitute(this.nls.sortingState,[this.nls.nestedSort,this.nls.unsorted]);
if(_38){
_2a.innerHTML=this._a11yText.dojoxGridDescendingTip;
}
}
}
_5.addClass(_2a,(_29.index===0?"dojoxGridSortNodeMain":"dojoxGridSortNodeSub"));
},isAsc:function(_39){
return this._sortData[_39].order==="asc";
},isDesc:function(_3a){
return this._sortData[_3a].order==="desc";
},_getCellByNode:function(_3b){
var i;
for(i=0;i<this._headerNodes.length;i++){
if(this._headerNodes[i]===_3b){
return this.grid.layout.cells[i];
}
}
return null;
},clearSort:function(){
this._sortData={};
this._sortDef.length=0;
},initCookieHandler:function(){
if(this.grid.addCookieHandler){
this.grid.addCookieHandler({name:"sortOrder",onLoad:_4.hitch(this,"_loadNestedSortingProps"),onSave:_4.hitch(this,"_saveNestedSortingProps")});
}
},_loadNestedSortingProps:function(_3c,_3d){
this._setGridSortIndex(_3c);
},_saveNestedSortingProps:function(_3e){
return this.getSortProps();
},_initFocus:function(){
var f=this.focus=this.grid.focus;
this._focusRegions=this._getRegions();
if(!this._headerArea){
var _3f=this._headerArea=f.getArea("header");
_3f.onFocus=f.focusHeader=_4.hitch(this,"_focusHeader");
_3f.onBlur=f.blurHeader=f._blurHeader=_4.hitch(this,"_blurHeader");
_3f.onMove=_4.hitch(this,"_onMove");
_3f.onKeyDown=_4.hitch(this,"_onKeyDown");
_3f._regions=[];
_3f.getRegions=null;
this.connect(this.grid,"onBlur","_blurHeader");
}
},_focusHeader:function(e){
if(this._currRegionIdx===-1){
this._onMove(0,1,null);
}else{
this._focusRegion(this._getCurrentRegion());
}
try{
_6.stop(e);
}
catch(e){
}
return true;
},_blurHeader:function(e){
this._blurRegion(this._getCurrentRegion());
return true;
},_onMove:function(_40,_41,e){
var _42=this._currRegionIdx||0,_43=this._focusRegions;
var _44=_43[_42+_41];
if(!_44){
return;
}else{
if(_5.style(_44,"display")==="none"||_5.style(_44,"visibility")==="hidden"){
this._onMove(_40,_41+(_41>0?1:-1),e);
return;
}
}
this._focusRegion(_44);
var _45=this._getRegionView(_44);
_45.scrollboxNode.scrollLeft=_45.headerNode.scrollLeft;
},_onKeyDown:function(e,_46){
if(_46){
switch(e.keyCode){
case _8.ENTER:
case _8.SPACE:
if(_5.hasClass(e.target,"dojoxGridSortBtnSingle")||_5.hasClass(e.target,"dojoxGridSortBtnNested")){
this._onSortBtnClick(e);
}
}
}
},_getRegionView:function(_47){
var _48=_47;
while(_48&&!_5.hasClass(_48,"dojoxGridHeader")){
_48=_48.parentNode;
}
if(_48){
return _2.filter(this.grid.views.views,function(_49){
return _49.headerNode===_48;
})[0]||null;
}
return null;
},_getRegions:function(){
var _4a=[],_4b=this.grid.layout.cells;
this._headerNodes.forEach(function(n,i){
if(_5.style(n,"display")==="none"){
return;
}
if(_4b[i]["isRowSelector"]){
_4a.push(n);
return;
}
_9(".dojoxGridSortNode,.dojoxGridSortBtnNested,.dojoxGridSortBtnSingle",n).forEach(function(_4c){
_4c.setAttribute("tabindex",0);
_4a.push(_4c);
});
},this);
return _4a;
},_focusRegion:function(_4d){
if(!_4d){
return;
}
var _4e=this._getCurrentRegion();
if(_4e&&_4d!==_4e){
this._blurRegion(_4e);
}
var _4f=this._getRegionHeader(_4d);
_5.addClass(_4f,"dojoxGridCellSortFocus");
if(_5.hasClass(_4d,"dojoxGridSortNode")){
_5.addClass(_4d,"dojoxGridSortNodeFocus");
}else{
if(_5.hasClass(_4d,"dojoxGridSortBtn")){
_5.addClass(_4d,"dojoxGridSortBtnFocus");
}
}
_4d.focus();
this.focus.currentArea("header");
this._currRegionIdx=_2.indexOf(this._focusRegions,_4d);
},_blurRegion:function(_50){
if(!_50){
return;
}
var _51=this._getRegionHeader(_50);
_5.removeClass(_51,"dojoxGridCellSortFocus");
if(_5.hasClass(_50,"dojoxGridSortNode")){
_5.removeClass(_50,"dojoxGridSortNodeFocus");
}else{
if(_5.hasClass(_50,"dojoxGridSortBtn")){
_5.removeClass(_50,"dojoxGridSortBtnFocus");
}
}
_50.blur();
},_getCurrentRegion:function(){
return this._focusRegions?this._focusRegions[this._currRegionIdx]:null;
},_getRegionHeader:function(_52){
while(_52&&!_5.hasClass(_52,"dojoxGridCell")){
_52=_52.parentNode;
}
return _52;
},destroy:function(){
this._sortDef=this._sortData=null;
this._headerNodes=this._focusRegions=null;
this.inherited(arguments);
}});
_c.registerPlugin(_d);
return _d;
});
