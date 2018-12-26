//>>built
define("dojox/grid/cells/dijit",["dojo/_base/kernel","../../main","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/json","dojo/_base/connect","dojo/_base/sniff","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/dom-geometry","dojo/data/ItemFileReadStore","dijit/form/DateTextBox","dijit/form/TimeTextBox","dijit/form/ComboBox","dijit/form/CheckBox","dijit/form/TextBox","dijit/form/NumberSpinner","dijit/form/NumberTextBox","dijit/form/CurrencyTextBox","dijit/form/HorizontalSlider","dijit/Editor","../util","./_base"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19){
var _1a=_3("dojox.grid.cells._Widget",_19,{widgetClass:_12,constructor:function(_1b){
this.widget=null;
if(typeof this.widgetClass=="string"){
_1.deprecated("Passing a string to widgetClass is deprecated","pass the widget class object instead","2.0");
this.widgetClass=_5.getObject(this.widgetClass);
}
},formatEditing:function(_1c,_1d){
this.needFormatNode(_1c,_1d);
return "<div></div>";
},getValue:function(_1e){
return this.widget.get("value");
},_unescapeHTML:function(_1f){
return (_1f&&_1f.replace&&this.grid.escapeHTMLInData)?_1f.replace(/&lt;/g,"<").replace(/&amp;/g,"&"):_1f;
},setValue:function(_20,_21){
if(this.widget&&this.widget.set){
_21=this._unescapeHTML(_21);
if(this.widget.onLoadDeferred){
var _22=this;
this.widget.onLoadDeferred.addCallback(function(){
_22.widget.set("value",_21===null?"":_21);
});
}else{
this.widget.set("value",_21);
}
}else{
this.inherited(arguments);
}
},getWidgetProps:function(_23){
return _5.mixin({dir:this.dir,lang:this.lang},this.widgetProps||{},{constraints:_5.mixin({},this.constraint)||{},value:this._unescapeHTML(_23)});
},createWidget:function(_24,_25,_26){
return new this.widgetClass(this.getWidgetProps(_25),_24);
},attachWidget:function(_27,_28,_29){
_27.appendChild(this.widget.domNode);
this.setValue(_29,_28);
},formatNode:function(_2a,_2b,_2c){
if(!this.widgetClass){
return _2b;
}
if(!this.widget){
this.widget=this.createWidget.apply(this,arguments);
}else{
this.attachWidget.apply(this,arguments);
}
this.sizeWidget.apply(this,arguments);
this.grid.views.renormalizeRow(_2c);
this.grid.scroller.rowHeightChanged(_2c,true);
this.focus();
return undefined;
},sizeWidget:function(_2d,_2e,_2f){
var p=this.getNode(_2f),box=_1.contentBox(p);
_1.marginBox(this.widget.domNode,{w:box.w});
},focus:function(_30,_31){
if(this.widget){
setTimeout(_5.hitch(this.widget,function(){
_18.fire(this,"focus");
}),0);
}
},_finish:function(_32){
this.inherited(arguments);
_18.removeNode(this.widget.domNode);
if(_8("ie")){
_9.setSelectable(this.widget.domNode,true);
}
}});
_1a.markupFactory=function(_33,_34){
_19.markupFactory(_33,_34);
var _35=_5.trim(_a.get(_33,"widgetProps")||"");
var _36=_5.trim(_a.get(_33,"constraint")||"");
var _37=_5.trim(_a.get(_33,"widgetClass")||"");
if(_35){
_34.widgetProps=_6.fromJson(_35);
}
if(_36){
_34.constraint=_6.fromJson(_36);
}
if(_37){
_34.widgetClass=_5.getObject(_37);
}
};
var _10=_3("dojox.grid.cells.ComboBox",_1a,{widgetClass:_10,getWidgetProps:function(_38){
var _39=[];
_4.forEach(this.options,function(o){
_39.push({name:o,value:o});
});
var _3a=new _d({data:{identifier:"name",items:_39}});
return _5.mixin({},this.widgetProps||{},{value:_38,store:_3a});
},getValue:function(){
var e=this.widget;
e.set("displayedValue",e.get("displayedValue"));
return e.get("value");
}});
_10.markupFactory=function(_3b,_3c){
_1a.markupFactory(_3b,_3c);
var _3d=_5.trim(_a.get(_3b,"options")||"");
if(_3d){
var o=_3d.split(",");
if(o[0]!=_3d){
_3c.options=o;
}
}
};
var _e=_3("dojox.grid.cells.DateTextBox",_1a,{widgetClass:_e,setValue:function(_3e,_3f){
if(this.widget){
this.widget.set("value",new Date(_3f));
}else{
this.inherited(arguments);
}
},getWidgetProps:function(_40){
return _5.mixin(this.inherited(arguments),{value:new Date(_40)});
}});
_e.markupFactory=function(_41,_42){
_1a.markupFactory(_41,_42);
};
var _11=_3("dojox.grid.cells.CheckBox",_1a,{widgetClass:_11,getValue:function(){
return this.widget.checked;
},setValue:function(_43,_44){
if(this.widget&&this.widget.attributeMap.checked){
this.widget.set("checked",_44);
}else{
this.inherited(arguments);
}
},sizeWidget:function(_45,_46,_47){
return;
}});
_11.markupFactory=function(_48,_49){
_1a.markupFactory(_48,_49);
};
var _17=_3("dojox.grid.cells.Editor",_1a,{widgetClass:_17,getWidgetProps:function(_4a){
return _5.mixin({},this.widgetProps||{},{height:this.widgetHeight||"100px"});
},createWidget:function(_4b,_4c,_4d){
var _4e=new this.widgetClass(this.getWidgetProps(_4c),_4b);
_7.connect(_4e,"onLoad",_5.hitch(this,"populateEditor"));
return _4e;
},formatNode:function(_4f,_50,_51){
this.content=_50;
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
_17.markupFactory=function(_52,_53){
_1a.markupFactory(_52,_53);
var h=_5.trim(_a.get(_52,"widgetHeight")||"");
if(h){
if((h!="auto")&&(h.substr(-2)!="em")){
h=parseInt(h,10)+"px";
}
_53.widgetHeight=h;
}
};
return _2.grid.cells.dijit;
});
