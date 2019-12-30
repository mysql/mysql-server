//>>built
define("dojox/mobile/TabBar",["dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./TabBarButton"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _2("dojox.mobile.TabBar",[_a,_9,_8],{iconBase:"",iconPos:"",barType:"tabBar",closable:false,center:true,syncWithViews:false,tag:"ul",selectOne:true,baseClass:"mblTabBar",_fixedButtonWidth:76,_fixedButtonMargin:17,_largeScreenWidth:500,buildRendering:function(){
this.domNode=this.srcNodeRef||_5.create(this.tag);
this.reset();
this.inherited(arguments);
},postCreate:function(){
if(this.syncWithViews){
var f=function(_c,_d,_e,_f,_10,_11){
var _12=_1.filter(this.getChildren(),function(w){
return w.moveTo==="#"+_c.id||w.moveTo===_c.id;
})[0];
if(_12){
_12.set("selected",true);
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
var _13=this._barType;
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
if(_13){
_4.remove(this.domNode,this.baseClass+cap(_13));
}
_4.add(this.domNode,this.baseClass+cap(this._barType));
},resize:function(_14){
var i,w;
if(_14&&_14.w){
_6.setMarginBox(this.domNode,_14);
w=_14.w;
}else{
w=_7.get(this.domNode,"position")==="absolute"?_6.getContentBox(this.domNode).w:_6.getMarginBox(this.domNode).w;
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
var _15=0;
if(this._barType=="tabBar"){
this.containerNode.style.paddingLeft="";
_15=Math.floor((w-(bw+bm*2)*arr.length)/2);
if(w<this._largeScreenWidth||_15<0){
for(i=0;i<arr.length;i++){
arr[i].style.width=Math.round(98/arr.length)+"%";
arr[i].style.margin="0";
}
}else{
for(i=0;i<arr.length;i++){
arr[i].style.width=bw+"px";
arr[i].style.margin="0 "+bm+"px";
}
if(arr.length>0){
arr[0].style.marginLeft=_15+bm+"px";
}
this.containerNode.style.padding="0px";
}
}else{
for(i=0;i<arr.length;i++){
arr[i].style.width=arr[i].style.margin="";
}
var _16=this.getParent();
if(this.center&&(!_16||!_4.contains(_16.domNode,"mblHeading"))){
_15=w;
for(i=0;i<arr.length;i++){
_15-=_6.getMarginBox(arr[i]).w;
}
_15=Math.floor(_15/2);
}
this.containerNode.style.paddingLeft=_15?_15+"px":"";
}
},getSelectedTab:function(){
return _1.filter(this.getChildren(),function(w){
return w.selected;
})[0];
},onCloseButtonClick:function(tab){
return true;
}});
});
