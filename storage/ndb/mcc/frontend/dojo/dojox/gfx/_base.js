//>>built
define("dojox/gfx/_base",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/Color","dojo/_base/sniff","dojo/_base/window","dojo/_base/array","dojo/dom","dojo/dom-construct","dojo/dom-geometry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var g=_2.getObject("dojox.gfx",true),b=g._base={};
g._hasClass=function(_a,_b){
var _c=_a.getAttribute("className");
return _c&&(" "+_c+" ").indexOf(" "+_b+" ")>=0;
};
g._addClass=function(_d,_e){
var _f=_d.getAttribute("className")||"";
if(!_f||(" "+_f+" ").indexOf(" "+_e+" ")<0){
_d.setAttribute("className",_f+(_f?" ":"")+_e);
}
};
g._removeClass=function(_10,_11){
var cls=_10.getAttribute("className");
if(cls){
_10.setAttribute("className",cls.replace(new RegExp("(^|\\s+)"+_11+"(\\s+|$)"),"$1$2"));
}
};
b._getFontMeasurements=function(){
var _12={"1em":0,"1ex":0,"100%":0,"12pt":0,"16px":0,"xx-small":0,"x-small":0,"small":0,"medium":0,"large":0,"x-large":0,"xx-large":0};
var p,_13;
if(_4("ie")){
_13=_5.doc.documentElement.style.fontSize||"";
if(!_13){
_5.doc.documentElement.style.fontSize="100%";
}
}
var div=_8.create("div",{style:{position:"absolute",left:"0",top:"-100px",width:"30px",height:"1000em",borderWidth:"0",margin:"0",padding:"0",outline:"none",lineHeight:"1",overflow:"hidden"}},_5.body());
for(p in _12){
div.style.fontSize=p;
_12[p]=Math.round(div.offsetHeight*12/16)*16/12/1000;
}
if(_4("ie")){
_5.doc.documentElement.style.fontSize=_13;
}
_5.body().removeChild(div);
return _12;
};
var _14=null;
b._getCachedFontMeasurements=function(_15){
if(_15||!_14){
_14=b._getFontMeasurements();
}
return _14;
};
var _16=null,_17={};
b._getTextBox=function(_18,_19,_1a){
var m,s,al=arguments.length;
var i;
if(!_16){
_16=_8.create("div",{style:{position:"absolute",top:"-10000px",left:"0"}},_5.body());
}
m=_16;
m.className="";
s=m.style;
s.borderWidth="0";
s.margin="0";
s.padding="0";
s.outline="0";
if(al>1&&_19){
for(i in _19){
if(i in _17){
continue;
}
s[i]=_19[i];
}
}
if(al>2&&_1a){
m.className=_1a;
}
m.innerHTML=_18;
if(m["getBoundingClientRect"]){
var bcr=m.getBoundingClientRect();
return {l:bcr.left,t:bcr.top,w:bcr.width||(bcr.right-bcr.left),h:bcr.height||(bcr.bottom-bcr.top)};
}else{
return _9.getMarginBox(m);
}
};
var _1b=0;
b._getUniqueId=function(){
var id;
do{
id=_1._scopeName+"xUnique"+(++_1b);
}while(_7.byId(id));
return id;
};
_2.mixin(g,{defaultPath:{type:"path",path:""},defaultPolyline:{type:"polyline",points:[]},defaultRect:{type:"rect",x:0,y:0,width:100,height:100,r:0},defaultEllipse:{type:"ellipse",cx:0,cy:0,rx:200,ry:100},defaultCircle:{type:"circle",cx:0,cy:0,r:100},defaultLine:{type:"line",x1:0,y1:0,x2:100,y2:100},defaultImage:{type:"image",x:0,y:0,width:0,height:0,src:""},defaultText:{type:"text",x:0,y:0,text:"",align:"start",decoration:"none",rotated:false,kerning:true},defaultTextPath:{type:"textpath",text:"",align:"start",decoration:"none",rotated:false,kerning:true},defaultStroke:{type:"stroke",color:"black",style:"solid",width:1,cap:"butt",join:4},defaultLinearGradient:{type:"linear",x1:0,y1:0,x2:100,y2:100,colors:[{offset:0,color:"black"},{offset:1,color:"white"}]},defaultRadialGradient:{type:"radial",cx:0,cy:0,r:100,colors:[{offset:0,color:"black"},{offset:1,color:"white"}]},defaultPattern:{type:"pattern",x:0,y:0,width:0,height:0,src:""},defaultFont:{type:"font",style:"normal",variant:"normal",weight:"normal",size:"10pt",family:"serif"},getDefault:(function(){
var _1c={};
return function(_1d){
var t=_1c[_1d];
if(t){
return new t();
}
t=_1c[_1d]=new Function();
t.prototype=g["default"+_1d];
return new t();
};
})(),normalizeColor:function(_1e){
return (_1e instanceof _3)?_1e:new _3(_1e);
},normalizeParameters:function(_1f,_20){
var x;
if(_20){
var _21={};
for(x in _1f){
if(x in _20&&!(x in _21)){
_1f[x]=_20[x];
}
}
}
return _1f;
},makeParameters:function(_22,_23){
var i=null;
if(!_23){
return _2.delegate(_22);
}
var _24={};
for(i in _22){
if(!(i in _24)){
_24[i]=_2.clone((i in _23)?_23[i]:_22[i]);
}
}
return _24;
},formatNumber:function(x,_25){
var val=x.toString();
if(val.indexOf("e")>=0){
val=x.toFixed(4);
}else{
var _26=val.indexOf(".");
if(_26>=0&&val.length-_26>5){
val=x.toFixed(4);
}
}
if(x<0){
return val;
}
return _25?" "+val:val;
},makeFontString:function(_27){
return _27.style+" "+_27.variant+" "+_27.weight+" "+_27.size+" "+_27.family;
},splitFontString:function(str){
var _28=g.getDefault("Font");
var t=str.split(/\s+/);
do{
if(t.length<5){
break;
}
_28.style=t[0];
_28.variant=t[1];
_28.weight=t[2];
var i=t[3].indexOf("/");
_28.size=i<0?t[3]:t[3].substring(0,i);
var j=4;
if(i<0){
if(t[4]=="/"){
j=6;
}else{
if(t[4].charAt(0)=="/"){
j=5;
}
}
}
if(j<t.length){
_28.family=t.slice(j).join(" ");
}
}while(false);
return _28;
},cm_in_pt:72/2.54,mm_in_pt:7.2/2.54,px_in_pt:function(){
return g._base._getCachedFontMeasurements()["12pt"]/12;
},pt2px:function(len){
return len*g.px_in_pt();
},px2pt:function(len){
return len/g.px_in_pt();
},normalizedLength:function(len){
if(len.length===0){
return 0;
}
if(len.length>2){
var _29=g.px_in_pt();
var val=parseFloat(len);
switch(len.slice(-2)){
case "px":
return val;
case "pt":
return val*_29;
case "in":
return val*72*_29;
case "pc":
return val*12*_29;
case "mm":
return val*g.mm_in_pt*_29;
case "cm":
return val*g.cm_in_pt*_29;
}
}
return parseFloat(len);
},pathVmlRegExp:/([A-Za-z]+)|(\d+(\.\d+)?)|(\.\d+)|(-\d+(\.\d+)?)|(-\.\d+)/g,pathSvgRegExp:/([A-Za-z])|(\d+(\.\d+)?)|(\.\d+)|(-\d+(\.\d+)?)|(-\.\d+)/g,equalSources:function(a,b){
return a&&b&&a===b;
},switchTo:function(_2a){
var ns=typeof _2a=="string"?g[_2a]:_2a;
if(ns){
_6.forEach(["Group","Rect","Ellipse","Circle","Line","Polyline","Image","Text","Path","TextPath","Surface","createSurface","fixTarget"],function(_2b){
g[_2b]=ns[_2b];
});
}
}});
return g;
});
