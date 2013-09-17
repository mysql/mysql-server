//>>built
define("dojox/mvc/Output",["dojo/_base/declare","dojo/_base/lang","dojo/dom","dijit/_WidgetBase"],function(_1,_2,_3,_4){
return _1("dojox.mvc.Output",[_4],{templateString:"",postscript:function(_5,_6){
this.srcNodeRef=_3.byId(_6);
if(this.srcNodeRef){
this.templateString=this.srcNodeRef.innerHTML;
this.srcNodeRef.innerHTML="";
}
this.inherited(arguments);
},set:function(_7,_8){
this.inherited(arguments);
if(_7==="value"){
this._output();
}
},_updateBinding:function(_9,_a,_b){
this.inherited(arguments);
this._output();
},_output:function(){
var _c=this.srcNodeRef||this.domNode;
_c.innerHTML=this.templateString?this._exprRepl(this.templateString):this.value;
},_exprRepl:function(_d){
var _e=this,_f=function(_10,key){
if(!_10){
return "";
}
var exp=_10.substr(2);
exp=exp.substr(0,exp.length-1);
with(_e){
return eval(exp)||"";
}
};
_f=_2.hitch(this,_f);
return _d.replace(/\$\{.*?\}/g,function(_11,key,_12){
return _f(_11,key).toString();
});
}});
});
