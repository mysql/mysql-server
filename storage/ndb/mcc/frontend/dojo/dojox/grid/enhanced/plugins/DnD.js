//>>built
define("dojox/grid/enhanced/plugins/DnD",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/_base/lang","dojo/_base/html","dojo/_base/json","dojo/_base/window","dojo/query","dojo/keys","dojo/dnd/Source","dojo/dnd/Avatar","../_Plugin","../../EnhancedGrid","./Selector","./Rearrange","dojo/dnd/Manager"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
var _f=function(a){
a.sort(function(v1,v2){
return v1-v2;
});
var arr=[[a[0]]];
for(var i=1,j=0;i<a.length;++i){
if(a[i]==a[i-1]+1){
arr[j].push(a[i]);
}else{
arr[++j]=[a[i]];
}
}
return arr;
},_10=function(_11){
var a=_11[0];
for(var i=1;i<_11.length;++i){
a=a.concat(_11[i]);
}
return a;
};
var _12=_2("dojox.grid.enhanced.plugins.GridDnDElement",null,{constructor:function(_13){
this.plugin=_13;
this.node=_6.create("div");
this._items={};
},destroy:function(){
this.plugin=null;
_6.destroy(this.node);
this.node=null;
this._items=null;
},createDnDNodes:function(_14){
this.destroyDnDNodes();
var _15=["grid/"+_14.type+"s"];
var _16=this.plugin.grid.id+"_dndItem";
_4.forEach(_14.selected,function(_17,i){
var id=_16+i;
this._items[id]={"type":_15,"data":_17,"dndPlugin":this.plugin};
this.node.appendChild(_6.create("div",{"id":id}));
},this);
},getDnDNodes:function(){
return _4.map(this.node.childNodes,function(_18){
return _18;
});
},destroyDnDNodes:function(){
_6.empty(this.node);
this._items={};
},getItem:function(_19){
return this._items[_19];
}});
var _1a=_2("dojox.grid.enhanced.plugins.GridDnDSource",_b,{accept:["grid/cells","grid/rows","grid/cols"],constructor:function(_1b,_1c){
this.grid=_1c.grid;
this.dndElem=_1c.dndElem;
this.dndPlugin=_1c.dnd;
this.sourcePlugin=null;
},destroy:function(){
this.inherited(arguments);
this.grid=null;
this.dndElem=null;
this.dndPlugin=null;
this.sourcePlugin=null;
},getItem:function(_1d){
return this.dndElem.getItem(_1d);
},checkAcceptance:function(_1e,_1f){
if(this!=_1e&&_1f[0]){
var _20=_1e.getItem(_1f[0].id);
if(_20.dndPlugin){
var _21=_20.type;
for(var j=0;j<_21.length;++j){
if(_21[j] in this.accept){
if(this.dndPlugin._canAccept(_20.dndPlugin)){
this.sourcePlugin=_20.dndPlugin;
}else{
return false;
}
break;
}
}
}else{
if("grid/rows" in this.accept){
var _22=[];
_4.forEach(_1f,function(_23){
var _24=_1e.getItem(_23.id);
if(_24.data&&_4.indexOf(_24.type,"grid/rows")>=0){
var _25=_24.data;
if(typeof _24.data=="string"){
_25=_7.fromJson(_24.data);
}
if(_25){
_22.push(_25);
}
}
});
if(_22.length){
this.sourcePlugin={_dndRegion:{type:"row",selected:[_22]}};
}else{
return false;
}
}
}
}
return this.inherited(arguments);
},onDraggingOver:function(){
this.dndPlugin.onDraggingOver(this.sourcePlugin);
},onDraggingOut:function(){
this.dndPlugin.onDraggingOut(this.sourcePlugin);
},onDndDrop:function(_26,_27,_28,_29){
this.onDndCancel();
if(this!=_26&&this==_29){
this.dndPlugin.onDragIn(this.sourcePlugin,_28);
}
}});
var _2a=_2("dojox.grid.enhanced.plugins.GridDnDAvatar",_c,{construct:function(){
this._itemType=this.manager._dndPlugin._dndRegion.type;
this._itemCount=this._getItemCount();
this.isA11y=_6.hasClass(_8.body(),"dijit_a11y");
var a=_6.create("table",{"border":"0","cellspacing":"0","class":"dojoxGridDndAvatar","style":{position:"absolute",zIndex:"1999",margin:"0px"}}),_2b=this.manager.source,b=_6.create("tbody",null,a),tr=_6.create("tr",null,b),td=_6.create("td",{"class":"dojoxGridDnDIcon"},tr);
if(this.isA11y){
_6.create("span",{"id":"a11yIcon","innerHTML":this.manager.copy?"+":"<"},td);
}
td=_6.create("td",{"class":"dojoxGridDnDItemIcon "+this._getGridDnDIconClass()},tr);
td=_6.create("td",null,tr);
_6.create("span",{"class":"dojoxGridDnDItemCount","innerHTML":_2b.generateText?this._generateText():""},td);
_6.style(tr,{"opacity":0.9});
this.node=a;
},_getItemCount:function(){
var _2c=this.manager._dndPlugin._dndRegion.selected,_2d=0;
switch(this._itemType){
case "cell":
_2c=_2c[0];
var _2e=this.manager._dndPlugin.grid.layout.cells,_2f=_2c.max.col-_2c.min.col+1,_30=_2c.max.row-_2c.min.row+1;
if(_2f>1){
for(var i=_2c.min.col;i<=_2c.max.col;++i){
if(_2e[i].hidden){
--_2f;
}
}
}
_2d=_2f*_30;
break;
case "row":
case "col":
_2d=_10(_2c).length;
}
return _2d;
},_getGridDnDIconClass:function(){
return {"row":["dojoxGridDnDIconRowSingle","dojoxGridDnDIconRowMulti"],"col":["dojoxGridDnDIconColSingle","dojoxGridDnDIconColMulti"],"cell":["dojoxGridDnDIconCellSingle","dojoxGridDnDIconCellMulti"]}[this._itemType][this._itemCount==1?0:1];
},_generateText:function(){
return "("+this._itemCount+")";
}});
var DnD=_2("dojox.grid.enhanced.plugins.DnD",_d,{name:"dnd",_targetAnchorBorderWidth:2,_copyOnly:false,_config:{"row":{"within":true,"in":true,"out":true},"col":{"within":true,"in":true,"out":true},"cell":{"within":true,"in":true,"out":true}},constructor:function(_31,_32){
this.grid=_31;
this._config=_5.clone(this._config);
_32=_5.isObject(_32)?_32:{};
this.setupConfig(_32.dndConfig);
this._copyOnly=!!_32.copyOnly;
this._mixinGrid();
this.selector=_31.pluginMgr.getPlugin("selector");
this.rearranger=_31.pluginMgr.getPlugin("rearrange");
this.rearranger.setArgs(_32);
this._clear();
this._elem=new _12(this);
this._source=new _1a(this._elem.node,{"grid":_31,"dndElem":this._elem,"dnd":this});
this._container=_9(".dojoxGridMasterView",this.grid.domNode)[0];
this._initEvents();
},destroy:function(){
this.inherited(arguments);
this._clear();
this._source.destroy();
this._elem.destroy();
this._container=null;
this.grid=null;
this.selector=null;
this.rearranger=null;
this._config=null;
},_mixinGrid:function(){
this.grid.setupDnDConfig=_5.hitch(this,"setupConfig");
this.grid.dndCopyOnly=_5.hitch(this,"copyOnly");
},setupConfig:function(_33){
if(_33&&_5.isObject(_33)){
var _34=["row","col","cell"],_35=["within","in","out"],cfg=this._config;
_4.forEach(_34,function(_36){
if(_36 in _33){
var t=_33[_36];
if(t&&_5.isObject(t)){
_4.forEach(_35,function(_37){
if(_37 in t){
cfg[_36][_37]=!!t[_37];
}
});
}else{
_4.forEach(_35,function(_38){
cfg[_36][_38]=!!t;
});
}
}
});
_4.forEach(_35,function(_39){
if(_39 in _33){
var m=_33[_39];
if(m&&_5.isObject(m)){
_4.forEach(_34,function(_3a){
if(_3a in m){
cfg[_3a][_39]=!!m[_3a];
}
});
}else{
_4.forEach(_34,function(_3b){
cfg[_3b][_39]=!!m;
});
}
}
});
}
},copyOnly:function(_3c){
if(typeof _3c!="undefined"){
this._copyOnly=!!_3c;
}
return this._copyOnly;
},_isOutOfGrid:function(evt){
var _3d=_6.position(this.grid.domNode),x=evt.clientX,y=evt.clientY;
return y<_3d.y||y>_3d.y+_3d.h||x<_3d.x||x>_3d.x+_3d.w;
},_onMouseMove:function(evt){
if(this._dndRegion&&!this._dnding&&!this._externalDnd){
this._dnding=true;
this._startDnd(evt);
}else{
if(this._isMouseDown&&!this._dndRegion){
delete this._isMouseDown;
this._oldCursor=_6.style(_8.body(),"cursor");
_6.style(_8.body(),"cursor","not-allowed");
}
var _3e=this._isOutOfGrid(evt);
if(!this._alreadyOut&&_3e){
this._alreadyOut=true;
if(this._dnding){
this._destroyDnDUI(true,false);
}
this._moveEvent=evt;
this._source.onOutEvent();
}else{
if(this._alreadyOut&&!_3e){
this._alreadyOut=false;
if(this._dnding){
this._createDnDUI(evt,true);
}
this._moveEvent=evt;
this._source.onOverEvent();
}
}
}
},_onMouseUp:function(){
if(!this._extDnding&&!this._isSource){
var _3f=this._dnding&&!this._alreadyOut;
if(_3f&&this._config[this._dndRegion.type]["within"]){
this._rearrange();
}
this._endDnd(_3f);
}
_6.style(_8.body(),"cursor",this._oldCursor||"");
delete this._isMouseDown;
},_initEvents:function(){
var g=this.grid,s=this.selector;
this.connect(_8.doc,"onmousemove","_onMouseMove");
this.connect(_8.doc,"onmouseup","_onMouseUp");
this.connect(g,"onCellMouseOver",function(evt){
if(!this._dnding&&!s.isSelecting()&&!evt.ctrlKey){
this._dndReady=s.isSelected("cell",evt.rowIndex,evt.cell.index);
s.selectEnabled(!this._dndReady);
}
});
this.connect(g,"onHeaderCellMouseOver",function(evt){
if(this._dndReady){
s.selectEnabled(true);
}
});
this.connect(g,"onRowMouseOver",function(evt){
if(this._dndReady&&!evt.cell){
s.selectEnabled(true);
}
});
this.connect(g,"onCellMouseDown",function(evt){
if(!evt.ctrlKey&&this._dndReady){
this._dndRegion=this._getDnDRegion(evt.rowIndex,evt.cell.index);
this._isMouseDown=true;
}
});
this.connect(g,"onCellMouseUp",function(evt){
if(!this._dndReady&&!s.isSelecting()&&evt.cell){
this._dndReady=s.isSelected("cell",evt.rowIndex,evt.cell.index);
s.selectEnabled(!this._dndReady);
}
});
this.connect(g,"onCellClick",function(evt){
if(this._dndReady&&!evt.ctrlKey&&!evt.shiftKey){
s.select("cell",evt.rowIndex,evt.cell.index);
}
});
this.connect(g,"onEndAutoScroll",function(_40,_41,_42,_43,evt){
if(this._dnding){
this._markTargetAnchor(evt);
}
});
this.connect(_8.doc,"onkeydown",function(evt){
if(evt.keyCode==_a.ESCAPE){
this._endDnd(false);
}else{
if(evt.keyCode==_a.CTRL){
s.selectEnabled(true);
this._isCopy=true;
}
}
});
this.connect(_8.doc,"onkeyup",function(evt){
if(evt.keyCode==_a.CTRL){
s.selectEnabled(!this._dndReady);
this._isCopy=false;
}
});
},_clear:function(){
this._dndRegion=null;
this._target=null;
this._moveEvent=null;
this._targetAnchor={};
this._dnding=false;
this._externalDnd=false;
this._isSource=false;
this._alreadyOut=false;
this._extDnding=false;
},_getDnDRegion:function(_44,_45){
var s=this.selector,_46=s._selected,_47=(!!_46.cell.length)|(!!_46.row.length<<1)|(!!_46.col.length<<2),_48;
switch(_47){
case 1:
_48="cell";
if(!this._config[_48]["within"]&&!this._config[_48]["out"]){
return null;
}
var _49=this.grid.layout.cells,_4a=function(_4b){
var _4c=0;
for(var i=_4b.min.col;i<=_4b.max.col;++i){
if(_49[i].hidden){
++_4c;
}
}
return (_4b.max.row-_4b.min.row+1)*(_4b.max.col-_4b.min.col+1-_4c);
},_4d=function(_4e,_4f){
return _4e.row>=_4f.min.row&&_4e.row<=_4f.max.row&&_4e.col>=_4f.min.col&&_4e.col<=_4f.max.col;
},_50={max:{row:-1,col:-1},min:{row:Infinity,col:Infinity}};
_4.forEach(_46[_48],function(_51){
if(_51.row<_50.min.row){
_50.min.row=_51.row;
}
if(_51.row>_50.max.row){
_50.max.row=_51.row;
}
if(_51.col<_50.min.col){
_50.min.col=_51.col;
}
if(_51.col>_50.max.col){
_50.max.col=_51.col;
}
});
if(_4.some(_46[_48],function(_52){
return _52.row==_44&&_52.col==_45;
})){
if(_4a(_50)==_46[_48].length&&_4.every(_46[_48],function(_53){
return _4d(_53,_50);
})){
return {"type":_48,"selected":[_50],"handle":{"row":_44,"col":_45}};
}
}
return null;
case 2:
case 4:
_48=_47==2?"row":"col";
if(!this._config[_48]["within"]&&!this._config[_48]["out"]){
return null;
}
var res=s.getSelected(_48);
if(res.length){
return {"type":_48,"selected":_f(res),"handle":_47==2?_44:_45};
}
return null;
}
return null;
},_startDnd:function(evt){
this._createDnDUI(evt);
},_endDnd:function(_54){
this._destroyDnDUI(false,_54);
this._clear();
},_createDnDUI:function(evt,_55){
var _56=_6.position(this.grid.views.views[0].domNode);
_6.style(this._container,"height",_56.h+"px");
try{
if(!_55){
this._createSource(evt);
}
this._createMoveable(evt);
this._oldCursor=_6.style(_8.body(),"cursor");
_6.style(_8.body(),"cursor","default");
}
catch(e){
console.warn("DnD._createDnDUI() error:",e);
}
},_destroyDnDUI:function(_57,_58){
try{
if(_58){
this._destroySource();
}
this._unmarkTargetAnchor();
if(!_57){
this._destroyMoveable();
}
_6.style(_8.body(),"cursor",this._oldCursor);
}
catch(e){
console.warn("DnD._destroyDnDUI() error:",this.grid.id,e);
}
},_createSource:function(evt){
this._elem.createDnDNodes(this._dndRegion);
var m=_1.dnd.manager();
var _59=m.makeAvatar;
m._dndPlugin=this;
m.makeAvatar=function(){
var _5a=new _2a(m);
delete m._dndPlugin;
return _5a;
};
m.startDrag(this._source,this._elem.getDnDNodes(),evt.ctrlKey);
m.makeAvatar=_59;
m.onMouseMove(evt);
},_destroySource:function(){
_3.publish("/dnd/cancel");
},_createMoveable:function(evt){
if(!this._markTagetAnchorHandler){
this._markTagetAnchorHandler=this.connect(_8.doc,"onmousemove","_markTargetAnchor");
}
},_destroyMoveable:function(){
this.disconnect(this._markTagetAnchorHandler);
delete this._markTagetAnchorHandler;
},_calcColTargetAnchorPos:function(evt,_5b){
var i,_5c,_5d,_5e,ex=evt.clientX,_5f=this.grid.layout.cells,ltr=_6._isBodyLtr(),_60=this._getVisibleHeaders();
for(i=0;i<_60.length;++i){
_5c=_6.position(_60[i].node);
if(ltr?((i===0||ex>=_5c.x)&&ex<_5c.x+_5c.w):((i===0||ex<_5c.x+_5c.w)&&ex>=_5c.x)){
_5d=_5c.x+(ltr?0:_5c.w);
break;
}else{
if(ltr?(i===_60.length-1&&ex>=_5c.x+_5c.w):(i===_60.length-1&&ex<_5c.x)){
++i;
_5d=_5c.x+(ltr?_5c.w:0);
break;
}
}
}
if(i<_60.length){
_5e=_60[i].cell.index;
if(this.selector.isSelected("col",_5e)&&this.selector.isSelected("col",_5e-1)){
var _61=this._dndRegion.selected;
for(i=0;i<_61.length;++i){
if(_4.indexOf(_61[i],_5e)>=0){
_5e=_61[i][0];
_5c=_6.position(_5f[_5e].getHeaderNode());
_5d=_5c.x+(ltr?0:_5c.w);
break;
}
}
}
}else{
_5e=_5f.length;
}
this._target=_5e;
return _5d-_5b.x;
},_calcRowTargetAnchorPos:function(evt,_62){
var g=this.grid,top,i=0,_63=g.layout.cells;
while(_63[i].hidden){
++i;
}
var _64=g.layout.cells[i],_65=g.scroller.firstVisibleRow,_66=_64.getNode(_65);
if(!_66){
this._target=-1;
return 0;
}
var _67=_6.position(_66);
while(_67.y+_67.h<evt.clientY){
if(++_65>=g.rowCount){
break;
}
_67=_6.position(_64.getNode(_65));
}
if(_65<g.rowCount){
if(this.selector.isSelected("row",_65)&&this.selector.isSelected("row",_65-1)){
var _68=this._dndRegion.selected;
for(i=0;i<_68.length;++i){
if(_4.indexOf(_68[i],_65)>=0){
_65=_68[i][0];
_67=_6.position(_64.getNode(_65));
break;
}
}
}
top=_67.y;
}else{
top=_67.y+_67.h;
}
this._target=_65;
return top-_62.y;
},_calcCellTargetAnchorPos:function(evt,_69,_6a){
var s=this._dndRegion.selected[0],_6b=this._dndRegion.handle,g=this.grid,ltr=_6._isBodyLtr(),_6c=g.layout.cells,_6d,_6e,_6f,_70,_71,_72,_73,top,_74,_75,i,_76=_6b.col-s.min.col,_77=s.max.col-_6b.col,_78,_79;
if(!_6a.childNodes.length){
_78=_6.create("div",{"class":"dojoxGridCellBorderLeftTopDIV"},_6a);
_79=_6.create("div",{"class":"dojoxGridCellBorderRightBottomDIV"},_6a);
}else{
_78=_9(".dojoxGridCellBorderLeftTopDIV",_6a)[0];
_79=_9(".dojoxGridCellBorderRightBottomDIV",_6a)[0];
}
for(i=s.min.col+1;i<_6b.col;++i){
if(_6c[i].hidden){
--_76;
}
}
for(i=_6b.col+1;i<s.max.col;++i){
if(_6c[i].hidden){
--_77;
}
}
_70=this._getVisibleHeaders();
for(i=_76;i<_70.length-_77;++i){
_6d=_6.position(_70[i].node);
if((evt.clientX>=_6d.x&&evt.clientX<_6d.x+_6d.w)||(i==_76&&(ltr?evt.clientX<_6d.x:evt.clientX>=_6d.x+_6d.w))||(i==_70.length-_77-1&&(ltr?evt.clientX>=_6d.x+_6d.w:evt<_6d.x))){
_74=_70[i-_76];
_75=_70[i+_77];
_6e=_6.position(_74.node);
_6f=_6.position(_75.node);
_74=_74.cell.index;
_75=_75.cell.index;
_73=ltr?_6e.x:_6f.x;
_72=ltr?(_6f.x+_6f.w-_6e.x):(_6e.x+_6e.w-_6f.x);
break;
}
}
i=0;
while(_6c[i].hidden){
++i;
}
var _7a=_6c[i],_7b=g.scroller.firstVisibleRow,_7c=_6.position(_7a.getNode(_7b));
while(_7c.y+_7c.h<evt.clientY){
if(++_7b<g.rowCount){
_7c=_6.position(_7a.getNode(_7b));
}else{
break;
}
}
var _7d=_7b>=_6b.row-s.min.row?_7b-_6b.row+s.min.row:0;
var _7e=_7d+s.max.row-s.min.row;
if(_7e>=g.rowCount){
_7e=g.rowCount-1;
_7d=_7e-s.max.row+s.min.row;
}
_6e=_6.position(_7a.getNode(_7d));
_6f=_6.position(_7a.getNode(_7e));
top=_6e.y;
_71=_6f.y+_6f.h-_6e.y;
this._target={"min":{"row":_7d,"col":_74},"max":{"row":_7e,"col":_75}};
var _7f=(_6.marginBox(_78).w-_6.contentBox(_78).w)/2;
var _80=_6.position(_6c[_74].getNode(_7d));
_6.style(_78,{"width":(_80.w-_7f)+"px","height":(_80.h-_7f)+"px"});
var _81=_6.position(_6c[_75].getNode(_7e));
_6.style(_79,{"width":(_81.w-_7f)+"px","height":(_81.h-_7f)+"px"});
return {h:_71,w:_72,l:_73-_69.x,t:top-_69.y};
},_markTargetAnchor:function(evt){
try{
var t=this._dndRegion.type;
if(this._alreadyOut||(this._dnding&&!this._config[t]["within"])||(this._extDnding&&!this._config[t]["in"])){
return;
}
var _82,_83,_84,top,_85=this._targetAnchor[t],pos=_6.position(this._container);
if(!_85){
_85=this._targetAnchor[t]=_6.create("div",{"class":(t=="cell")?"dojoxGridCellBorderDIV":"dojoxGridBorderDIV"});
_6.style(_85,"display","none");
this._container.appendChild(_85);
}
switch(t){
case "col":
_82=pos.h;
_83=this._targetAnchorBorderWidth;
_84=this._calcColTargetAnchorPos(evt,pos);
top=0;
break;
case "row":
_82=this._targetAnchorBorderWidth;
_83=pos.w;
_84=0;
top=this._calcRowTargetAnchorPos(evt,pos);
break;
case "cell":
var _86=this._calcCellTargetAnchorPos(evt,pos,_85);
_82=_86.h;
_83=_86.w;
_84=_86.l;
top=_86.t;
}
if(typeof _82=="number"&&typeof _83=="number"&&typeof _84=="number"&&typeof top=="number"){
_6.style(_85,{"height":_82+"px","width":_83+"px","left":_84+"px","top":top+"px"});
_6.style(_85,"display","");
}else{
this._target=null;
}
}
catch(e){
console.warn("DnD._markTargetAnchor() error:",e);
}
},_unmarkTargetAnchor:function(){
if(this._dndRegion){
var _87=this._targetAnchor[this._dndRegion.type];
if(_87){
_6.style(this._targetAnchor[this._dndRegion.type],"display","none");
}
}
},_getVisibleHeaders:function(){
return _4.map(_4.filter(this.grid.layout.cells,function(_88){
return !_88.hidden;
}),function(_89){
return {"node":_89.getHeaderNode(),"cell":_89};
});
},_rearrange:function(){
if(this._target===null){
return;
}
var t=this._dndRegion.type;
var _8a=this._dndRegion.selected;
if(t==="cell"){
this.rearranger[(this._isCopy||this._copyOnly)?"copyCells":"moveCells"](_8a[0],this._target===-1?null:this._target);
}else{
this.rearranger[t=="col"?"moveColumns":"moveRows"](_10(_8a),this._target===-1?null:this._target);
}
this._target=null;
},onDraggingOver:function(_8b){
if(!this._dnding&&_8b){
_8b._isSource=true;
this._extDnding=true;
if(!this._externalDnd){
this._externalDnd=true;
this._dndRegion=this._mapRegion(_8b.grid,_8b._dndRegion);
}
this._createDnDUI(this._moveEvent,true);
this.grid.pluginMgr.getPlugin("autoScroll").readyForAutoScroll=true;
}
},_mapRegion:function(_8c,_8d){
if(_8d.type==="cell"){
var _8e=_8d.selected[0];
var _8f=this.grid.layout.cells;
var _90=_8c.layout.cells;
var c,cnt=0;
for(c=_8e.min.col;c<=_8e.max.col;++c){
if(!_90[c].hidden){
++cnt;
}
}
for(c=0;cnt>0;++c){
if(!_8f[c].hidden){
--cnt;
}
}
var _91=_5.clone(_8d);
_91.selected[0].min.col=0;
_91.selected[0].max.col=c-1;
for(c=_8e.min.col;c<=_8d.handle.col;++c){
if(!_90[c].hidden){
++cnt;
}
}
for(c=0;cnt>0;++c){
if(!_8f[c].hidden){
--cnt;
}
}
_91.handle.col=c;
}
return _8d;
},onDraggingOut:function(_92){
if(this._externalDnd){
this._extDnding=false;
this._destroyDnDUI(true,false);
if(_92){
_92._isSource=false;
}
}
},onDragIn:function(_93,_94){
var _95=false;
if(this._target!==null){
var _96=_93._dndRegion.type;
var _97=_93._dndRegion.selected;
switch(_96){
case "cell":
this.rearranger.changeCells(_93.grid,_97[0],this._target);
break;
case "row":
var _98=_10(_97);
this.rearranger.insertRows(_93.grid,_98,this._target);
break;
}
_95=true;
}
this._endDnd(true);
if(_93.onDragOut){
_93.onDragOut(_95&&!_94);
}
},onDragOut:function(_99){
if(_99&&!this._copyOnly){
var _9a=this._dndRegion.type;
var _9b=this._dndRegion.selected;
switch(_9a){
case "cell":
this.rearranger.clearCells(_9b[0]);
break;
case "row":
this.rearranger.removeRows(_10(_9b));
break;
}
}
this._endDnd(true);
},_canAccept:function(_9c){
if(!_9c){
return false;
}
var _9d=_9c._dndRegion;
var _9e=_9d.type;
if(!this._config[_9e]["in"]||!_9c._config[_9e]["out"]){
return false;
}
var g=this.grid;
var _9f=_9d.selected;
var _a0=_4.filter(g.layout.cells,function(_a1){
return !_a1.hidden;
}).length;
var _a2=g.rowCount;
var res=true;
switch(_9e){
case "cell":
_9f=_9f[0];
res=g.store.getFeatures()["dojo.data.api.Write"]&&(_9f.max.row-_9f.min.row)<=_a2&&_4.filter(_9c.grid.layout.cells,function(_a3){
return _a3.index>=_9f.min.col&&_a3.index<=_9f.max.col&&!_a3.hidden;
}).length<=_a0;
case "row":
if(_9c._allDnDItemsLoaded()){
return res;
}
}
return false;
},_allDnDItemsLoaded:function(){
if(this._dndRegion){
var _a4=this._dndRegion.type,_a5=this._dndRegion.selected,_a6=[];
switch(_a4){
case "cell":
for(var i=_a5[0].min.row,max=_a5[0].max.row;i<=max;++i){
_a6.push(i);
}
break;
case "row":
_a6=_10(_a5);
break;
default:
return false;
}
var _a7=this.grid._by_idx;
return _4.every(_a6,function(_a8){
return !!_a7[_a8];
});
}
return false;
}});
_e.registerPlugin(DnD,{"dependency":["selector","rearrange"]});
return DnD;
});
