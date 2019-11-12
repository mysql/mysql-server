//>>built
define("dojox/grid/_ViewManager",["dojo/_base/declare","dojo/_base/sniff","dojo/dom-class"],function(_1,_2,_3){
return _1("dojox.grid._ViewManager",null,{constructor:function(_4){
this.grid=_4;
},defaultWidth:200,views:[],resize:function(){
this.onEach("resize");
},render:function(){
this.onEach("render");
},addView:function(_5){
_5.idx=this.views.length;
this.views.push(_5);
},destroyViews:function(){
for(var i=0,v;v=this.views[i];i++){
v.destroy();
}
this.views=[];
},getContentNodes:function(){
var _6=[];
for(var i=0,v;v=this.views[i];i++){
_6.push(v.contentNode);
}
return _6;
},forEach:function(_7){
for(var i=0,v;v=this.views[i];i++){
_7(v,i);
}
},onEach:function(_8,_9){
_9=_9||[];
for(var i=0,v;v=this.views[i];i++){
if(_8 in v){
v[_8].apply(v,_9);
}
}
},normalizeHeaderNodeHeight:function(){
var _a=[];
for(var i=0,v;(v=this.views[i]);i++){
if(v.headerContentNode.firstChild){
_a.push(v.headerContentNode);
}
}
this.normalizeRowNodeHeights(_a);
},normalizeRowNodeHeights:function(_b){
var h=0;
var _c=[];
if(this.grid.rowHeight){
h=this.grid.rowHeight;
}else{
if(_b.length<=1){
return;
}
for(var i=0,n;(n=_b[i]);i++){
if(!_3.contains(n,"dojoxGridNonNormalizedCell")){
_c[i]=n.firstChild.offsetHeight;
h=Math.max(h,_c[i]);
}
}
h=(h>=0?h:0);
if((_2("mozilla")||_2("ie")>8)&&h){
h++;
}
}
for(i=0;(n=_b[i]);i++){
if(_c[i]!=h){
n.firstChild.style.height=h+"px";
}
}
},resetHeaderNodeHeight:function(){
for(var i=0,v,n;(v=this.views[i]);i++){
n=v.headerContentNode.firstChild;
if(n){
n.style.height="";
}
}
},renormalizeRow:function(_d){
var _e=[];
for(var i=0,v,n;(v=this.views[i])&&(n=v.getRowNode(_d));i++){
n.firstChild.style.height="";
_e.push(n);
}
this.normalizeRowNodeHeights(_e);
},getViewWidth:function(_f){
return this.views[_f].getWidth()||this.defaultWidth;
},measureHeader:function(){
this.resetHeaderNodeHeight();
this.forEach(function(_10){
_10.headerContentNode.style.height="";
});
var h=0;
this.forEach(function(_11){
h=Math.max(_11.headerNode.offsetHeight,h);
});
return h;
},measureContent:function(){
var h=0;
this.forEach(function(_12){
h=Math.max(_12.domNode.offsetHeight,h);
});
return h;
},findClient:function(_13){
var c=this.grid.elasticView||-1;
if(c<0){
for(var i=1,v;(v=this.views[i]);i++){
if(v.viewWidth){
for(i=1;(v=this.views[i]);i++){
if(!v.viewWidth){
c=i;
break;
}
}
break;
}
}
}
if(c<0){
c=Math.floor(this.views.length/2);
}
return c;
},arrange:function(l,w){
var i,v,vw,len=this.views.length,_14=this;
var c=(w<=0?len:this.findClient());
var _15=function(v,l){
var ds=v.domNode.style;
var hs=v.headerNode.style;
if(!_14.grid.isLeftToRight()){
ds.right=l+"px";
if(_2("ff")<4){
hs.right=l+v.getScrollbarWidth()+"px";
}else{
hs.right=l+"px";
}
if(!_2("webkit")){
hs.width=parseInt(hs.width,10)-v.getScrollbarWidth()+"px";
}
}else{
ds.left=l+"px";
hs.left=l+"px";
}
ds.top=0+"px";
hs.top=0;
};
for(i=0;(v=this.views[i])&&(i<c);i++){
vw=this.getViewWidth(i);
v.setSize(vw,0);
_15(v,l);
if(v.headerContentNode&&v.headerContentNode.firstChild){
vw=v.getColumnsWidth()+v.getScrollbarWidth();
}else{
vw=v.domNode.offsetWidth;
}
l+=vw;
}
i++;
var r=w;
for(var j=len-1;(v=this.views[j])&&(i<=j);j--){
vw=this.getViewWidth(j);
v.setSize(vw,0);
vw=v.domNode.offsetWidth;
r-=vw;
_15(v,r);
}
if(c<len){
v=this.views[c];
vw=Math.max(1,r-l);
v.setSize(vw+"px",0);
_15(v,l);
}
return l;
},renderRow:function(_16,_17,_18){
var _19=[];
for(var i=0,v,n,_1a;(v=this.views[i])&&(n=_17[i]);i++){
_1a=v.renderRow(_16);
n.appendChild(_1a);
_19.push(_1a);
}
if(!_18){
this.normalizeRowNodeHeights(_19);
}
},rowRemoved:function(_1b){
this.onEach("rowRemoved",[_1b]);
},updateRow:function(_1c,_1d){
for(var i=0,v;v=this.views[i];i++){
v.updateRow(_1c);
}
if(!_1d){
this.renormalizeRow(_1c);
}
},updateRowStyles:function(_1e){
this.onEach("updateRowStyles",[_1e]);
},setScrollTop:function(_1f){
var top=_1f;
for(var i=0,v;v=this.views[i];i++){
top=v.setScrollTop(_1f);
if(_2("ie")&&v.headerNode&&v.scrollboxNode){
v.headerNode.scrollLeft=v.scrollboxNode.scrollLeft;
}
}
return top;
},getFirstScrollingView:function(){
for(var i=0,v;(v=this.views[i]);i++){
if(v.hasHScrollbar()||v.hasVScrollbar()){
return v;
}
}
return null;
}});
});
