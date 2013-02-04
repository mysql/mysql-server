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
this.defaultPageHeight=this.defaultRowHeight*this.rowsPerPage;
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
var _1f=this.grid.rowHeight+1;
return ((_1e+1)*this.rowsPerPage>this.rowCount?this.rowCount-_1e*this.rowsPerPage:this.rowsPerPage)*_1f;
}
var n=this.getDefaultPageNode(_1e);
return (n&&n.innerHTML)?n.offsetHeight:undefined;
},positionPage:function(_20,_21){
for(var i=0;i<this.colCount;i++){
this.pageNodes[i][_20].style.top=_21+"px";
}
},repositionPages:function(_22){
var _23=this.getDefaultNodes();
var _24=0;
for(var i=0;i<this.stack.length;i++){
_24=Math.max(this.stack[i],_24);
}
var n=_23[_22];
var y=(n?this.getPageNodePosition(n)+this.getPageHeight(_22):0);
for(var p=_22+1;p<=_24;p++){
n=_23[p];
if(n){
if(this.getPageNodePosition(n)==y){
return;
}
this.positionPage(p,y);
}
y+=this.getPageHeight(p);
}
},installPage:function(_25){
for(var i=0;i<this.colCount;i++){
this.contentNodes[i].appendChild(this.pageNodes[i][_25]);
}
},preparePage:function(_26,_27){
var p=(_27?this.popPage():null);
for(var i=0;i<this.colCount;i++){
var _28=this.pageNodes[i];
var _29=(p===null?this.createPageNode():this.invalidatePageNode(p,_28));
_29.pageIndex=_26;
_28[_26]=_29;
}
},renderPage:function(_2a){
var _2b=[];
var i,j;
for(i=0;i<this.colCount;i++){
_2b[i]=this.pageNodes[i][_2a];
}
for(i=0,j=_2a*this.rowsPerPage;(i<this.rowsPerPage)&&(j<this.rowCount);i++,j++){
this.renderRow(j,_2b);
}
},removePage:function(_2c){
for(var i=0,j=_2c*this.rowsPerPage;i<this.rowsPerPage;i++,j++){
this.removeRow(j);
}
},destroyPage:function(_2d){
for(var i=0;i<this.colCount;i++){
var n=this.invalidatePageNode(_2d,this.pageNodes[i]);
if(n){
_5.destroy(n);
}
}
},pacify:function(_2e){
},pacifying:false,pacifyTicks:200,setPacifying:function(_2f){
if(this.pacifying!=_2f){
this.pacifying=_2f;
this.pacify(this.pacifying);
}
},startPacify:function(){
this.startPacifyTicks=new Date().getTime();
},doPacify:function(){
var _30=(new Date().getTime()-this.startPacifyTicks)>this.pacifyTicks;
this.setPacifying(true);
this.startPacify();
return _30;
},endPacify:function(){
this.setPacifying(false);
},resize:function(){
if(this.scrollboxNode){
this.windowHeight=this.scrollboxNode.clientHeight;
}
for(var i=0;i<this.colCount;i++){
_4.setStyleHeightPx(this.contentNodes[i],Math.max(1,this.height));
}
var _31=(!this._invalidating);
if(!_31){
var ah=this.grid.get("autoHeight");
if(typeof ah=="number"&&ah<=Math.min(this.rowsPerPage,this.rowCount)){
_31=true;
}
}
if(_31){
this.needPage(this.page,this.pageTop);
}
var _32=(this.page<this.pageCount-1)?this.rowsPerPage:((this.rowCount%this.rowsPerPage)||this.rowsPerPage);
var _33=this.getPageHeight(this.page);
this.averageRowHeight=(_33>0&&_32>0)?(_33/_32):0;
},calcLastPageHeight:function(){
if(!this.pageCount){
return 0;
}
var _34=this.pageCount-1;
var _35=((this.rowCount%this.rowsPerPage)||(this.rowsPerPage))*this.defaultRowHeight;
this.pageHeights[_34]=_35;
return _35;
},updateContentHeight:function(_36){
this.height+=_36;
this.resize();
},updatePageHeight:function(_37,_38,_39){
if(this.pageExists(_37)){
var oh=this.getPageHeight(_37);
var h=(this.measurePage(_37));
if(h===undefined){
h=oh;
}
this.pageHeights[_37]=h;
if(oh!=h){
this.updateContentHeight(h-oh);
var ah=this.grid.get("autoHeight");
if((typeof ah=="number"&&ah>this.rowCount)||(ah===true&&!_38)){
if(!_39){
this.grid.sizeChange();
}else{
var ns=this.grid.viewsNode.style;
ns.height=parseInt(ns.height)+h-oh+"px";
this.repositionPages(_37);
}
}else{
this.repositionPages(_37);
}
}
return h;
}
return 0;
},rowHeightChanged:function(_3a,_3b){
this.updatePageHeight(Math.floor(_3a/this.rowsPerPage),false,_3b);
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
},getPageHeight:function(_3c){
var ph=this.pageHeights[_3c];
return (ph!==undefined?ph:this.defaultPageHeight);
},pushPage:function(_3d){
return this.stack.push(_3d);
},popPage:function(){
return this.stack.shift();
},findPage:function(_3e){
var i=0,h=0;
for(var ph=0;i<this.pageCount;i++,h+=ph){
ph=this.getPageHeight(i);
if(h+ph>=_3e){
break;
}
}
this.page=i;
this.pageTop=h;
},buildPage:function(_3f,_40,_41){
this.preparePage(_3f,_40);
this.positionPage(_3f,_41);
this.installPage(_3f);
this.renderPage(_3f);
this.pushPage(_3f);
},needPage:function(_42,_43){
var h=this.getPageHeight(_42),oh=h;
if(!this.pageExists(_42)){
this.buildPage(_42,(!this.grid._autoHeight&&this.keepPages&&(this.stack.length>=this.keepPages)),_43);
h=this.updatePageHeight(_42,true);
}else{
this.positionPage(_42,_43);
}
return h;
},onscroll:function(){
this.scroll(this.scrollboxNode.scrollTop);
},scroll:function(_44){
this.grid.scrollTop=_44;
if(this.colCount){
this.startPacify();
this.findPage(_44);
var h=this.height;
var b=this.getScrollBottom(_44);
for(var p=this.page,y=this.pageTop;(p<this.pageCount)&&((b<0)||(y<b));p++){
y+=this.needPage(p,y);
}
this.firstVisibleRow=this.getFirstVisibleRow(this.page,this.pageTop,_44);
this.lastVisibleRow=this.getLastVisibleRow(p-1,y,b);
if(h!=this.height){
this.repositionPages(p-1);
}
this.endPacify();
}
},getScrollBottom:function(_45){
return (this.windowHeight>=0?_45+this.windowHeight:-1);
},processNodeEvent:function(e,_46){
var t=e.target;
while(t&&(t!=_46)&&t.parentNode&&(t.parentNode.parentNode!=_46)){
t=t.parentNode;
}
if(!t||!t.parentNode||(t.parentNode.parentNode!=_46)){
return false;
}
var _47=t.parentNode;
e.topRowIndex=_47.pageIndex*this.rowsPerPage;
e.rowIndex=e.topRowIndex+_6(t);
e.rowTarget=t;
return true;
},processEvent:function(e){
return this.processNodeEvent(e,this.contentNode);
},renderRow:function(_48,_49){
},removeRow:function(_4a){
},getDefaultPageNode:function(_4b){
return this.getDefaultNodes()[_4b];
},positionPageNode:function(_4c,_4d){
},getPageNodePosition:function(_4e){
return _4e.offsetTop;
},invalidatePageNode:function(_4f,_50){
var p=_50[_4f];
if(p){
delete _50[_4f];
this.removePage(_4f,p);
_8(p);
p.innerHTML="";
}
return p;
},getPageRow:function(_51){
return _51*this.rowsPerPage;
},getLastPageRow:function(_52){
return Math.min(this.rowCount,this.getPageRow(_52+1))-1;
},getFirstVisibleRow:function(_53,_54,_55){
if(!this.pageExists(_53)){
return 0;
}
var row=this.getPageRow(_53);
var _56=this.getDefaultNodes();
var _57=_11(_56[_53]);
for(var i=0,l=_57.length;i<l&&_54<_55;i++,row++){
_54+=_57[i].offsetHeight;
}
return (row?row-1:row);
},getLastVisibleRow:function(_58,_59,_5a){
if(!this.pageExists(_58)){
return 0;
}
var _5b=this.getDefaultNodes();
var row=this.getLastPageRow(_58);
var _5c=_11(_5b[_58]);
for(var i=_5c.length-1;i>=0&&_59>_5a;i--,row--){
_59-=_5c[i].offsetHeight;
}
return row+1;
},findTopRow:function(_5d){
var _5e=this.getDefaultNodes();
var _5f=_11(_5e[this.page]);
for(var i=0,l=_5f.length,t=this.pageTop,h;i<l;i++){
h=_5f[i].offsetHeight;
t+=h;
if(t>=_5d){
this.offset=h-(t-_5d);
return i+this.page*this.rowsPerPage;
}
}
return -1;
},findScrollTop:function(_60){
var _61=Math.floor(_60/this.rowsPerPage);
var t=0;
var i,l;
for(i=0;i<_61;i++){
t+=this.getPageHeight(i);
}
this.pageTop=t;
this.page=_61;
this.needPage(_61,this.pageTop);
var _62=this.getDefaultNodes();
var _63=_11(_62[_61]);
var r=_60-this.rowsPerPage*_61;
for(i=0,l=_63.length;i<l&&i<r;i++){
t+=_63[i].offsetHeight;
}
return t;
},dummy:0});
});
