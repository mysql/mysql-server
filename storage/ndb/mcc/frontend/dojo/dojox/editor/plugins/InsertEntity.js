//>>built
define("dojox/editor/plugins/InsertEntity",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/TooltipDialog","dijit/form/DropDownButton","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojox/html/entities","dojox/editor/plugins/EntityPalette","dojo/i18n!dojox/editor/plugins/nls/InsertEntity"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.InsertEntity",_4,{iconClassPrefix:"dijitAdditionalEditorIcon",_initButton:function(){
this.dropDown=new _3.editor.plugins.EntityPalette({showCode:this.showCode,showEntityName:this.showEntityName});
this.connect(this.dropDown,"onChange",function(_6){
this.button.closeDropDown();
this.editor.focus();
this.editor.execCommand("inserthtml",_6);
});
var _7=_1.i18n.getLocalization("dojox.editor.plugins","InsertEntity");
this.button=new _2.form.DropDownButton({label:_7["insertEntity"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"InsertEntity",tabIndex:"-1",dropDown:this.dropDown});
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_8){
this.editor=_8;
this._initButton();
this.editor.addKeyHandler("s",true,true,_1.hitch(this,function(){
this.button.openDropDown();
this.dropDown.focus();
}));
_8.contentPreFilters.push(this._preFilterEntities);
_8.contentPostFilters.push(this._postFilterEntities);
},_preFilterEntities:function(s){
return _3.html.entities.decode(s,_3.html.entities.latin);
},_postFilterEntities:function(s){
return _3.html.entities.encode(s,_3.html.entities.latin);
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _9=o.args.name?o.args.name.toLowerCase():"";
if(_9==="insertentity"){
o.plugin=new _5({showCode:("showCode" in o.args)?o.args.showCode:false,showEntityName:("showEntityName" in o.args)?o.args.showEntityName:false});
}
});
return _5;
});
