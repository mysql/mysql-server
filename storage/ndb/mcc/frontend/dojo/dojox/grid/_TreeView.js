//>>built
require({cache:{"url:dojox/grid/resources/Expando.html":"<div class=\"dojoxGridExpando\"\n\t><div class=\"dojoxGridExpandoNode\" dojoAttachEvent=\"onclick:onToggle\"\n\t\t><div class=\"dojoxGridExpandoNodeInner\" dojoAttachPoint=\"expandoInner\"></div\n\t></div\n></div>\n"}});
define("dojox/grid/_TreeView",["dijit/registry","../main","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/event","dojo/dom-attr","dojo/dom-class","dojo/dom-style","dojo/dom-construct","dojo/query","dojo/parser","dojo/text!./resources/Expando.html","dijit/_Widget","dijit/_TemplatedMixin","./_View","./_Builder","./util"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12){
_3("dojox.grid._Expando",[_e,_f],{open:false,toggleClass:"",itemId:"",cellIdx:-1,view:null,rowNode:null,rowIdx:-1,expandoCell:null,level:0,templateString:_d,_toggleRows:function(_13,_14){
if(!_13||!this.rowNode){
return;
}
if(_b("table.dojoxGridRowTableNeedsRowUpdate").length){
if(this._initialized){
this.view.grid.updateRow(this.rowIdx);
}
return;
}
var _15=this;
var g=this.view.grid;
if(g.treeModel){
var p=this._tableRow?_7.get(this._tableRow,"dojoxTreeGridPath"):"";
if(p){
_b("tr[dojoxTreeGridPath^=\""+p+"/\"]",this.rowNode).forEach(function(n){
var en=_b(".dojoxGridExpando",n)[0];
if(en&&en.parentNode&&en.parentNode.parentNode&&!_8.contains(en.parentNode.parentNode,"dojoxGridNoChildren")){
var ew=_1.byNode(en);
if(ew){
ew._toggleRows(_13,ew.open&&_14);
}
}
n.style.display=_14?"":"none";
});
}
}else{
_b("tr."+_13,this.rowNode).forEach(function(n){
if(_8.contains(n,"dojoxGridExpandoRow")){
var en=_b(".dojoxGridExpando",n)[0];
if(en){
var ew=_1.byNode(en);
var _16=ew?ew.toggleClass:en.getAttribute("toggleClass");
var _17=ew?ew.open:_15.expandoCell.getOpenState(en.getAttribute("itemId"));
_15._toggleRows(_16,_17&&_14);
}
}
n.style.display=_14?"":"none";
});
}
},setOpen:function(_18){
if(_18&&_8.contains(this.domNode,"dojoxGridExpandoLoading")){
_18=false;
}
var _19=this.view;
var _1a=_19.grid;
var _1b=_1a.store;
var _1c=_1a.treeModel;
var d=this;
var idx=this.rowIdx;
var me=_1a._by_idx[idx];
if(!me){
return;
}
if(_1c&&!this._loadedChildren){
if(_18){
var itm=_1a.getItem(_7.get(this._tableRow,"dojoxTreeGridPath"));
if(itm){
this.expandoInner.innerHTML="o";
_8.add(this.domNode,"dojoxGridExpandoLoading");
_1c.getChildren(itm,function(_1d){
d._loadedChildren=true;
d._setOpen(_18);
});
}else{
this._setOpen(_18);
}
}else{
this._setOpen(_18);
}
}else{
if(!_1c&&_1b){
if(_18){
var _1e=_1a._by_idx[this.rowIdx];
if(_1e&&!_1b.isItemLoaded(_1e.item)){
this.expandoInner.innerHTML="o";
_8.add(this.domNode,"dojoxGridExpandoLoading");
_1b.loadItem({item:_1e.item,onItem:_5.hitch(this,function(i){
var _1f=_1b.getIdentity(i);
_1a._by_idty[_1f]=_1a._by_idx[this.rowIdx]={idty:_1f,item:i};
this._setOpen(_18);
})});
}else{
this._setOpen(_18);
}
}else{
this._setOpen(_18);
}
}else{
this._setOpen(_18);
}
}
},_setOpen:function(_20){
if(_20&&this._tableRow&&_8.contains(this._tableRow,"dojoxGridNoChildren")){
this._setOpen(false);
return;
}
this.expandoInner.innerHTML=_20?"-":"+";
_8.remove(this.domNode,"dojoxGridExpandoLoading");
_8.toggle(this.domNode,"dojoxGridExpandoOpened",_20);
if(this._tableRow){
_8.toggle(this._tableRow,"dojoxGridRowCollapsed",!_20);
var _21=_7.get(this._tableRow,"dojoxTreeGridBaseClasses");
var _22="";
if(_20){
_22=_5.trim((" "+_21+" ").replace(" dojoxGridRowCollapsed "," "));
}else{
if((" "+_21+" ").indexOf(" dojoxGridRowCollapsed ")<0){
_22=_21+(_21?" ":"")+"dojoxGridRowCollapsed";
}else{
_22=_21;
}
}
_7.set(this._tableRow,"dojoxTreeGridBaseClasses",_22);
}
var _23=(this.open!==_20);
this.open=_20;
if(this.expandoCell&&this.itemId){
this.expandoCell.openStates[this.itemId]=_20;
}
var v=this.view;
var g=v.grid;
if(this.toggleClass&&_23){
if(!this._tableRow||!this._tableRow.style.display){
this._toggleRows(this.toggleClass,_20);
}
}
if(v&&this._initialized&&this.rowIdx>=0){
g.rowHeightChanged(this.rowIdx);
g.postresize();
v.hasVScrollbar(true);
}
this._initialized=true;
},onToggle:function(e){
this.setOpen(!this.open);
_6.stop(e);
},setRowNode:function(_24,_25,_26){
if(this.cellIdx<0||!this.itemId){
return false;
}
this._initialized=false;
this.view=_26;
this.rowNode=_25;
this.rowIdx=_24;
this.expandoCell=_26.structure.cells[0][this.cellIdx];
var d=this.domNode;
if(d&&d.parentNode&&d.parentNode.parentNode){
this._tableRow=d.parentNode.parentNode;
}
this.open=this.expandoCell.getOpenState(this.itemId);
if(_26.grid.treeModel){
_9.set(this.domNode,"marginLeft",(this.level*18)+"px");
if(this.domNode.parentNode){
_9.set(this.domNode.parentNode,"backgroundPosition",((this.level*18)+(3))+"px");
}
}
this.setOpen(this.open);
return true;
}});
var _27=_3("dojox.grid._TreeContentBuilder",_11._ContentBuilder,{generateHtml:function(_28,_29){
var _2a=this.getTableArray(),v=this.view,row=v.structure.cells[0],_2b=this.grid.getItem(_29),_2c=this.grid,_2d=this.grid.store;
_12.fire(this.view,"onBeforeRow",[_29,[row]]);
var _2e=function(_2f,_30,_31,_32,_33,_34){
if(!_34){
if(_2a[0].indexOf("dojoxGridRowTableNeedsRowUpdate")==-1){
_2a[0]=_2a[0].replace("dojoxGridRowTable","dojoxGridRowTable dojoxGridRowTableNeedsRowUpdate");
}
return;
}
var _35=_2a.length;
_32=_32||[];
var _36=_32.join("|");
var _37=_32[_32.length-1];
var _38=_37+(_31?" dojoxGridSummaryRow":"");
var _39="";
if(_2c.treeModel&&_30&&!_2c.treeModel.mayHaveChildren(_30)){
_38+=" dojoxGridNoChildren";
}
_2a.push("<tr style=\""+_39+"\" class=\""+_38+"\" dojoxTreeGridPath=\""+_33.join("/")+"\" dojoxTreeGridBaseClasses=\""+_38+"\">");
var _3a=_2f+1;
var _3b=null;
for(var i=0,_3c;(_3c=row[i]);i++){
var m=_3c.markup,cc=_3c.customClasses=[],cs=_3c.customStyles=[];
m[5]=_3c.formatAtLevel(_33,_30,_2f,_31,_37,cc);
m[1]=cc.join(" ");
m[3]=cs.join(";");
_2a.push.apply(_2a,m);
if(!_3b&&_3c.level===_3a&&_3c.parentCell){
_3b=_3c.parentCell;
}
}
_2a.push("</tr>");
if(_30&&_2d&&_2d.isItem(_30)){
var _3d=_2d.getIdentity(_30);
if(typeof _2c._by_idty_paths[_3d]=="undefined"){
_2c._by_idty_paths[_3d]=_33.join("/");
}
}
var _3e;
var _3f;
var _40;
var _41;
var _42=_33.concat([]);
if(_2c.treeModel&&_30){
if(_2c.treeModel.mayHaveChildren(_30)){
_3e=v.structure.cells[0][_2c.expandoCell||0];
_3f=_3e.getOpenState(_30)&&_34;
_40=new _2.grid.TreePath(_33.join("/"),_2c);
_41=_40.children(true)||[];
_4.forEach(_41,function(_43,idx){
var _44=_36.split("|");
_44.push(_44[_44.length-1]+"-"+idx);
_42.push(idx);
_2e(_3a,_43,false,_44,_42,_3f);
_42.pop();
});
}
}else{
if(_30&&_3b&&!_31){
_3e=v.structure.cells[0][_3b.level];
_3f=_3e.getOpenState(_30)&&_34;
if(_2d.hasAttribute(_30,_3b.field)){
var _45=_36.split("|");
_45.pop();
_40=new _2.grid.TreePath(_33.join("/"),_2c);
_41=_40.children(true)||[];
if(_41.length){
_2a[_35]="<tr class=\""+_45.join(" ")+" dojoxGridExpandoRow\" dojoxTreeGridPath=\""+_33.join("/")+"\">";
_4.forEach(_41,function(_46,idx){
var _47=_36.split("|");
_47.push(_47[_47.length-1]+"-"+idx);
_42.push(idx);
_2e(_3a,_46,false,_47,_42,_3f);
_42.pop();
});
_42.push(_41.length);
_2e(_2f,_30,true,_32,_42,_3f);
}else{
_2a[_35]="<tr class=\""+_37+" dojoxGridNoChildren\" dojoxTreeGridPath=\""+_33.join("/")+"\">";
}
}else{
if(!_2d.isItemLoaded(_30)){
_2a[0]=_2a[0].replace("dojoxGridRowTable","dojoxGridRowTable dojoxGridRowTableNeedsRowUpdate");
}else{
_2a[_35]="<tr class=\""+_37+" dojoxGridNoChildren\" dojoxTreeGridPath=\""+_33.join("/")+"\">";
}
}
}else{
if(_30&&!_31&&_32.length>1){
_2a[_35]="<tr class=\""+_32[_32.length-2]+"\" dojoxTreeGridPath=\""+_33.join("/")+"\">";
}
}
}
};
_2e(0,_2b,false,["dojoxGridRowToggle-"+_29],[_29],true);
_2a.push("</table>");
return _2a.join("");
},findTarget:function(_48,_49){
var n=_48;
while(n&&(n!=this.domNode)){
if(n.tagName&&n.tagName.toLowerCase()=="tr"){
break;
}
n=n.parentNode;
}
return (n!=this.domNode)?n:null;
},getCellNode:function(_4a,_4b){
var _4c=_b("td[idx='"+_4b+"']",_4a)[0];
if(_4c&&_4c.parentNode&&!_8.contains(_4c.parentNode,"dojoxGridSummaryRow")){
return _4c;
}
},decorateEvent:function(e){
e.rowNode=this.findRowTarget(e.target);
if(!e.rowNode){
return false;
}
e.rowIndex=_7.get(e.rowNode,"dojoxTreeGridPath");
this.baseDecorateEvent(e);
e.cell=this.grid.getCell(e.cellIndex);
return true;
}});
return _3("dojox.grid._TreeView",_10,{_contentBuilderClass:_27,_onDndDrop:function(_4d,_4e,_4f){
if(this.grid&&this.grid.aggregator){
this.grid.aggregator.clearSubtotalCache();
}
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
this.connect(this.grid,"_cleanupExpandoCache","_cleanupExpandoCache");
},_cleanupExpandoCache:function(_50,_51,_52){
if(_50==-1){
return;
}
_4.forEach(this.grid.layout.cells,function(_53){
if(typeof _53["openStates"]!="undefined"){
if(_51 in _53.openStates){
delete _53.openStates[_51];
}
}
});
if(typeof _50=="string"&&_50.indexOf("/")>-1){
var _54=new _2.grid.TreePath(_50,this.grid);
var _55=_54.parent();
while(_55){
_54=_55;
_55=_54.parent();
}
var _56=_54.item();
if(!_56){
return;
}
var _57=this.grid.store.getIdentity(_56);
if(typeof this._expandos[_57]!="undefined"){
for(var i in this._expandos[_57]){
var exp=this._expandos[_57][i];
if(exp){
exp.destroy();
}
delete this._expandos[_57][i];
}
delete this._expandos[_57];
}
}else{
for(var i in this._expandos){
if(typeof this._expandos[i]!="undefined"){
for(var j in this._expandos[i]){
var exp=this._expandos[i][j];
if(exp){
exp.destroy();
}
}
}
}
this._expandos={};
}
},postMixInProperties:function(){
this.inherited(arguments);
this._expandos={};
},onBeforeRow:function(_58,_59){
var g=this.grid;
if(g._by_idx&&g._by_idx[_58]&&g._by_idx[_58].idty){
var _5a=g._by_idx[_58].idty;
this._expandos[_5a]=this._expandos[_5a]||{};
}
this.inherited(arguments);
},onAfterRow:function(_5b,_5c,_5d){
_4.forEach(_b("span.dojoxGridExpando",_5d),function(n){
if(n&&n.parentNode){
var tc=n.getAttribute("toggleClass");
var _5e;
var _5f;
var g=this.grid;
if(g._by_idx&&g._by_idx[_5b]&&g._by_idx[_5b].idty){
_5e=g._by_idx[_5b].idty;
_5f=this._expandos[_5e][tc];
}
if(_5f){
_a.place(_5f.domNode,n,"replace");
_5f.itemId=n.getAttribute("itemId");
_5f.cellIdx=parseInt(n.getAttribute("cellIdx"),10);
if(isNaN(_5f.cellIdx)){
_5f.cellIdx=-1;
}
}else{
if(_5e){
_5f=_c.parse(n.parentNode)[0];
this._expandos[_5e][tc]=_5f;
}
}
if(_5f&&!_5f.setRowNode(_5b,_5d,this)){
_5f.domNode.parentNode.removeChild(_5f.domNode);
}
}
},this);
var alt=false;
var _60=this;
_b("tr[dojoxTreeGridPath]",_5d).forEach(function(n){
_8.toggle(n,"dojoxGridSubRowAlt",alt);
_7.set(n,"dojoxTreeGridBaseClasses",n.className);
alt=!alt;
_60.grid.rows.styleRowNode(_7.get(n,"dojoxTreeGridPath"),n);
});
this.inherited(arguments);
},updateRowStyles:function(_61){
var _62=_b("tr[dojoxTreeGridPath='"+_61+"']",this.domNode);
if(_62.length){
this.styleRowNode(_61,_62[0]);
}
},getCellNode:function(_63,_64){
var row=_b("tr[dojoxTreeGridPath='"+_63+"']",this.domNode)[0];
if(row){
return this.content.getCellNode(row,_64);
}
},destroy:function(){
this._cleanupExpandoCache();
this.inherited(arguments);
}});
});
