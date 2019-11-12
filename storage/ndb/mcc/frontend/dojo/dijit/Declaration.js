//>>built
define("dijit/Declaration",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/parser","dojo/query","./_Widget","./_TemplatedMixin","./_WidgetsInTemplateMixin","dojo/NodeList-dom"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _3("dijit.Declaration",_7,{_noScript:true,stopParser:true,widgetClass:"",defaults:null,mixins:[],buildRendering:function(){
var _a=this.srcNodeRef.parentNode.removeChild(this.srcNodeRef),_b=_6("> script[type^='dojo/method']",_a).orphan(),_c=_6("> script[type^='dojo/connect']",_a).orphan(),_d=_a.nodeName;
var _e=this.defaults||{};
_1.forEach(_b,function(s){
var _f=s.getAttribute("event")||s.getAttribute("data-dojo-event"),_10=_5._functionFromScript(s);
if(_f){
_e[_f]=_10;
}else{
_c.push(s);
}
});
if(this.mixins.length){
this.mixins=_1.map(this.mixins,function(_11){
return _4.getObject(_11);
});
}else{
this.mixins=[_7,_8,_9];
}
_e._skipNodeCache=true;
_e.templateString="<"+_d+" class='"+_a.className+"'"+" data-dojo-attach-point='"+(_a.getAttribute("data-dojo-attach-point")||_a.getAttribute("dojoAttachPoint")||"")+"' data-dojo-attach-event='"+(_a.getAttribute("data-dojo-attach-event")||_a.getAttribute("dojoAttachEvent")||"")+"' >"+_a.innerHTML.replace(/\%7B/g,"{").replace(/\%7D/g,"}")+"</"+_d+">";
var wc=_3(this.widgetClass,this.mixins,_e);
_1.forEach(_c,function(s){
var evt=s.getAttribute("event")||s.getAttribute("data-dojo-event")||"postscript",_12=_5._functionFromScript(s);
_2.connect(wc.prototype,evt,_12);
});
}});
});
