//>>built
define("dojox/mobile/_ScrollableMixin",["dojo/_base/kernel","dojo/_base/config","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom","dojo/dom-class","dijit/registry","./scrollable"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_3("dojox.mobile._ScrollableMixin",_9,{fixedHeader:"",fixedFooter:"",_fixedAppFooter:"",scrollableParams:null,allowNestedScrolls:true,appBars:true,constructor:function(){
this.scrollableParams={};
},destroy:function(){
this.cleanup();
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
if(this._fixedAppFooter){
this._fixedAppFooter=_6.byId(this._fixedAppFooter);
}
this.findAppBars();
var _b,_c=this.scrollableParams;
if(this.fixedHeader){
_b=_6.byId(this.fixedHeader);
if(_b.parentNode==this.domNode){
this.isLocalHeader=true;
}
_c.fixedHeaderHeight=_b.offsetHeight;
}
if(this.fixedFooter){
_b=_6.byId(this.fixedFooter);
if(_b.parentNode==this.domNode){
this.isLocalFooter=true;
_b.style.bottom="0px";
}
_c.fixedFooterHeight=_b.offsetHeight;
}
this.scrollType=this.scrollType||_2.mblScrollableScrollType||0;
this.init(_c);
if(this.allowNestedScrolls){
for(var p=this.getParent();p;p=p.getParent()){
if(p&&p.scrollableParams){
this.dirLock=true;
p.dirLock=true;
break;
}
}
}
this._resizeHandle=this.subscribe("/dojox/mobile/afterResizeAll",function(){
if(this.domNode.style.display==="none"){
return;
}
var _d=_5.doc.activeElement;
if(this.isFormElement(_d)&&_6.isDescendant(_d,this.containerNode)){
this.scrollIntoView(_d);
}
});
this.inherited(arguments);
},findAppBars:function(){
if(!this.appBars){
return;
}
var i,_e,c;
for(i=0,_e=_5.body().childNodes.length;i<_e;i++){
c=_5.body().childNodes[i];
this.checkFixedBar(c,false);
}
if(this.domNode.parentNode){
for(i=0,_e=this.domNode.parentNode.childNodes.length;i<_e;i++){
c=this.domNode.parentNode.childNodes[i];
this.checkFixedBar(c,false);
}
}
this.fixedFooterHeight=this.fixedFooter?this.fixedFooter.offsetHeight:0;
},checkFixedBar:function(_f,_10){
if(_f.nodeType===1){
var _11=_f.getAttribute("fixed")||_f.getAttribute("data-mobile-fixed")||(_8.byNode(_f)&&_8.byNode(_f).fixed);
if(_11==="top"){
_7.add(_f,"mblFixedHeaderBar");
if(_10){
_f.style.top="0px";
this.fixedHeader=_f;
}
return _11;
}else{
if(_11==="bottom"){
_7.add(_f,"mblFixedBottomBar");
if(_10){
this.fixedFooter=_f;
}else{
this._fixedAppFooter=_f;
}
return _11;
}
}
}
return null;
}});
return _a;
});
