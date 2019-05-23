//>>built
define("dojox/grid/cells/dijit",["dojo/_base/kernel","../../main","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/json","dojo/_base/connect","dojo/_base/sniff","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/dom-style","dojo/dom-geometry","dojo/data/ItemFileReadStore","dijit/form/DateTextBox","dijit/form/TimeTextBox","dijit/form/ComboBox","dijit/form/CheckBox","dijit/form/TextBox","dijit/form/NumberSpinner","dijit/form/NumberTextBox","dijit/form/CurrencyTextBox","dijit/form/HorizontalSlider","dijit/form/_TextBoxMixin","dijit/Editor","../util","./_base"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b){
var _1c=_3("dojox.grid.cells._Widget",_1b,{widgetClass:_13,constructor:function(_1d){
this.widget=null;
if(typeof this.widgetClass=="string"){
_1.deprecated("Passing a string to widgetClass is deprecated","pass the widget class object instead","2.0");
this.widgetClass=_5.getObject(this.widgetClass);
}
},formatEditing:function(_1e,_1f){
this.needFormatNode(_1e,_1f);
return "<div></div>";
},getValue:function(_20){
return this.widget.get("value");
},_unescapeHTML:function(_21){
return (_21&&_21.replace&&this.grid.escapeHTMLInData)?_21.replace(/&lt;/g,"<").replace(/&amp;/g,"&"):_21;
},setValue:function(_22,_23){
if(this.widget&&this.widget.set){
_23=this._unescapeHTML(_23);
if(this.widget.onLoadDeferred){
var _24=this;
this.widget.onLoadDeferred.addCallback(function(){
_24.widget.set("value",_23===null?"":_23);
});
}else{
this.widget.set("value",_23);
}
}else{
this.inherited(arguments);
}
},getWidgetProps:function(_25){
return _5.mixin({dir:this.dir,lang:this.lang},this.widgetProps||{},{constraints:_5.mixin({},this.constraint)||{},required:(this.constraint||{}).required,value:this._unescapeHTML(_25)});
},createWidget:function(_26,_27,_28){
return new this.widgetClass(this.getWidgetProps(_27),_26);
},attachWidget:function(_29,_2a,_2b){
_29.appendChild(this.widget.domNode);
this.setValue(_2b,_2a);
},formatNode:function(_2c,_2d,_2e){
if(!this.widgetClass){
return _2d;
}
if(!this.widget){
this.widget=this.createWidget.apply(this,arguments);
}else{
this.attachWidget.apply(this,arguments);
}
this.sizeWidget.apply(this,arguments);
this.grid.views.renormalizeRow(_2e);
this.grid.scroller.rowHeightChanged(_2e,true);
this.focus();
return undefined;
},sizeWidget:function(_2f,_30,_31){
var p=this.getNode(_31);
_1.marginBox(this.widget.domNode,{w:_c.get(p,"width")});
},focus:function(_32,_33){
if(this.widget){
setTimeout(_5.hitch(this.widget,function(){
_1a.fire(this,"focus");
if(this.focusNode&&this.focusNode.tagName==="INPUT"){
_18.selectInputText(this.focusNode);
}
}),0);
}
},_finish:function(_34){
this.inherited(arguments);
_1a.removeNode(this.widget.domNode);
if(_8("ie")){
_9.setSelectable(this.widget.domNode,true);
}
}});
_1c.markupFactory=function(_35,_36){
_1b.markupFactory(_35,_36);
var _37=_5.trim(_a.get(_35,"widgetProps")||"");
var _38=_5.trim(_a.get(_35,"constraint")||"");
var _39=_5.trim(_a.get(_35,"widgetClass")||"");
if(_37){
_36.widgetProps=_6.fromJson(_37);
}
if(_38){
_36.constraint=_6.fromJson(_38);
}
if(_39){
_36.widgetClass=_5.getObject(_39);
}
};
var _11=_3("dojox.grid.cells.ComboBox",_1c,{widgetClass:_11,getWidgetProps:function(_3a){
var _3b=[];
_4.forEach(this.options,function(o){
_3b.push({name:o,value:o});
});
var _3c=new _e({data:{identifier:"name",items:_3b}});
return _5.mixin({},this.widgetProps||{},{value:_3a,store:_3c});
},getValue:function(){
var e=this.widget;
e.set("displayedValue",e.get("displayedValue"));
return e.get("value");
}});
_11.markupFactory=function(_3d,_3e){
_1c.markupFactory(_3d,_3e);
var _3f=_5.trim(_a.get(_3d,"options")||"");
if(_3f){
var o=_3f.split(",");
if(o[0]!=_3f){
_3e.options=o;
}
}
};
var _f=_3("dojox.grid.cells.DateTextBox",_1c,{widgetClass:_f,setValue:function(_40,_41){
if(this.widget){
this.widget.set("value",new Date(_41));
}else{
this.inherited(arguments);
}
},getWidgetProps:function(_42){
return _5.mixin(this.inherited(arguments),{value:new Date(_42)});
}});
_f.markupFactory=function(_43,_44){
_1c.markupFactory(_43,_44);
};
var _12=_3("dojox.grid.cells.CheckBox",_1c,{widgetClass:_12,getValue:function(){
return this.widget.checked;
},setValue:function(_45,_46){
if(this.widget&&this.widget.attributeMap.checked){
this.widget.set("checked",_46);
}else{
this.inherited(arguments);
}
},sizeWidget:function(_47,_48,_49){
return;
}});
_12.markupFactory=function(_4a,_4b){
_1c.markupFactory(_4a,_4b);
};
var _19=_3("dojox.grid.cells.Editor",_1c,{widgetClass:_19,getWidgetProps:function(_4c){
return _5.mixin({},this.widgetProps||{},{height:this.widgetHeight||"100px"});
},createWidget:function(_4d,_4e,_4f){
var _50=new this.widgetClass(this.getWidgetProps(_4e),_4d);
_50.onLoadDeferred.then(_5.hitch(this,"populateEditor"));
return _50;
},formatNode:function(_51,_52,_53){
this.content=_52;
this.inherited(arguments);
if(_8("mozilla")){
var e=this.widget;
e.open();
if(this.widgetToolbar){
_b.place(e.toolbar.domNode,e.editingArea,"before");
}
}
},populateEditor:function(){
this.widget.set("value",this.content);
this.widget.placeCursorAtEnd();
}});
_19.markupFactory=function(_54,_55){
_1c.markupFactory(_54,_55);
var h=_5.trim(_a.get(_54,"widgetHeight")||"");
if(h){
if((h!="auto")&&(h.substr(-2)!="em")){
h=parseInt(h,10)+"px";
}
_55.widgetHeight=h;
}
};
return _2.grid.cells.dijit;
});
