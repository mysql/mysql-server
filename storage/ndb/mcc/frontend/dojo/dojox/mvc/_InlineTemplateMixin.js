//>>built
define("dojox/mvc/_InlineTemplateMixin",["dojo/_base/declare","dojo/_base/lang","dojo/has"],function(_1,_2,_3){
_3.add("dom-qsa",!!document.createElement("div").querySelectorAll);
return _1("dojox.mvc._InlineTemplateMixin",null,{buildRendering:function(){
var _4=this.srcNodeRef;
if(_4){
var _5=_3("dom-qsa")?_4.querySelectorAll("script[type='dojox/mvc/InlineTemplate']"):_4.getElementsByTagName("script"),_6=[];
for(var i=0,l=_5.length;i<l;++i){
if(!_3("dom-qsa")&&_5[i].getAttribute("type")!="dojox/mvc/InlineTemplate"){
continue;
}
_6.push(_5[i].innerHTML);
}
var _7=_2.trim(_6.join(""));
if(_7){
this.templateString=_7;
}
}
this.inherited(arguments);
}});
});
