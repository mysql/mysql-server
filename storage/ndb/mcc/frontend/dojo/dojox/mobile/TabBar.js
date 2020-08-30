//>>built
define("dojox/mobile/TabBar",["dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/dom-attr","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./TabBarButton","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/TabBar"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
var _f=_2(_d("dojo-bidi")?"dojox.mobile.NonBidiTabBar":"dojox.mobile.TabBar",[_b,_a,_9],{iconBase:"",iconPos:"",barType:"tabBar",closable:false,center:true,syncWithViews:false,tag:"ul",fill:"auto",selectOne:true,baseClass:"mblTabBar",_fixedButtonWidth:76,_fixedButtonMargin:17,_largeScreenWidth:500,buildRendering:function(){
this.domNode=this.srcNodeRef||_5.create(this.tag);
_8.set(this.domNode,"role","tablist");
this.reset();
this.inherited(arguments);
},postCreate:function(){
if(this.syncWithViews){
var f=function(_10,_11,dir,_12,_13,_14){
var _15=_1.filter(this.getChildren(),function(w){
return w.moveTo==="#"+_10.id||w.moveTo===_10.id;
})[0];
if(_15){
_15.set("selected",true);
}
};
this.subscribe("/dojox/mobile/afterTransitionIn",f);
this.subscribe("/dojox/mobile/startView",f);
}
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
this.resize();
},reset:function(){
var _16=this._barType;
if(typeof this.barType==="object"){
this._barType=this.barType["*"];
for(var c in this.barType){
if(_4.contains(_3.doc.documentElement,c)){
this._barType=this.barType[c];
break;
}
}
}else{
this._barType=this.barType;
}
var cap=function(s){
return s.charAt(0).toUpperCase()+s.substring(1);
};
if(_16){
_4.remove(this.domNode,this.baseClass+cap(_16));
}
_4.add(this.domNode,this.baseClass+cap(this._barType));
},resize:function(_17){
var i,w;
if(_17&&_17.w){
w=_17.w;
}else{
w=_6.getMarginBox(this.domNode).w;
}
var bw=this._fixedButtonWidth;
var bm=this._fixedButtonMargin;
var arr=_1.map(this.getChildren(),function(w){
return w.domNode;
});
_4.toggle(this.domNode,"mblTabBarNoIcons",!_1.some(this.getChildren(),function(w){
return w.iconNode1;
}));
_4.toggle(this.domNode,"mblTabBarNoText",!_1.some(this.getChildren(),function(w){
return w.label;
}));
var _18=0;
if(this._barType=="tabBar"){
this.containerNode.style.paddingLeft="";
_18=Math.floor((w-(bw+bm*2)*arr.length)/2);
if(this.fill=="always"||(this.fill=="auto"&&(w<this._largeScreenWidth||_18<0))){
_4.add(this.domNode,"mblTabBarFill");
for(i=0;i<arr.length;i++){
arr[i].style.width=(100/arr.length)+"%";
arr[i].style.margin="0";
}
}else{
for(i=0;i<arr.length;i++){
arr[i].style.width=bw+"px";
arr[i].style.margin="0 "+bm+"px";
}
if(arr.length>0){
if(_d("dojo-bidi")&&!this.isLeftToRight()){
arr[0].style.marginLeft="0px";
arr[0].style.marginRight=_18+bm+"px";
}else{
arr[0].style.marginLeft=_18+bm+"px";
}
}
this.containerNode.style.padding="0px";
}
}else{
for(i=0;i<arr.length;i++){
arr[i].style.width=arr[i].style.margin="";
}
var _19=this.getParent();
if(this.fill=="always"){
_4.add(this.domNode,"mblTabBarFill");
for(i=0;i<arr.length;i++){
arr[i].style.width=(100/arr.length)+"%";
if(this._barType!="segmentedControl"&&this._barType!="standardTab"){
arr[i].style.margin="0";
}
}
}else{
if(this.center&&(!_19||!_4.contains(_19.domNode,"mblHeading"))){
_18=w;
for(i=0;i<arr.length;i++){
_18-=_6.getMarginBox(arr[i]).w;
}
_18=Math.floor(_18/2);
}
if(_d("dojo-bidi")&&!this.isLeftToRight()){
this.containerNode.style.paddingLeft="0px";
this.containerNode.style.paddingRight=_18?_18+"px":"";
}else{
this.containerNode.style.paddingLeft=_18?_18+"px":"";
}
}
}
if(_17&&_17.w){
_6.setMarginBox(this.domNode,_17);
}
},getSelectedTab:function(){
return _1.filter(this.getChildren(),function(w){
return w.selected;
})[0];
},onCloseButtonClick:function(tab){
return true;
}});
return _d("dojo-bidi")?_2("dojox.mobile.TabBar",[_f,_e]):_f;
});
