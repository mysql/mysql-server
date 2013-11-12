//>>built
define("dojox/mobile/_ScrollableMixin",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom","dojo/dom-class","dijit/registry","./scrollable"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_2("dojox.mobile._ScrollableMixin",null,{fixedHeader:"",fixedFooter:"",scrollableParams:null,allowNestedScrolls:true,constructor:function(){
this.scrollableParams={};
},destroy:function(){
this.cleanup();
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
var _a;
var _b=this.scrollableParams;
if(this.fixedHeader){
_a=_5.byId(this.fixedHeader);
if(_a.parentNode==this.domNode){
this.isLocalHeader=true;
}
_b.fixedHeaderHeight=_a.offsetHeight;
}
if(this.fixedFooter){
_a=_5.byId(this.fixedFooter);
if(_a.parentNode==this.domNode){
this.isLocalFooter=true;
_a.style.bottom="0px";
}
_b.fixedFooterHeight=_a.offsetHeight;
}
this.init(_b);
if(this.allowNestedScrolls){
for(var p=this.getParent();p;p=p.getParent()){
if(p&&p.scrollableParams){
this.isNested=true;
this.dirLock=true;
p.dirLock=true;
break;
}
}
}
this.inherited(arguments);
},findAppBars:function(){
var i,_c,c;
for(i=0,_c=_4.body().childNodes.length;i<_c;i++){
c=_4.body().childNodes[i];
this.checkFixedBar(c,false);
}
if(this.domNode.parentNode){
for(i=0,_c=this.domNode.parentNode.childNodes.length;i<_c;i++){
c=this.domNode.parentNode.childNodes[i];
this.checkFixedBar(c,false);
}
}
this.fixedFooterHeight=this.fixedFooter?this.fixedFooter.offsetHeight:0;
},checkFixedBar:function(_d,_e){
if(_d.nodeType===1){
var _f=_d.getAttribute("fixed")||(_7.byNode(_d)&&_7.byNode(_d).fixed);
if(_f==="top"){
_6.add(_d,"mblFixedHeaderBar");
if(_e){
_d.style.top="0px";
this.fixedHeader=_d;
}
return _f;
}else{
if(_f==="bottom"){
_6.add(_d,"mblFixedBottomBar");
this.fixedFooter=_d;
return _f;
}
}
}
return null;
}});
_3.extend(_9,new _8(_1,dojox));
return _9;
});
