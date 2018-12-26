//>>built
define("dojox/mvc/Generate",["dojo/_base/lang","dojo/_base/declare","./_Container","./Group","dijit/form/TextBox"],function(_1,_2,_3){
return _2("dojox.mvc.Generate",[_3],{_counter:0,_defaultWidgetMapping:{"String":"dijit.form.TextBox"},_defaultClassMapping:{"Label":"generate-label-cell","String":"generate-dijit-cell","Heading":"generate-heading","Row":"row"},_defaultIdNameMapping:{"String":"textbox_t"},_updateBinding:function(){
this.inherited(arguments);
this._buildContained();
},_buildContained:function(){
this._destroyBody();
this._counter=0;
this.srcNodeRef.innerHTML=this._generateBody(this.get("binding"));
this._createBody();
},_generateBody:function(_4,_5){
var _6="";
for(var _7 in _4){
if(_4[_7]&&_1.isFunction(_4[_7].toPlainObject)){
if(_4[_7].get(0)){
_6+=this._generateRepeat(_4[_7],_7);
}else{
if(_4[_7].value){
_6+=this._generateTextBox(_7);
}else{
_6+=this._generateGroup(_4[_7],_7,_5);
}
}
}
}
return _6;
},_generateRepeat:function(_8,_9){
var _a=(this.classMapping&&this.classMapping["Heading"])?this.classMapping["Heading"]:this._defaultClassMapping["Heading"];
var _b="<div data-dojo-type=\"dojox.mvc.Group\" data-dojo-props=\"ref: '"+_9+"'\" + id=\""+this.id+"_r"+this._counter++ +"\">"+"<div class=\""+_a+"\">"+_9+"</div>";
_b+=this._generateBody(_8,true);
_b+="</div>";
return _b;
},_generateGroup:function(_c,_d,_e){
var _f="<div data-dojo-type=\"dojox.mvc.Group\" data-dojo-props=\"ref: '"+_d+"'\" + id=\""+this.id+"_g"+this._counter++ +"\">";
if(!_e){
var _10=(this.classMapping&&this.classMapping["Heading"])?this.classMapping["Heading"]:this._defaultClassMapping["Heading"];
_f+="<div class=\""+_10+"\">"+_d+"</div>";
}
_f+=this._generateBody(_c);
_f+="</div>";
return _f;
},_generateTextBox:function(_11){
var _12=this.idNameMapping?this.idNameMapping["String"]:this._defaultIdNameMapping["String"];
_12=_12+this._counter++;
var _13=this.widgetMapping?this.widgetMapping["String"]:this._defaultWidgetMapping["String"];
var _14=(this.classMapping&&this.classMapping["Label"])?this.classMapping["Label"]:this._defaultClassMapping["Label"];
var _15=(this.classMapping&&this.classMapping["String"])?this.classMapping["String"]:this._defaultClassMapping["String"];
var _16=(this.classMapping&&this.classMapping["Row"])?this.classMapping["Row"]:this._defaultClassMapping["Row"];
return "<div class=\""+_16+"\">"+"<label class=\""+_14+"\">"+_11+":</label>"+"<input class=\""+_15+"\" data-dojo-type=\""+_13+"\" data-dojo-props=\"name: '"+_12+"', ref: '"+_11+"'\" id=\""+_12+"\"></input>"+"</div>";
}});
});
