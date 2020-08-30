//>>built
define("dojox/grid/_Scroller",["dijit/registry","dojo/_base/declare","dojo/_base/lang","./util","dojo/_base/html"],function(_1,_2,_3,_4,_5){
var _6=function(_7){
var i=0,n,p=_7.parentNode;
while((n=p.childNodes[i++])){
if(n==_7){
return i-1;
}
}
return -1;
};
var _8=function(_9){
if(!_9){
return;
}
dojo.forEach(_1.toArray(),function(w){
if(w.domNode&&_5.isDescendant(w.domNode,_9,true)){
w.destroy();
}
});
};
var _a=function(_b){
var _c=_5.byId(_b);
return (_c&&_c.tagName?_c.tagName.toLowerCase():"");
};
var _d=function(_e,_f){
var _10=[];
var i=0,n;
while((n=_e.childNodes[i])){
i++;
if(_a(n)==_f){
_10.push(n);
}
}
return _10;
};
var _11=function(_12){
return _d(_12,"div");
};
return _2("dojox.grid._Scroller",null,{constructor:function(_13){
this.setContentNodes(_13);
this.pageHeights=[];
this.pageNodes=[];
this.stack=[];
},rowCount:0,defaultRowHeight:32,keepRows:100,contentNode:null,scrollboxNode:null,defaultPageHeight:0,keepPages:10,pageCount:0,windowHeight:0,firstVisibleRow:0,lastVisibleRow:0,averageRowHeight:0,page:0,pageTop:0,init:function(_14,_15,_16){
switch(arguments.length){
case 3:
this.rowsPerPage=_16;
case 2:
this.keepRows=_15;
case 1:
this.rowCount=_14;
default:
break;
}
this.defaultPageHeight=(this.grid.rowHeight>0?this.grid.rowHeight:this.defaultRowHeight)*this.rowsPerPage;
this.pageCount=this._getPageCount(this.rowCount,this.rowsPerPage);
this.setKeepInfo(this.keepRows);
this.invalidate();
if(this.scrollboxNode){
this.scrollboxNode.scrollTop=0;
this.scroll(0);
this.scrollboxNode.onscroll=_3.hitch(this,"onscroll");
}
},_getPageCount:function(_17,_18){
return _17?(Math.ceil(_17/_18)||1):0;
},destroy:function(){
this.invalidateNodes();
delete this.contentNodes;
delete this.contentNode;
delete this.scrollboxNode;
},setKeepInfo:function(_19){
this.keepRows=_19;
this.keepPages=!this.keepRows?this.keepPages:Math.max(Math.ceil(this.keepRows/this.rowsPerPage),2);
},setContentNodes:function(_1a){
this.contentNodes=_1a;
this.colCount=(this.contentNodes?this.contentNodes.length:0);
this.pageNodes=[];
for(var i=0;i<this.colCount;i++){
this.pageNodes[i]=[];
}
},getDefaultNodes:function(){
return this.pageNodes[0]||[];
},invalidate:function(){
this._invalidating=true;
this.invalidateNodes();
this.pageHeights=[];
this.height=(this.pageCount?(this.pageCount-1)*this.defaultPageHeight+this.calcLastPageHeight():0);
this.resize();
this._invalidating=false;
},updateRowCount:function(_1b){
this.invalidateNodes();
this.rowCount=_1b;
var _1c=this.pageCount;
if(_1c===0){
this.height=1;
}
this.pageCount=this._getPageCount(this.rowCount,this.rowsPerPage);
if(this.pageCount<_1c){
for(var i=_1c-1;i>=this.pageCount;i--){
this.height-=this.getPageHeight(i);
delete this.pageHeights[i];
}
}else{
if(this.pageCount>_1c){
this.height+=this.defaultPageHeight*(this.pageCount-_1c-1)+this.calcLastPageHeight();
}
}
this.resize();
},pageExists:function(_1d){
return Boolean(this.getDefaultPageNode(_1d));
},measurePage:function(_1e){
if(this.grid.rowHeight){
return ((_1e+1)*this.rowsPerPage>this.rowCount?this.rowCount-_1e*this.rowsPerPage:this.rowsPerPage)*this.grid.rowHeight;
}
var n=this.getDefaultPageNode(_1e);
return (n&&n.innerHTML)?n.offsetHeight:undefined;
},positionPage:function(_1f,_20){
for(var i=0;i<this.colCount;i++){
this.pageNodes[i][_1f].style.top=_20+"px";
}
},repositionPages:function(_21){
var _22=this.getDefaultNodes();
var _23=0;
for(var i=0;i<this.stack.length;i++){
_23=Math.max(this.stack[i],_23);
}
var n=_22[_21];
var y=(n?this.getPageNodePosition(n)+this.getPageHeight(_21):0);
for(var p=_21+1;p<=_23;p++){
n=_22[p];
if(n){
if(this.getPageNodePosition(n)==y){
return;
}
this.positionPage(p,y);
}
y+=this.getPageHeight(p);
}
},installPage:function(_24){
for(var i=0;i<this.colCount;i++){
this.contentNodes[i].appendChild(this.pageNodes[i][_24]);
}
},preparePage:function(_25,_26){
var p=(_26?this.popPage():null);
for(var i=0;i<this.colCount;i++){
var _27=this.pageNodes[i];
var _28=(p===null?this.createPageNode():this.invalidatePageNode(p,_27));
_28.pageIndex=_25;
_27[_25]=_28;
}
},renderPage:function(_29){
var _2a=[];
var i,j;
for(i=0;i<this.colCount;i++){
_2a[i]=this.pageNodes[i][_29];
}
for(i=0,j=_29*this.rowsPerPage;(i<this.rowsPerPage)&&(j<this.rowCount);i++,j++){
this.renderRow(j,_2a);
}
},removePage:function(_2b){
for(var i=0,j=_2b*this.rowsPerPage;i<this.rowsPerPage;i++,j++){
this.removeRow(j);
}
},destroyPage:function(_2c){
for(var i=0;i<this.colCount;i++){
var n=this.invalidatePageNode(_2c,this.pageNodes[i]);
if(n){
_5.destroy(n);
}
}
},pacify:function(_2d){
},pacifying:false,pacifyTicks:200,setPacifying:function(_2e){
if(this.pacifying!=_2e){
this.pacifying=_2e;
this.pacify(this.pacifying);
}
},startPacify:function(){
this.startPacifyTicks=new Date().getTime();
},doPacify:function(){
var _2f=(new Date().getTime()-this.startPacifyTicks)>this.pacifyTicks;
this.setPacifying(true);
this.startPacify();
return _2f;
},endPacify:function(){
this.setPacifying(false);
},resize:function(){
if(this.scrollboxNode){
this.windowHeight=this.scrollboxNode.clientHeight;
}
for(var i=0;i<this.colCount;i++){
_4.setStyleHeightPx(this.contentNodes[i],Math.max(1,this.height));
}
var _30=(!this._invalidating);
if(!_30){
var ah=this.grid.get("autoHeight");
if(typeof ah=="number"&&ah<=Math.min(this.rowsPerPage,this.rowCount)){
_30=true;
}
}
if(_30){
this.needPage(this.page,this.pageTop);
}
var _31=(this.page<this.pageCount-1)?this.rowsPerPage:((this.rowCount%this.rowsPerPage)||this.rowsPerPage);
var _32=this.getPageHeight(this.page);
this.averageRowHeight=(_32>0&&_31>0)?(_32/_31):0;
},calcLastPageHeight:function(){
if(!this.pageCount){
return 0;
}
var _33=this.pageCount-1;
var _34=((this.rowCount%this.rowsPerPage)||(this.rowsPerPage))*this.defaultRowHeight;
this.pageHeights[_33]=_34;
return _34;
},updateContentHeight:function(_35){
this.height+=_35;
this.resize();
},updatePageHeight:function(_36,_37,_38){
if(this.pageExists(_36)){
var oh=this.getPageHeight(_36);
var h=(this.measurePage(_36));
if(h===undefined){
h=oh;
}
this.pageHeights[_36]=h;
if(oh!=h){
this.updateContentHeight(h-oh);
var ah=this.grid.get("autoHeight");
if((typeof ah=="number"&&ah>this.rowCount)||(ah===true&&!_37)){
if(!_38){
this.grid.sizeChange();
}else{
var ns=this.grid.viewsNode.style;
ns.height=parseInt(ns.height)+h-oh+"px";
this.repositionPages(_36);
}
}else{
this.repositionPages(_36);
}
}
return h;
}
return 0;
},rowHeightChanged:function(_39,_3a){
this.updatePageHeight(Math.floor(_39/this.rowsPerPage),false,_3a);
},invalidateNodes:function(){
while(this.stack.length){
this.destroyPage(this.popPage());
}
},createPageNode:function(){
var p=document.createElement("div");
_5.attr(p,"role","presentation");
p.style.position="absolute";
p.style[this.grid.isLeftToRight()?"left":"right"]="0";
return p;
},getPageHeight:function(_3b){
var ph=this.pageHeights[_3b];
return (ph!==undefined?ph:this.defaultPageHeight);
},pushPage:function(_3c){
return this.stack.push(_3c);
},popPage:function(){
return this.stack.shift();
},findPage:function(_3d){
var i=0,h=0;
for(var ph=0;i<this.pageCount;i++,h+=ph){
ph=this.getPageHeight(i);
if(h+ph>=_3d){
break;
}
}
this.page=i;
this.pageTop=h;
},buildPage:function(_3e,_3f,_40){
this.preparePage(_3e,_3f);
this.positionPage(_3e,_40);
this.installPage(_3e);
this.renderPage(_3e);
this.pushPage(_3e);
},needPage:function(_41,_42){
var h=this.getPageHeight(_41),oh=h;
if(!this.pageExists(_41)){
this.buildPage(_41,(!this.grid._autoHeight&&this.keepPages&&(this.stack.length>=this.keepPages)),_42);
h=this.updatePageHeight(_41,true);
}else{
this.positionPage(_41,_42);
}
return h;
},onscroll:function(){
this.scroll(this.scrollboxNode.scrollTop);
},scroll:function(_43){
this.grid.scrollTop=_43;
if(this.colCount){
this.startPacify();
this.findPage(_43);
var h=this.height;
var b=this.getScrollBottom(_43);
for(var p=this.page,y=this.pageTop;(p<this.pageCount)&&((b<0)||(y<b));p++){
y+=this.needPage(p,y);
}
this.firstVisibleRow=this.getFirstVisibleRow(this.page,this.pageTop,_43);
this.lastVisibleRow=this.getLastVisibleRow(p-1,y,b);
if(h!=this.height){
this.repositionPages(p-1);
}
this.endPacify();
}
},getScrollBottom:function(_44){
return (this.windowHeight>=0?_44+this.windowHeight:-1);
},processNodeEvent:function(e,_45){
var t=e.target;
while(t&&(t!=_45)&&t.parentNode&&(t.parentNode.parentNode!=_45)){
t=t.parentNode;
}
if(!t||!t.parentNode||(t.parentNode.parentNode!=_45)){
return false;
}
var _46=t.parentNode;
e.topRowIndex=_46.pageIndex*this.rowsPerPage;
e.rowIndex=e.topRowIndex+_6(t);
e.rowTarget=t;
return true;
},processEvent:function(e){
return this.processNodeEvent(e,this.contentNode);
},renderRow:function(_47,_48){
},removeRow:function(_49){
},getDefaultPageNode:function(_4a){
return this.getDefaultNodes()[_4a];
},positionPageNode:function(_4b,_4c){
},getPageNodePosition:function(_4d){
return _4d.offsetTop;
},invalidatePageNode:function(_4e,_4f){
var p=_4f[_4e];
if(p){
delete _4f[_4e];
this.removePage(_4e,p);
_8(p);
p.innerHTML="";
}
return p;
},getPageRow:function(_50){
return _50*this.rowsPerPage;
},getLastPageRow:function(_51){
return Math.min(this.rowCount,this.getPageRow(_51+1))-1;
},getFirstVisibleRow:function(_52,_53,_54){
if(!this.pageExists(_52)){
return 0;
}
var row=this.getPageRow(_52);
var _55=this.getDefaultNodes();
var _56=_11(_55[_52]);
for(var i=0,l=_56.length;i<l&&_53<_54;i++,row++){
_53+=_56[i].offsetHeight;
}
return (row?row-1:row);
},getLastVisibleRow:function(_57,_58,_59){
if(!this.pageExists(_57)){
return 0;
}
var _5a=this.getDefaultNodes();
var row=this.getLastPageRow(_57);
var _5b=_11(_5a[_57]);
for(var i=_5b.length-1;i>=0&&_58>_59;i--,row--){
_58-=_5b[i].offsetHeight;
}
return row+1;
},findTopRow:function(_5c){
var _5d=this.getDefaultNodes();
var _5e=_11(_5d[this.page]);
for(var i=0,l=_5e.length,t=this.pageTop,h;i<l;i++){
h=_5e[i].offsetHeight;
t+=h;
if(t>=_5c){
this.offset=h-(t-_5c);
return i+this.page*this.rowsPerPage;
}
}
return -1;
},findScrollTop:function(_5f){
var _60=Math.floor(_5f/this.rowsPerPage);
var t=0;
var i,l;
for(i=0;i<_60;i++){
t+=this.getPageHeight(i);
}
this.pageTop=t;
this.page=_60;
this.needPage(_60,this.pageTop);
var _61=this.getDefaultNodes();
var _62=_11(_61[_60]);
var r=_5f-this.rowsPerPage*_60;
for(i=0,l=_62.length;i<l&&i<r;i++){
t+=_62[i].offsetHeight;
}
return t;
},dummy:0});
});
