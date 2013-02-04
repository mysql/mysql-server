//>>built
define("dojox/mvc/_Container",["dojo/_base/declare","dojo/_base/lang","dijit/_WidgetBase","dojo/regexp"],function(_1,_2,_3,_4){
return _1("dojox.mvc._Container",[_3],{stopParser:true,exprchar:"$",templateString:"",_containedWidgets:[],_parser:null,_createBody:function(){
if(!this._parser){
try{
this._parser=require("dojo/parser");
}
catch(e){
try{
this._parser=require("dojox/mobile/parser");
}
catch(e){
console.error("Add explicit require(['dojo/parser']) or explicit require(['dojox/mobile/parser']), one of the parsers is required!");
}
}
}
if(this._parser){
this._containedWidgets=this._parser.parse(this.srcNodeRef,{template:true,inherited:{dir:this.dir,lang:this.lang},propsThis:this,scope:"dojo"});
}
},_destroyBody:function(){
if(this._containedWidgets&&this._containedWidgets.length>0){
for(var n=this._containedWidgets.length-1;n>-1;n--){
var w=this._containedWidgets[n];
if(w&&!w._destroyed&&w.destroy){
w.destroy();
}
}
}
},_exprRepl:function(_5){
var _6=this,_7=function(_8,_9){
if(!_8){
return "";
}
var _a=_8.substr(2);
_a=_a.substr(0,_a.length-1);
with(_6){
return eval(_a);
}
};
_7=_2.hitch(this,_7);
return _5.replace(new RegExp(_4.escapeString(this.exprchar)+"({.*?})","g"),function(_b,_c,_d){
return _7(_b,_c).toString();
});
}});
});
