//>>built
define("dojox/charting/axis2d/common",["dojo/_base/lang","dojo/_base/window","dojo/dom-geometry","dojox/gfx","dojo/has"],function(_1,_2,_3,g,_4){
var _5=_1.getObject("dojox.charting.axis2d.common",true);
var _6=function(s){
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
var _7=function(n){
if(n["getBoundingClientRect"]){
var _8=n.getBoundingClientRect();
return _8.width||(_8.right-_8.left);
}else{
return _3.getMarginBox(n).w;
}
};
return _1.mixin(_5,{createText:{gfx:function(_9,_a,x,y,_b,_c,_d,_e){
return _a.createText({x:x,y:y,text:_c,align:_b}).setFont(_d).setFill(_e);
},html:function(_f,_10,x,y,_11,_12,_13,_14,_15){
var p=_2.doc.createElement("div"),s=p.style,_16;
if(_f.getTextDir){
p.dir=_f.getTextDir(_12);
}
_6(s);
s.font=_13;
p.innerHTML=String(_12).replace(/\s/g,"&nbsp;");
s.color=_14;
s.position="absolute";
s.left="-10000px";
_2.body().appendChild(p);
var _17=g.normalizedLength(g.splitFontString(_13).size);
if(!_15){
_16=_7(p);
}
if(p.dir=="rtl"){
x+=_15?_15:_16;
}
_2.body().removeChild(p);
s.position="relative";
if(_15){
s.width=_15+"px";
switch(_11){
case "middle":
s.textAlign="center";
s.left=(x-_15/2)+"px";
break;
case "end":
s.textAlign="right";
s.left=(x-_15)+"px";
break;
default:
s.left=x+"px";
s.textAlign="left";
break;
}
}else{
switch(_11){
case "middle":
s.left=Math.floor(x-_16/2)+"px";
break;
case "end":
s.left=Math.floor(x-_16)+"px";
break;
default:
s.left=Math.floor(x)+"px";
break;
}
}
s.top=Math.floor(y-_17)+"px";
s.whiteSpace="nowrap";
var _18=_2.doc.createElement("div"),w=_18.style;
_6(w);
w.width="0px";
w.height="0px";
_18.appendChild(p);
_f.node.insertBefore(_18,_f.node.firstChild);
if(_4("dojo-bidi")){
_f.htmlElementsRegistry.push([_18,x,y,_11,_12,_13,_14]);
}
return _18;
}}});
});
