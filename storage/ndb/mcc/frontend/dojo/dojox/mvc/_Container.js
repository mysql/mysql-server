//>>built
define("dojox/mvc/_Container",["dojo/_base/declare","dojo/_base/lang","dojo/when","dijit/_WidgetBase","dojo/regexp"],function(_1,_2,_3,_4,_5){
return _1("dojox.mvc._Container",_4,{stopParser:true,exprchar:"$",templateString:"",inlineTemplateString:"",_containedWidgets:[],_parser:null,_createBody:function(){
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
var _6=this;
if(this._parser){
return _3(this._parser.parse(this.srcNodeRef,{template:true,inherited:{dir:this.dir,lang:this.lang},propsThis:this,scope:"dojo"}),function(_7){
_6._containedWidgets=_7;
});
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
},_exprRepl:function(_8){
var _9=this,_a=function(_b,_c){
if(!_b){
return "";
}
var _d=_b.substr(2);
_d=_d.substr(0,_d.length-1);
with(_9){
return eval(_d);
}
};
_a=_2.hitch(this,_a);
return _8.replace(new RegExp(_5.escapeString(this.exprchar)+"({.*?})","g"),function(_e,_f,_10){
return _a(_e,_f).toString();
});
}});
});
