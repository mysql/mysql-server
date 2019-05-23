//>>built
define("dojox/mobile/SpinWheelSlot",["dojo/_base/kernel","dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dijit/_Contained","dijit/_WidgetBase","./scrollable"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _3("dojox.mobile.SpinWheelSlot",[_9,_8,_a],{items:[],labels:[],labelFrom:0,labelTo:0,zeroPad:0,value:"",step:1,tabIndex:"0",_setTabIndexAttr:"",baseClass:"mblSpinWheelSlot",maxSpeed:500,minItems:15,centerPos:0,scrollBar:false,constraint:false,propagatable:false,androidWorkaroud:false,buildRendering:function(){
this.inherited(arguments);
this.initLabels();
if(this.labels.length>0){
this.items=[];
for(i=0;i<this.labels.length;i++){
this.items.push([i,this.labels[i]]);
}
}
this.containerNode=_7.create("div",{className:"mblSpinWheelSlotContainer"});
this.containerNode.style.height=(_5.global.innerHeight||_5.doc.documentElement.clientHeight)*2+"px";
this.panelNodes=[];
for(var k=0;k<3;k++){
this.panelNodes[k]=_7.create("div",{className:"mblSpinWheelSlotPanel"});
var _b=this.items.length;
var n=Math.ceil(this.minItems/_b);
for(j=0;j<n;j++){
for(i=0;i<_b;i++){
_7.create("div",{className:"mblSpinWheelSlotLabel",name:this.items[i][0],val:this.items[i][1],innerHTML:this._cv?this._cv(this.items[i][1]):this.items[i][1]},this.panelNodes[k]);
}
}
this.containerNode.appendChild(this.panelNodes[k]);
}
this.domNode.appendChild(this.containerNode);
this.touchNode=_7.create("div",{className:"mblSpinWheelSlotTouch"},this.domNode);
this.setSelectable(this.domNode,false);
if(this.value===""&&this.items.length>0){
this.value=this.items[0][1];
}
this._initialValue=this.value;
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
this.noResize=true;
this.init();
this.centerPos=this.getParent().centerPos;
var _c=this.panelNodes[1].childNodes;
this._itemHeight=_c[0].offsetHeight;
this.adjust();
this._keydownHandle=this.connect(this.domNode,"onkeydown","_onKeyDown");
},initLabels:function(){
if(this.labelFrom!==this.labelTo){
var a=this.labels=[],_d=this.zeroPad&&Array(this.zeroPad).join("0");
for(var i=this.labelFrom;i<=this.labelTo;i+=this.step){
a.push(this.zeroPad?(_d+i).slice(-this.zeroPad):i+"");
}
}
},adjust:function(){
var _e=this.panelNodes[1].childNodes;
var _f;
for(var i=0,len=_e.length;i<len;i++){
var _10=_e[i];
if(_10.offsetTop<=this.centerPos&&this.centerPos<_10.offsetTop+_10.offsetHeight){
_f=this.centerPos-(_10.offsetTop+Math.round(_10.offsetHeight/2));
break;
}
}
var h=this.panelNodes[0].offsetHeight;
this.panelNodes[0].style.top=-h+_f+"px";
this.panelNodes[1].style.top=_f+"px";
this.panelNodes[2].style.top=h+_f+"px";
},setInitialValue:function(){
this.set("value",this._initialValue);
},_onKeyDown:function(e){
if(!e||e.type!=="keydown"){
return;
}
if(e.keyCode===40){
this.spin(-1);
}else{
if(e.keyCode===38){
this.spin(1);
}
}
},_getCenterPanel:function(){
var pos=this.getPos();
for(var i=0,len=this.panelNodes.length;i<len;i++){
var top=pos.y+this.panelNodes[i].offsetTop;
if(top<=this.centerPos&&this.centerPos<top+this.panelNodes[i].offsetHeight){
return this.panelNodes[i];
}
}
return null;
},setColor:function(_11,_12){
_2.forEach(this.panelNodes,function(_13){
_2.forEach(_13.childNodes,function(_14,i){
_6.toggle(_14,_12||"mblSpinWheelSlotLabelBlue",_14.innerHTML===_11);
},this);
},this);
},disableValues:function(n){
_2.forEach(this.panelNodes,function(_15){
for(var i=27;i<31;i++){
_6.toggle(_15.childNodes[i],"mblSpinWheelSlotLabelGray",i>=nDays);
}
});
},getCenterItem:function(){
var pos=this.getPos();
var _16=this._getCenterPanel();
if(_16){
var top=pos.y+_16.offsetTop;
var _17=_16.childNodes;
for(var i=0,len=_17.length;i<len;i++){
if(top+_17[i].offsetTop<=this.centerPos&&this.centerPos<top+_17[i].offsetTop+_17[i].offsetHeight){
return _17[i];
}
}
}
return null;
},_getKeyAttr:function(){
var _18=this.getCenterItem();
return (_18&&_18.getAttribute("name"));
},_getValueAttr:function(){
var _19=this.getCenterItem();
return (_19&&_19.getAttribute("val"));
},_setValueAttr:function(_1a){
var _1b,_1c;
var _1d=this.get("value");
if(!_1d){
this._penddingValue=_1a;
return;
}
this._penddingValue=undefined;
this._set("value",_1a);
var n=this.items.length;
for(var i=0;i<n;i++){
if(this.items[i][1]===String(_1d)){
_1b=i;
}
if(this.items[i][1]===String(_1a)){
_1c=i;
}
if(_1b!==undefined&&_1c!==undefined){
break;
}
}
var d=_1c-(_1b||0);
var m;
if(d>0){
m=(d<n-d)?-d:n-d;
}else{
m=(-d<n+d)?-d:-(n+d);
}
this.spin(m);
},stopAnimation:function(){
this.inherited(arguments);
this._set("value",this.get("value"));
},spin:function(_1e){
if(!this._started){
return;
}
var to=this.getPos();
if(to.y%this._itemHeight){
return;
}
to.y+=_1e*this._itemHeight;
this.slideTo(to,1);
},getSpeed:function(){
var y=0,n=this._time.length;
var _1f=(new Date()).getTime()-this.startTime-this._time[n-1];
if(n>=2&&_1f<200){
var dy=this._posY[n-1]-this._posY[(n-6)>=0?n-6:0];
var dt=this._time[n-1]-this._time[(n-6)>=0?n-6:0];
y=this.calcSpeed(dy,dt);
}
return {x:0,y:y};
},calcSpeed:function(d,t){
var _20=this.inherited(arguments);
if(!_20){
return 0;
}
var v=Math.abs(_20);
var ret=_20;
if(v>this.maxSpeed){
ret=this.maxSpeed*(_20/v);
}
return ret;
},adjustDestination:function(to,pos,dim){
var h=this._itemHeight;
var j=to.y+Math.round(h/2);
var a=Math.abs(j);
var r=j>=0?j%h:j%h+h;
to.y=j-r;
return true;
},resize:function(e){
if(this._penddingValue){
this.set("value",this._penddingValue);
}
},slideTo:function(to,_21,_22){
var pos=this.getPos();
var top=pos.y+this.panelNodes[1].offsetTop;
var _23=top+this.panelNodes[1].offsetHeight;
var vh=this.domNode.parentNode.offsetHeight;
var t;
if(pos.y<to.y){
if(_23>vh){
t=this.panelNodes[2];
t.style.top=this.panelNodes[0].offsetTop-this.panelNodes[0].offsetHeight+"px";
this.panelNodes[2]=this.panelNodes[1];
this.panelNodes[1]=this.panelNodes[0];
this.panelNodes[0]=t;
}
}else{
if(pos.y>to.y){
if(top<0){
t=this.panelNodes[0];
t.style.top=this.panelNodes[2].offsetTop+this.panelNodes[2].offsetHeight+"px";
this.panelNodes[0]=this.panelNodes[1];
this.panelNodes[1]=this.panelNodes[2];
this.panelNodes[2]=t;
}
}
}
if(!this._initialized){
_21=0;
this._initialized=true;
}else{
if(Math.abs(this._speed.y)<40){
_21=0.2;
}
}
this.inherited(arguments,[to,_21,_22]);
}});
});
