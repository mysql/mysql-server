//>>built
define("dojox/mobile/Switch",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/touch","dijit/_Contained","dijit/_WidgetBase","./sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _3("dojox.mobile.Switch",[_b,_a],{value:"on",name:"",leftLabel:"ON",rightLabel:"OFF",shape:"mblSwDefaultShape",tabIndex:"0",_setTabIndexAttr:"",baseClass:"mblSwitch",role:"",_createdMasks:[],buildRendering:function(){
this.domNode=(this.srcNodeRef&&this.srcNodeRef.tagName==="SPAN")?this.srcNodeRef:_7.create("span");
this.inherited(arguments);
var c=(this.srcNodeRef&&this.srcNodeRef.className)||this.className||this["class"];
if((c=c.match(/mblSw.*Shape\d*/))){
this.shape=c;
}
_6.add(this.domNode,this.shape);
var _d=this.name?" name=\""+this.name+"\"":"";
this.domNode.innerHTML="<div class=\"mblSwitchInner\">"+"<div class=\"mblSwitchBg mblSwitchBgLeft\">"+"<div class=\"mblSwitchText mblSwitchTextLeft\"></div>"+"</div>"+"<div class=\"mblSwitchBg mblSwitchBgRight\">"+"<div class=\"mblSwitchText mblSwitchTextRight\"></div>"+"</div>"+"<div class=\"mblSwitchKnob\"></div>"+"<input type=\"hidden\""+_d+"></div>"+"</div>";
var n=this.inner=this.domNode.firstChild;
this.left=n.childNodes[0];
this.right=n.childNodes[1];
this.knob=n.childNodes[2];
this.input=n.childNodes[3];
},postCreate:function(){
this._clickHandle=this.connect(this.domNode,"onclick","_onClick");
this._keydownHandle=this.connect(this.domNode,"onkeydown","_onClick");
this._startHandle=this.connect(this.domNode,_9.press,"onTouchStart");
this._initialValue=this.value;
},_changeState:function(_e,_f){
var on=(_e==="on");
this.left.style.display="";
this.right.style.display="";
this.inner.style.left="";
if(_f){
_6.add(this.domNode,"mblSwitchAnimation");
}
_6.remove(this.domNode,on?"mblSwitchOff":"mblSwitchOn");
_6.add(this.domNode,on?"mblSwitchOn":"mblSwitchOff");
var _10=this;
setTimeout(function(){
_10.left.style.display=on?"":"none";
_10.right.style.display=!on?"":"none";
_6.remove(_10.domNode,"mblSwitchAnimation");
},_f?300:0);
},_createMaskImage:function(){
if(this._hasMaskImage){
return;
}
this._width=this.domNode.offsetWidth-this.knob.offsetWidth;
this._hasMaskImage=true;
if(!_c("webkit")){
return;
}
var _11=_8.get(this.left,"borderTopLeftRadius");
if(_11=="0px"){
return;
}
var _12=_11.split(" ");
var rx=parseFloat(_12[0]),ry=(_12.length==1)?rx:parseFloat(_12[1]);
var w=this.domNode.offsetWidth,h=this.domNode.offsetHeight;
var id=(this.shape+"Mask"+w+h+rx+ry).replace(/\./,"_");
if(!this._createdMasks[id]){
this._createdMasks[id]=1;
var ctx=_5.doc.getCSSCanvasContext("2d",id,w,h);
ctx.fillStyle="#000000";
ctx.beginPath();
if(rx==ry){
ctx.moveTo(rx,0);
ctx.arcTo(0,0,0,rx,rx);
ctx.lineTo(0,h-rx);
ctx.arcTo(0,h,rx,h,rx);
ctx.lineTo(w-rx,h);
ctx.arcTo(w,h,w,rx,rx);
ctx.lineTo(w,rx);
ctx.arcTo(w,0,w-rx,0,rx);
}else{
var pi=Math.PI;
ctx.scale(1,ry/rx);
ctx.moveTo(rx,0);
ctx.arc(rx,rx,rx,1.5*pi,0.5*pi,true);
ctx.lineTo(w-rx,2*rx);
ctx.arc(w-rx,rx,rx,0.5*pi,1.5*pi,true);
}
ctx.closePath();
ctx.fill();
}
this.domNode.style.webkitMaskImage="-webkit-canvas("+id+")";
},_onClick:function(e){
if(e&&e.type==="keydown"&&e.keyCode!==13){
return;
}
if(this.onClick(e)===false){
return;
}
if(this._moved){
return;
}
this.value=this.input.value=(this.value=="on")?"off":"on";
this._changeState(this.value,true);
this.onStateChanged(this.value);
},onClick:function(){
},onTouchStart:function(e){
this._moved=false;
this.innerStartX=this.inner.offsetLeft;
if(!this._conn){
this._conn=[this.connect(this.inner,_9.move,"onTouchMove"),this.connect(this.inner,_9.release,"onTouchEnd")];
}
this.touchStartX=e.touches?e.touches[0].pageX:e.clientX;
this.left.style.display="";
this.right.style.display="";
_4.stop(e);
this._createMaskImage();
},onTouchMove:function(e){
e.preventDefault();
var dx;
if(e.targetTouches){
if(e.targetTouches.length!=1){
return;
}
dx=e.targetTouches[0].clientX-this.touchStartX;
}else{
dx=e.clientX-this.touchStartX;
}
var pos=this.innerStartX+dx;
var d=10;
if(pos<=-(this._width-d)){
pos=-this._width;
}
if(pos>=-d){
pos=0;
}
this.inner.style.left=pos+"px";
if(Math.abs(dx)>d){
this._moved=true;
}
},onTouchEnd:function(e){
_1.forEach(this._conn,_2.disconnect);
this._conn=null;
if(this.innerStartX==this.inner.offsetLeft){
if(_c("touch")&&!(_c("android")>=4.1)){
var ev=_5.doc.createEvent("MouseEvents");
ev.initEvent("click",true,true);
this.inner.dispatchEvent(ev);
}
return;
}
var _13=(this.inner.offsetLeft<-(this._width/2))?"off":"on";
this._changeState(_13,true);
if(_13!=this.value){
this.value=this.input.value=_13;
this.onStateChanged(_13);
}
},onStateChanged:function(_14){
},_setValueAttr:function(_15){
this._changeState(_15,false);
if(this.value!=_15){
this.onStateChanged(_15);
}
this.value=this.input.value=_15;
},_setLeftLabelAttr:function(_16){
this.leftLabel=_16;
this.left.firstChild.innerHTML=this._cv?this._cv(_16):_16;
},_setRightLabelAttr:function(_17){
this.rightLabel=_17;
this.right.firstChild.innerHTML=this._cv?this._cv(_17):_17;
},reset:function(){
this.set("value",this._initialValue);
}});
});
