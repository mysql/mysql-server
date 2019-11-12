//>>built
require({cache:{"url:dojox/layout/resources/GridContainer.html":"<div id=\"${id}\" class=\"gridContainer\" dojoAttachPoint=\"containerNode\" tabIndex=\"0\" dojoAttachEvent=\"onkeypress:_selectFocus\">\n\t<div dojoAttachPoint=\"gridContainerDiv\">\n\t\t<table class=\"gridContainerTable\" dojoAttachPoint=\"gridContainerTable\" cellspacing=\"0\" cellpadding=\"0\">\n\t\t\t<tbody>\n\t\t\t\t<tr dojoAttachPoint=\"gridNode\" >\n\t\t\t\t\t\n\t\t\t\t</tr>\n\t\t\t</tbody>\n\t\t</table>\n\t</div>\n</div>"}});
define("dojox/layout/GridContainerLite",["dojo/_base/kernel","dojo/text!./resources/GridContainer.html","dojo/_base/declare","dojo/query","dojo/_base/sniff","dojo/dom-class","dojo/dom-style","dojo/dom-geometry","dojo/dom-construct","dojo/dom-attr","dojo/_base/array","dojo/_base/lang","dojo/_base/event","dojo/keys","dojo/topic","dijit/registry","dijit/focus","dijit/_base/focus","dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/layout/_LayoutWidget","dojo/_base/NodeList","dojox/mdnd/AreaManager","dojox/mdnd/DropIndicator","dojox/mdnd/dropMode/OverDropMode","dojox/mdnd/AutoScroll"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16){
var gcl=_3("dojox.layout.GridContainerLite",[_15,_14],{autoRefresh:true,templateString:_2,dragHandleClass:"dojoxDragHandle",nbZones:1,doLayout:true,isAutoOrganized:true,acceptTypes:[],colWidths:"",constructor:function(_17,_18){
this.acceptTypes=(_17||{}).acceptTypes||["text"];
this._disabled=true;
},postCreate:function(){
this.inherited(arguments);
this._grid=[];
this._createCells();
this.subscribe("/dojox/mdnd/drop","resizeChildAfterDrop");
this.subscribe("/dojox/mdnd/drag/start","resizeChildAfterDragStart");
this._dragManager=dojox.mdnd.areaManager();
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
_b.forEach(this.getChildren(),function(_19){
_19.startup();
});
if(this._isShown()){
this.enableDnd();
}
this.inherited(arguments);
},resizeChildAfterDrop:function(_1a,_1b,_1c){
if(this._disabled){
return false;
}
if(_10.getEnclosingWidget(_1b.node)==this){
var _1d=_10.byNode(_1a);
if(_1d.resize&&_c.isFunction(_1d.resize)){
_1d.resize();
}
_1d.set("column",_1a.parentNode.cellIndex);
if(this.doLayout){
var _1e=this._contentBox.h,_1f=_8.getContentBox(this.gridContainerDiv).h;
if(_1f>=_1e){
_7.set(this.gridContainerTable,"height",(_1e-this._border.h)+"px");
}
}
return true;
}
return false;
},resizeChildAfterDragStart:function(_20,_21,_22){
if(this._disabled){
return false;
}
if(_10.getEnclosingWidget(_21.node)==this){
this._draggedNode=_20;
if(this.doLayout){
_8.getMarginBox(this.gridContainerTable,{h:_8.getContentBox(this.gridContainerDiv).h-this._border.h});
}
return true;
}
return false;
},getChildren:function(){
var _23=new _16();
_b.forEach(this._grid,function(_24){
_4("> [widgetId]",_24.node).map(_10.byNode).forEach(function(_25){
_23.push(_25);
});
});
return _23;
},_isShown:function(){
if("open" in this){
return this.open;
}else{
var _26=this.domNode;
return (_26.style.display!="none")&&(_26.style.visibility!="hidden")&&!_6.contains(_26,"dijitHidden");
}
},layout:function(){
if(this.doLayout){
var _27=this._contentBox;
_8.getMarginBox(this.gridContainerTable,{h:_27.h-this._border.h});
_8.getContentBox(this.domNode,{w:_27.w-this._border.w});
}
_b.forEach(this.getChildren(),function(_28){
if(_28.resize&&_c.isFunction(_28.resize)){
_28.resize();
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
var _29=this.acceptTypes.join(","),i=0;
var _2a=this.colWidths||[];
var _2b=[];
var _2c;
var _2d=0;
for(i=0;i<this.nbZones;i++){
if(_2b.length<_2a.length){
_2d+=_2a[i];
_2b.push(_2a[i]);
}else{
if(!_2c){
_2c=(100-_2d)/(this.nbZones-i);
}
_2b.push(_2c);
}
}
i=0;
while(i<this.nbZones){
this._grid.push({node:_9.create("td",{"class":"gridContainerZone",accept:_29,id:this.id+"_dz"+i,style:{"width":_2b[i]+"%"}},this.gridNode)});
i++;
}
},_getZonesAttr:function(){
return _4(".gridContainerZone",this.containerNode);
},enableDnd:function(){
var m=this._dragManager;
_b.forEach(this._grid,function(_2e){
m.registerByNode(_2e.node);
});
m._dropMode.updateAreas(m._areaList);
this._disabled=false;
},disableDnd:function(){
var m=this._dragManager;
_b.forEach(this._grid,function(_2f){
m.unregister(_2f.node);
});
m._dropMode.updateAreas(m._areaList);
this._disabled=true;
},_organizeChildren:function(){
var _30=dojox.layout.GridContainerLite.superclass.getChildren.call(this);
var _31=this.nbZones,_32=Math.floor(_30.length/_31),mod=_30.length%_31,i=0;
for(var z=0;z<_31;z++){
for(var r=0;r<_32;r++){
this._insertChild(_30[i],z);
i++;
}
if(mod>0){
try{
this._insertChild(_30[i],z);
i++;
}
catch(e){
console.error("Unable to insert child in GridContainer",e);
}
mod--;
}else{
if(_32===0){
break;
}
}
}
},_organizeChildrenManually:function(){
var _33=dojox.layout.GridContainerLite.superclass.getChildren.call(this),_34=_33.length,_35;
for(var i=0;i<_34;i++){
_35=_33[i];
try{
this._insertChild(_35,_35.column-1);
}
catch(e){
console.error("Unable to insert child in GridContainer",e);
}
}
},_insertChild:function(_36,_37,p){
var _38=this._grid[_37].node,_39=_38.childNodes.length;
if(typeof p==="undefined"||p>_39){
p=_39;
}
if(this._disabled){
_9.place(_36.domNode,_38,p);
_a.set(_36.domNode,"tabIndex","0");
}else{
if(!_36.dragRestriction){
this._dragManager.addDragItem(_38,_36.domNode,p,true);
}else{
_9.place(_36.domNode,_38,p);
_a.set(_36.domNode,"tabIndex","0");
}
}
_36.set("column",_37);
return _36;
},removeChild:function(_3a){
if(this._disabled){
this.inherited(arguments);
}else{
this._dragManager.removeDragItem(_3a.domNode.parentNode,_3a.domNode);
}
},addService:function(_3b,_3c,p){
kernel.deprecated("addService is deprecated.","Please use  instead.","Future");
this.addChild(_3b,_3c,p);
},addChild:function(_3d,_3e,p){
_3d.domNode.id=_3d.id;
dojox.layout.GridContainerLite.superclass.addChild.call(this,_3d,0);
if(_3e<0||_3e==undefined){
_3e=0;
}
if(p<=0){
p=0;
}
try{
return this._insertChild(_3d,_3e,p);
}
catch(e){
console.error("Unable to insert child in GridContainer",e);
}
return null;
},_setColWidthsAttr:function(_3f){
this.colWidths=_c.isString(_3f)?_3f.split(","):(_c.isArray(_3f)?_3f:[_3f]);
if(this._started){
this._updateColumnsWidth();
}
},_updateColumnsWidth:function(_40){
var _41=this._grid.length;
var _42=this.colWidths||[];
var _43=[];
var _44;
var _45=0;
var i;
for(i=0;i<_41;i++){
if(_43.length<_42.length){
_45+=_42[i]*1;
_43.push(_42[i]);
}else{
if(!_44){
_44=(100-_45)/(this.nbZones-i);
if(_44<0){
_44=100/this.nbZones;
}
}
_43.push(_44);
_45+=_44*1;
}
}
if(_45>100){
var _46=100/_45;
for(i=0;i<_43.length;i++){
_43[i]*=_46;
}
}
for(i=0;i<_41;i++){
this._grid[i].node.style.width=_43[i]+"%";
}
},_selectFocus:function(_47){
if(this._disabled){
return;
}
var key=_47.keyCode,k=_e,_48=null,_49=_12.getFocus(),_4a=_49.node,m=this._dragManager,_4b,i,j,r,_4c,_4d,_4e;
if(_4a==this.containerNode){
_4d=this.gridNode.childNodes;
switch(key){
case k.DOWN_ARROW:
case k.RIGHT_ARROW:
_4b=false;
for(i=0;i<_4d.length;i++){
_4c=_4d[i].childNodes;
for(j=0;j<_4c.length;j++){
_48=_4c[j];
if(_48!=null&&_48.style.display!="none"){
_11.focus(_48);
_d.stop(_47);
_4b=true;
break;
}
}
if(_4b){
break;
}
}
break;
case k.UP_ARROW:
case k.LEFT_ARROW:
_4d=this.gridNode.childNodes;
_4b=false;
for(i=_4d.length-1;i>=0;i--){
_4c=_4d[i].childNodes;
for(j=_4c.length;j>=0;j--){
_48=_4c[j];
if(_48!=null&&_48.style.display!="none"){
_11.focus(_48);
_d.stop(_47);
_4b=true;
break;
}
}
if(_4b){
break;
}
}
break;
}
}else{
if(_4a.parentNode.parentNode==this.gridNode){
var _4f=(key==k.UP_ARROW||key==k.LEFT_ARROW)?"lastChild":"firstChild";
var pos=(key==k.UP_ARROW||key==k.LEFT_ARROW)?"previousSibling":"nextSibling";
switch(key){
case k.UP_ARROW:
case k.DOWN_ARROW:
_d.stop(_47);
_4b=false;
var _50=_4a;
while(!_4b){
_4c=_50.parentNode.childNodes;
var num=0;
for(i=0;i<_4c.length;i++){
if(_4c[i].style.display!="none"){
num++;
}
if(num>1){
break;
}
}
if(num==1){
return;
}
if(_50[pos]==null){
_48=_50.parentNode[_4f];
}else{
_48=_50[pos];
}
if(_48.style.display==="none"){
_50=_48;
}else{
_4b=true;
}
}
if(_47.shiftKey){
var _51=_4a.parentNode;
for(i=0;i<this.gridNode.childNodes.length;i++){
if(_51==this.gridNode.childNodes[i]){
break;
}
}
_4c=this.gridNode.childNodes[i].childNodes;
for(j=0;j<_4c.length;j++){
if(_48==_4c[j]){
break;
}
}
if(_5("mozilla")||_5("webkit")){
i--;
}
_4e=_10.byNode(_4a);
if(!_4e.dragRestriction){
r=m.removeDragItem(_51,_4a);
this.addChild(_4e,i,j);
_a.set(_4a,"tabIndex","0");
_11.focus(_4a);
}else{
_f.publish("/dojox/layout/gridContainer/moveRestriction",this);
}
}else{
_11.focus(_48);
}
break;
case k.RIGHT_ARROW:
case k.LEFT_ARROW:
_d.stop(_47);
if(_47.shiftKey){
var z=0;
if(_4a.parentNode[pos]==null){
if(_5("ie")&&key==k.LEFT_ARROW){
z=this.gridNode.childNodes.length-1;
}
}else{
if(_4a.parentNode[pos].nodeType==3){
z=this.gridNode.childNodes.length-2;
}else{
for(i=0;i<this.gridNode.childNodes.length;i++){
if(_4a.parentNode[pos]==this.gridNode.childNodes[i]){
break;
}
z++;
}
if(_5("mozilla")||_5("webkit")){
z--;
}
}
}
_4e=_10.byNode(_4a);
var _52=_4a.getAttribute("dndtype");
if(_52==null){
if(_4e&&_4e.dndType){
_52=_4e.dndType.split(/\s*,\s*/);
}else{
_52=["text"];
}
}else{
_52=_52.split(/\s*,\s*/);
}
var _53=false;
for(i=0;i<this.acceptTypes.length;i++){
for(j=0;j<_52.length;j++){
if(_52[j]==this.acceptTypes[i]){
_53=true;
break;
}
}
}
if(_53&&!_4e.dragRestriction){
var _54=_4a.parentNode,_55=0;
if(k.LEFT_ARROW==key){
var t=z;
if(_5("mozilla")||_5("webkit")){
t=z+1;
}
_55=this.gridNode.childNodes[t].childNodes.length;
}
r=m.removeDragItem(_54,_4a);
this.addChild(_4e,z,_55);
_a.set(r,"tabIndex","0");
_11.focus(r);
}else{
_f.publish("/dojox/layout/gridContainer/moveRestriction",this);
}
}else{
var _56=_4a.parentNode;
while(_48===null){
if(_56[pos]!==null&&_56[pos].nodeType!==3){
_56=_56[pos];
}else{
if(pos==="previousSibling"){
_56=_56.parentNode.childNodes[_56.parentNode.childNodes.length-1];
}else{
_56=_56.parentNode.childNodes[_5("ie")?0:1];
}
}
_48=_56[_4f];
if(_48&&_48.style.display=="none"){
_4c=_48.parentNode.childNodes;
var _57=null;
if(pos=="previousSibling"){
for(i=_4c.length-1;i>=0;i--){
if(_4c[i].style.display!="none"){
_57=_4c[i];
break;
}
}
}else{
for(i=0;i<_4c.length;i++){
if(_4c[i].style.display!="none"){
_57=_4c[i];
break;
}
}
}
if(!_57){
_4a=_48;
_56=_4a.parentNode;
_48=null;
}else{
_48=_57;
}
}
}
_11.focus(_48);
}
break;
}
}
}
},destroy:function(){
var m=this._dragManager;
_b.forEach(this._grid,function(_58){
m.unregister(_58.node);
});
this.inherited(arguments);
}});
gcl.ChildWidgetProperties={column:"1",dragRestriction:false};
_c.extend(_13,gcl.ChildWidgetProperties);
return gcl;
});
