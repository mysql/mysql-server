//>>built
define("dojox/widget/Standby",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/array","dojo/_base/event","dojo/_base/sniff","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/window","dojo/_base/window","dojo/_base/fx","dojo/fx","dijit/_Widget","dijit/_TemplatedMixin","dijit/registry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,fx,_e,_f,_10){
_1.experimental("dojox.widget.Standby");
return _2("dojox.widget.Standby",[_e,_f],{templateString:"<div>"+"<div style=\"display: none; opacity: 0; z-index: 9999; "+"position: absolute; cursor:wait;\" dojoAttachPoint=\"_underlayNode\"></div>"+"<img src=\"${image}\" style=\"opacity: 0; display: none; z-index: -10000; "+"position: absolute; top: 0px; left: 0px; cursor:wait;\" "+"dojoAttachPoint=\"_imageNode\">"+"<div style=\"opacity: 0; display: none; z-index: -10000; position: absolute; "+"top: 0px;\" dojoAttachPoint=\"_textNode\"></div>"+"</div>",_underlayNode:null,_imageNode:null,_textNode:null,_centerNode:null,image:require.toUrl("dojox/widget/Standby/images/loading.gif").toString(),imageText:"Please Wait...",text:"Please wait...",centerIndicator:"image",_displayed:false,_resizeCheck:null,target:"",color:"#C0C0C0",duration:500,_started:false,_parent:null,zIndex:"auto",startup:function(_11){
if(!this._started){
if(typeof this.target==="string"){
var w=_10.byId(this.target);
this.target=w?w.domNode:_6.byId(this.target);
}
if(this.text){
this._textNode.innerHTML=this.text;
}
if(this.centerIndicator==="image"){
this._centerNode=this._imageNode;
_7.set(this._imageNode,"src",this.image);
_7.set(this._imageNode,"alt",this.imageText);
}else{
this._centerNode=this._textNode;
}
_a.set(this._underlayNode,{display:"none",backgroundColor:this.color});
_a.set(this._centerNode,"display","none");
this.connect(this._underlayNode,"onclick","_ignore");
if(this.domNode.parentNode&&this.domNode.parentNode!=_c.body()){
_c.body().appendChild(this.domNode);
}
if(_5("ie")==7){
this._ieFixNode=_8.create("div");
_a.set(this._ieFixNode,{opacity:"0",zIndex:"-1000",position:"absolute",top:"-1000px"});
_c.body().appendChild(this._ieFixNode);
}
this.inherited(arguments);
}
},show:function(){
if(!this._displayed){
if(this._anim){
this._anim.stop();
delete this._anim;
}
this._displayed=true;
this._size();
this._disableOverflow();
this._fadeIn();
}
},hide:function(){
if(this._displayed){
if(this._anim){
this._anim.stop();
delete this._anim;
}
this._size();
this._fadeOut();
this._displayed=false;
if(this._resizeCheck!==null){
clearInterval(this._resizeCheck);
this._resizeCheck=null;
}
}
},isVisible:function(){
return this._displayed;
},onShow:function(){
},onHide:function(){
},uninitialize:function(){
this._displayed=false;
if(this._resizeCheck){
clearInterval(this._resizeCheck);
}
_a.set(this._centerNode,"display","none");
_a.set(this._underlayNode,"display","none");
if(_5("ie")==7&&this._ieFixNode){
_c.body().removeChild(this._ieFixNode);
delete this._ieFixNode;
}
if(this._anim){
this._anim.stop();
delete this._anim;
}
this.target=null;
this._imageNode=null;
this._textNode=null;
this._centerNode=null;
this.inherited(arguments);
},_size:function(){
if(this._displayed){
var dir=_7.get(_c.body(),"dir");
if(dir){
dir=dir.toLowerCase();
}
var _12;
var _13=this._scrollerWidths();
var _14=this.target;
var _15=_a.get(this._centerNode,"display");
_a.set(this._centerNode,"display","block");
var box=_9.position(_14,true);
if(_14===_c.body()||_14===_c.doc){
box=_b.getBox();
box.x=box.l;
box.y=box.t;
}
var _16=_9.getMarginBox(this._centerNode);
_a.set(this._centerNode,"display",_15);
if(this._ieFixNode){
_12=-this._ieFixNode.offsetTop/1000;
box.x=Math.floor((box.x+0.9)/_12);
box.y=Math.floor((box.y+0.9)/_12);
box.w=Math.floor((box.w+0.9)/_12);
box.h=Math.floor((box.h+0.9)/_12);
}
var zi=_a.get(_14,"zIndex");
var _17=zi;
var _18=zi;
if(this.zIndex==="auto"){
if(zi!="auto"){
_17=parseInt(_17,10)+1;
_18=parseInt(_18,10)+2;
}else{
var _19=_14.parentNode;
var _1a=-100000;
while(_19&&_19!==_c.body()){
zi=_a.get(_19,"zIndex");
if(!zi||zi==="auto"){
_19=_19.parentNode;
}else{
var _1b=parseInt(zi,10);
if(_1a<_1b){
_1a=_1b;
_17=_1b+1;
_18=_1b+2;
}
_19=_19.parentNode;
}
}
}
}else{
_17=parseInt(this.zIndex,10)+1;
_18=parseInt(this.zIndex,10)+2;
}
_a.set(this._centerNode,"zIndex",_18);
_a.set(this._underlayNode,"zIndex",_17);
var pn=_14.parentNode;
if(pn&&pn!==_c.body()&&_14!==_c.body()&&_14!==_c.doc){
var obh=box.h;
var obw=box.w;
var _1c=_9.position(pn,true);
if(this._ieFixNode){
_12=-this._ieFixNode.offsetTop/1000;
_1c.x=Math.floor((_1c.x+0.9)/_12);
_1c.y=Math.floor((_1c.y+0.9)/_12);
_1c.w=Math.floor((_1c.w+0.9)/_12);
_1c.h=Math.floor((_1c.h+0.9)/_12);
}
_1c.w-=pn.scrollHeight>pn.clientHeight&&pn.clientHeight>0?_13.v:0;
_1c.h-=pn.scrollWidth>pn.clientWidth&&pn.clientWidth>0?_13.h:0;
if(dir==="rtl"){
if(_5("opera")){
box.x+=pn.scrollHeight>pn.clientHeight&&pn.clientHeight>0?_13.v:0;
_1c.x+=pn.scrollHeight>pn.clientHeight&&pn.clientHeight>0?_13.v:0;
}else{
if(_5("ie")){
_1c.x+=pn.scrollHeight>pn.clientHeight&&pn.clientHeight>0?_13.v:0;
}else{
if(_5("webkit")){
}
}
}
}
if(_1c.w<box.w){
box.w=box.w-_1c.w;
}
if(_1c.h<box.h){
box.h=box.h-_1c.h;
}
var _1d=_1c.y;
var _1e=_1c.y+_1c.h;
var _1f=box.y;
var _20=box.y+obh;
var _21=_1c.x;
var _22=_1c.x+_1c.w;
var _23=box.x;
var _24=box.x+obw;
var _25;
if(_20>_1d&&_1f<_1d){
box.y=_1c.y;
_25=_1d-_1f;
var _26=obh-_25;
if(_26<_1c.h){
box.h=_26;
}else{
box.h-=2*(pn.scrollWidth>pn.clientWidth&&pn.clientWidth>0?_13.h:0);
}
}else{
if(_1f<_1e&&_20>_1e){
box.h=_1e-_1f;
}else{
if(_20<=_1d||_1f>=_1e){
box.h=0;
}
}
}
if(_24>_21&&_23<_21){
box.x=_1c.x;
_25=_21-_23;
var _27=obw-_25;
if(_27<_1c.w){
box.w=_27;
}else{
box.w-=2*(pn.scrollHeight>pn.clientHeight&&pn.clientHeight>0?_13.w:0);
}
}else{
if(_23<_22&&_24>_22){
box.w=_22-_23;
}else{
if(_24<=_21||_23>=_22){
box.w=0;
}
}
}
}
if(box.h>0&&box.w>0){
_a.set(this._underlayNode,{display:"block",width:box.w+"px",height:box.h+"px",top:box.y+"px",left:box.x+"px"});
var _28=["borderRadius","borderTopLeftRadius","borderTopRightRadius","borderBottomLeftRadius","borderBottomRightRadius"];
this._cloneStyles(_28);
if(!_5("ie")){
_28=["MozBorderRadius","MozBorderRadiusTopleft","MozBorderRadiusTopright","MozBorderRadiusBottomleft","MozBorderRadiusBottomright","WebkitBorderRadius","WebkitBorderTopLeftRadius","WebkitBorderTopRightRadius","WebkitBorderBottomLeftRadius","WebkitBorderBottomRightRadius"];
this._cloneStyles(_28,this);
}
var _29=(box.h/2)-(_16.h/2);
var _2a=(box.w/2)-(_16.w/2);
if(box.h>=_16.h&&box.w>=_16.w){
_a.set(this._centerNode,{top:(_29+box.y)+"px",left:(_2a+box.x)+"px",display:"block"});
}else{
_a.set(this._centerNode,"display","none");
}
}else{
_a.set(this._underlayNode,"display","none");
_a.set(this._centerNode,"display","none");
}
if(this._resizeCheck===null){
var _2b=this;
this._resizeCheck=setInterval(function(){
_2b._size();
},100);
}
}
},_cloneStyles:function(_2c){
_3.forEach(_2c,function(s){
_a.set(this._underlayNode,s,_a.get(this.target,s));
},this);
},_fadeIn:function(){
var _2d=this;
var _2e=_d.animateProperty({duration:_2d.duration,node:_2d._underlayNode,properties:{opacity:{start:0,end:0.75}}});
var _2f=_d.animateProperty({duration:_2d.duration,node:_2d._centerNode,properties:{opacity:{start:0,end:1}},onEnd:function(){
_2d.onShow();
delete _2d._anim;
}});
this._anim=fx.combine([_2e,_2f]);
this._anim.play();
},_fadeOut:function(){
var _30=this;
var _31=_d.animateProperty({duration:_30.duration,node:_30._underlayNode,properties:{opacity:{start:0.75,end:0}},onEnd:function(){
_a.set(this.node,{"display":"none","zIndex":"-1000"});
}});
var _32=_d.animateProperty({duration:_30.duration,node:_30._centerNode,properties:{opacity:{start:1,end:0}},onEnd:function(){
_a.set(this.node,{"display":"none","zIndex":"-1000"});
_30.onHide();
_30._enableOverflow();
delete _30._anim;
}});
this._anim=fx.combine([_31,_32]);
this._anim.play();
},_ignore:function(e){
if(e){
_4.stop(e);
}
},_scrollerWidths:function(){
var div=_8.create("div");
_a.set(div,{position:"absolute",opacity:0,overflow:"hidden",width:"50px",height:"50px",zIndex:"-100",top:"-200px",padding:"0px",margin:"0px"});
var _33=_8.create("div");
_a.set(_33,{width:"200px",height:"10px"});
div.appendChild(_33);
_c.body().appendChild(div);
var b=_9.getContentBox(div);
_a.set(div,"overflow","scroll");
var a=_9.getContentBox(div);
_c.body().removeChild(div);
return {v:b.w-a.w,h:b.h-a.h};
},_setTextAttr:function(_34){
this._textNode.innerHTML=_34;
this.text=_34;
},_setColorAttr:function(c){
_a.set(this._underlayNode,"backgroundColor",c);
this.color=c;
},_setImageTextAttr:function(_35){
_7.set(this._imageNode,"alt",_35);
this.imageText=_35;
},_setImageAttr:function(url){
_7.set(this._imageNode,"src",url);
this.image=url;
},_setCenterIndicatorAttr:function(_36){
this.centerIndicator=_36;
if(_36==="image"){
this._centerNode=this._imageNode;
_a.set(this._textNode,"display","none");
}else{
this._centerNode=this._textNode;
_a.set(this._imageNode,"display","none");
}
},_disableOverflow:function(){
if(this.target===_c.body()||this.target===_c.doc){
this._overflowDisabled=true;
var _37=_c.body();
if(_37.style&&_37.style.overflow){
this._oldOverflow=_a.set(_37,"overflow");
}else{
this._oldOverflow="";
}
if(_5("ie")&&!_5("quirks")){
if(_37.parentNode&&_37.parentNode.style&&_37.parentNode.style.overflow){
this._oldBodyParentOverflow=_37.parentNode.style.overflow;
}else{
try{
this._oldBodyParentOverflow=_a.set(_37.parentNode,"overflow");
}
catch(e){
this._oldBodyParentOverflow="scroll";
}
}
_a.set(_37.parentNode,"overflow","hidden");
}
_a.set(_37,"overflow","hidden");
}
},_enableOverflow:function(){
if(this._overflowDisabled){
delete this._overflowDisabled;
var _38=_c.body();
if(_5("ie")&&!_5("quirks")){
_38.parentNode.style.overflow=this._oldBodyParentOverflow;
delete this._oldBodyParentOverflow;
}
_a.set(_38,"overflow",this._oldOverflow);
if(_5("webkit")){
var div=_8.create("div",{style:{height:"2px"}});
_38.appendChild(div);
setTimeout(function(){
_38.removeChild(div);
},0);
}
delete this._oldOverflow;
}
}});
});
