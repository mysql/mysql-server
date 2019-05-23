//>>built
define("dojox/grid/enhanced/plugins/DnD",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/_base/lang","dojo/_base/html","dojo/_base/json","dojo/_base/window","dojo/query","dojo/keys","dojo/dnd/Source","dojo/dnd/Avatar","../_Plugin","../../EnhancedGrid","dojo/dnd/Manager","./Selector","./Rearrange"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
var _10=function(a){
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
},_11=function(_12){
var a=_12[0];
for(var i=1;i<_12.length;++i){
a=a.concat(_12[i]);
}
return a;
};
var _13=_2("dojox.grid.enhanced.plugins.GridDnDElement",null,{constructor:function(_14){
this.plugin=_14;
this.node=_6.create("div");
this._items={};
},destroy:function(){
this.plugin=null;
_6.destroy(this.node);
this.node=null;
this._items=null;
},createDnDNodes:function(_15){
this.destroyDnDNodes();
var _16=["grid/"+_15.type+"s"];
var _17=this.plugin.grid.id+"_dndItem";
_4.forEach(_15.selected,function(_18,i){
var id=_17+i;
this._items[id]={"type":_16,"data":_18,"dndPlugin":this.plugin};
this.node.appendChild(_6.create("div",{"id":id}));
},this);
},getDnDNodes:function(){
return _4.map(this.node.childNodes,function(_19){
return _19;
});
},destroyDnDNodes:function(){
_6.empty(this.node);
this._items={};
},getItem:function(_1a){
return this._items[_1a];
}});
var _1b=_2("dojox.grid.enhanced.plugins.GridDnDSource",_b,{accept:["grid/cells","grid/rows","grid/cols"],constructor:function(_1c,_1d){
this.grid=_1d.grid;
this.dndElem=_1d.dndElem;
this.dndPlugin=_1d.dnd;
this.sourcePlugin=null;
},destroy:function(){
this.inherited(arguments);
this.grid=null;
this.dndElem=null;
this.dndPlugin=null;
this.sourcePlugin=null;
},getItem:function(_1e){
return this.dndElem.getItem(_1e);
},checkAcceptance:function(_1f,_20){
if(this!=_1f&&_20[0]){
var _21=_1f.getItem(_20[0].id);
if(_21.dndPlugin){
var _22=_21.type;
for(var j=0;j<_22.length;++j){
if(_22[j] in this.accept){
if(this.dndPlugin._canAccept(_21.dndPlugin)){
this.sourcePlugin=_21.dndPlugin;
}else{
return false;
}
break;
}
}
}else{
if("grid/rows" in this.accept){
var _23=[];
_4.forEach(_20,function(_24){
var _25=_1f.getItem(_24.id);
if(_25.data&&_4.indexOf(_25.type,"grid/rows")>=0){
var _26=_25.data;
if(typeof _25.data=="string"){
_26=_7.fromJson(_25.data);
}
if(_26){
_23.push(_26);
}
}
});
if(_23.length){
this.sourcePlugin={_dndRegion:{type:"row",selected:[_23]}};
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
},onDndDrop:function(_27,_28,_29,_2a){
this.onDndCancel();
if(this!=_27&&this==_2a){
this.dndPlugin.onDragIn(this.sourcePlugin,_29);
}
}});
var _2b=_2("dojox.grid.enhanced.plugins.GridDnDAvatar",_c,{construct:function(){
this._itemType=this.manager._dndPlugin._dndRegion.type;
this._itemCount=this._getItemCount();
this.isA11y=_6.hasClass(_8.body(),"dijit_a11y");
var a=_6.create("table",{"border":"0","cellspacing":"0","class":"dojoxGridDndAvatar","style":{position:"absolute",zIndex:"1999",margin:"0px"}}),_2c=this.manager.source,b=_6.create("tbody",null,a),tr=_6.create("tr",null,b),td=_6.create("td",{"class":"dojoxGridDnDIcon"},tr);
if(this.isA11y){
_6.create("span",{"id":"a11yIcon","innerHTML":this.manager.copy?"+":"<"},td);
}
td=_6.create("td",{"class":"dojoxGridDnDItemIcon "+this._getGridDnDIconClass()},tr);
td=_6.create("td",null,tr);
_6.create("span",{"class":"dojoxGridDnDItemCount","innerHTML":_2c.generateText?this._generateText():""},td);
_6.style(tr,{"opacity":0.9});
this.node=a;
},_getItemCount:function(){
var _2d=this.manager._dndPlugin._dndRegion.selected,_2e=0;
switch(this._itemType){
case "cell":
_2d=_2d[0];
var _2f=this.manager._dndPlugin.grid.layout.cells,_30=_2d.max.col-_2d.min.col+1,_31=_2d.max.row-_2d.min.row+1;
if(_30>1){
for(var i=_2d.min.col;i<=_2d.max.col;++i){
if(_2f[i].hidden){
--_30;
}
}
}
_2e=_30*_31;
break;
case "row":
case "col":
_2e=_11(_2d).length;
}
return _2e;
},_getGridDnDIconClass:function(){
return {"row":["dojoxGridDnDIconRowSingle","dojoxGridDnDIconRowMulti"],"col":["dojoxGridDnDIconColSingle","dojoxGridDnDIconColMulti"],"cell":["dojoxGridDnDIconCellSingle","dojoxGridDnDIconCellMulti"]}[this._itemType][this._itemCount==1?0:1];
},_generateText:function(){
return "("+this._itemCount+")";
}});
var DnD=_2("dojox.grid.enhanced.plugins.DnD",_d,{name:"dnd",_targetAnchorBorderWidth:2,_copyOnly:false,_config:{"row":{"within":true,"in":true,"out":true},"col":{"within":true,"in":true,"out":true},"cell":{"within":true,"in":true,"out":true}},constructor:function(_32,_33){
this.grid=_32;
this._config=_5.clone(this._config);
_33=_5.isObject(_33)?_33:{};
this.setupConfig(_33.dndConfig);
this._copyOnly=!!_33.copyOnly;
this._mixinGrid();
this.selector=_32.pluginMgr.getPlugin("selector");
this.rearranger=_32.pluginMgr.getPlugin("rearrange");
this.rearranger.setArgs(_33);
this._clear();
this._elem=new _13(this);
this._source=new _1b(this._elem.node,{"grid":_32,"dndElem":this._elem,"dnd":this});
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
},setupConfig:function(_34){
if(_34&&_5.isObject(_34)){
var _35=["row","col","cell"],_36=["within","in","out"],cfg=this._config;
_4.forEach(_35,function(_37){
if(_37 in _34){
var t=_34[_37];
if(t&&_5.isObject(t)){
_4.forEach(_36,function(_38){
if(_38 in t){
cfg[_37][_38]=!!t[_38];
}
});
}else{
_4.forEach(_36,function(_39){
cfg[_37][_39]=!!t;
});
}
}
});
_4.forEach(_36,function(_3a){
if(_3a in _34){
var m=_34[_3a];
if(m&&_5.isObject(m)){
_4.forEach(_35,function(_3b){
if(_3b in m){
cfg[_3b][_3a]=!!m[_3b];
}
});
}else{
_4.forEach(_35,function(_3c){
cfg[_3c][_3a]=!!m;
});
}
}
});
}
},copyOnly:function(_3d){
if(typeof _3d!="undefined"){
this._copyOnly=!!_3d;
}
return this._copyOnly;
},_isOutOfGrid:function(evt){
var _3e=_6.position(this.grid.domNode),x=evt.clientX,y=evt.clientY;
return y<_3e.y||y>_3e.y+_3e.h||x<_3e.x||x>_3e.x+_3e.w;
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
var _3f=this._isOutOfGrid(evt);
if(!this._alreadyOut&&_3f){
this._alreadyOut=true;
if(this._dnding){
this._destroyDnDUI(true,false);
}
this._moveEvent=evt;
this._source.onOutEvent();
}else{
if(this._alreadyOut&&!_3f){
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
var _40=this._dnding&&!this._alreadyOut;
if(_40&&this._config[this._dndRegion.type]["within"]){
this._rearrange();
}
this._endDnd(_40);
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
this.connect(g,"onEndAutoScroll",function(_41,_42,_43,_44,evt){
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
},_getDnDRegion:function(_45,_46){
var s=this.selector,_47=s._selected,_48=(!!_47.cell.length)|(!!_47.row.length<<1)|(!!_47.col.length<<2),_49;
switch(_48){
case 1:
_49="cell";
if(!this._config[_49]["within"]&&!this._config[_49]["out"]){
return null;
}
var _4a=this.grid.layout.cells,_4b=function(_4c){
var _4d=0;
for(var i=_4c.min.col;i<=_4c.max.col;++i){
if(_4a[i].hidden){
++_4d;
}
}
return (_4c.max.row-_4c.min.row+1)*(_4c.max.col-_4c.min.col+1-_4d);
},_4e=function(_4f,_50){
return _4f.row>=_50.min.row&&_4f.row<=_50.max.row&&_4f.col>=_50.min.col&&_4f.col<=_50.max.col;
},_51={max:{row:-1,col:-1},min:{row:Infinity,col:Infinity}};
_4.forEach(_47[_49],function(_52){
if(_52.row<_51.min.row){
_51.min.row=_52.row;
}
if(_52.row>_51.max.row){
_51.max.row=_52.row;
}
if(_52.col<_51.min.col){
_51.min.col=_52.col;
}
if(_52.col>_51.max.col){
_51.max.col=_52.col;
}
});
if(_4.some(_47[_49],function(_53){
return _53.row==_45&&_53.col==_46;
})){
if(_4b(_51)==_47[_49].length&&_4.every(_47[_49],function(_54){
return _4e(_54,_51);
})){
return {"type":_49,"selected":[_51],"handle":{"row":_45,"col":_46}};
}
}
return null;
case 2:
case 4:
_49=_48==2?"row":"col";
if(!this._config[_49]["within"]&&!this._config[_49]["out"]){
return null;
}
var res=s.getSelected(_49);
if(res.length){
return {"type":_49,"selected":_10(res),"handle":_48==2?_45:_46};
}
return null;
}
return null;
},_startDnd:function(evt){
this._createDnDUI(evt);
},_endDnd:function(_55){
this._destroyDnDUI(false,_55);
this._clear();
},_createDnDUI:function(evt,_56){
var _57=_6.position(this.grid.views.views[0].domNode);
_6.style(this._container,"height",_57.h+"px");
try{
if(!_56){
this._createSource(evt);
}
this._createMoveable(evt);
this._oldCursor=_6.style(_8.body(),"cursor");
_6.style(_8.body(),"cursor","default");
}
catch(e){
console.warn("DnD._createDnDUI() error:",e);
}
},_destroyDnDUI:function(_58,_59){
try{
if(_59){
this._destroySource();
}
this._unmarkTargetAnchor();
if(!_58){
this._destroyMoveable();
}
_6.style(_8.body(),"cursor",this._oldCursor);
}
catch(e){
console.warn("DnD._destroyDnDUI() error:",this.grid.id,e);
}
},_createSource:function(evt){
this._elem.createDnDNodes(this._dndRegion);
var m=_f.manager();
var _5a=m.makeAvatar;
m._dndPlugin=this;
m.makeAvatar=function(){
var _5b=new _2b(m);
delete m._dndPlugin;
return _5b;
};
m.startDrag(this._source,this._elem.getDnDNodes(),evt.ctrlKey);
m.makeAvatar=_5a;
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
},_calcColTargetAnchorPos:function(evt,_5c){
var i,_5d,_5e,_5f,ex=evt.clientX,_60=this.grid.layout.cells,ltr=_6._isBodyLtr(),_61=this._getVisibleHeaders();
for(i=0;i<_61.length;++i){
_5d=_6.position(_61[i].node);
if(ltr?((i===0||ex>=_5d.x)&&ex<_5d.x+_5d.w):((i===0||ex<_5d.x+_5d.w)&&ex>=_5d.x)){
_5e=_5d.x+(ltr?0:_5d.w);
break;
}else{
if(ltr?(i===_61.length-1&&ex>=_5d.x+_5d.w):(i===_61.length-1&&ex<_5d.x)){
++i;
_5e=_5d.x+(ltr?_5d.w:0);
break;
}
}
}
if(i<_61.length){
_5f=_61[i].cell.index;
if(this.selector.isSelected("col",_5f)&&this.selector.isSelected("col",_5f-1)){
var _62=this._dndRegion.selected;
for(i=0;i<_62.length;++i){
if(_4.indexOf(_62[i],_5f)>=0){
_5f=_62[i][0];
_5d=_6.position(_60[_5f].getHeaderNode());
_5e=_5d.x+(ltr?0:_5d.w);
break;
}
}
}
}else{
_5f=_60.length;
}
this._target=_5f;
return _5e-_5c.x;
},_calcRowTargetAnchorPos:function(evt,_63){
var g=this.grid,top,i=0,_64=g.layout.cells;
while(_64[i].hidden){
++i;
}
var _65=g.layout.cells[i],_66=g.scroller.firstVisibleRow,_67=_65.getNode(_66);
if(!_67){
this._target=-1;
return 0;
}
var _68=_6.position(_67);
while(_68.y+_68.h<evt.clientY){
if(++_66>=g.rowCount){
break;
}
_68=_6.position(_65.getNode(_66));
}
if(_66<g.rowCount){
if(this.selector.isSelected("row",_66)&&this.selector.isSelected("row",_66-1)){
var _69=this._dndRegion.selected;
for(i=0;i<_69.length;++i){
if(_4.indexOf(_69[i],_66)>=0){
_66=_69[i][0];
_68=_6.position(_65.getNode(_66));
break;
}
}
}
top=_68.y;
}else{
top=_68.y+_68.h;
}
this._target=_66;
return top-_63.y;
},_calcCellTargetAnchorPos:function(evt,_6a,_6b){
var s=this._dndRegion.selected[0],_6c=this._dndRegion.handle,g=this.grid,ltr=_6._isBodyLtr(),_6d=g.layout.cells,_6e,_6f,_70,_71,_72,_73,_74,top,_75,_76,i,_77=_6c.col-s.min.col,_78=s.max.col-_6c.col,_79,_7a;
if(!_6b.childNodes.length){
_79=_6.create("div",{"class":"dojoxGridCellBorderLeftTopDIV"},_6b);
_7a=_6.create("div",{"class":"dojoxGridCellBorderRightBottomDIV"},_6b);
}else{
_79=_9(".dojoxGridCellBorderLeftTopDIV",_6b)[0];
_7a=_9(".dojoxGridCellBorderRightBottomDIV",_6b)[0];
}
for(i=s.min.col+1;i<_6c.col;++i){
if(_6d[i].hidden){
--_77;
}
}
for(i=_6c.col+1;i<s.max.col;++i){
if(_6d[i].hidden){
--_78;
}
}
_71=this._getVisibleHeaders();
for(i=_77;i<_71.length-_78;++i){
_6e=_6.position(_71[i].node);
if((evt.clientX>=_6e.x&&evt.clientX<_6e.x+_6e.w)||(i==_77&&(ltr?evt.clientX<_6e.x:evt.clientX>=_6e.x+_6e.w))||(i==_71.length-_78-1&&(ltr?evt.clientX>=_6e.x+_6e.w:evt<_6e.x))){
_75=_71[i-_77];
_76=_71[i+_78];
_6f=_6.position(_75.node);
_70=_6.position(_76.node);
_75=_75.cell.index;
_76=_76.cell.index;
_74=ltr?_6f.x:_70.x;
_73=ltr?(_70.x+_70.w-_6f.x):(_6f.x+_6f.w-_70.x);
break;
}
}
i=0;
while(_6d[i].hidden){
++i;
}
var _7b=_6d[i],_7c=g.scroller.firstVisibleRow,_7d=_6.position(_7b.getNode(_7c));
while(_7d.y+_7d.h<evt.clientY){
if(++_7c<g.rowCount){
_7d=_6.position(_7b.getNode(_7c));
}else{
break;
}
}
var _7e=_7c>=_6c.row-s.min.row?_7c-_6c.row+s.min.row:0;
var _7f=_7e+s.max.row-s.min.row;
if(_7f>=g.rowCount){
_7f=g.rowCount-1;
_7e=_7f-s.max.row+s.min.row;
}
_6f=_6.position(_7b.getNode(_7e));
_70=_6.position(_7b.getNode(_7f));
top=_6f.y;
_72=_70.y+_70.h-_6f.y;
this._target={"min":{"row":_7e,"col":_75},"max":{"row":_7f,"col":_76}};
var _80=(_6.marginBox(_79).w-_6.contentBox(_79).w)/2;
var _81=_6.position(_6d[_75].getNode(_7e));
_6.style(_79,{"width":(_81.w-_80)+"px","height":(_81.h-_80)+"px"});
var _82=_6.position(_6d[_76].getNode(_7f));
_6.style(_7a,{"width":(_82.w-_80)+"px","height":(_82.h-_80)+"px"});
return {h:_72,w:_73,l:_74-_6a.x,t:top-_6a.y};
},_markTargetAnchor:function(evt){
try{
var t=this._dndRegion.type;
if(this._alreadyOut||(this._dnding&&!this._config[t]["within"])||(this._extDnding&&!this._config[t]["in"])){
return;
}
var _83,_84,_85,top,_86=this._targetAnchor[t],pos=_6.position(this._container);
if(!_86){
_86=this._targetAnchor[t]=_6.create("div",{"class":(t=="cell")?"dojoxGridCellBorderDIV":"dojoxGridBorderDIV"});
_6.style(_86,"display","none");
this._container.appendChild(_86);
}
switch(t){
case "col":
_83=pos.h;
_84=this._targetAnchorBorderWidth;
_85=this._calcColTargetAnchorPos(evt,pos);
top=0;
break;
case "row":
_83=this._targetAnchorBorderWidth;
_84=pos.w;
_85=0;
top=this._calcRowTargetAnchorPos(evt,pos);
break;
case "cell":
var _87=this._calcCellTargetAnchorPos(evt,pos,_86);
_83=_87.h;
_84=_87.w;
_85=_87.l;
top=_87.t;
}
if(typeof _83=="number"&&typeof _84=="number"&&typeof _85=="number"&&typeof top=="number"){
_6.style(_86,{"height":_83+"px","width":_84+"px","left":_85+"px","top":top+"px"});
_6.style(_86,"display","");
}else{
this._target=null;
}
}
catch(e){
console.warn("DnD._markTargetAnchor() error:",e);
}
},_unmarkTargetAnchor:function(){
if(this._dndRegion){
var _88=this._targetAnchor[this._dndRegion.type];
if(_88){
_6.style(this._targetAnchor[this._dndRegion.type],"display","none");
}
}
},_getVisibleHeaders:function(){
return _4.map(_4.filter(this.grid.layout.cells,function(_89){
return !_89.hidden;
}),function(_8a){
return {"node":_8a.getHeaderNode(),"cell":_8a};
});
},_rearrange:function(){
if(this._target===null){
return;
}
var t=this._dndRegion.type;
var _8b=this._dndRegion.selected;
if(t==="cell"){
this.rearranger[(this._isCopy||this._copyOnly)?"copyCells":"moveCells"](_8b[0],this._target===-1?null:this._target);
}else{
this.rearranger[t=="col"?"moveColumns":"moveRows"](_11(_8b),this._target===-1?null:this._target);
}
this._target=null;
},onDraggingOver:function(_8c){
if(!this._dnding&&_8c){
_8c._isSource=true;
this._extDnding=true;
if(!this._externalDnd){
this._externalDnd=true;
this._dndRegion=this._mapRegion(_8c.grid,_8c._dndRegion);
}
this._createDnDUI(this._moveEvent,true);
this.grid.pluginMgr.getPlugin("autoScroll").readyForAutoScroll=true;
}
},_mapRegion:function(_8d,_8e){
if(_8e.type==="cell"){
var _8f=_8e.selected[0];
var _90=this.grid.layout.cells;
var _91=_8d.layout.cells;
var c,cnt=0;
for(c=_8f.min.col;c<=_8f.max.col;++c){
if(!_91[c].hidden){
++cnt;
}
}
for(c=0;cnt>0;++c){
if(!_90[c].hidden){
--cnt;
}
}
var _92=_5.clone(_8e);
_92.selected[0].min.col=0;
_92.selected[0].max.col=c-1;
for(c=_8f.min.col;c<=_8e.handle.col;++c){
if(!_91[c].hidden){
++cnt;
}
}
for(c=0;cnt>0;++c){
if(!_90[c].hidden){
--cnt;
}
}
_92.handle.col=c;
}
return _8e;
},onDraggingOut:function(_93){
if(this._externalDnd){
this._extDnding=false;
this._destroyDnDUI(true,false);
if(_93){
_93._isSource=false;
}
}
},onDragIn:function(_94,_95){
var _96=false;
if(this._target!==null){
var _97=_94._dndRegion.type;
var _98=_94._dndRegion.selected;
switch(_97){
case "cell":
this.rearranger.changeCells(_94.grid,_98[0],this._target);
break;
case "row":
var _99=_11(_98);
this.rearranger.insertRows(_94.grid,_99,this._target);
break;
}
_96=true;
}
this._endDnd(true);
if(_94.onDragOut){
_94.onDragOut(_96&&!_95);
}
},onDragOut:function(_9a){
if(_9a&&!this._copyOnly){
var _9b=this._dndRegion.type;
var _9c=this._dndRegion.selected;
switch(_9b){
case "cell":
this.rearranger.clearCells(_9c[0]);
break;
case "row":
this.rearranger.removeRows(_11(_9c));
break;
}
}
this._endDnd(true);
},_canAccept:function(_9d){
if(!_9d){
return false;
}
var _9e=_9d._dndRegion;
var _9f=_9e.type;
if(!this._config[_9f]["in"]||!_9d._config[_9f]["out"]){
return false;
}
var g=this.grid;
var _a0=_9e.selected;
var _a1=_4.filter(g.layout.cells,function(_a2){
return !_a2.hidden;
}).length;
var _a3=g.rowCount;
var res=true;
switch(_9f){
case "cell":
_a0=_a0[0];
res=g.store.getFeatures()["dojo.data.api.Write"]&&(_a0.max.row-_a0.min.row)<=_a3&&_4.filter(_9d.grid.layout.cells,function(_a4){
return _a4.index>=_a0.min.col&&_a4.index<=_a0.max.col&&!_a4.hidden;
}).length<=_a1;
case "row":
if(_9d._allDnDItemsLoaded()){
return res;
}
}
return false;
},_allDnDItemsLoaded:function(){
if(this._dndRegion){
var _a5=this._dndRegion.type,_a6=this._dndRegion.selected,_a7=[];
switch(_a5){
case "cell":
for(var i=_a6[0].min.row,max=_a6[0].max.row;i<=max;++i){
_a7.push(i);
}
break;
case "row":
_a7=_11(_a6);
break;
default:
return false;
}
var _a8=this.grid._by_idx;
return _4.every(_a7,function(_a9){
return !!_a8[_a9];
});
}
return false;
}});
_e.registerPlugin(DnD,{"dependency":["selector","rearrange"]});
return DnD;
});
