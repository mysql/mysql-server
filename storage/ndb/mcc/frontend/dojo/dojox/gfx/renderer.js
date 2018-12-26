//>>built
define("dojox/gfx/renderer",["./_base","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/_base/config"],function(g,_1,_2,_3,_4){
var _5=null;
return {load:function(id,_6,_7){
if(_5&&id!="force"){
_7(_5);
return;
}
var _8=_4.forceGfxRenderer,_9=!_8&&(_1.isString(_4.gfxRenderer)?_4.gfxRenderer:"svg,vml,canvas,silverlight").split(","),_a,_b;
while(!_8&&_9.length){
switch(_9.shift()){
case "svg":
if("SVGAngle" in _3.global){
_8="svg";
}
break;
case "vml":
if(_2("ie")){
_8="vml";
}
break;
case "silverlight":
try{
if(_2("ie")){
_a=new ActiveXObject("AgControl.AgControl");
if(_a&&_a.IsVersionSupported("1.0")){
_b=true;
}
}else{
if(navigator.plugins["Silverlight Plug-In"]){
_b=true;
}
}
}
catch(e){
_b=false;
}
finally{
_a=null;
}
if(_b){
_8="silverlight";
}
break;
case "canvas":
if(_3.global.CanvasRenderingContext2D){
_8="canvas";
}
break;
}
}
if(_8==="canvas"&&_4.canvasEvents!==false){
_8="canvasWithEvents";
}
if(_4.isDebug){
}
function _c(){
_6(["dojox/gfx/"+_8],function(_d){
g.renderer=_8;
_5=_d;
_7(_d);
});
};
if(_8=="svg"&&typeof window.svgweb!="undefined"){
window.svgweb.addOnLoad(_c);
}else{
_c();
}
}};
});
