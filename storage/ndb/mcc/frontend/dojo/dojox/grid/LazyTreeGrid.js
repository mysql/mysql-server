//>>built
require({cache:{"url:dojox/grid/resources/Expando.html":"<div class=\"dojoxGridExpando\"\n\t><div class=\"dojoxGridExpandoNode\" dojoAttachEvent=\"onclick:onToggle\"\n\t\t><div class=\"dojoxGridExpandoNodeInner\" dojoAttachPoint=\"expandoInner\"></div\n\t></div\n></div>\n"}});
define("dojox/grid/LazyTreeGrid",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/event","dojo/_base/array","dojo/query","dojo/parser","dojo/dom-construct","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/dom","dojo/keys","dojo/text!./resources/Expando.html","dijit/_Widget","dijit/_TemplatedMixin","./TreeGrid","./_Builder","./_View","./_Layout","./cells/tree","./_RowManager","./_FocusManager","./_EditManager","./DataSelection","./util"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a){
var _1b=_2("dojox.grid._LazyExpando",[_f,_10],{grid:null,view:null,rowIdx:-1,cellIdx:-1,level:0,itemId:"",templateString:_e,onToggle:function(evt){
if(this.grid._treeCache.items[this.rowIdx]){
this.grid.focus.setFocusIndex(this.rowIdx,this.cellIdx);
this.setOpen(!this.grid._treeCache.items[this.rowIdx].opened);
try{
_4.stop(evt);
}
catch(e){
}
}
},setOpen:function(_1c){
var g=this.grid,_1d=g._by_idx[this.rowIdx].item;
if(_1d&&g.treeModel.mayHaveChildren(_1d)&&!g._loading&&g._treeCache.items[this.rowIdx].opened!==_1c){
g._treeCache.items[this.rowIdx].opened=_1c;
g.expandoFetch(this.rowIdx,_1c);
this._updateOpenState(_1d);
}
},_updateOpenState:function(_1e){
var g=this.grid,_1f;
if(_1e&&g.treeModel.mayHaveChildren(_1e)){
_1f=g._treeCache.items[this.rowIdx].opened;
this.expandoInner.innerHTML=_1f?"-":"+";
_9.toggle(this.domNode,"dojoxGridExpandoOpened",_1f);
this.domNode.parentNode.setAttribute("aria-expanded",_1f);
}else{
_9.remove(this.domNode,"dojoxGridExpandoOpened");
}
},setRowNode:function(_20,_21,_22){
if(this.cellIdx<0||!this.itemId){
return false;
}
this.view=_22;
this.grid=_22.grid;
this.rowIdx=_20;
var _23=this.grid.isLeftToRight()?"marginLeft":"marginRight";
_a.set(this.domNode.parentNode,_23,(this.level*1.125)+"em");
this._updateOpenState(this.grid._by_idx[this.rowIdx].item);
return true;
}});
var _24=_2("dojox.grid._TreeGridContentBuilder",_12._ContentBuilder,{generateHtml:function(_25,_26){
var _27=this.getTableArray(),_28=this.grid,v=this.view,_29=v.structure.cells,_2a=_28.getItem(_26),_2b=0,_2c="",_2d=_28._treeCache.items[_26]?_28._treeCache.items[_26].treePath:null;
_1a.fire(this.view,"onBeforeRow",[_26,_29]);
if(_2a&&_3.isArray(_2d)){
_2b=_2d.length;
_2c=_28.treeModel.mayHaveChildren(_2a)?"":"dojoxGridNoChildren";
}
var i=0,j=0,row,_2e,_2f,_30=0,_31=[];
for(;row=_29[j];j++){
if(row.hidden||row.header){
continue;
}
_27.push("<tr class=\""+_2c+"\">");
_2f=this._getColSpans(_2b);
if(_2f){
_5.forEach(_2f,function(c){
for(i=0;_2e=row[i];i++){
if(i>=c.start&&i<=c.end){
_30+=this._getCellWidth(row,i);
}
}
_31.push(_30);
_30=0;
},this);
}
var m,cc,cs,pbm,k=0;
for(i=0;_2e=row[i];i++){
m=_2e.markup;
cc=_2e.customClasses=[];
cs=_2e.customStyles=[];
if(_2f&&_2f[k]&&(i>=_2f[k].start&&i<=_2f[k].end)){
var _32=_2f[k].primary||_2f[k].start;
if(i==_32){
m[5]=_2e.formatAtLevel(_2a,_2b,_26);
m[1]=cc.join(" ");
pbm=_b.getMarginBox(_2e.getHeaderNode()).w-_b.getContentBox(_2e.getHeaderNode()).w;
cs=_2e.customStyles=["width:"+(_31[k]-pbm)+"px"];
m[3]=cs.join(";");
_27.push.apply(_27,m);
}else{
if(i==_2f[k].end){
k++;
continue;
}else{
continue;
}
}
}else{
m[5]=_2e.formatAtLevel(_2a,_2b,_26);
m[1]=cc.join(" ");
m[3]=cs.join(";");
_27.push.apply(_27,m);
}
}
_27.push("</tr>");
}
_27.push("</table>");
return _27.join("");
},_getColSpans:function(_33){
var _34=this.grid.colSpans;
return _34&&_34[_33]?_34[_33]:null;
},_getCellWidth:function(_35,_36){
var _37=_35[_36],_38=_37.getHeaderNode();
if(_37.hidden){
return 0;
}
if(_36==_35.length-1||_5.every(_35.slice(_36+1),function(_39){
return _39.hidden;
})){
var _3a=_b.position(_35[_36].view.headerContentNode.firstChild);
return _3a.x+_3a.w-_b.position(_38).x;
}else{
var _3b;
do{
_3b=_35[++_36];
}while(_3b.hidden);
return _b.position(_3b.getHeaderNode()).x-_b.position(_38).x;
}
}});
_2("dojox.grid._TreeGridView",_13,{_contentBuilderClass:_24,postCreate:function(){
this.inherited(arguments);
this._expandos={};
this.connect(this.grid,"_onCleanupExpandoCache","_cleanupExpandoCache");
},destroy:function(){
this._cleanupExpandoCache();
this.inherited(arguments);
},_cleanupExpandoCache:function(_3c){
if(_3c&&this._expandos[_3c]){
this._expandos[_3c].destroy();
delete this._expandos[_3c];
}else{
var i;
for(i in this._expandos){
this._expandos[i].destroy();
}
this._expandos={};
}
},onAfterRow:function(_3d,_3e,_3f){
_6("span.dojoxGridExpando",_3f).forEach(function(n){
if(n&&n.parentNode){
var _40,_41,_42=this.grid._by_idx;
if(_42&&_42[_3d]&&_42[_3d].idty){
_40=_42[_3d].idty;
_41=this._expandos[_40];
}
if(_41){
_8.place(_41.domNode,n,"replace");
_41.itemId=n.getAttribute("itemId");
_41.cellIdx=parseInt(n.getAttribute("cellIdx"),10);
if(isNaN(_41.cellIdx)){
_41.cellIdx=-1;
}
}else{
_41=_7.parse(n.parentNode)[0];
if(_40){
this._expandos[_40]=_41;
}
}
if(!_41.setRowNode(_3d,_3f,this)){
_41.domNode.parentNode.removeChild(_41.domNode);
}
_8.destroy(n);
}
},this);
this.inherited(arguments);
},updateRow:function(_43){
var _44=this.grid,_45;
if(_44.keepSelection){
_45=_44.getItem(_43);
if(_45){
_44.selection.preserver._reSelectById(_45,_43);
}
}
this.inherited(arguments);
}});
var _46=_3.mixin(_3.clone(_15),{formatAtLevel:function(_47,_48,_49){
if(!_47){
return this.formatIndexes(_49,_47,_48);
}
var _4a="",ret="",_4b;
if(this.isCollapsable&&this.grid.store.isItem(_47)){
ret="<span "+_1._scopeName+"Type=\"dojox.grid._LazyExpando\" level=\""+_48+"\" class=\"dojoxGridExpando\""+" itemId=\""+this.grid.store.getIdentity(_47)+"\" cellIdx=\""+this.index+"\"></span>";
}
_4b=this.formatIndexes(_49,_47,_48);
_4a=ret!==""?"<div>"+ret+_4b+"</div>":_4b;
return _4a;
},formatIndexes:function(_4c,_4d,_4e){
var _4f=this.grid.edit.info,d=this.get?this.get(_4c,_4d):(this.value||this.defaultValue);
if(this.editable&&(this.alwaysEditing||(_4f.rowIndex===_4c&&_4f.cell===this))){
return this.formatEditing(d,_4c);
}else{
var dir=this.textDir||this.grid.textDir;
var ret=this._defaultFormat(d,[d,_4c,_4e,this]);
if(dir&&this._enforceTextDirWithUcc){
ret=this._enforceTextDirWithUcc(dir,ret);
}
return ret;
}
}});
var _50=_2("dojox.grid._LazyTreeLayout",_14,{setStructure:function(_51){
var g=this.grid,s=_51;
if(g&&!_5.every(s,function(i){
return !!i.cells;
})){
s=arguments[0]=[{cells:[s]}];
}
if(s.length===1&&s[0].cells.length===1){
s[0].type="dojox.grid._TreeGridView";
this._isCollapsable=true;
s[0].cells[0][this.grid.expandoCell].isCollapsable=true;
}
this.inherited(arguments);
},addCellDef:function(_52,_53,def){
var obj=this.inherited(arguments);
return _3.mixin(obj,_46);
}});
var _54=_2("dojox.grid._LazyTreeGridCache",null,{constructor:function(){
this.items=[];
},getSiblingIndex:function(_55,_56){
var i=_55-1,_57=0,tp;
for(;i>=0;i--){
tp=this.items[i]?this.items[i].treePath:[];
if(tp.join("/")===_56.join("/")){
_57++;
}else{
if(tp.length<_56.length){
break;
}
}
}
return _57;
},removeChildren:function(_58){
var i=_58+1,_59,tp,_5a=this.items[_58]?this.items[_58].treePath:[];
for(;i<this.items.length;i++){
tp=this.items[i]?this.items[i].treePath:[];
if(tp.join("/")===_5a.join("/")||tp.length<=_5a.length){
break;
}
}
_59=i-(_58+1);
this.items.splice(_58+1,_59);
return _59;
}});
var _5b=_2("dojox.grid.LazyTreeGrid",_11,{_layoutClass:_50,_size:0,treeModel:null,defaultState:null,colSpans:null,postCreate:function(){
this._setState();
this.inherited(arguments);
if(!this._treeCache){
this._treeCache=new _54();
}
if(!this.treeModel||!(this.treeModel instanceof dijit.tree.ForestStoreModel)){
throw new Error("dojox.grid.LazyTreeGrid: must be used with a treeModel which is an instance of dijit.tree.ForestStoreModel");
}
_9.add(this.domNode,"dojoxGridTreeModel");
_c.setSelectable(this.domNode,this.selectable);
},createManagers:function(){
this.rows=new _16(this);
this.focus=new _17(this);
this.edit=new _18(this);
},createSelection:function(){
this.selection=new _19(this);
},setModel:function(_5c){
if(!_5c){
return;
}
this._setModel(_5c);
this._cleanup();
this._refresh(true);
},setStore:function(_5d,_5e,_5f){
if(!_5d){
return;
}
this._setQuery(_5e,_5f);
this.treeModel.query=_5e;
this.treeModel.store=_5d;
this.treeModel.root.children=[];
this.setModel(this.treeModel);
},onSetState:function(){
},_setState:function(){
if(this.defaultState){
this._treeCache=this.defaultState.cache;
this.sortInfo=this.defaultState.sortInfo||0;
this.query=this.defaultState.query||this.query;
this._lastScrollTop=this.defaultState.scrollTop;
if(this.keepSelection){
this.selection.preserver._selectedById=this.defaultState.selection;
}else{
this.selection.selected=this.defaultState.selection||[];
}
this.onSetState();
}
},getState:function(){
var _60=this,_61=this.keepSelection?this.selection.preserver._selectedById:this.selection.selected;
return {cache:_3.clone(_60._treeCache),query:_3.clone(_60.query),sortInfo:_3.clone(_60.sortInfo),scrollTop:_3.clone(_60.scrollTop),selection:_3.clone(_61)};
},_setQuery:function(_62,_63){
this.inherited(arguments);
this.treeModel.query=_62;
},filter:function(_64,_65){
this._cleanup();
this.inherited(arguments);
},destroy:function(){
this._cleanup();
this.inherited(arguments);
},expand:function(_66){
this._fold(_66,true);
},collapse:function(_67){
this._fold(_67,false);
},refresh:function(_68){
if(!_68){
this._cleanup();
}
this._refresh(true);
},_cleanup:function(){
this._treeCache.items=[];
this._onCleanupExpandoCache();
},setSortIndex:function(_69,_6a){
if(this.canSort(_69+1)){
this._cleanup();
}
this.inherited(arguments);
},_refresh:function(_6b){
this._clearData();
this.updateRowCount(this._size);
this._fetch(0,true);
},render:function(){
this.inherited(arguments);
this.setScrollTop(this.scrollTop);
},_onNew:function(_6c,_6d){
var _6e=_6d&&this.store.isItem(_6d.item)&&_5.some(this.treeModel.childrenAttrs,function(c){
return c===_6d.attribute;
});
var _6f=this._treeCache.items,_70=this._by_idx;
if(!_6e){
_6f.push({opened:false,treePath:[]});
this._size+=1;
this.inherited(arguments);
}else{
var _71=_6d.item,_72=this.store.getIdentity(_71),_73=-1,i=0;
for(;i<_70.length;i++){
if(_72===_70[i].idty){
_73=i;
break;
}
}
if(_73>=0){
if(_6f[_73]&&_6f[_73].opened){
var _74=_6f[_73].treePath,pos=_73+1;
for(;pos<_6f.length;pos++){
if(_6f[pos].treePath.length<=_74.length){
break;
}
}
var _75=_74.slice();
_75.push(_72);
this._treeCache.items.splice(pos,0,{opened:false,treePath:_75});
var _76=this.store.getIdentity(_6c);
this._by_idty[_76]={idty:_76,item:_6c};
_70.splice(pos,0,this._by_idty[_76]);
this._size+=1;
this.updateRowCount(this._size);
this._updateRenderedRows(pos);
}else{
this.updateRow(_73);
}
}
}
},_onDelete:function(_77){
var i=0,_78=-1,_79=this.store.getIdentity(_77);
for(;i<this._by_idx.length;i++){
if(_79===this._by_idx[i].idty){
_78=i;
break;
}
}
if(_78>=0){
var _7a=this._treeCache.items,_7b=_7a[_78]?_7a[_78].treePath:[],tp,_7c=1;
i=_78+1;
for(;i<this._size;i++,_7c++){
tp=_7a[i]?_7a[i].treePath:[];
if(_7a[i].treePath.length<=_7b.length){
break;
}
}
_7a.splice(_78,_7c);
this._onCleanupExpandoCache(_79);
this._by_idx.splice(_78,_7c);
this._size-=_7c;
this.updateRowCount(this._size);
this._updateRenderedRows(_78);
}
},_onCleanupExpandoCache:function(_7d){
},_fetch:function(_7e,_7f){
if(!this._loading){
this._loading=true;
}
_7e=_7e||0;
var _80=this._size-_7e>0?Math.min(this.rowsPerPage,this._size-_7e):this.rowsPerPage;
var i=0;
var _81=[];
this._reqQueueLen=0;
for(;i<_80;i++){
if(this._by_idx[_7e+i]){
_81.push(this._by_idx[_7e+i].item);
}else{
break;
}
}
if(_81.length===_80){
this._reqQueueLen=1;
this._onFetchBegin(this._size,{startRowIdx:_7e,count:_80});
this._onFetchComplete(_81,{startRowIdx:_7e,count:_80});
}else{
var _82,_83,len=1,_84=this._treeCache.items,_85=_84[_7e]?_84[_7e].treePath:[];
for(i=1;i<_80;i++){
_82=_84[_7e+len-1]?_84[_7e+len-1].treePath.length:0;
_83=_84[_7e+len]?_84[_7e+len].treePath.length:0;
if(_82!==_83){
this._reqQueueLen++;
this._fetchItems({startRowIdx:_7e,count:len,treePath:_85});
_7e=_7e+len;
len=1;
_85=_84[_7e]?_84[_7e].treePath:0;
}else{
len++;
}
}
this._reqQueueLen++;
this._fetchItems({startRowIdx:_7e,count:len,treePath:_85});
}
},_fetchItems:function(req){
if(this._pending_requests[req.startRowIdx]){
return;
}
this.showMessage(this.loadingMessage);
this._pending_requests[req.startRowIdx]=true;
var _86=_3.hitch(this,"_onFetchError"),_87=this._treeCache.getSiblingIndex(req.startRowIdx,req.treePath);
if(req.treePath.length===0){
this.store.fetch({start:_87,startRowIdx:req.startRowIdx,treePath:req.treePath,count:req.count,query:this.query,sort:this.getSortProps(),queryOptions:this.queryOptions,onBegin:_3.hitch(this,"_onFetchBegin"),onComplete:_3.hitch(this,"_onFetchComplete"),onError:_3.hitch(this,"_onFetchError")});
}else{
var _88=req.treePath[req.treePath.length-1],_89;
var _8a={start:_87,startRowIdx:req.startRowIdx,treePath:req.treePath,count:req.count,parentId:_88,sort:this.getSortProps()};
var _8b=this;
var _8c=function(){
var f=_3.hitch(_8b,"_onFetchComplete");
if(arguments.length==1){
f.apply(_8b,[arguments[0],_8a]);
}else{
f.apply(_8b,arguments);
}
};
if(this._by_idty[_88]){
_89=this._by_idty[_88].item;
this.treeModel.getChildren(_89,_8c,_86,_8a);
}else{
this.store.fetchItemByIdentity({identity:_88,onItem:function(_8d){
_8b.treeModel.getChildren(_8d,_8c,_86,_8a);
},onError:_86});
}
}
},_onFetchBegin:function(_8e,_8f){
if(this._treeCache.items.length===0){
this._size=parseInt(_8e,10);
}
_8e=this._size;
this.inherited(arguments);
},_onFetchComplete:function(_90,_91){
var _92=_91.startRowIdx,_93=_91.count,_94=_90.length<=_93?0:_91.start,_95=_91.treePath||[];
if(_3.isArray(_90)&&_90.length>0){
var i=0,len=Math.min(_93,_90.length);
for(;i<len;i++){
if(!this._treeCache.items[_92+i]){
this._treeCache.items[_92+i]={opened:false,treePath:_95};
}
if(!this._by_idx[_92+i]){
this._addItem(_90[_94+i],_92+i,true);
}
}
this.updateRows(_92,len);
}
if(this._size==0){
this.showMessage(this.noDataMessage);
}else{
this.showMessage();
}
this._pending_requests[_92]=false;
this._reqQueueLen--;
if(this._loading&&this._reqQueueLen===0){
this._loading=false;
if(this._lastScrollTop){
this.setScrollTop(this._lastScrollTop);
}
}
},expandoFetch:function(_96,_97){
if(this._loading||!this._by_idx[_96]){
return;
}
this._loading=true;
this._toggleLoadingClass(_96,true);
this.expandoRowIndex=_96;
var _98=this._by_idx[_96].item;
if(_97){
var _99={start:0,count:this.rowsPerPage,parentId:this.store.getIdentity(this._by_idx[_96].item),sort:this.getSortProps()};
this.treeModel.getChildren(_98,_3.hitch(this,"_onExpandoComplete"),_3.hitch(this,"_onFetchError"),_99);
}else{
var num=this._treeCache.removeChildren(_96);
this._by_idx.splice(_96+1,num);
this._bop=this._eop=-1;
this._size-=num;
this.updateRowCount(this._size);
this._updateRenderedRows(_96+1);
this._toggleLoadingClass(_96,false);
if(this._loading){
this._loading=false;
}
this.focus._delayedCellFocus();
}
},_onExpandoComplete:function(_9a,_9b,_9c){
_9c=isNaN(_9c)?_9a.length:parseInt(_9c,10);
var _9d=this._treeCache.items[this.expandoRowIndex].treePath.slice(0);
_9d.push(this.store.getIdentity(this._by_idx[this.expandoRowIndex].item));
var i=1,_9e;
for(;i<=_9c;i++){
this._treeCache.items.splice(this.expandoRowIndex+i,0,{treePath:_9d,opened:false});
}
this._size+=_9c;
this.updateRowCount(this._size);
for(i=0;i<_9c;i++){
if(_9a[i]){
_9e=this.store.getIdentity(_9a[i]);
this._by_idty[_9e]={idty:_9e,item:_9a[i]};
this._by_idx.splice(this.expandoRowIndex+1+i,0,this._by_idty[_9e]);
}else{
this._by_idx.splice(this.expandoRowIndex+1+i,0,null);
}
}
this._updateRenderedRows(this.expandoRowIndex+1);
this._toggleLoadingClass(this.expandoRowIndex,false);
this.stateChangeNode=null;
if(this._loading){
this._loading=false;
}
if(this.autoHeight===true){
this._resize();
}
this.focus._delayedCellFocus();
},styleRowNode:function(_9f,_a0){
if(_a0){
this.rows.styleRowNode(_9f,_a0);
}
},onStyleRow:function(row){
if(!this.layout._isCollapsable){
this.inherited(arguments);
return;
}
row.customClasses+=(row.odd?" dojoxGridRowOdd":"")+(row.selected?" dojoxGridRowSelected":"")+(row.over?" dojoxGridRowOver":"");
this.focus.styleRow(row);
this.edit.styleRow(row);
},onKeyDown:function(e){
if(e.altKey||e.metaKey){
return;
}
var _a1=dijit.findWidgets(e.target)[0];
if(e.keyCode===_d.ENTER&&_a1 instanceof _1b){
_a1.onToggle();
}
this.inherited(arguments);
},_toggleLoadingClass:function(_a2,_a3){
var _a4=this.views.views,_a5,_a6=_a4[_a4.length-1].getRowNode(_a2);
if(_a6){
_a5=_6(".dojoxGridExpando",_a6)[0];
if(_a5){
_9.toggle(_a5,"dojoxGridExpandoLoading",_a3);
}
}
},_updateRenderedRows:function(_a7){
_5.forEach(this.scroller.stack,function(p){
if(p*this.rowsPerPage>=_a7){
this.updateRows(p*this.rowsPerPage,this.rowsPerPage);
}else{
if((p+1)*this.rowsPerPage>=_a7){
this.updateRows(_a7,(p+1)*this.rowsPerPage-_a7+1);
}
}
},this);
},_fold:function(_a8,_a9){
var _aa=-1,i=0,_ab=this._by_idx,_ac=this._by_idty[_a8];
if(_ac&&_ac.item&&this.treeModel.mayHaveChildren(_ac.item)){
for(;i<_ab.length;i++){
if(_ab[i]&&_ab[i].idty===_a8){
_aa=i;
break;
}
}
if(_aa>=0){
var _ad=this.views.views[this.views.views.length-1].getRowNode(_aa);
if(_ad){
var _ae=dijit.findWidgets(_ad)[0];
if(_ae){
_ae.setOpen(_a9);
}
}
}
}
}});
_5b.markupFactory=function(_af,_b0,_b1,_b2){
return _11.markupFactory(_af,_b0,_b1,_b2);
};
return _5b;
});
