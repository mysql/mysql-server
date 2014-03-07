//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/NodeList-traverse"],function(_1,_2,_3){
_2.provide("dojox.rails");
_2.require("dojo.NodeList-traverse");
_3.rails.live=function(_4,_5,fn){
if(_2.isIE&&_5.match(/^(on)?submit$/i)){
_3.rails.live(_4,"click",function(_6){
var _7=_6.target,_8=_7.tagName.toLowerCase();
if((_8=="input"||_8=="button")&&_2.attr(_7,"type").toLowerCase()=="submit"){
var _9=_2.query(_7).closest("form");
if(_9.length){
var h=_2.connect(_9[0],"submit",function(_a){
_2.disconnect(h);
fn.call(_a.target,_a);
});
}
}
});
}else{
_2.connect(_2.body(),_5,function(_b){
var nl=_2.query(_b.target).closest(_4);
if(nl.length){
fn.call(nl[0],_b);
}
});
}
};
_2.ready((function(d,dr,dg){
return function(){
var q=d.query,_c=dr.live,_d=q("meta[name=csrf-token]").attr("content"),_e=q("meta[name=csrf-param]").attr("content");
var _f=function(url,_10){
var _11="<form style=\"display:none\" method=\"post\" action=\""+url+"\">"+"<input type=\"hidden\" name=\"_method\" value=\""+_10+"\" />"+"<input type=\"hidden\" name=\""+_e+"\" value=\""+_d+"\" />"+"</form>";
return _2.place(_11,_2.body());
};
var _12=function(_13){
d.forEach(_13,function(_14){
if(!d.attr(_14,"disabled")){
var _15=_14.tagName.toLowerCase()=="input"?"value":"innerHTML";
var _16=d.attr(_14,"data-disable-with");
var _17=d.attr(_14,_15);
d.attr(_14,"disabled",true);
d.attr(_14,"data-original-value",_17);
d.attr(_14,_15,_16);
}
});
};
var _18={"text":"text","json":"application/json","json-comment-optional":"text","json-comment-filtered":"text","javascript":"application/javascript","xml":"text/xml"};
var _19=function(evt){
var el=evt.target,tag=el.tagName.toLowerCase();
var _1a=tag.toLowerCase()=="form"?d.formToObject(el):{},_1b=d.attr(el,"data-type")||"javascript",_1c=(d.attr(el,"method")||d.attr(el,"data-method")||"get").toLowerCase(),url=d.attr(el,"action")||d.attr(el,"href");
if(tag!="form"&&_1c!="get"){
el=_f(url,_1c);
_1c="POST";
}
evt.preventDefault();
d.publish("ajax:before",[el]);
var _1d=d.xhr(_1c,{url:url,headers:{"Accept":_18[_1b]},content:_1a,handleAs:_1b,load:function(_1e,_1f){
d.publish("ajax:success",[el,_1e,_1f]);
},error:function(_20,_21){
d.publish("ajax:failure",[el,_20,_21]);
},handle:function(_22,_23){
d.publish("ajax:complete",[el,_22,_23]);
}});
d.publish("ajax:after",[el]);
};
var _24=function(el){
q("*[data-disable-with][disabled]",el).forEach(function(_25){
var _26=_25.tagName.toLowerCase()=="input"?"value":"innerHTML";
var _27=d.attr(_25,"data-original-value");
d.attr(_25,"disabled",false);
d.attr(_25,"data-original-value",null);
d.attr(_25,_26,_27);
});
};
var _28=function(evt){
var el=evt.target,_29=_f(el.href,_2.attr(el,"data-method"));
evt.preventDefault();
_29.submit();
};
var _2a=function(evt){
var el=evt.target,_2b=q("*[data-disable-with]",el);
if(_2b.length){
_12(_2b);
}
if(d.attr(el,"data-remote")){
evt.preventDefault();
_19(evt);
}
};
var _2c=function(evt){
var _2d=dg.confirm(d.attr(evt.target,"data-confirm"));
if(!_2d){
evt.preventDefault();
}else{
if(d.attr(evt.target,"data-remote")){
_19(evt);
}
}
};
_c("*[data-confirm]","click",_2c);
d.subscribe("ajax:complete",_24);
_c("a[data-remote]:not([data-confirm])","click",_19);
_c("a[data-method]:not([data-remote])","click",_28);
_c("form","submit",_2a);
};
})(_2,_3.rails,_2.global));
});
