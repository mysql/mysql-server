//>>built
define("dojox/mobile/_maskUtils",["dojo/_base/window","dojo/dom-style","./sniff"],function(_1,_2,_3){
_3.add("mask-image-css",function(_4,_5,_6){
return typeof _5.getCSSCanvasContext==="function"&&typeof _6.style.webkitMaskImage!=="undefined";
});
_3.add("mask-image",function(){
return _3("mask-image-css")||_3("svg");
});
var _7={};
return {createRoundMask:function(_8,x,y,r,b,w,h,rx,ry,e){
var tw=x+w+r;
var th=y+h+b;
if(_3("mask-image-css")){
var id=("DojoMobileMask"+x+y+w+h+rx+ry).replace(/\./g,"_");
if(!_7[id]){
_7[id]=1;
var _9=_1.doc.getCSSCanvasContext("2d",id,tw,th);
_9.beginPath();
if(rx==ry){
if(rx==2&&w==5){
_9.fillStyle="rgba(0,0,0,0.5)";
_9.fillRect(1,0,3,2);
_9.fillRect(0,1,5,1);
_9.fillRect(0,h-2,5,1);
_9.fillRect(1,h-1,3,2);
_9.fillStyle="rgb(0,0,0)";
_9.fillRect(0,2,5,h-4);
}else{
if(rx==2&&h==5){
_9.fillStyle="rgba(0,0,0,0.5)";
_9.fillRect(0,1,2,3);
_9.fillRect(1,0,1,5);
_9.fillRect(w-2,0,1,5);
_9.fillRect(w-1,1,2,3);
_9.fillStyle="rgb(0,0,0)";
_9.fillRect(2,0,w-4,5);
}else{
_9.fillStyle="#000000";
_9.moveTo(x+rx,y);
_9.arcTo(x,y,x,y+rx,rx);
_9.lineTo(x,y+h-rx);
_9.arcTo(x,y+h,x+rx,y+h,rx);
_9.lineTo(x+w-rx,y+h);
_9.arcTo(x+w,y+h,x+w,y+rx,rx);
_9.lineTo(x+w,y+rx);
_9.arcTo(x+w,y,x+w-rx,y,rx);
}
}
}else{
var pi=Math.PI;
_9.scale(1,ry/rx);
_9.moveTo(x+rx,y);
_9.arc(x+rx,y+rx,rx,1.5*pi,0.5*pi,true);
_9.lineTo(x+w-rx,y+2*rx);
_9.arc(x+w-rx,y+rx,rx,0.5*pi,1.5*pi,true);
}
_9.closePath();
_9.fill();
}
_8.style.webkitMaskImage="-webkit-canvas("+id+")";
}else{
if(_3("svg")){
if(_8._svgMask){
_8.removeChild(_8._svgMask);
}
var bg=null;
for(var p=_8.parentNode;p;p=p.parentNode){
bg=_2.getComputedStyle(p).backgroundColor;
if(bg&&bg!="transparent"&&!bg.match(/rgba\(.*,\s*0\s*\)/)){
break;
}
}
var _a="http://www.w3.org/2000/svg";
var _b=_1.doc.createElementNS(_a,"svg");
_b.setAttribute("width",tw);
_b.setAttribute("height",th);
_b.style.position="absolute";
_b.style.pointerEvents="none";
_b.style.opacity="1";
_b.style.zIndex="2147483647";
var _c=_1.doc.createElementNS(_a,"path");
e=e||0;
rx+=e;
ry+=e;
var d=" M"+(x+rx-e)+","+(y-e)+" a"+rx+","+ry+" 0 0,0 "+(-rx)+","+ry+" v"+(-ry)+" h"+rx+" Z"+" M"+(x-e)+","+(y+h-ry+e)+" a"+rx+","+ry+" 0 0,0 "+rx+","+ry+" h"+(-rx)+" v"+(-ry)+" z"+" M"+(x+w-rx+e)+","+(y+h+e)+" a"+rx+","+ry+" 0 0,0 "+rx+","+(-ry)+" v"+ry+" h"+(-rx)+" z"+" M"+(x+w+e)+","+(y+ry-e)+" a"+rx+","+ry+" 0 0,0 "+(-rx)+","+(-ry)+" h"+rx+" v"+ry+" z";
if(y>0){
d+=" M0,0 h"+tw+" v"+y+" h"+(-tw)+" z";
}
if(b>0){
d+=" M0,"+(y+h)+" h"+tw+" v"+b+" h"+(-tw)+" z";
}
_c.setAttribute("d",d);
_c.setAttribute("fill",bg);
_c.setAttribute("stroke",bg);
_c.style.opacity="1";
_b.appendChild(_c);
_8._svgMask=_b;
_8.appendChild(_b);
}
}
}};
});
