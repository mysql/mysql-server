//>>built
define("dojox/grid/cells/_base",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/event","dojo/_base/connect","dojo/_base/array","dojo/_base/sniff","dojo/dom","dojo/dom-attr","dojo/dom-construct","dijit/_Widget","../util"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_2("dojox.grid._DeferredTextWidget",_b,{deferred:null,_destroyOnRemove:true,postCreate:function(){
if(this.deferred){
this.deferred.addBoth(_3.hitch(this,function(_e){
if(this.domNode){
this.domNode.innerHTML=_e;
}
}));
}
}});
var _f=function(_10){
try{
_c.fire(_10,"focus");
_c.fire(_10,"select");
}
catch(e){
}
};
var _11=function(){
setTimeout(_3.hitch.apply(_1,arguments),0);
};
var _12=_2("dojox.grid.cells._Base",null,{styles:"",classes:"",editable:false,alwaysEditing:false,formatter:null,defaultValue:"...",value:null,hidden:false,noresize:false,draggable:true,_valueProp:"value",_formatPending:false,constructor:function(_13){
this._props=_13||{};
_3.mixin(this,_13);
if(this.draggable===undefined){
this.draggable=true;
}
},_defaultFormat:function(_14,_15){
var s=this.grid.formatterScope||this;
var f=this.formatter;
if(f&&s&&typeof f=="string"){
f=this.formatter=s[f];
}
var v=(_14!=this.defaultValue&&f)?f.apply(s,_15):_14;
if(typeof v=="undefined"){
return this.defaultValue;
}
if(v&&v.addBoth){
v=new _d({deferred:v},_a.create("span",{innerHTML:this.defaultValue}));
}
if(v&&v.declaredClass&&v.startup){
return "<div class='dojoxGridStubNode' linkWidget='"+v.id+"' cellIdx='"+this.index+"'>"+this.defaultValue+"</div>";
}
return v;
},format:function(_16,_17){
var f,i=this.grid.edit.info,d=this.get?this.get(_16,_17):(this.value||this.defaultValue);
d=(d&&d.replace&&this.grid.escapeHTMLInData)?d.replace(/&/g,"&amp;").replace(/</g,"&lt;"):d;
if(this.editable&&(this.alwaysEditing||(i.rowIndex==_16&&i.cell==this))){
return this.formatEditing(d,_16);
}else{
return this._defaultFormat(d,[d,_16,this]);
}
},formatEditing:function(_18,_19){
},getNode:function(_1a){
return this.view.getCellNode(_1a,this.index);
},getHeaderNode:function(){
return this.view.getHeaderCellNode(this.index);
},getEditNode:function(_1b){
return (this.getNode(_1b)||0).firstChild||0;
},canResize:function(){
var uw=this.unitWidth;
return uw&&(uw!=="auto");
},isFlex:function(){
var uw=this.unitWidth;
return uw&&_3.isString(uw)&&(uw=="auto"||uw.slice(-1)=="%");
},applyEdit:function(_1c,_1d){
if(this.getNode(_1d)){
this.grid.edit.applyCellEdit(_1c,this,_1d);
}
},cancelEdit:function(_1e){
this.grid.doCancelEdit(_1e);
},_onEditBlur:function(_1f){
if(this.grid.edit.isEditCell(_1f,this.index)){
this.grid.edit.apply();
}
},registerOnBlur:function(_20,_21){
if(this.commitOnBlur){
_5.connect(_20,"onblur",function(e){
setTimeout(_3.hitch(this,"_onEditBlur",_21),250);
});
}
},needFormatNode:function(_22,_23){
this._formatPending=true;
_11(this,"_formatNode",_22,_23);
},cancelFormatNode:function(){
this._formatPending=false;
},_formatNode:function(_24,_25){
if(this._formatPending){
this._formatPending=false;
if(!_7("ie")){
_8.setSelectable(this.grid.domNode,true);
}
this.formatNode(this.getEditNode(_25),_24,_25);
}
},formatNode:function(_26,_27,_28){
if(_7("ie")){
_11(this,"focus",_28,_26);
}else{
this.focus(_28,_26);
}
},dispatchEvent:function(m,e){
if(m in this){
return this[m](e);
}
},getValue:function(_29){
return this.getEditNode(_29)[this._valueProp];
},setValue:function(_2a,_2b){
var n=this.getEditNode(_2a);
if(n){
n[this._valueProp]=_2b;
}
},focus:function(_2c,_2d){
_f(_2d||this.getEditNode(_2c));
},save:function(_2e){
this.value=this.value||this.getValue(_2e);
},restore:function(_2f){
this.setValue(_2f,this.value);
},_finish:function(_30){
_8.setSelectable(this.grid.domNode,false);
this.cancelFormatNode();
},apply:function(_31){
this.applyEdit(this.getValue(_31),_31);
this._finish(_31);
},cancel:function(_32){
this.cancelEdit(_32);
this._finish(_32);
}});
_12.markupFactory=function(_33,_34){
var _35=_3.trim(_9.get(_33,"formatter")||"");
if(_35){
_34.formatter=_3.getObject(_35)||_35;
}
var get=_3.trim(_9.get(_33,"get")||"");
if(get){
_34.get=_3.getObject(get);
}
var _36=function(_37,_38,_39){
var _3a=_3.trim(_9.get(_33,_37)||"");
if(_3a){
_38[_39||_37]=!(_3a.toLowerCase()=="false");
}
};
_36("sortDesc",_34);
_36("editable",_34);
_36("alwaysEditing",_34);
_36("noresize",_34);
_36("draggable",_34);
var _3b=_3.trim(_9.get(_33,"loadingText")||_9.get(_33,"defaultValue")||"");
if(_3b){
_34.defaultValue=_3b;
}
var _3c=function(_3d,_3e,_3f){
var _40=_3.trim(_9.get(_33,_3d)||"")||undefined;
if(_40){
_3e[_3f||_3d]=_40;
}
};
_3c("styles",_34);
_3c("headerStyles",_34);
_3c("cellStyles",_34);
_3c("classes",_34);
_3c("headerClasses",_34);
_3c("cellClasses",_34);
};
var _41=_2("dojox.grid.cells.Cell",_12,{constructor:function(){
this.keyFilter=this.keyFilter;
},keyFilter:null,formatEditing:function(_42,_43){
this.needFormatNode(_42,_43);
return "<input class=\"dojoxGridInput\" type=\"text\" value=\""+_42+"\">";
},formatNode:function(_44,_45,_46){
this.inherited(arguments);
this.registerOnBlur(_44,_46);
},doKey:function(e){
if(this.keyFilter){
var key=String.fromCharCode(e.charCode);
if(key.search(this.keyFilter)==-1){
_4.stop(e);
}
}
},_finish:function(_47){
this.inherited(arguments);
var n=this.getEditNode(_47);
try{
_c.fire(n,"blur");
}
catch(e){
}
}});
_41.markupFactory=function(_48,_49){
_12.markupFactory(_48,_49);
var _4a=_3.trim(_9.get(_48,"keyFilter")||"");
if(_4a){
_49.keyFilter=new RegExp(_4a);
}
};
var _4b=_2("dojox.grid.cells.RowIndex",_41,{name:"Row",postscript:function(){
this.editable=false;
},get:function(_4c){
return _4c+1;
}});
_4b.markupFactory=function(_4d,_4e){
_41.markupFactory(_4d,_4e);
};
var _4f=_2("dojox.grid.cells.Select",_41,{options:null,values:null,returnIndex:-1,constructor:function(_50){
this.values=this.values||this.options;
},formatEditing:function(_51,_52){
this.needFormatNode(_51,_52);
var h=["<select class=\"dojoxGridSelect\">"];
for(var i=0,o,v;((o=this.options[i])!==undefined)&&((v=this.values[i])!==undefined);i++){
v=v.replace?v.replace(/&/g,"&amp;").replace(/</g,"&lt;"):v;
o=o.replace?o.replace(/&/g,"&amp;").replace(/</g,"&lt;"):o;
h.push("<option",(_51==v?" selected":"")," value=\""+v+"\"",">",o,"</option>");
}
h.push("</select>");
return h.join("");
},_defaultFormat:function(_53,_54){
var v=this.inherited(arguments);
if(!this.formatter&&this.values&&this.options){
var i=_6.indexOf(this.values,v);
if(i>=0){
v=this.options[i];
}
}
return v;
},getValue:function(_55){
var n=this.getEditNode(_55);
if(n){
var i=n.selectedIndex,o=n.options[i];
return this.returnIndex>-1?i:o.value||o.innerHTML;
}
}});
_4f.markupFactory=function(_56,_57){
_41.markupFactory(_56,_57);
var _58=_3.trim(_9.get(_56,"options")||"");
if(_58){
var o=_58.split(",");
if(o[0]!=_58){
_57.options=o;
}
}
var _59=_3.trim(_9.get(_56,"values")||"");
if(_59){
var v=_59.split(",");
if(v[0]!=_59){
_57.values=v;
}
}
};
var _5a=_2("dojox.grid.cells.AlwaysEdit",_41,{alwaysEditing:true,_formatNode:function(_5b,_5c){
this.formatNode(this.getEditNode(_5c),_5b,_5c);
},applyStaticValue:function(_5d){
var e=this.grid.edit;
e.applyCellEdit(this.getValue(_5d),this,_5d);
e.start(this,_5d,true);
}});
_5a.markupFactory=function(_5e,_5f){
_41.markupFactory(_5e,_5f);
};
var _60=_2("dojox.grid.cells.Bool",_5a,{_valueProp:"checked",formatEditing:function(_61,_62){
return "<input class=\"dojoxGridInput\" type=\"checkbox\""+(_61?" checked=\"checked\"":"")+" style=\"width: auto\" />";
},doclick:function(e){
if(e.target.tagName=="INPUT"){
this.applyStaticValue(e.rowIndex);
}
}});
_60.markupFactory=function(_63,_64){
_5a.markupFactory(_63,_64);
};
return _12;
});
