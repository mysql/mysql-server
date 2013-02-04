//>>built
define("dojox/editor/plugins/CollapsibleToolbar",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_editor/_Plugin","dijit/form/Button","dijit/focus","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/CollapsibleToolbar"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins._CollapsibleToolbarButton",[_2._Widget,_2._TemplatedMixin],{templateString:"<div tabindex='0' role='button' title='${title}' class='${buttonClass}' "+"dojoAttachEvent='ondijitclick: onClick'><span class='${textClass}'>${text}</span></div>",title:"",buttonClass:"",text:"",textClass:"",onClick:function(e){
}});
_1.declare("dojox.editor.plugins.CollapsibleToolbar",_2._editor._Plugin,{_myWidgets:null,setEditor:function(_4){
this.editor=_4;
this._constructContainer();
},_constructContainer:function(){
var _5=_1.i18n.getLocalization("dojox.editor.plugins","CollapsibleToolbar");
this._myWidgets=[];
var _6=_1.create("table",{style:{width:"100%"},tabindex:-1,"class":"dojoxCollapsibleToolbarContainer"});
var _7=_1.create("tbody",{tabindex:-1},_6);
var _8=_1.create("tr",{tabindex:-1},_7);
var _9=_1.create("td",{"class":"dojoxCollapsibleToolbarControl",tabindex:-1},_8);
var _a=_1.create("td",{"class":"dojoxCollapsibleToolbarControl",tabindex:-1},_8);
var _b=_1.create("td",{style:{width:"100%"},tabindex:-1},_8);
var m=_1.create("span",{style:{width:"100%"},tabindex:-1},_b);
var _c=new _3.editor.plugins._CollapsibleToolbarButton({buttonClass:"dojoxCollapsibleToolbarCollapse",title:_5.collapse,text:"-",textClass:"dojoxCollapsibleToolbarCollapseText"});
_1.place(_c.domNode,_9);
var _d=new _3.editor.plugins._CollapsibleToolbarButton({buttonClass:"dojoxCollapsibleToolbarExpand",title:_5.expand,text:"+",textClass:"dojoxCollapsibleToolbarExpandText"});
_1.place(_d.domNode,_a);
this._myWidgets.push(_c);
this._myWidgets.push(_d);
_1.style(_a,"display","none");
_1.place(_6,this.editor.toolbar.domNode,"after");
_1.place(this.editor.toolbar.domNode,m);
this.openTd=_9;
this.closeTd=_a;
this.menu=m;
this.connect(_c,"onClick","_onClose");
this.connect(_d,"onClick","_onOpen");
},_onClose:function(e){
if(e){
_1.stopEvent(e);
}
var _e=_1.marginBox(this.editor.domNode);
_1.style(this.openTd,"display","none");
_1.style(this.closeTd,"display","");
_1.style(this.menu,"display","none");
this.editor.resize({h:_e.h});
if(_1.isIE){
this.editor.header.className=this.editor.header.className;
this.editor.footer.className=this.editor.footer.className;
}
_2.focus(this.closeTd.firstChild);
},_onOpen:function(e){
if(e){
_1.stopEvent(e);
}
var _f=_1.marginBox(this.editor.domNode);
_1.style(this.closeTd,"display","none");
_1.style(this.openTd,"display","");
_1.style(this.menu,"display","");
this.editor.resize({h:_f.h});
if(_1.isIE){
this.editor.header.className=this.editor.header.className;
this.editor.footer.className=this.editor.footer.className;
}
_2.focus(this.openTd.firstChild);
},destroy:function(){
this.inherited(arguments);
if(this._myWidgets){
while(this._myWidgets.length){
this._myWidgets.pop().destroy();
}
delete this._myWidgets;
}
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _10=o.args.name.toLowerCase();
if(_10==="collapsibletoolbar"){
o.plugin=new _3.editor.plugins.CollapsibleToolbar({});
}
});
return _3.editor.plugins.CollapsibleToolbar;
});
