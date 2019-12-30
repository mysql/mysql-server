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
if(this.grid.plugin("rearrange")){
this.subscribe("dojox/grid/rearrange/move/"+this.grid.id,_4.hitch(this,"_onColumnDnD"));
}else{
this.connect(this.grid.layout,"moveColumn","_onMoveColumn");
}
},onStartUp:function(){
this.inherited(arguments);
this.connect(this.grid,"onHeaderCellClick","_onHeaderCellClick");
this.connect(this.grid,"onHeaderCellMouseOver","_onHeaderCellMouseOver");
this.connect(this.grid,"onHeaderCellMouseOut","_onHeaderCellMouseOut");
},_onMoveColumn:function(_e,_f,_10,_11,_12){
var cr=this._getCurrentRegion(),idx=cr&&this._getRegionHeader(cr).getAttribute("idx"),c=this._headerNodes[idx],_13=this._sortData,_14={},_15,_16;
if(cr){
this._blurRegion(cr);
this._currRegionIdx=_2.indexOf(this._getRegions(),c.firstChild);
}
if(_11<_10){
for(_15 in _13){
_15=parseInt(_15,10);
_16=_13[_15];
if(_16){
if(_15>=_11&&_15<_10){
_14[_15+1]=_16;
}else{
if(_15==_10){
_14[_11]=_16;
}else{
_14[_15]=_16;
}
}
}
}
}else{
if(_11>_10+1){
if(!_12){
_11++;
}
for(_15 in _13){
_15=parseInt(_15,10);
_16=_13[_15];
if(_16){
if(_15>_10&&_15<_11){
_14[_15-1]=_16;
}else{
if(_15==_10){
_14[_11-1]=_16;
}else{
_14[_15]=_16;
}
}
}
}
}
}
this._sortData=_14;
this._initSort(false);
},_onColumnDnD:function(_17,_18){
if(_17!=="col"){
return;
}
var m=_18,obj={},d=this._sortData,p;
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
},_setGridSortIndex:function(_19,_1a,_1b){
if(_4.isArray(_19)){
var i,d,_1c;
for(i=0;i<_19.length;i++){
d=_19[i];
_1c=this.grid.getCellByField(d.attribute);
if(!_1c){
console.warn("Invalid sorting option, column ",d.attribute," not found.");
return;
}
if(_1c["nosort"]||!this.grid.canSort(_1c.index,_1c.field)){
console.warn("Invalid sorting option, column ",d.attribute," is unsortable.");
return;
}
}
this.clearSort();
_2.forEach(_19,function(d,i){
_1c=this.grid.getCellByField(d.attribute);
this.setSortData(_1c.index,"index",i);
this.setSortData(_1c.index,"order",d.descending?"desc":"asc");
},this);
}else{
if(!isNaN(_19)){
if(_1a===undefined){
return;
}
this.setSortData(_19,"order",_1a?"asc":"desc");
}else{
return;
}
}
this._updateSortDef();
if(!_1b){
this.grid.sort();
}
},getSortProps:function(){
return this._sortDef.length?this._sortDef:null;
},_initSort:function(_1d){
var g=this.grid,n=g.domNode,len=this._sortDef.length;
_5.toggleClass(n,"dojoxGridSorted",!!len);
_5.toggleClass(n,"dojoxGridSingleSorted",len===1);
_5.toggleClass(n,"dojoxGridNestSorted",len>1);
if(len>0){
this._currMainSort=this._sortDef[0].descending?"desc":"asc";
}
var idx,_1e=this._excludedCoIdx=[];
this._headerNodes=_9("th",g.viewsHeaderNode).forEach(function(n){
idx=parseInt(n.getAttribute("idx"),10);
if(_5.style(n,"display")==="none"||g.layout.cells[idx]["nosort"]||(g.canSort&&!g.canSort(idx,g.layout.cells[idx]["field"]))){
_1e.push(idx);
}
});
this._headerNodes.forEach(this._initHeaderNode,this);
this._initFocus();
if(_1d){
this._focusHeader();
}
},_initHeaderNode:function(_1f){
_5.toggleClass(_1f,"dojoxGridSortNoWrap",true);
var _20=_9(".dojoxGridSortNode",_1f)[0];
if(_20){
_5.toggleClass(_20,"dojoxGridSortNoWrap",true);
}
if(_2.indexOf(this._excludedCoIdx,_1f.getAttribute("idx"))>=0){
_5.addClass(_1f,"dojoxGridNoSort");
return;
}
if(!_9(".dojoxGridSortBtn",_1f).length){
this._connects=_2.filter(this._connects,function(_21){
if(_21._sort){
_3.disconnect(_21);
return false;
}
return true;
});
var n=_5.create("a",{className:"dojoxGridSortBtn dojoxGridSortBtnNested",title:_a.substitute(this.nls.sortingState,[this.nls.nestedSort,this.nls.ascending]),innerHTML:"1"},_1f.firstChild,"last");
n.onmousedown=_6.stop;
n=_5.create("a",{className:"dojoxGridSortBtn dojoxGridSortBtnSingle",title:_a.substitute(this.nls.sortingState,[this.nls.singleSort,this.nls.ascending])},_1f.firstChild,"last");
n.onmousedown=_6.stop;
}else{
var a1=_9(".dojoxGridSortBtnSingle",_1f)[0];
var a2=_9(".dojoxGridSortBtnNested",_1f)[0];
a1.className="dojoxGridSortBtn dojoxGridSortBtnSingle";
a2.className="dojoxGridSortBtn dojoxGridSortBtnNested";
a2.innerHTML="1";
_5.removeClass(_1f,"dojoxGridCellShowIndex");
_5.removeClass(_1f.firstChild,"dojoxGridSortNodeSorted");
_5.removeClass(_1f.firstChild,"dojoxGridSortNodeAsc");
_5.removeClass(_1f.firstChild,"dojoxGridSortNodeDesc");
_5.removeClass(_1f.firstChild,"dojoxGridSortNodeMain");
_5.removeClass(_1f.firstChild,"dojoxGridSortNodeSub");
}
this._updateHeaderNodeUI(_1f);
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
var i=e.cell.index,_22=e.cellNode;
var _23=_9(".dojoxGridSortBtnSingle",_22)[0];
var _24=_9(".dojoxGridSortBtnNested",_22)[0];
var _25="none";
if(_5.hasClass(this.grid.domNode,"dojoxGridSingleSorted")){
_25="single";
}else{
if(_5.hasClass(this.grid.domNode,"dojoxGridNestSorted")){
_25="nested";
}
}
var _26=_24.getAttribute("orderIndex");
if(_26===null||_26===undefined){
_24.setAttribute("orderIndex",_24.innerHTML);
_26=_24.innerHTML;
}
if(this.isAsc(i)){
_24.innerHTML=_26+this._a11yText.dojoxGridDescending;
}else{
if(this.isDesc(i)){
_24.innerHTML=_26+this._a11yText.dojoxGridUnsortedTip;
}else{
_24.innerHTML=_26+this._a11yText.dojoxGridAscending;
}
}
if(this._currMainSort==="none"){
_23.innerHTML=this._a11yText.dojoxGridAscending;
}else{
if(this._currMainSort==="asc"){
_23.innerHTML=this._a11yText.dojoxGridDescending;
}else{
if(this._currMainSort==="desc"){
_23.innerHTML=this._a11yText.dojoxGridUnsortedTip;
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
var _27=e.cell.index;
if(_5.hasClass(e.target,"dojoxGridSortBtnSingle")){
this._prepareSingleSort(_27);
}else{
if(_5.hasClass(e.target,"dojoxGridSortBtnNested")){
this._prepareNestedSort(_27);
}else{
return;
}
}
_6.stop(e);
this._doSort(_27);
},_doSort:function(_28){
if(!this._sortData[_28]||!this._sortData[_28].order){
this.setSortData(_28,"order","asc");
}else{
if(this.isAsc(_28)){
this.setSortData(_28,"order","desc");
}else{
if(this.isDesc(_28)){
this.removeSortData(_28);
}
}
}
this._updateSortDef();
this.grid.sort();
this._initSort(true);
},setSortData:function(_29,_2a,_2b){
var sd=this._sortData[_29];
if(!sd){
sd=this._sortData[_29]={};
}
sd[_2a]=_2b;
},removeSortData:function(_2c){
var d=this._sortData,i=d[_2c].index,p;
delete d[_2c];
for(p in d){
if(d[p].index>i){
d[p].index--;
}
}
},_prepareSingleSort:function(_2d){
var d=this._sortData,p;
for(p in d){
delete d[p];
}
this.setSortData(_2d,"index",0);
this.setSortData(_2d,"order",this._currMainSort==="none"?null:this._currMainSort);
if(!this._sortData[_2d]||!this._sortData[_2d].order){
this._currMainSort="asc";
}else{
if(this.isAsc(_2d)){
this._currMainSort="desc";
}else{
if(this.isDesc(_2d)){
this._currMainSort="none";
}
}
}
},_prepareNestedSort:function(_2e){
var i=this._sortData[_2e]?this._sortData[_2e].index:null;
if(i===0||!!i){
return;
}
this.setSortData(_2e,"index",this._sortDef.length);
},_updateSortDef:function(){
this._sortDef.length=0;
var d=this._sortData,p;
for(p in d){
this._sortDef[d[p].index]={attribute:this.grid.layout.cells[p].field,descending:d[p].order==="desc"};
}
},_updateHeaderNodeUI:function(_2f){
var _30=this._getCellByNode(_2f);
var _31=_30.index;
var _32=this._sortData[_31];
var _33=_9(".dojoxGridSortNode",_2f)[0];
var _34=_9(".dojoxGridSortBtnSingle",_2f)[0];
var _35=_9(".dojoxGridSortBtnNested",_2f)[0];
_5.toggleClass(_34,"dojoxGridSortBtnAsc",this._currMainSort==="asc");
_5.toggleClass(_34,"dojoxGridSortBtnDesc",this._currMainSort==="desc");
if(this._currMainSort==="asc"){
_34.title=_a.substitute(this.nls.sortingState,[this.nls.singleSort,this.nls.descending]);
}else{
if(this._currMainSort==="desc"){
_34.title=_a.substitute(this.nls.sortingState,[this.nls.singleSort,this.nls.unsorted]);
}else{
_34.title=_a.substitute(this.nls.sortingState,[this.nls.singleSort,this.nls.ascending]);
}
}
var _36=this;
function _37(){
var _38="Column "+(_30.index+1)+" "+_30.field;
var _39="none";
var _3a="ascending";
if(_32){
_39=_32.order==="asc"?"ascending":"descending";
_3a=_32.order==="asc"?"descending":"none";
}
var _3b=_38+" - is sorted by "+_39;
var _3c=_38+" - is nested sorted by "+_39;
var _3d=_38+" - choose to sort by "+_3a;
var _3e=_38+" - choose to nested sort by "+_3a;
_34.setAttribute("aria-label",_3b);
_35.setAttribute("aria-label",_3c);
var _3f=[_36.connect(_34,"onmouseover",function(){
_34.setAttribute("aria-label",_3d);
}),_36.connect(_34,"onmouseout",function(){
_34.setAttribute("aria-label",_3b);
}),_36.connect(_35,"onmouseover",function(){
_35.setAttribute("aria-label",_3e);
}),_36.connect(_35,"onmouseout",function(){
_35.setAttribute("aria-label",_3c);
})];
_2.forEach(_3f,function(_40){
_40._sort=true;
});
};
_37();
var _41=_5.hasClass(_7.body(),"dijit_a11y");
if(!_32){
_35.innerHTML=this._sortDef.length+1;
_35.title=_a.substitute(this.nls.sortingState,[this.nls.nestedSort,this.nls.ascending]);
if(_41){
_33.innerHTML=this._a11yText.dojoxGridUnsortedTip;
}
return;
}
if(_32.index||(_32.index===0&&this._sortDef.length>1)){
_35.innerHTML=_32.index+1;
}
_5.addClass(_33,"dojoxGridSortNodeSorted");
if(this.isAsc(_31)){
_5.addClass(_33,"dojoxGridSortNodeAsc");
_35.title=_a.substitute(this.nls.sortingState,[this.nls.nestedSort,this.nls.descending]);
if(_41){
_33.innerHTML=this._a11yText.dojoxGridAscendingTip;
}
}else{
if(this.isDesc(_31)){
_5.addClass(_33,"dojoxGridSortNodeDesc");
_35.title=_a.substitute(this.nls.sortingState,[this.nls.nestedSort,this.nls.unsorted]);
if(_41){
_33.innerHTML=this._a11yText.dojoxGridDescendingTip;
}
}
}
_5.addClass(_33,(_32.index===0?"dojoxGridSortNodeMain":"dojoxGridSortNodeSub"));
},isAsc:function(_42){
return this._sortData[_42].order==="asc";
},isDesc:function(_43){
return this._sortData[_43].order==="desc";
},_getCellByNode:function(_44){
var i;
for(i=0;i<this._headerNodes.length;i++){
if(this._headerNodes[i]===_44){
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
},_loadNestedSortingProps:function(_45,_46){
this._setGridSortIndex(_45);
},_saveNestedSortingProps:function(_47){
return this.getSortProps();
},_initFocus:function(){
var f=this.focus=this.grid.focus;
this._focusRegions=this._getRegions();
if(!this._headerArea){
var _48=this._headerArea=f.getArea("header");
_48.onFocus=f.focusHeader=_4.hitch(this,"_focusHeader");
_48.onBlur=f.blurHeader=f._blurHeader=_4.hitch(this,"_blurHeader");
_48.onMove=_4.hitch(this,"_onMove");
_48.onKeyDown=_4.hitch(this,"_onKeyDown");
_48._regions=[];
_48.getRegions=null;
this.connect(this.grid,"onBlur","_blurHeader");
}
},_focusHeader:function(e){
if(this._currRegionIdx===-1){
this._onMove(0,1,null);
}else{
this._focusRegion(this._getCurrentRegion());
}
try{
if(e){
_6.stop(e);
}
}
catch(e){
}
return true;
},_blurHeader:function(e){
this._blurRegion(this._getCurrentRegion());
return true;
},_onMove:function(_49,_4a,e){
var _4b=this._currRegionIdx||0,_4c=this._focusRegions;
var _4d=_4c[_4b+_4a];
if(!_4d){
return;
}else{
if(_5.style(_4d,"display")==="none"||_5.style(_4d,"visibility")==="hidden"){
this._onMove(_49,_4a+(_4a>0?1:-1),e);
return;
}
}
this._focusRegion(_4d);
var _4e=this._getRegionView(_4d);
_4e.scrollboxNode.scrollLeft=_4e.headerNode.scrollLeft;
},_onKeyDown:function(e,_4f){
if(_4f){
switch(e.keyCode){
case _8.ENTER:
case _8.SPACE:
if(_5.hasClass(e.target,"dojoxGridSortBtnSingle")||_5.hasClass(e.target,"dojoxGridSortBtnNested")){
this._onSortBtnClick(e);
}
}
}
},_getRegionView:function(_50){
var _51=_50;
while(_51&&!_5.hasClass(_51,"dojoxGridHeader")){
_51=_51.parentNode;
}
if(_51){
return _2.filter(this.grid.views.views,function(_52){
return _52.headerNode===_51;
})[0]||null;
}
return null;
},_getRegions:function(){
var _53=[],_54=this.grid.layout.cells;
this._headerNodes.forEach(function(n,i){
if(_5.style(n,"display")==="none"){
return;
}
if(_54[i]["isRowSelector"]){
_53.push(n);
return;
}
_9(".dojoxGridSortNode,.dojoxGridSortBtnNested,.dojoxGridSortBtnSingle",n).forEach(function(_55){
_55.setAttribute("tabindex",0);
_53.push(_55);
});
},this);
return _53;
},_focusRegion:function(_56){
if(!_56){
return;
}
var _57=this._getCurrentRegion();
if(_57&&_56!==_57){
this._blurRegion(_57);
}
var _58=this._getRegionHeader(_56);
_5.addClass(_58,"dojoxGridCellSortFocus");
if(_5.hasClass(_56,"dojoxGridSortNode")){
_5.addClass(_56,"dojoxGridSortNodeFocus");
}else{
if(_5.hasClass(_56,"dojoxGridSortBtn")){
_5.addClass(_56,"dojoxGridSortBtnFocus");
}
}
try{
_56.focus();
}
catch(e){
}
this.focus.currentArea("header");
this._currRegionIdx=_2.indexOf(this._focusRegions,_56);
},_blurRegion:function(_59){
if(!_59){
return;
}
var _5a=this._getRegionHeader(_59);
_5.removeClass(_5a,"dojoxGridCellSortFocus");
if(_5.hasClass(_59,"dojoxGridSortNode")){
_5.removeClass(_59,"dojoxGridSortNodeFocus");
}else{
if(_5.hasClass(_59,"dojoxGridSortBtn")){
_5.removeClass(_59,"dojoxGridSortBtnFocus");
}
}
_59.blur();
},_getCurrentRegion:function(){
return this._focusRegions?this._focusRegions[this._currRegionIdx]:null;
},_getRegionHeader:function(_5b){
while(_5b&&!_5.hasClass(_5b,"dojoxGridCell")){
_5b=_5b.parentNode;
}
return _5b;
},destroy:function(){
this._sortDef=this._sortData=null;
this._headerNodes=this._focusRegions=null;
this.inherited(arguments);
}});
_c.registerPlugin(_d);
return _d;
});
