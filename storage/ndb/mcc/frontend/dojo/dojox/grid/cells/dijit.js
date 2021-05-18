//>>built
define("dojox/grid/cells/dijit",["dojo/_base/kernel","../../main","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/json","dojo/_base/connect","dojo/_base/sniff","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/dom-style","dojo/dom-geometry","dojo/data/ItemFileReadStore","dijit/form/DateTextBox","dijit/form/TimeTextBox","dijit/form/ComboBox","dijit/form/CheckBox","dijit/form/TextBox","dijit/form/NumberSpinner","dijit/form/NumberTextBox","dijit/form/CurrencyTextBox","dijit/form/HorizontalSlider","dijit/form/_TextBoxMixin","dijit/Editor","../util","./_base"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b){
var _1c={};
var _1d=_1c._Widget=_3("dojox.grid.cells._Widget",_1b,{widgetClass:_13,constructor:function(_1e){
this.widget=null;
if(typeof this.widgetClass=="string"){
_1.deprecated("Passing a string to widgetClass is deprecated","pass the widget class object instead","2.0");
this.widgetClass=_5.getObject(this.widgetClass);
}
},formatEditing:function(_1f,_20){
this.needFormatNode(_1f,_20);
return "<div></div>";
},getValue:function(_21){
return this.widget.get("value");
},_unescapeHTML:function(_22){
return (_22&&_22.replace&&this.grid.escapeHTMLInData)?_22.replace(/&lt;/g,"<").replace(/&amp;/g,"&"):_22;
},setValue:function(_23,_24){
if(this.widget&&this.widget.set){
_24=this._unescapeHTML(_24);
if(this.widget.onLoadDeferred){
var _25=this;
this.widget.onLoadDeferred.addCallback(function(){
_25.widget.set("value",_24===null?"":_24);
});
}else{
this.widget.set("value",_24);
}
}else{
this.inherited(arguments);
}
},getWidgetProps:function(_26){
return _5.mixin({dir:this.dir,lang:this.lang},this.widgetProps||{},{constraints:_5.mixin({},this.constraint)||{},required:(this.constraint||{}).required,value:this._unescapeHTML(_26)});
},createWidget:function(_27,_28,_29){
return new this.widgetClass(this.getWidgetProps(_28),_27);
},attachWidget:function(_2a,_2b,_2c){
_2a.appendChild(this.widget.domNode);
this.setValue(_2c,_2b);
},formatNode:function(_2d,_2e,_2f){
if(!this.widgetClass){
return _2e;
}
if(!this.widget){
this.widget=this.createWidget.apply(this,arguments);
}else{
this.attachWidget.apply(this,arguments);
}
this.sizeWidget.apply(this,arguments);
this.grid.views.renormalizeRow(_2f);
this.grid.scroller.rowHeightChanged(_2f,true);
this.focus();
return undefined;
},sizeWidget:function(_30,_31,_32){
var p=this.getNode(_32);
_1.marginBox(this.widget.domNode,{w:_c.get(p,"width")});
},focus:function(_33,_34){
if(this.widget){
setTimeout(_5.hitch(this.widget,function(){
_1a.fire(this,"focus");
if(this.focusNode&&this.focusNode.tagName==="INPUT"){
_18.selectInputText(this.focusNode);
}
}),0);
}
},_finish:function(_35){
this.inherited(arguments);
_1a.removeNode(this.widget.domNode);
if(_8("ie")){
_9.setSelectable(this.widget.domNode,true);
}
}});
_1d.markupFactory=function(_36,_37){
_1b.markupFactory(_36,_37);
var _38=_5.trim(_a.get(_36,"widgetProps")||"");
var _39=_5.trim(_a.get(_36,"constraint")||"");
var _3a=_5.trim(_a.get(_36,"widgetClass")||"");
if(_38){
_37.widgetProps=_6.fromJson(_38);
}
if(_39){
_37.constraint=_6.fromJson(_39);
}
if(_3a){
_37.widgetClass=_5.getObject(_3a);
}
};
var _11=_1c.ComboBox=_3("dojox.grid.cells.ComboBox",_1d,{widgetClass:_11,getWidgetProps:function(_3b){
var _3c=[];
_4.forEach(this.options,function(o){
_3c.push({name:o,value:o});
});
var _3d=new _e({data:{identifier:"name",items:_3c}});
return _5.mixin({},this.widgetProps||{},{value:_3b,store:_3d});
},getValue:function(){
var e=this.widget;
e.set("displayedValue",e.get("displayedValue"));
return e.get("value");
}});
_11.markupFactory=function(_3e,_3f){
_1d.markupFactory(_3e,_3f);
var _40=_5.trim(_a.get(_3e,"options")||"");
if(_40){
var o=_40.split(",");
if(o[0]!=_40){
_3f.options=o;
}
}
};
var _f=_1c.DateTextBox=_3("dojox.grid.cells.DateTextBox",_1d,{widgetClass:_f,setValue:function(_41,_42){
if(this.widget){
this.widget.set("value",new Date(_42));
}else{
this.inherited(arguments);
}
},getWidgetProps:function(_43){
return _5.mixin(this.inherited(arguments),{value:new Date(_43)});
}});
_f.markupFactory=function(_44,_45){
_1d.markupFactory(_44,_45);
};
var _12=_1c.CheckBox=_3("dojox.grid.cells.CheckBox",_1d,{widgetClass:_12,getValue:function(){
return this.widget.checked;
},setValue:function(_46,_47){
if(this.widget&&this.widget.attributeMap.checked){
this.widget.set("checked",_47);
}else{
this.inherited(arguments);
}
},sizeWidget:function(_48,_49,_4a){
return;
}});
_12.markupFactory=function(_4b,_4c){
_1d.markupFactory(_4b,_4c);
};
var _19=_1c.Editor=_3("dojox.grid.cells.Editor",_1d,{widgetClass:_19,getWidgetProps:function(_4d){
return _5.mixin({},this.widgetProps||{},{height:this.widgetHeight||"100px"});
},createWidget:function(_4e,_4f,_50){
var _51=new this.widgetClass(this.getWidgetProps(_4f),_4e);
_51.onLoadDeferred.then(_5.hitch(this,"populateEditor"));
return _51;
},formatNode:function(_52,_53,_54){
this.content=_53;
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
_19.markupFactory=function(_55,_56){
_1d.markupFactory(_55,_56);
var h=_5.trim(_a.get(_55,"widgetHeight")||"");
if(h){
if((h!="auto")&&(h.substr(-2)!="em")){
h=parseInt(h,10)+"px";
}
_56.widgetHeight=h;
}
};
return _1c;
});
