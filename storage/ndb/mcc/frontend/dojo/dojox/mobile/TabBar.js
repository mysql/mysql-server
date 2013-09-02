//>>built
define("dojox/mobile/TabBar",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./Heading","./TabBarButton"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _2("dojox.mobile.TabBar",[_9,_8,_7],{iconBase:"",iconPos:"",barType:"tabBar",inHeading:false,tag:"UL",_fixedButtonWidth:76,_fixedButtonMargin:17,_largeScreenWidth:500,buildRendering:function(){
this._clsName=this.barType=="segmentedControl"?"mblTabButton":"mblTabBarButton";
this.domNode=this.containerNode=this.srcNodeRef||_4.create(this.tag);
this.domNode.className=this.barType=="segmentedControl"?"mblTabPanelHeader":"mblTabBar";
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
this.resize();
},resize:function(_c){
var i,w;
if(_c&&_c.w){
_5.setMarginBox(this.domNode,_c);
w=_c.w;
}else{
w=_6.get(this.domNode,"position")==="absolute"?_5.getContentBox(this.domNode).w:_5.getMarginBox(this.domNode).w;
}
var bw=this._fixedButtonWidth;
var bm=this._fixedButtonMargin;
var _d=this.containerNode.childNodes;
var _e=[];
for(i=0;i<_d.length;i++){
var c=_d[i];
if(c.nodeType!=1){
continue;
}
if(_3.contains(c,this._clsName)){
_e.push(c);
}
}
var _f;
if(this.barType=="segmentedControl"){
_f=w;
var _10=0;
for(i=0;i<_e.length;i++){
_f-=_5.getMarginBox(_e[i]).w;
_10+=_e[i].offsetWidth;
}
_f=Math.floor(_f/2);
var _11=this.getParent();
var _12=this.inHeading||_11 instanceof _a;
this.containerNode.style.padding=(_12?0:3)+"px 0px 0px "+(_12?0:_f)+"px";
if(_12){
_6.set(this.domNode,{background:"none",border:"none",width:_10+2+"px"});
}
_3.add(this.domNode,"mblTabBar"+(_12?"Head":"Top"));
}else{
_f=Math.floor((w-(bw+bm*2)*_e.length)/2);
if(w<this._largeScreenWidth||_f<0){
for(i=0;i<_e.length;i++){
_e[i].style.width=Math.round(98/_e.length)+"%";
_e[i].style.margin="0px";
}
this.containerNode.style.padding="0px 0px 0px 1%";
}else{
for(i=0;i<_e.length;i++){
_e[i].style.width=bw+"px";
_e[i].style.margin="0 "+bm+"px";
}
if(_e.length>0){
_e[0].style.marginLeft=_f+bm+"px";
}
this.containerNode.style.padding="0px";
}
}
if(!_1.some(this.getChildren(),function(_13){
return _13.iconNode1;
})){
_3.add(this.domNode,"mblTabBarNoIcons");
}else{
_3.remove(this.domNode,"mblTabBarNoIcons");
}
if(!_1.some(this.getChildren(),function(_14){
return _14.label;
})){
_3.add(this.domNode,"mblTabBarNoText");
}else{
_3.remove(this.domNode,"mblTabBarNoText");
}
}});
});
