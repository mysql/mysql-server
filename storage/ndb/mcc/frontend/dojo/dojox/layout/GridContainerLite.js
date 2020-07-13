//>>built
require({cache:{"url:dojox/layout/resources/GridContainer.html":"<div id=\"${id}\" class=\"gridContainer\" dojoAttachPoint=\"containerNode\" tabIndex=\"0\" dojoAttachEvent=\"onkeypress:_selectFocus\">\n\t<div dojoAttachPoint=\"gridContainerDiv\">\n\t\t<table class=\"gridContainerTable\" dojoAttachPoint=\"gridContainerTable\" cellspacing=\"0\" cellpadding=\"0\">\n\t\t\t<tbody>\n\t\t\t\t<tr dojoAttachPoint=\"gridNode\" >\n\t\t\t\t\t\n\t\t\t\t</tr>\n\t\t\t</tbody>\n\t\t</table>\n\t</div>\n</div>"}});
define("dojox/layout/GridContainerLite",["dojo/_base/kernel","dojo/text!./resources/GridContainer.html","dojo/_base/declare","dojo/query","dojo/_base/sniff","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/dom-construct","dojo/dom-attr","dojo/_base/array","dojo/_base/lang","dojo/_base/event","dojo/keys","dojo/topic","dijit/registry","dijit/focus","dijit/_base/focus","dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/layout/_LayoutWidget","dojo/_base/NodeList","dojox/mdnd/AreaManager","dojox/mdnd/DropIndicator","dojox/mdnd/dropMode/OverDropMode","dojox/mdnd/AutoScroll"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17){
var gcl=_3("dojox.layout.GridContainerLite",[_15,_14],{autoRefresh:true,templateString:_2,dragHandleClass:"dojoxDragHandle",nbZones:1,doLayout:true,isAutoOrganized:true,acceptTypes:[],colWidths:"",constructor:function(_18,_19){
this.acceptTypes=(_18||{}).acceptTypes||["text"];
this._disabled=true;
},postCreate:function(){
this.inherited(arguments);
this._grid=[];
this._createCells();
this.subscribe("/dojox/mdnd/drop","resizeChildAfterDrop");
this.subscribe("/dojox/mdnd/drag/start","resizeChildAfterDragStart");
this._dragManager=_17.areaManager();
this._dragManager.autoRefresh=this.autoRefresh;
this._dragManager.dragHandleClass=this.dragHandleClass;
if(this.doLayout){
this._border={h:_5("ie")?_8.getBorderExtents(this.gridContainerTable).h:0,w:(_5("ie")==6)?1:0};
}else{
_7.set(this.domNode,"overflowY","hidden");
_7.set(this.gridContainerTable,"height","auto");
}
},startup:function(){
if(this._started){
return;
}
if(this.isAutoOrganized){
this._organizeChildren();
}else{
this._organizeChildrenManually();
}
_b.forEach(this.getChildren(),function(_1a){
_1a.startup();
});
if(this._isShown()){
this.enableDnd();
}
this.inherited(arguments);
},resizeChildAfterDrop:function(_1b,_1c,_1d){
if(this._disabled){
return false;
}
if(_10.getEnclosingWidget(_1c.node)==this){
var _1e=_10.byNode(_1b);
if(_1e.resize&&_c.isFunction(_1e.resize)){
_1e.resize();
}
_1e.set("column",_1b.parentNode.cellIndex);
if(this.doLayout){
var _1f=this._contentBox.h,_20=_8.getContentBox(this.gridContainerDiv).h;
if(_20>=_1f){
_7.set(this.gridContainerTable,"height",(_1f-this._border.h)+"px");
}
}
return true;
}
return false;
},resizeChildAfterDragStart:function(_21,_22,_23){
if(this._disabled){
return false;
}
if(_10.getEnclosingWidget(_22.node)==this){
this._draggedNode=_21;
if(this.doLayout){
_8.setMarginBox(this.gridContainerTable,{h:_8.getContentBox(this.gridContainerDiv).h-this._border.h});
}
return true;
}
return false;
},getChildren:function(){
var _24=new _16();
_b.forEach(this._grid,function(_25){
_4("> [widgetId]",_25.node).map(_10.byNode).forEach(function(_26){
_24.push(_26);
});
});
return _24;
},_isShown:function(){
if("open" in this){
return this.open;
}else{
var _27=this.domNode;
return (_27.style.display!="none")&&(_27.style.visibility!="hidden")&&!_6.contains(_27,"dijitHidden");
}
},layout:function(){
if(this.doLayout){
var _28=this._contentBox;
_8.setMarginBox(this.gridContainerTable,{h:_28.h-this._border.h});
_8.setContentSize(this.domNode,{w:_28.w-this._border.w});
}
_b.forEach(this.getChildren(),function(_29){
if(_29.resize&&_c.isFunction(_29.resize)){
_29.resize();
}
});
},onShow:function(){
if(this._disabled){
this.enableDnd();
}
},onHide:function(){
if(!this._disabled){
this.disableDnd();
}
},_createCells:function(){
if(this.nbZones===0){
this.nbZones=1;
}
var _2a=this.acceptTypes.join(","),i=0;
var _2b=this._computeColWidth();
while(i<this.nbZones){
this._grid.push({node:_9.create("td",{"class":"gridContainerZone",accept:_2a,id:this.id+"_dz"+i,style:{"width":_2b[i]+"%"}},this.gridNode)});
i++;
}
},_getZonesAttr:function(){
return _4(".gridContainerZone",this.containerNode);
},enableDnd:function(){
var m=this._dragManager;
_b.forEach(this._grid,function(_2c){
m.registerByNode(_2c.node);
});
m._dropMode.updateAreas(m._areaList);
this._disabled=false;
},disableDnd:function(){
var m=this._dragManager;
_b.forEach(this._grid,function(_2d){
m.unregister(_2d.node);
});
m._dropMode.updateAreas(m._areaList);
this._disabled=true;
},_organizeChildren:function(){
var _2e=gcl.superclass.getChildren.call(this);
var _2f=this.nbZones,_30=Math.floor(_2e.length/_2f),mod=_2e.length%_2f,i=0;
for(var z=0;z<_2f;z++){
for(var r=0;r<_30;r++){
this._insertChild(_2e[i],z);
i++;
}
if(mod>0){
try{
this._insertChild(_2e[i],z);
i++;
}
catch(e){
console.error("Unable to insert child in GridContainer",e);
}
mod--;
}else{
if(_30===0){
break;
}
}
}
},_organizeChildrenManually:function(){
var _31=gcl.superclass.getChildren.call(this),_32=_31.length,_33;
for(var i=0;i<_32;i++){
_33=_31[i];
try{
this._insertChild(_33,_33.column-1);
}
catch(e){
console.error("Unable to insert child in GridContainer",e);
}
}
},_insertChild:function(_34,_35,p){
var _36=this._grid[_35].node,_37=_36.childNodes.length;
if(typeof p==="undefined"||p>_37){
p=_37;
}
if(this._disabled){
_9.place(_34.domNode,_36,p);
_a.set(_34.domNode,"tabIndex","0");
}else{
if(!_34.dragRestriction){
this._dragManager.addDragItem(_36,_34.domNode,p,true);
}else{
_9.place(_34.domNode,_36,p);
_a.set(_34.domNode,"tabIndex","0");
}
}
_34.set("column",_35);
return _34;
},removeChild:function(_38){
if(this._disabled){
this.inherited(arguments);
}else{
this._dragManager.removeDragItem(_38.domNode.parentNode,_38.domNode);
}
},addService:function(_39,_3a,p){
kernel.deprecated("addService is deprecated.","Please use  instead.","Future");
this.addChild(_39,_3a,p);
},addChild:function(_3b,_3c,p){
_3b.domNode.id=_3b.id;
gcl.superclass.addChild.call(this,_3b,0);
if(_3c<0||_3c===undefined){
_3c=0;
}
if(p<=0){
p=0;
}
try{
return this._insertChild(_3b,_3c,p);
}
catch(e){
console.error("Unable to insert child in GridContainer",e);
}
return null;
},_setColWidthsAttr:function(_3d){
this.colWidths=_c.isString(_3d)?_3d.split(","):(_c.isArray(_3d)?_3d:[_3d]);
if(this._started){
this._updateColumnsWidth();
}
},_updateColumnsWidth:function(_3e){
var _3f=this._grid.length;
var _40=this._computeColWidth();
for(var i=0;i<_3f;i++){
this._grid[i].node.style.width=_40[i]+"%";
}
},_computeColWidth:function(){
var _41=this.colWidths||[];
var _42=[];
var _43;
var _44=0;
var i;
for(i=0;i<this.nbZones;i++){
if(_42.length<_41.length){
_44+=_41[i]*1;
_42.push(_41[i]);
}else{
if(!_43){
_43=(100-_44)/(this.nbZones-i);
if(_43<0){
_43=100/this.nbZones;
}
}
_42.push(_43);
_44+=_43*1;
}
}
if(_44>100){
var _45=100/_44;
for(i=0;i<_42.length;i++){
_42[i]*=_45;
}
}
return _42;
},_selectFocus:function(_46){
if(this._disabled){
return;
}
var key=_46.keyCode,k=_e,_47=null,_48=_12.getFocus(),_49=_48.node,m=this._dragManager,_4a,i,j,r,_4b,_4c,_4d;
if(_49==this.containerNode){
_4c=this.gridNode.childNodes;
switch(key){
case k.DOWN_ARROW:
case k.RIGHT_ARROW:
_4a=false;
for(i=0;i<_4c.length;i++){
_4b=_4c[i].childNodes;
for(j=0;j<_4b.length;j++){
_47=_4b[j];
if(_47!==null&&_47.style.display!="none"){
_11.focus(_47);
_d.stop(_46);
_4a=true;
break;
}
}
if(_4a){
break;
}
}
break;
case k.UP_ARROW:
case k.LEFT_ARROW:
_4c=this.gridNode.childNodes;
_4a=false;
for(i=_4c.length-1;i>=0;i--){
_4b=_4c[i].childNodes;
for(j=_4b.length;j>=0;j--){
_47=_4b[j];
if(_47!==null&&_47.style.display!="none"){
_11.focus(_47);
_d.stop(_46);
_4a=true;
break;
}
}
if(_4a){
break;
}
}
break;
}
}else{
if(_49.parentNode.parentNode==this.gridNode){
var _4e=(key==k.UP_ARROW||key==k.LEFT_ARROW)?"lastChild":"firstChild";
var pos=(key==k.UP_ARROW||key==k.LEFT_ARROW)?"previousSibling":"nextSibling";
switch(key){
case k.UP_ARROW:
case k.DOWN_ARROW:
_d.stop(_46);
_4a=false;
var _4f=_49;
while(!_4a){
_4b=_4f.parentNode.childNodes;
var num=0;
for(i=0;i<_4b.length;i++){
if(_4b[i].style.display!="none"){
num++;
}
if(num>1){
break;
}
}
if(num==1){
return;
}
if(_4f[pos]===null){
_47=_4f.parentNode[_4e];
}else{
_47=_4f[pos];
}
if(_47.style.display==="none"){
_4f=_47;
}else{
_4a=true;
}
}
if(_46.shiftKey){
var _50=_49.parentNode;
for(i=0;i<this.gridNode.childNodes.length;i++){
if(_50==this.gridNode.childNodes[i]){
break;
}
}
_4b=this.gridNode.childNodes[i].childNodes;
for(j=0;j<_4b.length;j++){
if(_47==_4b[j]){
break;
}
}
if(_5("mozilla")||_5("webkit")){
i--;
}
_4d=_10.byNode(_49);
if(!_4d.dragRestriction){
r=m.removeDragItem(_50,_49);
this.addChild(_4d,i,j);
_a.set(_49,"tabIndex","0");
_11.focus(_49);
}else{
_f.publish("/dojox/layout/gridContainer/moveRestriction",this);
}
}else{
_11.focus(_47);
}
break;
case k.RIGHT_ARROW:
case k.LEFT_ARROW:
_d.stop(_46);
if(_46.shiftKey){
var z=0;
if(_49.parentNode[pos]===null){
if(_5("ie")&&key==k.LEFT_ARROW){
z=this.gridNode.childNodes.length-1;
}
}else{
if(_49.parentNode[pos].nodeType==3){
z=this.gridNode.childNodes.length-2;
}else{
for(i=0;i<this.gridNode.childNodes.length;i++){
if(_49.parentNode[pos]==this.gridNode.childNodes[i]){
break;
}
z++;
}
if(_5("mozilla")||_5("webkit")){
z--;
}
}
}
_4d=_10.byNode(_49);
var _51=_49.getAttribute("dndtype");
if(_51===null){
if(_4d&&_4d.dndType){
_51=_4d.dndType.split(/\s*,\s*/);
}else{
_51=["text"];
}
}else{
_51=_51.split(/\s*,\s*/);
}
var _52=false;
for(i=0;i<this.acceptTypes.length;i++){
for(j=0;j<_51.length;j++){
if(_51[j]==this.acceptTypes[i]){
_52=true;
break;
}
}
}
if(_52&&!_4d.dragRestriction){
var _53=_49.parentNode,_54=0;
if(k.LEFT_ARROW==key){
var t=z;
if(_5("mozilla")||_5("webkit")){
t=z+1;
}
_54=this.gridNode.childNodes[t].childNodes.length;
}
r=m.removeDragItem(_53,_49);
this.addChild(_4d,z,_54);
_a.set(r,"tabIndex","0");
_11.focus(r);
}else{
_f.publish("/dojox/layout/gridContainer/moveRestriction",this);
}
}else{
var _55=_49.parentNode;
while(_47===null){
if(_55[pos]!==null&&_55[pos].nodeType!==3){
_55=_55[pos];
}else{
if(pos==="previousSibling"){
_55=_55.parentNode.childNodes[_55.parentNode.childNodes.length-1];
}else{
_55=_55.parentNode.childNodes[_5("ie")?0:1];
}
}
_47=_55[_4e];
if(_47&&_47.style.display=="none"){
_4b=_47.parentNode.childNodes;
var _56=null;
if(pos=="previousSibling"){
for(i=_4b.length-1;i>=0;i--){
if(_4b[i].style.display!="none"){
_56=_4b[i];
break;
}
}
}else{
for(i=0;i<_4b.length;i++){
if(_4b[i].style.display!="none"){
_56=_4b[i];
break;
}
}
}
if(!_56){
_49=_47;
_55=_49.parentNode;
_47=null;
}else{
_47=_56;
}
}
}
_11.focus(_47);
}
break;
}
}
}
},destroy:function(){
var m=this._dragManager;
_b.forEach(this._grid,function(_57){
m.unregister(_57.node);
});
this.inherited(arguments);
}});
gcl.ChildWidgetProperties={column:"1",dragRestriction:false};
_c.extend(_13,gcl.ChildWidgetProperties);
return gcl;
});
