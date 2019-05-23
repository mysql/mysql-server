//>>built
define("dojox/mvc/Generate",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","./_Container","./at","./Group","dijit/form/TextBox"],function(_1,_2,_3,_4,at){
return _3("dojox.mvc.Generate",[_4],{_counter:0,_defaultWidgetMapping:{"String":"dijit/form/TextBox"},_defaultClassMapping:{"Label":"generate-label-cell","String":"generate-dijit-cell","Heading":"generate-heading","Row":"row"},_defaultIdNameMapping:{"String":"textbox_t"},children:null,_relTargetProp:"children",startup:function(){
this.inherited(arguments);
this._setChildrenAttr(this.children);
},_setChildrenAttr:function(_5){
var _6=this.children;
this._set("children",_5);
if(this.binding!=_5){
this.set("ref",_5);
}
if(this._started&&(!this._builtOnce||_6!=_5)){
this._builtOnce=true;
this._buildContained(_5);
}
},_buildContained:function(_7){
if(!_7){
return;
}
this._destroyBody();
this._counter=0;
this.srcNodeRef.innerHTML=this._generateBody(_7);
this._createBody();
},_generateBody:function(_8,_9){
if(_8===void 0){
return "";
}
var _a=[];
var _b=_2.isFunction(_8.toPlainObject);
function _c(_d,_e){
if(_b?(_d&&_2.isFunction(_d.toPlainObject)):!_2.isFunction(_d)){
if(_2.isArray(_d)){
_a.push(this._generateRepeat(_d,_e));
}else{
if(_b?_d.value:((_d==null||{}.toString.call(_d)!="[object Object]")&&(!(_d||{}).set||!(_d||{}).watch))){
_a.push(this._generateTextBox(_e,_b));
}else{
_a.push(this._generateGroup(_d,_e,_9));
}
}
}
};
if(_2.isArray(_8)){
_1.forEach(_8,_c,this);
}else{
for(var s in _8){
if(_8.hasOwnProperty(s)){
_c.call(this,_8[s],s);
}
}
}
return _a.join("");
},_generateRepeat:function(_f,_10){
var _11=(this.classMapping&&this.classMapping["Heading"])?this.classMapping["Heading"]:this._defaultClassMapping["Heading"];
return "<div data-dojo-type=\"dojox/mvc/Group\" data-dojo-props=\"target: at('rel:', '"+_10+"')\" + id=\""+this.id+"_r"+this._counter++ +"\">"+"<div class=\""+_11+"\">"+_10+"</div>"+this._generateBody(_f,true)+"</div>";
},_generateGroup:function(_12,_13,_14){
var _15=["<div data-dojo-type=\"dojox/mvc/Group\" data-dojo-props=\"target: at('rel:', '"+_13+"')\" + id=\""+this.id+"_g"+this._counter++ +"\">"];
if(!_14){
var _16=(this.classMapping&&this.classMapping["Heading"])?this.classMapping["Heading"]:this._defaultClassMapping["Heading"];
_15.push("<div class=\""+_16+"\">"+_13+"</div>");
}
_15.push(this._generateBody(_12)+"</div>");
return _15.join("");
},_generateTextBox:function(_17,_18){
var _19=this.idNameMapping?this.idNameMapping["String"]:this._defaultIdNameMapping["String"];
_19=_19+this._counter++;
var _1a=this.widgetMapping?this.widgetMapping["String"]:this._defaultWidgetMapping["String"];
var _1b=(this.classMapping&&this.classMapping["Label"])?this.classMapping["Label"]:this._defaultClassMapping["Label"];
var _1c=(this.classMapping&&this.classMapping["String"])?this.classMapping["String"]:this._defaultClassMapping["String"];
var _1d=(this.classMapping&&this.classMapping["Row"])?this.classMapping["Row"]:this._defaultClassMapping["Row"];
var _1e="value: at('rel:"+(_18&&_17||"")+"', '"+(_18?"value":_17)+"')";
return "<div class=\""+_1d+"\">"+"<label class=\""+_1b+"\">"+_17+":</label>"+"<input class=\""+_1c+"\" data-dojo-type=\""+_1a+"\""+" data-dojo-props=\"name: '"+_19+"', "+_1e+"\" id=\""+_19+"\"></input>"+"</div>";
}});
});
