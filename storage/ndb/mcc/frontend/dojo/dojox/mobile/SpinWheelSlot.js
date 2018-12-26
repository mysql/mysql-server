//>>built
define("dojox/mobile/SpinWheelSlot",["dojo/_base/declare","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dijit/_Contained","dijit/_WidgetBase","./_ScrollableMixin"],function(_1,_2,_3,_4,_5,_6,_7){
return _1("dojox.mobile.SpinWheelSlot",[_6,_5,_7],{items:[],labels:[],labelFrom:0,labelTo:0,value:"",maxSpeed:500,minItems:15,centerPos:0,scrollBar:false,constraint:false,allowNestedScrolls:false,androidWorkaroud:false,buildRendering:function(){
this.inherited(arguments);
_3.add(this.domNode,"mblSpinWheelSlot");
var i,j,_8;
if(this.labelFrom!==this.labelTo){
this.labels=[];
for(i=this.labelFrom,_8=0;i<=this.labelTo;i++,_8++){
this.labels[_8]=String(i);
}
}
if(this.labels.length>0){
this.items=[];
for(i=0;i<this.labels.length;i++){
this.items.push([i,this.labels[i]]);
}
}
this.containerNode=_4.create("DIV",{className:"mblSpinWheelSlotContainer"});
this.containerNode.style.height=(_2.global.innerHeight||_2.doc.documentElement.clientHeight)*2+"px";
this.panelNodes=[];
for(var k=0;k<3;k++){
this.panelNodes[k]=_4.create("DIV",{className:"mblSpinWheelSlotPanel"});
var _9=this.items.length;
var n=Math.ceil(this.minItems/_9);
for(j=0;j<n;j++){
for(i=0;i<_9;i++){
_4.create("DIV",{className:"mblSpinWheelSlotLabel",name:this.items[i][0],innerHTML:this._cv?this._cv(this.items[i][1]):this.items[i][1]},this.panelNodes[k]);
}
}
this.containerNode.appendChild(this.panelNodes[k]);
}
this.domNode.appendChild(this.containerNode);
this.touchNode=_4.create("DIV",{className:"mblSpinWheelSlotTouch"},this.domNode);
this.setSelectable(this.domNode,false);
},startup:function(){
this.inherited(arguments);
this.centerPos=this.getParent().centerPos;
var _a=this.panelNodes[1].childNodes;
this._itemHeight=_a[0].offsetHeight;
this.adjust();
},adjust:function(){
var _b=this.panelNodes[1].childNodes;
var _c;
for(var i=0,_d=_b.length;i<_d;i++){
var _e=_b[i];
if(_e.offsetTop<=this.centerPos&&this.centerPos<_e.offsetTop+_e.offsetHeight){
_c=this.centerPos-(_e.offsetTop+Math.round(_e.offsetHeight/2));
break;
}
}
var h=this.panelNodes[0].offsetHeight;
this.panelNodes[0].style.top=-h+_c+"px";
this.panelNodes[1].style.top=_c+"px";
this.panelNodes[2].style.top=h+_c+"px";
},setInitialValue:function(){
if(this.items.length>0){
var _f=(this.value!=="")?this.value:this.items[0][1];
this.setValue(_f);
}
},getCenterPanel:function(){
var pos=this.getPos();
for(var i=0,len=this.panelNodes.length;i<len;i++){
var top=pos.y+this.panelNodes[i].offsetTop;
if(top<=this.centerPos&&this.centerPos<top+this.panelNodes[i].offsetHeight){
return this.panelNodes[i];
}
}
return null;
},setColor:function(_10){
for(var i=0,len=this.panelNodes.length;i<len;i++){
var _11=this.panelNodes[i].childNodes;
for(var j=0;j<_11.length;j++){
if(_11[j].innerHTML===String(_10)){
_3.add(_11[j],"mblSpinWheelSlotLabelBlue");
}else{
_3.remove(_11[j],"mblSpinWheelSlotLabelBlue");
}
}
}
},disableValues:function(_12){
for(var i=0,len=this.panelNodes.length;i<len;i++){
var _13=this.panelNodes[i].childNodes;
for(var j=0;j<_13.length;j++){
_3.remove(_13[j],"mblSpinWheelSlotLabelGray");
for(var k=0;k<_12.length;k++){
if(_13[j].innerHTML===String(_12[k])){
_3.add(_13[j],"mblSpinWheelSlotLabelGray");
break;
}
}
}
}
},getCenterItem:function(){
var pos=this.getPos();
var _14=this.getCenterPanel();
if(_14){
var top=pos.y+_14.offsetTop;
var _15=_14.childNodes;
for(var i=0,len=_15.length;i<len;i++){
if(top+_15[i].offsetTop<=this.centerPos&&this.centerPos<top+_15[i].offsetTop+_15[i].offsetHeight){
return _15[i];
}
}
}
return null;
},getValue:function(){
var _16=this.getCenterItem();
return (_16&&_16.innerHTML);
},getKey:function(){
return this.getCenterItem().getAttribute("name");
},setValue:function(_17){
var _18,_19;
var _1a=this.getValue();
if(!_1a){
this._penddingValue=_17;
return;
}
this._penddingValue=undefined;
var n=this.items.length;
for(var i=0;i<n;i++){
if(this.items[i][1]===String(_1a)){
_18=i;
}
if(this.items[i][1]===String(_17)){
_19=i;
}
if(_18!==undefined&&_19!==undefined){
break;
}
}
var d=_19-(_18||0);
var m;
if(d>0){
m=(d<n-d)?-d:n-d;
}else{
m=(-d<n+d)?-d:-(n+d);
}
var to=this.getPos();
to.y+=m*this._itemHeight;
this.slideTo(to,1);
},getSpeed:function(){
var y=0,n=this._time.length;
var _1b=(new Date()).getTime()-this.startTime-this._time[n-1];
if(n>=2&&_1b<200){
var dy=this._posY[n-1]-this._posY[(n-6)>=0?n-6:0];
var dt=this._time[n-1]-this._time[(n-6)>=0?n-6:0];
y=this.calcSpeed(dy,dt);
}
return {x:0,y:y};
},calcSpeed:function(d,t){
var _1c=this.inherited(arguments);
if(!_1c){
return 0;
}
var v=Math.abs(_1c);
var ret=_1c;
if(v>this.maxSpeed){
ret=this.maxSpeed*(_1c/v);
}
return ret;
},adjustDestination:function(to,pos){
var h=this._itemHeight;
var j=to.y+Math.round(h/2);
var a=Math.abs(j);
var r=j>=0?j%h:j%h+h;
to.y=j-r;
},resize:function(e){
if(this._penddingValue){
this.setValue(this._penddingValue);
}
},slideTo:function(to,_1d,_1e){
var pos=this.getPos();
var top=pos.y+this.panelNodes[1].offsetTop;
var _1f=top+this.panelNodes[1].offsetHeight;
var vh=this.domNode.parentNode.offsetHeight;
var t;
if(pos.y<to.y){
if(_1f>vh){
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
_1d=0;
this._initialized=true;
}else{
if(Math.abs(this._speed.y)<40){
_1d=0.2;
}
}
this.inherited(arguments,[to,_1d,_1e]);
}});
});
