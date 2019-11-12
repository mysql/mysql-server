//>>built
define("dojox/charting/axis2d/common",["dojo/_base/lang","dojo/_base/window","dojo/dom-geometry","dojox/gfx"],function(_1,_2,_3,g){
var _4=_1.getObject("dojox.charting.axis2d.common",true);
var _5=function(s){
s.marginLeft="0px";
s.marginTop="0px";
s.marginRight="0px";
s.marginBottom="0px";
s.paddingLeft="0px";
s.paddingTop="0px";
s.paddingRight="0px";
s.paddingBottom="0px";
s.borderLeftWidth="0px";
s.borderTopWidth="0px";
s.borderRightWidth="0px";
s.borderBottomWidth="0px";
};
var _6=function(n){
if(n["getBoundingClientRect"]){
var _7=n.getBoundingClientRect();
return _7.width||(_7.right-_7.left);
}else{
return _3.getMarginBox(n).w;
}
};
return _1.mixin(_4,{createText:{gfx:function(_8,_9,x,y,_a,_b,_c,_d){
return _9.createText({x:x,y:y,text:_b,align:_a}).setFont(_c).setFill(_d);
},html:function(_e,_f,x,y,_10,_11,_12,_13,_14){
var p=_2.doc.createElement("div"),s=p.style,_15;
if(_e.getTextDir){
p.dir=_e.getTextDir(_11);
}
_5(s);
s.font=_12;
p.innerHTML=String(_11).replace(/\s/g,"&nbsp;");
s.color=_13;
s.position="absolute";
s.left="-10000px";
_2.body().appendChild(p);
var _16=g.normalizedLength(g.splitFontString(_12).size);
if(!_14){
_15=_6(p);
}
if(p.dir=="rtl"){
x+=_14?_14:_15;
}
_2.body().removeChild(p);
s.position="relative";
if(_14){
s.width=_14+"px";
switch(_10){
case "middle":
s.textAlign="center";
s.left=(x-_14/2)+"px";
break;
case "end":
s.textAlign="right";
s.left=(x-_14)+"px";
break;
default:
s.left=x+"px";
s.textAlign="left";
break;
}
}else{
switch(_10){
case "middle":
s.left=Math.floor(x-_15/2)+"px";
break;
case "end":
s.left=Math.floor(x-_15)+"px";
break;
default:
s.left=Math.floor(x)+"px";
break;
}
}
s.top=Math.floor(y-_16)+"px";
s.whiteSpace="nowrap";
var _17=_2.doc.createElement("div"),w=_17.style;
_5(w);
w.width="0px";
w.height="0px";
_17.appendChild(p);
_e.node.insertBefore(_17,_e.node.firstChild);
return _17;
}}});
});
