//>>built
define("dojox/grid/enhanced/plugins/Menu",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/html","dojo/_base/event","dojo/keys","../_Plugin","../../EnhancedGrid"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_1("dojox.grid.enhanced.plugins.Menu",_7,{name:"menus",types:["headerMenu","rowMenu","cellMenu","selectedRegionMenu"],constructor:function(){
var g=this.grid;
g.showMenu=_3.hitch(g,this.showMenu);
g._setRowMenuAttr=_3.hitch(this,"_setRowMenuAttr");
g._setCellMenuAttr=_3.hitch(this,"_setCellMenuAttr");
g._setSelectedRegionMenuAttr=_3.hitch(this,"_setSelectedRegionMenuAttr");
},onStartUp:function(){
var _a,_b=this.option;
for(_a in _b){
if(_2.indexOf(this.types,_a)>=0&&_b[_a]){
this._initMenu(_a,_b[_a]);
}
}
},_initMenu:function(_c,_d){
var g=this.grid;
if(!g[_c]){
var m=this._getMenuWidget(_d);
if(!m){
return;
}
g.set(_c,m);
if(_c!="headerMenu"){
m._scheduleOpen=function(){
return;
};
}else{
g.setupHeaderMenu();
}
}
},_getMenuWidget:function(_e){
return (_e instanceof dijit.Menu)?_e:dijit.byId(_e);
},_setRowMenuAttr:function(_f){
this._setMenuAttr(_f,"rowMenu");
},_setCellMenuAttr:function(_10){
this._setMenuAttr(_10,"cellMenu");
},_setSelectedRegionMenuAttr:function(_11){
this._setMenuAttr(_11,"selectedRegionMenu");
},_setMenuAttr:function(_12,_13){
var g=this.grid,n=g.domNode;
if(!_12||!(_12 instanceof dijit.Menu)){
console.warn(_13," of Grid ",g.id," is not existed!");
return;
}
if(g[_13]){
g[_13].unBindDomNode(n);
}
g[_13]=_12;
g[_13].bindDomNode(n);
},showMenu:function(e){
var _14=(e.cellNode&&_4.hasClass(e.cellNode,"dojoxGridRowSelected")||e.rowNode&&(_4.hasClass(e.rowNode,"dojoxGridRowSelected")||_4.hasClass(e.rowNode,"dojoxGridRowbarSelected")));
if(_14&&this.selectedRegionMenu){
this.onSelectedRegionContextMenu(e);
return;
}
var _15={target:e.target,coords:e.keyCode!==_6.F10&&"pageX" in e?{x:e.pageX,y:e.pageY}:null};
if(this.rowMenu&&(!this.cellMenu||this.selection.isSelected(e.rowIndex)||e.rowNode&&_4.hasClass(e.rowNode,"dojoxGridRowbar"))){
this.rowMenu._openMyself(_15);
_5.stop(e);
return;
}
if(this.cellMenu){
this.cellMenu._openMyself(_15);
}
_5.stop(e);
},destroy:function(){
var g=this.grid;
if(g.headerMenu){
g.headerMenu.unBindDomNode(g.viewsHeaderNode);
}
if(g.rowMenu){
g.rowMenu.unBindDomNode(g.domNode);
}
if(g.cellMenu){
g.cellMenu.unBindDomNode(g.domNode);
}
if(g.selectedRegionMenu){
g.selectedRegionMenu.destroy();
}
this.inherited(arguments);
}});
_8.registerPlugin(_9);
return _9;
});
