//>>built
require({cache:{"url:dojox/layout/resources/GridContainer.html":"<div id=\"${id}\" class=\"gridContainer\" dojoAttachPoint=\"containerNode\" tabIndex=\"0\" dojoAttachEvent=\"onkeypress:_selectFocus\">\n\t<div dojoAttachPoint=\"gridContainerDiv\">\n\t\t<table class=\"gridContainerTable\" dojoAttachPoint=\"gridContainerTable\" cellspacing=\"0\" cellpadding=\"0\">\n\t\t\t<tbody>\n\t\t\t\t<tr dojoAttachPoint=\"gridNode\" >\n\t\t\t\t\t\n\t\t\t\t</tr>\n\t\t\t</tbody>\n\t\t</table>\n\t</div>\n</div>"}});
define("dojox/layout/GridContainerLite",["dojo/_base/kernel","dojo/text!./resources/GridContainer.html","dojo/ready","dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dojo/text","dojo/_base/sniff","dojo/_base/html","dojox/mdnd/AreaManager","dojox/mdnd/DropIndicator","dojox/mdnd/dropMode/OverDropMode","dojox/mdnd/AutoScroll","dijit/_Templated","dijit/layout/_LayoutWidget","dijit/focus","dijit/_base/focus"],function(_1,_2){
var _3=_1.declare("dojox.layout.GridContainerLite",[dijit.layout._LayoutWidget,dijit._TemplatedMixin],{autoRefresh:true,templateString:_2,dragHandleClass:"dojoxDragHandle",nbZones:1,doLayout:true,isAutoOrganized:true,acceptTypes:[],colWidths:"",constructor:function(_4,_5){
this.acceptTypes=(_4||{}).acceptTypes||["text"];
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
this._border={"h":(_1.isIE)?_1._getBorderExtents(this.gridContainerTable).h:0,"w":(_1.isIE==6)?1:0};
}else{
_1.style(this.domNode,"overflowY","hidden");
_1.style(this.gridContainerTable,"height","auto");
}
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
if(this.isAutoOrganized){
this._organizeChildren();
}else{
this._organizeChildrenManually();
}
_1.forEach(this.getChildren(),function(_6){
_6.startup();
});
if(this._isShown()){
this.enableDnd();
}
this.inherited(arguments);
},resizeChildAfterDrop:function(_7,_8,_9){
if(this._disabled){
return false;
}
if(dijit.getEnclosingWidget(_8.node)==this){
var _a=dijit.byNode(_7);
if(_a.resize&&_1.isFunction(_a.resize)){
_a.resize();
}
_a.set("column",_7.parentNode.cellIndex);
if(this.doLayout){
var _b=this._contentBox.h,_c=_1.contentBox(this.gridContainerDiv).h;
if(_c>=_b){
_1.style(this.gridContainerTable,"height",(_b-this._border.h)+"px");
}
}
return true;
}
return false;
},resizeChildAfterDragStart:function(_d,_e,_f){
if(this._disabled){
return false;
}
if(dijit.getEnclosingWidget(_e.node)==this){
this._draggedNode=_d;
if(this.doLayout){
_1.marginBox(this.gridContainerTable,{"h":_1.contentBox(this.gridContainerDiv).h-this._border.h});
}
return true;
}
return false;
},getChildren:function(){
var _10=[];
_1.forEach(this._grid,function(_11){
_10=_10.concat(_1.query("> [widgetId]",_11.node).map(dijit.byNode));
});
return _10;
},_isShown:function(){
if("open" in this){
return this.open;
}else{
var _12=this.domNode;
return (_12.style.display!="none")&&(_12.style.visibility!="hidden")&&!_1.hasClass(_12,"dijitHidden");
}
},layout:function(){
if(this.doLayout){
var _13=this._contentBox;
_1.marginBox(this.gridContainerTable,{"h":_13.h-this._border.h});
_1.contentBox(this.domNode,{"w":_13.w-this._border.w});
}
_1.forEach(this.getChildren(),function(_14){
if(_14.resize&&_1.isFunction(_14.resize)){
_14.resize();
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
var _15=this.acceptTypes.join(","),i=0;
var _16=this.colWidths||[];
var _17=[];
var _18;
var _19=0;
for(i=0;i<this.nbZones;i++){
if(_17.length<_16.length){
_19+=_16[i];
_17.push(_16[i]);
}else{
if(!_18){
_18=(100-_19)/(this.nbZones-i);
}
_17.push(_18);
}
}
i=0;
while(i<this.nbZones){
this._grid.push({"node":_1.create("td",{"class":"gridContainerZone","accept":_15,"id":this.id+"_dz"+i,"style":{"width":_17[i]+"%"}},this.gridNode)});
i++;
}
},_getZonesAttr:function(){
return _1.query(".gridContainerZone",this.containerNode);
},enableDnd:function(){
var m=this._dragManager;
_1.forEach(this._grid,function(_1a){
m.registerByNode(_1a.node);
});
m._dropMode.updateAreas(m._areaList);
this._disabled=false;
},disableDnd:function(){
var m=this._dragManager;
_1.forEach(this._grid,function(_1b){
m.unregister(_1b.node);
});
m._dropMode.updateAreas(m._areaList);
this._disabled=true;
},_organizeChildren:function(){
var _1c=dojox.layout.GridContainerLite.superclass.getChildren.call(this);
var _1d=this.nbZones,_1e=Math.floor(_1c.length/_1d),mod=_1c.length%_1d,i=0;
for(var z=0;z<_1d;z++){
for(var r=0;r<_1e;r++){
this._insertChild(_1c[i],z);
i++;
}
if(mod>0){
try{
this._insertChild(_1c[i],z);
i++;
}
catch(e){
console.error("Unable to insert child in GridContainer",e);
}
mod--;
}else{
if(_1e===0){
break;
}
}
}
},_organizeChildrenManually:function(){
var _1f=dojox.layout.GridContainerLite.superclass.getChildren.call(this),_20=_1f.length,_21;
for(var i=0;i<_20;i++){
_21=_1f[i];
try{
this._insertChild(_21,_21.column-1);
}
catch(e){
console.error("Unable to insert child in GridContainer",e);
}
}
},_insertChild:function(_22,_23,p){
var _24=this._grid[_23].node,_25=_24.childNodes.length;
if(typeof (p)==undefined||p>_25){
p=_25;
}
if(this._disabled){
_1.place(_22.domNode,_24,p);
_1.attr(_22.domNode,"tabIndex","0");
}else{
if(!_22.dragRestriction){
this._dragManager.addDragItem(_24,_22.domNode,p,true);
}else{
_1.place(_22.domNode,_24,p);
_1.attr(_22.domNode,"tabIndex","0");
}
}
_22.set("column",_23);
return _22;
},removeChild:function(_26){
if(this._disabled){
this.inherited(arguments);
}else{
this._dragManager.removeDragItem(_26.domNode.parentNode,_26.domNode);
}
},addService:function(_27,_28,p){
_1.deprecated("addService is deprecated.","Please use  instead.","Future");
this.addChild(_27,_28,p);
},addChild:function(_29,_2a,p){
_29.domNode.id=_29.id;
dojox.layout.GridContainerLite.superclass.addChild.call(this,_29,0);
if(_2a<0||_2a==undefined){
_2a=0;
}
if(p<=0){
p=0;
}
try{
return this._insertChild(_29,_2a,p);
}
catch(e){
console.error("Unable to insert child in GridContainer",e);
}
return null;
},_setColWidthsAttr:function(_2b){
this.colWidths=_1.isString(_2b)?_2b.split(","):(_1.isArray(_2b)?_2b:[_2b]);
if(this._started){
this._updateColumnsWidth();
}
},_updateColumnsWidth:function(_2c){
var _2d=this._grid.length;
var _2e=this.colWidths||[];
var _2f=[];
var _30;
var _31=0;
var i;
for(i=0;i<_2d;i++){
if(_2f.length<_2e.length){
_31+=_2e[i]*1;
_2f.push(_2e[i]);
}else{
if(!_30){
_30=(100-_31)/(this.nbZones-i);
if(_30<0){
_30=100/this.nbZones;
}
}
_2f.push(_30);
_31+=_30*1;
}
}
if(_31>100){
var _32=100/_31;
for(i=0;i<_2f.length;i++){
_2f[i]*=_32;
}
}
for(i=0;i<_2d;i++){
this._grid[i].node.style.width=_2f[i]+"%";
}
},_selectFocus:function(_33){
if(this._disabled){
return;
}
var key=_33.keyCode,k=_1.keys,_34=null,_35=dijit.getFocus(),_36=_35.node,m=this._dragManager,_37,i,j,r,_38,_39,_3a;
if(_36==this.containerNode){
_39=this.gridNode.childNodes;
switch(key){
case k.DOWN_ARROW:
case k.RIGHT_ARROW:
_37=false;
for(i=0;i<_39.length;i++){
_38=_39[i].childNodes;
for(j=0;j<_38.length;j++){
_34=_38[j];
if(_34!=null&&_34.style.display!="none"){
dijit.focus(_34);
_1.stopEvent(_33);
_37=true;
break;
}
}
if(_37){
break;
}
}
break;
case k.UP_ARROW:
case k.LEFT_ARROW:
_39=this.gridNode.childNodes;
_37=false;
for(i=_39.length-1;i>=0;i--){
_38=_39[i].childNodes;
for(j=_38.length;j>=0;j--){
_34=_38[j];
if(_34!=null&&_34.style.display!="none"){
dijit.focus(_34);
_1.stopEvent(_33);
_37=true;
break;
}
}
if(_37){
break;
}
}
break;
}
}else{
if(_36.parentNode.parentNode==this.gridNode){
var _3b=(key==k.UP_ARROW||key==k.LEFT_ARROW)?"lastChild":"firstChild";
var pos=(key==k.UP_ARROW||key==k.LEFT_ARROW)?"previousSibling":"nextSibling";
switch(key){
case k.UP_ARROW:
case k.DOWN_ARROW:
_1.stopEvent(_33);
_37=false;
var _3c=_36;
while(!_37){
_38=_3c.parentNode.childNodes;
var num=0;
for(i=0;i<_38.length;i++){
if(_38[i].style.display!="none"){
num++;
}
if(num>1){
break;
}
}
if(num==1){
return;
}
if(_3c[pos]==null){
_34=_3c.parentNode[_3b];
}else{
_34=_3c[pos];
}
if(_34.style.display==="none"){
_3c=_34;
}else{
_37=true;
}
}
if(_33.shiftKey){
var _3d=_36.parentNode;
for(i=0;i<this.gridNode.childNodes.length;i++){
if(_3d==this.gridNode.childNodes[i]){
break;
}
}
_38=this.gridNode.childNodes[i].childNodes;
for(j=0;j<_38.length;j++){
if(_34==_38[j]){
break;
}
}
if(_1.isMoz||_1.isWebKit){
i--;
}
_3a=dijit.byNode(_36);
if(!_3a.dragRestriction){
r=m.removeDragItem(_3d,_36);
this.addChild(_3a,i,j);
_1.attr(_36,"tabIndex","0");
dijit.focus(_36);
}else{
_1.publish("/dojox/layout/gridContainer/moveRestriction",[this]);
}
}else{
dijit.focus(_34);
}
break;
case k.RIGHT_ARROW:
case k.LEFT_ARROW:
_1.stopEvent(_33);
if(_33.shiftKey){
var z=0;
if(_36.parentNode[pos]==null){
if(_1.isIE&&key==k.LEFT_ARROW){
z=this.gridNode.childNodes.length-1;
}
}else{
if(_36.parentNode[pos].nodeType==3){
z=this.gridNode.childNodes.length-2;
}else{
for(i=0;i<this.gridNode.childNodes.length;i++){
if(_36.parentNode[pos]==this.gridNode.childNodes[i]){
break;
}
z++;
}
if(_1.isMoz||_1.isWebKit){
z--;
}
}
}
_3a=dijit.byNode(_36);
var _3e=_36.getAttribute("dndtype");
if(_3e==null){
if(_3a&&_3a.dndType){
_3e=_3a.dndType.split(/\s*,\s*/);
}else{
_3e=["text"];
}
}else{
_3e=_3e.split(/\s*,\s*/);
}
var _3f=false;
for(i=0;i<this.acceptTypes.length;i++){
for(j=0;j<_3e.length;j++){
if(_3e[j]==this.acceptTypes[i]){
_3f=true;
break;
}
}
}
if(_3f&&!_3a.dragRestriction){
var _40=_36.parentNode,_41=0;
if(k.LEFT_ARROW==key){
var t=z;
if(_1.isMoz||_1.isWebKit){
t=z+1;
}
_41=this.gridNode.childNodes[t].childNodes.length;
}
r=m.removeDragItem(_40,_36);
this.addChild(_3a,z,_41);
_1.attr(r,"tabIndex","0");
dijit.focus(r);
}else{
_1.publish("/dojox/layout/gridContainer/moveRestriction",[this]);
}
}else{
var _42=_36.parentNode;
while(_34===null){
if(_42[pos]!==null&&_42[pos].nodeType!==3){
_42=_42[pos];
}else{
if(pos==="previousSibling"){
_42=_42.parentNode.childNodes[_42.parentNode.childNodes.length-1];
}else{
_42=(_1.isIE)?_42.parentNode.childNodes[0]:_42.parentNode.childNodes[1];
}
}
_34=_42[_3b];
if(_34&&_34.style.display=="none"){
_38=_34.parentNode.childNodes;
var _43=null;
if(pos=="previousSibling"){
for(i=_38.length-1;i>=0;i--){
if(_38[i].style.display!="none"){
_43=_38[i];
break;
}
}
}else{
for(i=0;i<_38.length;i++){
if(_38[i].style.display!="none"){
_43=_38[i];
break;
}
}
}
if(!_43){
_36=_34;
_42=_36.parentNode;
_34=null;
}else{
_34=_43;
}
}
}
dijit.focus(_34);
}
break;
}
}
}
},destroy:function(){
var m=this._dragManager;
_1.forEach(this._grid,function(_44){
m.unregister(_44.node);
});
this.inherited(arguments);
}});
_1.extend(dijit._Widget,{column:"1",dragRestriction:false});
return _3;
});
