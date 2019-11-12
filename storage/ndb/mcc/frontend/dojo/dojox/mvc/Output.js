//>>built
define("dojox/mvc/Output",["dojo/_base/declare","dojo/_base/lang","dojo/dom","dijit/_WidgetBase","dojo/regexp"],function(_1,_2,_3,_4,_5){
return _1("dojox.mvc.Output",_4,{exprchar:"$",templateString:"",postscript:function(_6,_7){
this.srcNodeRef=_3.byId(_7);
if(this.srcNodeRef){
this.templateString=this.srcNodeRef.innerHTML;
this.srcNodeRef.innerHTML="";
}
this.inherited(arguments);
},set:function(_8,_9){
this.inherited(arguments);
if(_8==="value"){
this._output();
}
},_updateBinding:function(_a,_b,_c){
this.inherited(arguments);
this._output();
},_output:function(){
var _d=this.srcNodeRef||this.domNode;
_d.innerHTML=this.templateString?this._exprRepl(this.templateString):this.value;
},_exprRepl:function(_e){
var _f=this,_10=function(_11,key){
if(!_11){
return "";
}
var exp=_11.substr(2);
exp=exp.substr(0,exp.length-1);
with(_f){
var val=eval(exp);
return (val||val==0?val:"");
}
};
_10=_2.hitch(this,_10);
return _e.replace(new RegExp(_5.escapeString(this.exprchar)+"({.*?})","g"),function(_12,key,_13){
return _10(_12,key).toString();
});
}});
});
