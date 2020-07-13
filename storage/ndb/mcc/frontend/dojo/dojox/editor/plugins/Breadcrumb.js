//>>built
define("dojox/editor/plugins/Breadcrumb",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_Contained","dijit/Toolbar","dijit/Menu","dijit/MenuItem","dijit/MenuSeparator","dijit/_editor/_Plugin","dijit/form/Button","dijit/form/ComboButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/string","dojo/i18n!dojox/editor/plugins/nls/Breadcrumb"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
_1.experimental("dojox.editor.plugins.Breadcrumb");
var _c=_1.declare("dojox.editor.plugins._BreadcrumbMenuTitle",[_4,_5,_6],{templateString:"<tr><td dojoAttachPoint=\"title\" colspan=\"4\" class=\"dijitToolbar\" style=\"font-weight: bold; padding: 3px;\"></td></tr>",menuTitle:"",postCreate:function(){
_1.setSelectable(this.domNode,false);
var _d=this.id+"_text";
this.domNode.setAttribute("aria-labelledby",_d);
},_setMenuTitleAttr:function(_e){
this.title.innerHTML=_e;
},_getMenuTitleAttr:function(_f){
return this.title.innerHTML;
}});
var _10=_1.declare("dojox.editor.plugins.Breadcrumb",_b,{_menu:null,breadcrumbBar:null,setEditor:function(_11){
this.editor=_11;
this._buttons=[];
this.breadcrumbBar=new _2.Toolbar();
var _12=_1.i18n.getLocalization("dojox.editor.plugins","Breadcrumb");
this._titleTemplate=_12.nodeActions;
_1.place(this.breadcrumbBar.domNode,_11.footer);
this.editor.onLoadDeferred.addCallback(_1.hitch(this,function(){
this._menu=new _2.Menu({});
_1.addClass(this.breadcrumbBar.domNode,"dojoxEditorBreadcrumbArrow");
var _13=this;
var _14=new _2.form.ComboButton({showLabel:true,label:"body",_selNode:_11.editNode,dropDown:this._menu,onClick:_1.hitch(this,function(){
this._menuTarget=_11.editNode;
this._selectContents();
})});
this._menuTitle=new _c({menuTitle:_12.nodeActions});
this._selCMenu=new _2.MenuItem({label:_12.selectContents,onClick:_1.hitch(this,this._selectContents)});
this._delCMenu=new _2.MenuItem({label:_12.deleteContents,onClick:_1.hitch(this,this._deleteContents)});
this._selEMenu=new _2.MenuItem({label:_12.selectElement,onClick:_1.hitch(this,this._selectElement)});
this._delEMenu=new _2.MenuItem({label:_12.deleteElement,onClick:_1.hitch(this,this._deleteElement)});
this._moveSMenu=new _2.MenuItem({label:_12.moveStart,onClick:_1.hitch(this,this._moveCToStart)});
this._moveEMenu=new _2.MenuItem({label:_12.moveEnd,onClick:_1.hitch(this,this._moveCToEnd)});
this._menu.addChild(this._menuTitle);
this._menu.addChild(this._selCMenu);
this._menu.addChild(this._delCMenu);
this._menu.addChild(new _2.MenuSeparator({}));
this._menu.addChild(this._selEMenu);
this._menu.addChild(this._delEMenu);
this._menu.addChild(new _2.MenuSeparator({}));
this._menu.addChild(this._moveSMenu);
this._menu.addChild(this._moveEMenu);
_14._ddConnect=_1.connect(_14,"openDropDown",_1.hitch(this,function(){
this._menuTarget=_14._selNode;
this._menuTitle.set("menuTitle",_1.string.substitute(this._titleTemplate,{"nodeName":"&lt;body&gt;"}));
this._selEMenu.set("disabled",true);
this._delEMenu.set("disabled",true);
this._selCMenu.set("disabled",false);
this._delCMenu.set("disabled",false);
this._moveSMenu.set("disabled",false);
this._moveEMenu.set("disabled",false);
}));
this.breadcrumbBar.addChild(_14);
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
var _15=this._menuTarget.tagName.toLowerCase();
switch(_15){
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
this.editor._sCall("collapse",[null]);
this.editor._sCall("selectElementChildren",[this._menuTarget]);
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
this.editor._sCall("remove",[this._menuTarget]);
this.editor.endEditing();
this._updateBreadcrumb();
this.editor.onDisplayChanged();
}
},_selectElement:function(){
this.editor.focus();
if(this._menuTarget){
this.editor._sCall("collapse",[null]);
this.editor._sCall("selectElement",[this._menuTarget]);
this.editor.onDisplayChanged();
}
},_deleteElement:function(){
if(this._menuTarget){
this.editor.beginEditing();
this._selectElement();
this.editor._sCall("remove",[this._menuTarget]);
this.editor.endEditing();
this._updateBreadcrumb();
this.editor.onDisplayChanged();
}
},_moveCToStart:function(){
this.editor.focus();
if(this._menuTarget){
this._selectContents();
this.editor._sCall("collapse",[true]);
}
},_moveCToEnd:function(){
this.editor.focus();
if(this._menuTarget){
this._selectContents();
this.editor._sCall("collapse",[false]);
}
},_updateBreadcrumb:function(){
var ed=this.editor;
if(ed.window){
var sel=_2.range.getSelection(ed.window);
if(sel&&sel.rangeCount>0){
var _16=sel.getRangeAt(0);
var _17=ed._sCall("getSelectedElement",[])||_16.startContainer;
var _18=[];
if(_17&&_17.ownerDocument===ed.document){
while(_17&&_17!==ed.editNode&&_17!=ed.document.body&&_17!=ed.document){
if(_17.nodeType===1){
_18.push({type:_17.tagName.toLowerCase(),node:_17});
}
_17=_17.parentNode;
}
_18=_18.reverse();
while(this._buttons.length){
var db=this._buttons.pop();
_1.disconnect(db._ddConnect);
this.breadcrumbBar.removeChild(db);
}
this._buttons=[];
var i;
var _19=this;
for(i=0;i<_18.length;i++){
var bc=_18[i];
var b=new _2.form.ComboButton({showLabel:true,label:bc.type,_selNode:bc.node,dropDown:this._menu,onClick:function(){
_19._menuTarget=this._selNode;
_19._selectContents();
}});
b._ddConnect=_1.connect(b,"openDropDown",_1.hitch(b,function(){
_19._menuTarget=this._selNode;
var _1a=_19._menuTarget.tagName.toLowerCase();
var _1b=_1.string.substitute(_19._titleTemplate,{"nodeName":"&lt;"+_1a+"&gt;"});
_19._menuTitle.set("menuTitle",_1b);
switch(_1a){
case "br":
case "hr":
case "img":
case "input":
case "base":
case "meta":
case "area":
case "basefont":
_19._selCMenu.set("disabled",true);
_19._delCMenu.set("disabled",true);
_19._moveSMenu.set("disabled",true);
_19._moveEMenu.set("disabled",true);
_19._selEMenu.set("disabled",false);
_19._delEMenu.set("disabled",false);
break;
default:
_19._selCMenu.set("disabled",false);
_19._delCMenu.set("disabled",false);
_19._selEMenu.set("disabled",false);
_19._delEMenu.set("disabled",false);
_19._moveSMenu.set("disabled",false);
_19._moveEMenu.set("disabled",false);
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
var _1c=_1.marginBox(this.editor.domNode);
this.editor.resize({h:_1c.h});
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
_10._BreadcrumbMenuTitle=_c;
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _1d=o.args.name.toLowerCase();
if(_1d==="breadcrumb"){
o.plugin=new _10({});
}
});
return _10;
});
