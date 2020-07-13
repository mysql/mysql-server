//>>built
define("dojox/editor/plugins/CollapsibleToolbar",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_editor/_Plugin","dijit/form/Button","dijit/focus","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/CollapsibleToolbar"],function(_1,_2,_3,_4,_5,_6){
var _7=_1.declare("dojox.editor.plugins._CollapsibleToolbarButton",[_4,_5],{templateString:"<div tabindex='0' role='button' title='${title}' class='${buttonClass}' "+"dojoAttachEvent='ondijitclick: onClick'><span class='${textClass}'>${text}</span></div>",title:"",buttonClass:"",text:"",textClass:"",onClick:function(e){
}});
var _8=_1.declare("dojox.editor.plugins.CollapsibleToolbar",_6,{_myWidgets:null,setEditor:function(_9){
this.editor=_9;
this._constructContainer();
},_constructContainer:function(){
var _a=_1.i18n.getLocalization("dojox.editor.plugins","CollapsibleToolbar");
this._myWidgets=[];
var _b=_1.create("table",{style:{width:"100%"},tabindex:-1,"class":"dojoxCollapsibleToolbarContainer"});
var _c=_1.create("tbody",{tabindex:-1},_b);
var _d=_1.create("tr",{tabindex:-1},_c);
var _e=_1.create("td",{"class":"dojoxCollapsibleToolbarControl",tabindex:-1},_d);
var _f=_1.create("td",{"class":"dojoxCollapsibleToolbarControl",tabindex:-1},_d);
var _10=_1.create("td",{style:{width:"100%"},tabindex:-1},_d);
var m=_1.create("span",{style:{width:"100%"},tabindex:-1},_10);
var _11=new _7({buttonClass:"dojoxCollapsibleToolbarCollapse",title:_a.collapse,text:"-",textClass:"dojoxCollapsibleToolbarCollapseText"});
_1.place(_11.domNode,_e);
var _12=new _7({buttonClass:"dojoxCollapsibleToolbarExpand",title:_a.expand,text:"+",textClass:"dojoxCollapsibleToolbarExpandText"});
_1.place(_12.domNode,_f);
this._myWidgets.push(_11);
this._myWidgets.push(_12);
_1.style(_f,"display","none");
_1.place(_b,this.editor.toolbar.domNode,"after");
_1.place(this.editor.toolbar.domNode,m);
this.openTd=_e;
this.closeTd=_f;
this.menu=m;
this.connect(_11,"onClick","_onClose");
this.connect(_12,"onClick","_onOpen");
},_onClose:function(e){
if(e){
_1.stopEvent(e);
}
var _13=_1.marginBox(this.editor.domNode);
_1.style(this.openTd,"display","none");
_1.style(this.closeTd,"display","");
_1.style(this.menu,"display","none");
this.editor.resize({h:_13.h});
if(_1.isIE){
this.editor.header.className=this.editor.header.className;
this.editor.footer.className=this.editor.footer.className;
}
_2.focus(this.closeTd.firstChild);
},_onOpen:function(e){
if(e){
_1.stopEvent(e);
}
var _14=_1.marginBox(this.editor.domNode);
_1.style(this.closeTd,"display","none");
_1.style(this.openTd,"display","");
_1.style(this.menu,"display","");
this.editor.resize({h:_14.h});
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
_8._CollapsibleToolbarButton=_7;
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _15=o.args.name.toLowerCase();
if(_15==="collapsibletoolbar"){
o.plugin=new _8({});
}
});
return _8;
});
