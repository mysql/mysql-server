//>>built
define("dojox/mobile/SpinWheelSlot",["dojo/_base/kernel","dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/SpinWheelSlot","dojo/touch","dojo/on","dijit/_Contained","dijit/_WidgetBase","./scrollable","./common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,on,_a,_b,_c){
var _d=_3(_7("dojo-bidi")?"dojox.mobile.NonBidiSpinWheelSlot":"dojox.mobile.SpinWheelSlot",[_b,_a,_c],{items:[],labels:[],labelFrom:0,labelTo:0,zeroPad:0,value:"",step:1,pageSteps:1,baseClass:"mblSpinWheelSlot",maxSpeed:500,minItems:15,centerPos:0,scrollBar:false,constraint:false,propagatable:false,androidWorkaroud:false,buildRendering:function(){
this.inherited(arguments);
this.initLabels();
var i,j;
if(this.labels.length>0){
this.items=[];
for(i=0;i<this.labels.length;i++){
this.items.push([i,this.labels[i]]);
}
}
this.containerNode=_6.create("div",{className:"mblSpinWheelSlotContainer"});
this.containerNode.style.height=(_4.global.innerHeight||_4.doc.documentElement.clientHeight)*2+"px";
this.panelNodes=[];
for(var k=0;k<3;k++){
this.panelNodes[k]=_6.create("div",{className:"mblSpinWheelSlotPanel"});
this.panelNodes[k].setAttribute("aria-hidden","true");
var _e=this.items.length;
if(_e>0){
var n=Math.ceil(this.minItems/_e);
for(j=0;j<n;j++){
for(i=0;i<_e;i++){
_6.create("div",{className:"mblSpinWheelSlotLabel",name:this.items[i][0],"data-mobile-val":this.items[i][1],innerHTML:this._cv?this._cv(this.items[i][1]):this.items[i][1]},this.panelNodes[k]);
}
}
}
this.containerNode.appendChild(this.panelNodes[k]);
}
this.domNode.appendChild(this.containerNode);
this.touchNode=_6.create("div",{className:"mblSpinWheelSlotTouch"},this.domNode);
this.setSelectable(this.domNode,false);
this.touchNode.setAttribute("tabindex",0);
this.touchNode.setAttribute("role","slider");
if(this.value===""&&this.items.length>0){
this.value=this.items[0][1];
}
this._initialValue=this.value;
if(_7("windows-theme")){
var _f=this,_10=this.containerNode,_11=5;
this.own(on(_f.touchNode,_9.press,function(e){
var _12=e.pageY,_13=_f.getParent().getChildren();
for(var i=0,ln=_13.length;i<ln;i++){
var _14=_13[i].containerNode;
if(_10!==_14){
_5.remove(_14,"mblSelectedSlot");
_14.selected=false;
}else{
_5.add(_10,"mblSelectedSlot");
}
}
var _15=on(_f.touchNode,_9.move,function(e){
if(Math.abs(e.pageY-_12)<_11){
return;
}
_15.remove();
_16.remove();
_10.selected=true;
var _17=_f.getCenterItem();
if(_17){
_5.remove(_17,"mblSelectedSlotItem");
}
});
var _16=on(_f.touchNode,_9.release,function(){
_16.remove();
_15.remove();
_10.selected?_5.remove(_10,"mblSelectedSlot"):_5.add(_10,"mblSelectedSlot");
_10.selected=!_10.selected;
});
}));
this.on("flickAnimationEnd",function(){
var _18=_f.getCenterItem();
if(_f.previousCenterItem){
_5.remove(_f.previousCenterItem,"mblSelectedSlotItem");
}
_5.add(_18,"mblSelectedSlotItem");
_f.previousCenterItem=_18;
});
}
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
this.noResize=true;
if(this.items.length>0){
this.init();
this.centerPos=this.getParent().centerPos;
var _19=this.panelNodes[1].childNodes;
this._itemHeight=_19[0].offsetHeight;
this.adjust();
this.connect(this.touchNode,"onkeydown","_onKeyDown");
}
if(_7("windows-theme")){
this.previousCenterItem=this.getCenterItem();
if(this.previousCenterItem){
_5.add(this.previousCenterItem,"mblSelectedSlotItem");
}
}
},initLabels:function(){
if(this.labelFrom!==this.labelTo){
var a=this.labels=[],_1a=this.zeroPad&&Array(this.zeroPad).join("0");
for(var i=this.labelFrom;i<=this.labelTo;i+=this.step){
a.push(this.zeroPad?(_1a+i).slice(-this.zeroPad):i+"");
}
}
},onTouchStart:function(e){
this.touchNode.focus();
this.inherited(arguments);
},adjust:function(){
var _1b=this.panelNodes[1].childNodes;
var _1c;
for(var i=0,len=_1b.length;i<len;i++){
var _1d=_1b[i];
if(_1d.offsetTop<=this.centerPos&&this.centerPos<_1d.offsetTop+_1d.offsetHeight){
_1c=this.centerPos-(_1d.offsetTop+Math.round(_1d.offsetHeight/2));
break;
}
}
var h=this.panelNodes[0].offsetHeight;
this.panelNodes[0].style.top=-h+_1c+"px";
this.panelNodes[1].style.top=_1c+"px";
this.panelNodes[2].style.top=h+_1c+"px";
},setInitialValue:function(){
this.set("value",this._initialValue);
this.touchNode.setAttribute("aria-valuetext",this._initialValue);
},_onKeyDown:function(e){
if(!e||e.type!=="keydown"||e.altKey||e.ctrlKey||e.shiftKey){
return true;
}
switch(e.keyCode){
case 38:
case 39:
this.spin(1);
e.stopPropagation();
return false;
case 40:
case 37:
this.spin(-1);
e.stopPropagation();
return false;
case 33:
this.spin(this.pageSteps);
e.stopPropagation();
return false;
case 34:
this.spin(-1*this.pageSteps);
e.stopPropagation();
return false;
}
return true;
},_getCenterPanel:function(){
var pos=this.getPos();
for(var i=0,len=this.panelNodes.length;i<len;i++){
var top=pos.y+this.panelNodes[i].offsetTop;
if(top<=this.centerPos&&this.centerPos<top+this.panelNodes[i].offsetHeight){
return this.panelNodes[i];
}
}
return null;
},setColor:function(_1e,_1f){
_2.forEach(this.panelNodes,function(_20){
_2.forEach(_20.childNodes,function(_21,i){
_5.toggle(_21,_1f||"mblSpinWheelSlotLabelBlue",_21.innerHTML===_1e);
},this);
},this);
},disableValues:function(n){
_2.forEach(this.panelNodes,function(_22){
for(var i=0;i<_22.childNodes.length;i++){
_5.toggle(_22.childNodes[i],"mblSpinWheelSlotLabelGray",i>=n);
}
});
},getCenterItem:function(){
var pos=this.getPos();
var _23=this._getCenterPanel();
if(_23){
var top=pos.y+_23.offsetTop;
var _24=_23.childNodes;
for(var i=0,len=_24.length;i<len;i++){
if(top+_24[i].offsetTop<=this.centerPos&&this.centerPos<top+_24[i].offsetTop+_24[i].offsetHeight){
return _24[i];
}
}
}
return null;
},_getKeyAttr:function(){
if(!this._started){
if(this.items){
var v=this.value;
for(var i=0;i<this.items.length;i++){
if(this.items[i][1]==this.value){
return this.items[i][0];
}
}
}
return null;
}
var _25=this.getCenterItem();
return (_25&&_25.getAttribute("name"));
},_getValueAttr:function(){
if(!this._started){
return this.value;
}
if(this.items.length>0){
var _26=this.getCenterItem();
return (_26&&_26.getAttribute("data-mobile-val"));
}else{
return this._initialValue;
}
},_setValueAttr:function(_27){
if(this.items.length>0){
this._spinToValue(_27,true);
}
},_spinToValue:function(_28,_29){
var _2a,_2b;
var _2c=this.get("value");
if(!_2c){
this._pendingValue=_28;
return;
}
if(_2c==_28){
return;
}
this._pendingValue=undefined;
if(_29){
this._set("value",_28);
}
var n=this.items.length;
for(var i=0;i<n;i++){
if(this.items[i][1]===String(_2c)){
_2a=i;
}
if(this.items[i][1]===String(_28)){
_2b=i;
}
if(_2a!==undefined&&_2b!==undefined){
break;
}
}
var d=_2b-(_2a||0);
var m;
if(d>0){
m=(d<n-d)?-d:n-d;
}else{
m=(-d<n+d)?-d:-(n+d);
}
this.spin(m);
},onFlickAnimationStart:function(e){
this._onFlickAnimationStartCalled=true;
this.inherited(arguments);
},onFlickAnimationEnd:function(e){
this._duringSlideTo=false;
this._onFlickAnimationStartCalled=false;
this.inherited(arguments);
this.touchNode.setAttribute("aria-valuetext",this.get("value"));
},spin:function(_2d){
if(!this._started||this._duringSlideTo){
return;
}
var to=this.getPos();
to.y+=_2d*this._itemHeight;
this.slideTo(to,1);
},getSpeed:function(){
var y=0,n=this._time.length;
var _2e=(new Date()).getTime()-this.startTime-this._time[n-1];
if(n>=2&&_2e<200){
var dy=this._posY[n-1]-this._posY[(n-6)>=0?n-6:0];
var dt=this._time[n-1]-this._time[(n-6)>=0?n-6:0];
y=this.calcSpeed(dy,dt);
}
return {x:0,y:y};
},calcSpeed:function(d,t){
var _2f=this.inherited(arguments);
if(!_2f){
return 0;
}
var v=Math.abs(_2f);
var ret=_2f;
if(v>this.maxSpeed){
ret=this.maxSpeed*(_2f/v);
}
return ret;
},adjustDestination:function(to,pos,dim){
var h=this._itemHeight;
var j=to.y+Math.round(h/2);
var r=j>=0?j%h:j%h+h;
to.y=j-r;
return true;
},resize:function(e){
if(this.panelNodes&&this.panelNodes.length>0){
var _30=this.panelNodes[1].childNodes;
if(_30.length>0&&!_7("windows-theme")){
var _31=this.getParent();
if(_31){
this._itemHeight=_30[0].offsetHeight;
this.centerPos=_31.centerPos;
if(!this.panelNodes[0].style.top){
this.adjust();
}
}
}
}
if(this._pendingValue){
this.set("value",this._pendingValue);
}
},slideTo:function(to,_32,_33){
this._duringSlideTo=true;
var pos=this.getPos();
var top=pos.y+this.panelNodes[1].offsetTop;
var _34=top+this.panelNodes[1].offsetHeight;
var vh=this.domNode.parentNode.offsetHeight;
var t;
if(pos.y<to.y){
if(_34>vh){
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
if(this.getParent()._duringStartup){
_32=0;
}else{
if(Math.abs(this._speed.y)<40){
_32=0.2;
}
}
if(_32&&_32>0){
this.inherited(arguments,[to,_32,_33]);
if(!this._onFlickAnimationStartCalled){
this._duringSlideTo=false;
}
}else{
this.onFlickAnimationStart();
this.scrollTo(to,true);
this.onFlickAnimationEnd();
}
}});
return _7("dojo-bidi")?_3("dojox.mobile.SpinWheelSlot",[_d,_8]):_d;
});
