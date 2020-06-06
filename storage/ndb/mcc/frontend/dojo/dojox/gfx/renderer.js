//>>built
define("dojox/gfx/renderer",["./_base","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/_base/config"],function(g,_1,_2,_3,_4){
var _5=null;
_2.add("vml",function(_6,_7,_8){
_8.innerHTML="<v:shape adj=\"1\"/>";
var _9=("adj" in _8.firstChild);
_8.innerHTML="";
return _9;
});
return {load:function(id,_a,_b){
if(_5&&id!="force"){
_b(_5);
return;
}
var _c=_4.forceGfxRenderer,_d=!_c&&(_1.isString(_4.gfxRenderer)?_4.gfxRenderer:"svg,vml,canvas,silverlight").split(","),_e,_f;
while(!_c&&_d.length){
switch(_d.shift()){
case "svg":
if("SVGAngle" in _3.global){
_c="svg";
}
break;
case "vml":
if(_2("vml")){
_c="vml";
}
break;
case "silverlight":
try{
if(_2("ie")){
_e=new ActiveXObject("AgControl.AgControl");
if(_e&&_e.IsVersionSupported("1.0")){
_f=true;
}
}else{
if(navigator.plugins["Silverlight Plug-In"]){
_f=true;
}
}
}
catch(e){
_f=false;
}
finally{
_e=null;
}
if(_f){
_c="silverlight";
}
break;
case "canvas":
if(_3.global.CanvasRenderingContext2D){
_c="canvas";
}
break;
}
}
if(_c==="canvas"&&_4.canvasEvents!==false){
_c="canvasWithEvents";
}
if(_4.isDebug){
}
function _10(){
_a(["dojox/gfx/"+_c],function(_11){
g.renderer=_c;
_5=_11;
_b(_11);
});
};
if(_c=="svg"&&typeof window.svgweb!="undefined"){
window.svgweb.addOnLoad(_10);
}else{
_10();
}
}};
});
