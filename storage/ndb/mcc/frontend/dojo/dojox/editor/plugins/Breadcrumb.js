//>>built
define("dojox/editor/plugins/Breadcrumb",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/Toolbar","dijit/Menu","dijit/MenuItem","dijit/MenuSeparator","dijit/_editor/range","dijit/_editor/selection","dijit/_editor/_Plugin","dijit/form/Button","dijit/form/ComboButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojo/i18n!dojox/editor/plugins/nls/Breadcrumb"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins.Breadcrumb");
_1.declare("dojox.editor.plugins._BreadcrumbMenuTitle",[_2._Widget,_2._TemplatedMixin,_2._Contained],{templateString:"<tr><td dojoAttachPoint=\"title\" colspan=\"4\" class=\"dijitToolbar\" style=\"font-weight: bold; padding: 3px;\"></td></tr>",menuTitle:"",postCreate:function(){
_1.setSelectable(this.domNode,false);
var _4=this.id+"_text";
this.domNode.setAttribute("aria-labelledby",_4);
},_setMenuTitleAttr:function(_5){
this.title.innerHTML=_5;
},_getMenuTitleAttr:function(_6){
return this.title.innerHTML;
}});
_1.declare("dojox.editor.plugins.Breadcrumb",_2._editor._Plugin,{_menu:null,breadcrumbBar:null,setEditor:function(_7){
this.editor=_7;
this._buttons=[];
this.breadcrumbBar=new _2.Toolbar();
var _8=_1.i18n.getLocalization("dojox.editor.plugins","Breadcrumb");
this._titleTemplate=_8.nodeActions;
_1.place(this.breadcrumbBar.domNode,_7.footer);
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
this._menu=new _2.Menu({});
_1.addClass(this.breadcrumbBar.domNode,"dojoxEditorBreadcrumbArrow");
var _9=this;
var _a=new _2.form.ComboButton({showLabel:true,label:"body",_selNode:_7.editNode,dropDown:this._menu,onClick:_1.hitch(this,function(){
this._menuTarget=_7.editNode;
this._selectContents();
})});
this._menuTitle=new _3.editor.plugins._BreadcrumbMenuTitle({menuTitle:_8.nodeActions});
this._selCMenu=new _2.MenuItem({label:_8.selectContents,onClick:_1.hitch(this,this._selectContents)});
this._delCMenu=new _2.MenuItem({label:_8.deleteContents,onClick:_1.hitch(this,this._deleteContents)});
this._selEMenu=new _2.MenuItem({label:_8.selectElement,onClick:_1.hitch(this,this._selectElement)});
this._delEMenu=new _2.MenuItem({label:_8.deleteElement,onClick:_1.hitch(this,this._deleteElement)});
this._moveSMenu=new _2.MenuItem({label:_8.moveStart,onClick:_1.hitch(this,this._moveCToStart)});
this._moveEMenu=new _2.MenuItem({label:_8.moveEnd,onClick:_1.hitch(this,this._moveCToEnd)});
this._menu.addChild(this._menuTitle);
this._menu.addChild(this._selCMenu);
this._menu.addChild(this._delCMenu);
this._menu.addChild(new _2.MenuSeparator({}));
this._menu.addChild(this._selEMenu);
this._menu.addChild(this._delEMenu);
this._menu.addChild(new _2.MenuSeparator({}));
this._menu.addChild(this._moveSMenu);
this._menu.addChild(this._moveEMenu);
_a._ddConnect=_1.connect(_a,"openDropDown",_1.hitch(this,function(){
this._menuTarget=_a._selNode;
this._menuTitle.set("menuTitle",_1.string.substitute(this._titleTemplate,{"nodeName":"&lt;body&gt;"}));
this._selEMenu.set("disabled",true);
this._delEMenu.set("disabled",true);
this._selCMenu.set("disabled",false);
this._delCMenu.set("disabled",false);
this._moveSMenu.set("disabled",false);
this._moveEMenu.set("disabled",false);
}));
this.breadcrumbBar.addChild(_a);
this.connect(this.editor,"onNormalizedDisplayChanged","updateState");
}));
this.breadcrumbBar.startup();
if(_1.isIE){
setTimeout(_1.hitch(this,function(){
this.breadcrumbBar.domNode.className=this.breadcrumbBar.domNode.className;
}),100);
}
},_selectContents:function(){
this.editor.focus();
if(this._menuTarget){
var _b=this._menuTarget.tagName.toLowerCase();
switch(_b){
case "br":
case "hr":
case "img":
case "input":
case "base":
case "meta":
case "area":
case "basefont":
break;
default:
try{
_1.withGlobal(this.editor.window,"collapse",_2._editor.selection,[null]);
_1.withGlobal(this.editor.window,"selectElementChildren",_2._editor.selection,[this._menuTarget]);
this.editor.onDisplayChanged();
}
catch(e){
}
}
}
},_deleteContents:function(){
if(this._menuTarget){
this.editor.beginEditing();
this._selectContents();
_1.withGlobal(this.editor.window,"remove",_2._editor.selection,[this._menuTarget]);
this.editor.endEditing();
this._updateBreadcrumb();
this.editor.onDisplayChanged();
}
},_selectElement:function(){
this.editor.focus();
if(this._menuTarget){
_1.withGlobal(this.editor.window,"collapse",_2._editor.selection,[null]);
_1.withGlobal(this.editor.window,"selectElement",_2._editor.selection,[this._menuTarget]);
this.editor.onDisplayChanged();
}
},_deleteElement:function(){
if(this._menuTarget){
this.editor.beginEditing();
this._selectElement();
_1.withGlobal(this.editor.window,"remove",_2._editor.selection,[this._menuTarget]);
this.editor.endEditing();
this._updateBreadcrumb();
this.editor.onDisplayChanged();
}
},_moveCToStart:function(){
this.editor.focus();
if(this._menuTarget){
this._selectContents();
_1.withGlobal(this.editor.window,"collapse",_2._editor.selection,[true]);
}
},_moveCToEnd:function(){
this.editor.focus();
if(this._menuTarget){
this._selectContents();
_1.withGlobal(this.editor.window,"collapse",_2._editor.selection,[false]);
}
},_updateBreadcrumb:function(){
var ed=this.editor;
if(ed.window){
var _c=_2.range.getSelection(ed.window);
if(_c&&_c.rangeCount>0){
var _d=_c.getRangeAt(0);
var _e=_1.withGlobal(ed.window,"getSelectedElement",_2._editor.selection)||_d.startContainer;
var _f=[];
if(_e&&_e.ownerDocument===ed.document){
while(_e&&_e!==ed.editNode&&_e!=ed.document.body&&_e!=ed.document){
if(_e.nodeType===1){
_f.push({type:_e.tagName.toLowerCase(),node:_e});
}
_e=_e.parentNode;
}
_f=_f.reverse();
while(this._buttons.length){
var db=this._buttons.pop();
_1.disconnect(db._ddConnect);
this.breadcrumbBar.removeChild(db);
}
this._buttons=[];
var i;
var _10=this;
for(i=0;i<_f.length;i++){
var bc=_f[i];
var b=new _2.form.ComboButton({showLabel:true,label:bc.type,_selNode:bc.node,dropDown:this._menu,onClick:function(){
_10._menuTarget=this._selNode;
_10._selectContents();
}});
b._ddConnect=_1.connect(b,"openDropDown",_1.hitch(b,function(){
_10._menuTarget=this._selNode;
var _11=_10._menuTarget.tagName.toLowerCase();
var _12=_1.string.substitute(_10._titleTemplate,{"nodeName":"&lt;"+_11+"&gt;"});
_10._menuTitle.set("menuTitle",_12);
switch(_11){
case "br":
case "hr":
case "img":
case "input":
case "base":
case "meta":
case "area":
case "basefont":
_10._selCMenu.set("disabled",true);
_10._delCMenu.set("disabled",true);
_10._moveSMenu.set("disabled",true);
_10._moveEMenu.set("disabled",true);
_10._selEMenu.set("disabled",false);
_10._delEMenu.set("disabled",false);
break;
default:
_10._selCMenu.set("disabled",false);
_10._delCMenu.set("disabled",false);
_10._selEMenu.set("disabled",false);
_10._delEMenu.set("disabled",false);
_10._moveSMenu.set("disabled",false);
_10._moveEMenu.set("disabled",false);
}
}));
this._buttons.push(b);
this.breadcrumbBar.addChild(b);
}
if(_1.isIE){
this.breadcrumbBar.domNode.className=this.breadcrumbBar.domNode.className;
}
}
}
}
},updateState:function(){
if(_1.style(this.editor.iframe,"display")==="none"||this.get("disabled")){
_1.style(this.breadcrumbBar.domNode,"display","none");
}else{
if(_1.style(this.breadcrumbBar.domNode,"display")==="none"){
_1.style(this.breadcrumbBar.domNode,"display","block");
}
this._updateBreadcrumb();
var _13=_1.marginBox(this.editor.domNode);
this.editor.resize({h:_13.h});
}
},destroy:function(){
if(this.breadcrumbBar){
this.breadcrumbBar.destroyRecursive();
this.breadcrumbBar=null;
}
if(this._menu){
this._menu.destroyRecursive();
delete this._menu;
}
this._buttons=null;
delete this.editor.breadcrumbBar;
this.inherited(arguments);
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _14=o.args.name.toLowerCase();
if(_14==="breadcrumb"){
o.plugin=new _3.editor.plugins.Breadcrumb({});
}
});
return _3.editor.plugins.Breadcrumb;
});
