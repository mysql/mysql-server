//>>built
define("dojox/mobile/Switch",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/_base/window","dojo/dom-class","dijit/_Contained","dijit/_WidgetBase","./sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _3("dojox.mobile.Switch",[_8,_7],{value:"on",name:"",leftLabel:"ON",rightLabel:"OFF",_width:53,buildRendering:function(){
this.domNode=_5.doc.createElement("DIV");
var c=(this.srcNodeRef&&this.srcNodeRef.className)||this.className||this["class"];
this._swClass=(c||"").replace(/ .*/,"");
this.domNode.className="mblSwitch";
var _a=this.name?" name=\""+this.name+"\"":"";
this.domNode.innerHTML="<div class=\"mblSwitchInner\">"+"<div class=\"mblSwitchBg mblSwitchBgLeft\">"+"<div class=\"mblSwitchText mblSwitchTextLeft\"></div>"+"</div>"+"<div class=\"mblSwitchBg mblSwitchBgRight\">"+"<div class=\"mblSwitchText mblSwitchTextRight\"></div>"+"</div>"+"<div class=\"mblSwitchKnob\"></div>"+"<input type=\"hidden\""+_a+"></div>"+"</div>";
var n=this.inner=this.domNode.firstChild;
this.left=n.childNodes[0];
this.right=n.childNodes[1];
this.knob=n.childNodes[2];
this.input=n.childNodes[3];
},postCreate:function(){
this.connect(this.domNode,"onclick","onClick");
this.connect(this.domNode,_9("touch")?"touchstart":"onmousedown","onTouchStart");
this._initialValue=this.value;
},_changeState:function(_b,_c){
var on=(_b==="on");
this.left.style.display="";
this.right.style.display="";
this.inner.style.left="";
if(_c){
_6.add(this.domNode,"mblSwitchAnimation");
}
_6.remove(this.domNode,on?"mblSwitchOff":"mblSwitchOn");
_6.add(this.domNode,on?"mblSwitchOn":"mblSwitchOff");
var _d=this;
setTimeout(function(){
_d.left.style.display=on?"":"none";
_d.right.style.display=!on?"":"none";
_6.remove(_d.domNode,"mblSwitchAnimation");
},_c?300:0);
},startup:function(){
if(this._swClass.indexOf("Round")!=-1){
var r=Math.round(this.domNode.offsetHeight/2);
this.createRoundMask(this._swClass,r,this.domNode.offsetWidth);
}
},createRoundMask:function(_e,r,w){
if(!_9("webkit")||!_e){
return;
}
if(!this._createdMasks){
this._createdMasks=[];
}
if(this._createdMasks[_e]){
return;
}
this._createdMasks[_e]=1;
var _f=_5.doc.getCSSCanvasContext("2d",_e+"Mask",w,100);
_f.fillStyle="#000000";
_f.beginPath();
_f.moveTo(r,0);
_f.arcTo(0,0,0,2*r,r);
_f.arcTo(0,2*r,r,2*r,r);
_f.lineTo(w-r,2*r);
_f.arcTo(w,2*r,w,r,r);
_f.arcTo(w,0,w-r,0,r);
_f.closePath();
_f.fill();
},onClick:function(e){
if(this._moved){
return;
}
this.value=this.input.value=(this.value=="on")?"off":"on";
this._changeState(this.value,true);
this.onStateChanged(this.value);
},onTouchStart:function(e){
this._moved=false;
this.innerStartX=this.inner.offsetLeft;
if(!this._conn){
this._conn=[];
this._conn.push(_2.connect(this.inner,_9("touch")?"touchmove":"onmousemove",this,"onTouchMove"));
this._conn.push(_2.connect(this.inner,_9("touch")?"touchend":"onmouseup",this,"onTouchEnd"));
}
this.touchStartX=e.touches?e.touches[0].pageX:e.clientX;
this.left.style.display="";
this.right.style.display="";
_4.stop(e);
},onTouchMove:function(e){
e.preventDefault();
var dx;
if(e.targetTouches){
if(e.targetTouches.length!=1){
return false;
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
if(_9("touch")){
var ev=_5.doc.createEvent("MouseEvents");
ev.initEvent("click",true,true);
this.inner.dispatchEvent(ev);
}
return;
}
var _10=(this.inner.offsetLeft<-(this._width/2))?"off":"on";
this._changeState(_10,true);
if(_10!=this.value){
this.value=this.input.value=_10;
this.onStateChanged(_10);
}
},onStateChanged:function(_11){
},_setValueAttr:function(_12){
this._changeState(_12,false);
if(this.value!=_12){
this.onStateChanged(_12);
}
this.value=this.input.value=_12;
},_setLeftLabelAttr:function(_13){
this.leftLabel=_13;
this.left.firstChild.innerHTML=this._cv?this._cv(_13):_13;
},_setRightLabelAttr:function(_14){
this.rightLabel=_14;
this.right.firstChild.innerHTML=this._cv?this._cv(_14):_14;
},reset:function(){
this.set("value",this._initialValue);
}});
});
