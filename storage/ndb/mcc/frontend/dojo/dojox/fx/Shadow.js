//>>built
define("dojox/fx/Shadow",["dojo/_base/kernel","dojo/_base/query","dojo/_base/lang","dojo/_base/declare","dojo/_base/sniff","dojo/dom-construct","dojo/dom-class","dojo/dom-geometry","dojo/_base/fx","dojo/fx","dijit/_Widget","dojo/NodeList-fx"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
_1.experimental("dojox.fx.Shadow");
return _4("dojox.fx.Shadow",_b,{shadowPng:_1.moduleUrl("dojox.fx","resources/shadow"),shadowThickness:7,shadowOffset:3,opacity:0.75,animate:false,node:null,startup:function(){
this.inherited(arguments);
this.node.style.position="relative";
this.pieces={};
var x1=-1*this.shadowThickness;
var y0=this.shadowOffset;
var y1=this.shadowOffset+this.shadowThickness;
this._makePiece("tl","top",y0,"left",x1);
this._makePiece("l","top",y1,"left",x1,"scale");
this._makePiece("tr","top",y0,"left",0);
this._makePiece("r","top",y1,"left",0,"scale");
this._makePiece("bl","top",0,"left",x1);
this._makePiece("b","top",0,"left",0,"crop");
this._makePiece("br","top",0,"left",0);
this.nodeList=_2(".shadowPiece",this.node);
this.setOpacity(this.opacity);
this.resize();
},_makePiece:function(_d,_e,_f,_10,_11,_12){
var img;
var url=this.shadowPng+_d.toUpperCase()+".png";
if(_5("ie")<7){
img=_6.create("div");
img.style.filter="progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+url+"'"+(_12?", sizingMethod='"+_12+"'":"")+")";
}else{
img=_6.create("img",{src:url});
}
img.style.position="absolute";
img.style[_e]=_f+"px";
img.style[_10]=_11+"px";
img.style.width=this.shadowThickness+"px";
img.style.height=this.shadowThickness+"px";
_7.add(img,"shadowPiece");
this.pieces[_d]=img;
this.node.appendChild(img);
},setOpacity:function(n,_13){
if(_5("ie")){
return;
}
if(!_13){
_13={};
}
if(this.animate){
var _14=[];
this.nodeList.forEach(function(_15){
_14.push(_9._fade(_3.mixin(_13,{node:_15,end:n})));
});
_a.combine(_14).play();
}else{
this.nodeList.style("opacity",n);
}
},setDisabled:function(_16){
if(_16){
if(this.disabled){
return;
}
if(this.animate){
this.nodeList.fadeOut().play();
}else{
this.nodeList.style("visibility","hidden");
}
this.disabled=true;
}else{
if(!this.disabled){
return;
}
if(this.animate){
this.nodeList.fadeIn().play();
}else{
this.nodeList.style("visibility","visible");
}
this.disabled=false;
}
},resize:function(_17){
var x;
var y;
if(_17){
x=_17.x;
y=_17.y;
}else{
var co=_8.position(this.node);
x=co.w;
y=co.h;
}
var _18=y-(this.shadowOffset+this.shadowThickness);
if(_18<0){
_18=0;
}
if(y<1){
y=1;
}
if(x<1){
x=1;
}
with(this.pieces){
l.style.height=_18+"px";
r.style.height=_18+"px";
b.style.width=x+"px";
bl.style.top=y+"px";
b.style.top=y+"px";
br.style.top=y+"px";
tr.style.left=x+"px";
r.style.left=x+"px";
br.style.left=x+"px";
}
}});
});
