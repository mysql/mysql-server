//>>built
define("dojox/layout/GridContainer",["dojo/_base/kernel","dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/html","dojo/_base/lang","dojo/_base/window","dojo/ready","dojox/layout/GridContainerLite"],function(_1){
return _1.declare("dojox.layout.GridContainer",dojox.layout.GridContainerLite,{hasResizableColumns:true,liveResizeColumns:false,minColWidth:20,minChildWidth:150,mode:"right",isRightFixed:false,isLeftFixed:false,startup:function(){
this.inherited(arguments);
if(this.hasResizableColumns){
for(var i=0;i<this._grid.length-1;i++){
this._createGrip(i);
}
if(!this.getParent()){
_1.ready(_1.hitch(this,"_placeGrips"));
}
}
},resizeChildAfterDrop:function(_2,_3,_4){
if(this.inherited(arguments)){
this._placeGrips();
}
},onShow:function(){
this.inherited(arguments);
this._placeGrips();
},resize:function(){
this.inherited(arguments);
if(this._isShown()&&this.hasResizableColumns){
this._placeGrips();
}
},_createGrip:function(_5){
var _6=this._grid[_5],_7=_1.create("div",{"class":"gridContainerGrip"},this.domNode);
_6.grip=_7;
_6.gripHandler=[this.connect(_7,"onmouseover",function(e){
var _8=false;
for(var i=0;i<this._grid.length-1;i++){
if(_1.hasClass(this._grid[i].grip,"gridContainerGripShow")){
_8=true;
break;
}
}
if(!_8){
_1.removeClass(e.target,"gridContainerGrip");
_1.addClass(e.target,"gridContainerGripShow");
}
})[0],this.connect(_7,"onmouseout",function(e){
if(!this._isResized){
_1.removeClass(e.target,"gridContainerGripShow");
_1.addClass(e.target,"gridContainerGrip");
}
})[0],this.connect(_7,"onmousedown","_resizeColumnOn")[0],this.connect(_7,"ondblclick","_onGripDbClick")[0]];
},_placeGrips:function(){
var _9,_a,_b=0,_c;
var _d=this.domNode.style.overflowY;
_1.forEach(this._grid,function(_e){
if(_e.grip){
_c=_e.grip;
if(!_9){
_9=_c.offsetWidth/2;
}
_b+=_1.marginBox(_e.node).w;
_1.style(_c,"left",(_b-_9)+"px");
if(!_a){
_a=_1.contentBox(this.gridNode).h;
}
if(_a>0){
_1.style(_c,"height",_a+"px");
}
}
},this);
},_onGripDbClick:function(){
this._updateColumnsWidth(this._dragManager);
this.resize();
},_resizeColumnOn:function(e){
this._activeGrip=e.target;
this._initX=e.pageX;
e.preventDefault();
_1.body().style.cursor="ew-resize";
this._isResized=true;
var _f=[];
var _10;
var i;
for(i=0;i<this._grid.length;i++){
_f[i]=_1.contentBox(this._grid[i].node).w;
}
this._oldTabSize=_f;
for(i=0;i<this._grid.length;i++){
_10=this._grid[i];
if(this._activeGrip==_10.grip){
this._currentColumn=_10.node;
this._currentColumnWidth=_f[i];
this._nextColumn=this._grid[i+1].node;
this._nextColumnWidth=_f[i+1];
}
_10.node.style.width=_f[i]+"px";
}
var _11=function(_12,_13){
var _14=0;
var _15=0;
_1.forEach(_12,function(_16){
if(_16.nodeType==1){
var _17=_1.getComputedStyle(_16);
var _18=(_1.isIE)?_13:parseInt(_17.minWidth);
_15=_18+parseInt(_17.marginLeft)+parseInt(_17.marginRight);
if(_14<_15){
_14=_15;
}
}
});
return _14;
};
var _19=_11(this._currentColumn.childNodes,this.minChildWidth);
var _1a=_11(this._nextColumn.childNodes,this.minChildWidth);
var _1b=Math.round((_1.marginBox(this.gridContainerTable).w*this.minColWidth)/100);
this._currentMinCol=_19;
this._nextMinCol=_1a;
if(_1b>this._currentMinCol){
this._currentMinCol=_1b;
}
if(_1b>this._nextMinCol){
this._nextMinCol=_1b;
}
this._connectResizeColumnMove=_1.connect(_1.doc,"onmousemove",this,"_resizeColumnMove");
this._connectOnGripMouseUp=_1.connect(_1.doc,"onmouseup",this,"_onGripMouseUp");
},_onGripMouseUp:function(){
_1.body().style.cursor="default";
_1.disconnect(this._connectResizeColumnMove);
_1.disconnect(this._connectOnGripMouseUp);
this._connectOnGripMouseUp=this._connectResizeColumnMove=null;
if(this._activeGrip){
_1.removeClass(this._activeGrip,"gridContainerGripShow");
_1.addClass(this._activeGrip,"gridContainerGrip");
}
this._isResized=false;
},_resizeColumnMove:function(e){
e.preventDefault();
if(!this._connectResizeColumnOff){
_1.disconnect(this._connectOnGripMouseUp);
this._connectOnGripMouseUp=null;
this._connectResizeColumnOff=_1.connect(_1.doc,"onmouseup",this,"_resizeColumnOff");
}
var d=e.pageX-this._initX;
if(d==0){
return;
}
if(!(this._currentColumnWidth+d<this._currentMinCol||this._nextColumnWidth-d<this._nextMinCol)){
this._currentColumnWidth+=d;
this._nextColumnWidth-=d;
this._initX=e.pageX;
this._activeGrip.style.left=parseInt(this._activeGrip.style.left)+d+"px";
if(this.liveResizeColumns){
this._currentColumn.style["width"]=this._currentColumnWidth+"px";
this._nextColumn.style["width"]=this._nextColumnWidth+"px";
this.resize();
}
}
},_resizeColumnOff:function(e){
_1.body().style.cursor="default";
_1.disconnect(this._connectResizeColumnMove);
_1.disconnect(this._connectResizeColumnOff);
this._connectResizeColumnOff=this._connectResizeColumnMove=null;
if(!this.liveResizeColumns){
this._currentColumn.style["width"]=this._currentColumnWidth+"px";
this._nextColumn.style["width"]=this._nextColumnWidth+"px";
}
var _1c=[],_1d=[],_1e=this.gridContainerTable.clientWidth,_1f,_20=false,i;
for(i=0;i<this._grid.length;i++){
_1f=this._grid[i].node;
if(_1.isIE){
_1c[i]=_1.marginBox(_1f).w;
_1d[i]=_1.contentBox(_1f).w;
}else{
_1c[i]=_1.contentBox(_1f).w;
_1d=_1c;
}
}
for(i=0;i<_1d.length;i++){
if(_1d[i]!=this._oldTabSize[i]){
_20=true;
break;
}
}
if(_20){
var mul=_1.isIE?100:10000;
for(i=0;i<this._grid.length;i++){
this._grid[i].node.style.width=Math.round((100*mul*_1c[i])/_1e)/mul+"%";
}
this.resize();
}
if(this._activeGrip){
_1.removeClass(this._activeGrip,"gridContainerGripShow");
_1.addClass(this._activeGrip,"gridContainerGrip");
}
this._isResized=false;
},setColumns:function(_21){
var z,j;
if(_21>0){
var _22=this._grid.length,_23=_22-_21;
if(_23>0){
var _24=[],_25,_26,end,_27;
if(this.mode=="right"){
end=(this.isLeftFixed&&_22>0)?1:0;
_26=(this.isRightFixed)?_22-2:_22-1;
for(z=_26;z>=end;z--){
_27=0;
_25=this._grid[z].node;
for(j=0;j<_25.childNodes.length;j++){
if(_25.childNodes[j].nodeType==1&&!(_25.childNodes[j].id=="")){
_27++;
break;
}
}
if(_27==0){
_24[_24.length]=z;
}
if(_24.length>=_23){
this._deleteColumn(_24);
break;
}
}
if(_24.length<_23){
_1.publish("/dojox/layout/gridContainer/noEmptyColumn",[this]);
}
}else{
_26=(this.isLeftFixed&&_22>0)?1:0;
end=(this.isRightFixed)?_22-1:_22;
for(z=_26;z<end;z++){
_27=0;
_25=this._grid[z].node;
for(j=0;j<_25.childNodes.length;j++){
if(_25.childNodes[j].nodeType==1&&!(_25.childNodes[j].id=="")){
_27++;
break;
}
}
if(_27==0){
_24[_24.length]=z;
}
if(_24.length>=_23){
this._deleteColumn(_24);
break;
}
}
if(_24.length<_23){
_1.publish("/dojox/layout/gridContainer/noEmptyColumn",[this]);
}
}
}else{
if(_23<0){
this._addColumn(Math.abs(_23));
}
}
if(this.hasResizableColumns){
this._placeGrips();
}
}
},_addColumn:function(_28){
var _29=this._grid,_2a,_2b,_2c,_2d,_2e=(this.mode=="right"),_2f=this.acceptTypes.join(","),m=this._dragManager;
if(this.hasResizableColumns&&((!this.isRightFixed&&_2e)||(this.isLeftFixed&&!_2e&&this.nbZones==1))){
this._createGrip(_29.length-1);
}
for(var i=0;i<_28;i++){
_2b=_1.create("td",{"class":"gridContainerZone dojoxDndArea","accept":_2f,"id":this.id+"_dz"+this.nbZones});
_2d=_29.length;
if(_2e){
if(this.isRightFixed){
_2c=_2d-1;
_29.splice(_2c,0,{"node":_29[_2c].node.parentNode.insertBefore(_2b,_29[_2c].node)});
}else{
_2c=_2d;
_29.push({"node":this.gridNode.appendChild(_2b)});
}
}else{
if(this.isLeftFixed){
_2c=(_2d==1)?0:1;
this._grid.splice(1,0,{"node":this._grid[_2c].node.parentNode.appendChild(_2b,this._grid[_2c].node)});
_2c=1;
}else{
_2c=_2d-this.nbZones;
this._grid.splice(_2c,0,{"node":_29[_2c].node.parentNode.insertBefore(_2b,_29[_2c].node)});
}
}
if(this.hasResizableColumns){
if((!_2e&&this.nbZones!=1)||(!_2e&&this.nbZones==1&&!this.isLeftFixed)||(_2e&&i<_28-1)||(_2e&&i==_28-1&&this.isRightFixed)){
this._createGrip(_2c);
}
}
m.registerByNode(_29[_2c].node);
this.nbZones++;
}
this._updateColumnsWidth(m);
},_deleteColumn:function(_30){
var _31,_32,_33,_34=0,_35=_30.length,m=this._dragManager;
for(var i=0;i<_35;i++){
_33=(this.mode=="right")?_30[i]:_30[i]-_34;
_32=this._grid[_33];
if(this.hasResizableColumns&&_32.grip){
_1.forEach(_32.gripHandler,function(_36){
_1.disconnect(_36);
});
_1.destroy(this.domNode.removeChild(_32.grip));
_32.grip=null;
}
m.unregister(_32.node);
_1.destroy(this.gridNode.removeChild(_32.node));
this._grid.splice(_33,1);
this.nbZones--;
_34++;
}
var _37=this._grid[this.nbZones-1];
if(_37.grip){
_1.forEach(_37.gripHandler,_1.disconnect);
_1.destroy(this.domNode.removeChild(_37.grip));
_37.grip=null;
}
this._updateColumnsWidth(m);
},_updateColumnsWidth:function(_38){
this.inherited(arguments);
_38._dropMode.updateAreas(_38._areaList);
},destroy:function(){
_1.unsubscribe(this._dropHandler);
this.inherited(arguments);
}});
});
