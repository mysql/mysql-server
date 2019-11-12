//>>built
define("dojox/mobile/pageTurningUtils",["dojo/_base/kernel","dojo/_base/array","dojo/_base/connect","dojo/_base/event","dojo/dom-class","dojo/dom-construct","dojo/dom-style"],function(_1,_2,_3,_4,_5,_6,_7){
_1.experimental("dojox.mobile.pageTurningUtils");
return function(){
this.w=0;
this.h=0;
this.turnfrom="top";
this.page=1;
this.dogear=1;
this.duration=2;
this.alwaysDogeared=false;
this._styleParams={};
this._catalogNode=null;
this._currentPageNode=null;
this._transitionEndHandle=null;
this.init=function(w,h,_8,_9,_a,_b,_c){
this.w=w;
this.h=h;
this.turnfrom=_8?_8:this.turnfrom;
this.page=_9?_9:this.page;
this.dogear=typeof _a!=="undefined"?_a:this.dogear;
this.duration=typeof _b!=="undefined"?_b:this.duration;
this.alwaysDogeared=typeof _c!=="undefined"?_c:this.alwaysDogeared;
if(this.turnfrom==="bottom"){
this.alwaysDogeared=true;
}
this._calcStyleParams();
};
this._calcStyleParams=function(){
var _d=Math.tan(58*Math.PI/180),_e=Math.cos(32*Math.PI/180),_f=Math.sin(32*Math.PI/180),_10=Math.tan(32*Math.PI/180),w=this.w,h=this.h,_11=this.page,_12=this.turnfrom,_13=this._styleParams;
var Q=fold=w*_d,fw=Q*_f+Q*_e*_d,fh=fold+w+w/_d,dw=w*0.11*this.dogear,pw=w-dw,_14=pw*_e,cx,cy,dx,dy,fy;
switch(this.turnfrom){
case "top":
cx=fw-_14;
cy=_14*_d;
dx=fw-dw;
dy=cy+pw/_d-7;
fy=cy/_e;
_13.init={page:{top:-fy+"px",left:(-fw+(_11===2?w:0))+"px",width:fw+"px",height:fh+"px",webkitTransformOrigin:"100% 0%"},front:{width:w+"px",height:h+"px",webkitBoxShadow:"0 0"},back:{width:w+"px",height:h+"px",webkitBoxShadow:"0 0"},shadow:{display:"",left:fw+"px",height:h*1.5+"px"}};
_13.turnForward={page:{webkitTransform:"rotate(0deg)"},front:{webkitTransform:"translate("+fw+"px,"+fy+"px) rotate(0deg)",webkitTransformOrigin:"-110px -18px"},back:{webkitTransform:"translate("+(fw-w)+"px,"+fy+"px) rotate(0deg)",webkitTransformOrigin:"0px 0px"}};
_13.turnBackward={page:{webkitTransform:"rotate(-32deg)"},front:{webkitTransform:"translate("+cx+"px,"+cy+"px) rotate(32deg)",webkitTransformOrigin:"0px 0px"},back:{webkitTransform:"translate("+dx+"px,"+dy+"px) rotate(-32deg)",webkitTransformOrigin:"0px 0px"}};
break;
case "bottom":
cx=fw-(h*_f+w*_e)-2;
cy=fh-(h+w/_10)*_e;
dx=fw;
dy=fh-w/_f-h;
fy=fh-w/_10-h;
_13.init={page:{top:(-fy+50)+"px",left:(-fw+(_11===2?w:0))+"px",width:fw+"px",height:fh+"px",webkitTransformOrigin:"100% 100%"},front:{width:w+"px",height:h+"px",webkitBoxShadow:"0 0"},back:{width:w+"px",height:h+"px",webkitBoxShadow:"0 0"},shadow:{display:"none"}};
_13.turnForward={page:{webkitTransform:"rotate(0deg)"},front:{webkitTransform:"translate("+fw+"px,"+fy+"px) rotate(0deg)",webkitTransformOrigin:"-220px 35px"},back:{webkitTransform:"translate("+(w*2)+"px,"+fy+"px) rotate(0deg)",webkitTransformOrigin:"0px 0px"}};
_13.turnBackward={page:{webkitTransform:"rotate(32deg)"},front:{webkitTransform:"translate("+cx+"px,"+cy+"px) rotate(-32deg)",webkitTransformOrigin:"0px 0px"},back:{webkitTransform:"translate("+dx+"px,"+dy+"px) rotate(0deg)",webkitTransformOrigin:"0px 0px"}};
break;
case "left":
cx=-w;
cy=pw/_10-2;
dx=-pw;
dy=fy=pw/_f+dw*_f;
_13.init={page:{top:-cy+"px",left:w+"px",width:fw+"px",height:fh+"px",webkitTransformOrigin:"0% 0%"},front:{width:w+"px",height:h+"px",webkitBoxShadow:"0 0"},back:{width:w+"px",height:h+"px",webkitBoxShadow:"0 0"},shadow:{display:"",left:"-4px",height:((_11===2?h*1.5:h)+50)+"px"}};
_13.turnForward={page:{webkitTransform:"rotate(0deg)"},front:{webkitTransform:"translate("+cx+"px,"+cy+"px) rotate(0deg)",webkitTransformOrigin:"160px 68px"},back:{webkitTransform:"translate(0px,"+cy+"px) rotate(0deg)",webkitTransformOrigin:"0px 0px"}};
_13.turnBackward={page:{webkitTransform:"rotate(32deg)"},front:{webkitTransform:"translate("+(-dw)+"px,"+dy+"px) rotate(-32deg)",webkitTransformOrigin:"0px 0px"},back:{webkitTransform:"translate("+dx+"px,"+dy+"px) rotate(32deg)",webkitTransformOrigin:"top right"}};
break;
}
_13.init.catalog={width:(_11===2?w*2:w)+"px",height:((_11===2?h*1.5:h)+(_12=="top"?0:50))+"px"};
};
this.getChildren=function(_15){
return _2.filter(_15.childNodes,function(n){
return n.nodeType===1;
});
};
this.getPages=function(){
return this._catalogNode?this.getChildren(this._catalogNode):null;
};
this.getCurrentPage=function(){
return this._currentPageNode;
};
this.getIndexOfPage=function(_16,_17){
if(!_17){
_17=this.getPages();
}
for(var i=0;i<_17.length;i++){
if(_16===_17[i]){
return i;
}
}
return -1;
};
this.getNextPage=function(_18){
for(var n=_18.nextSibling;n;n=n.nextSibling){
if(n.nodeType===1){
return n;
}
}
return null;
};
this.getPreviousPage=function(_19){
for(var n=_19.previousSibling;n;n=n.previousSibling){
if(n.nodeType===1){
return n;
}
}
return null;
};
this.isPageTurned=function(_1a){
return _1a.style.webkitTransform=="rotate(0deg)";
};
this._onPageTurned=function(e){
_4.stop(e);
if(_5.contains(e.target,"mblPageTurningPage")){
this.onPageTurned(e.target);
}
};
this.onPageTurned=function(){
};
this.initCatalog=function(_1b){
if(this._catalogNode!=_1b){
if(this._transitionEndHandle){
_3.disconnect(this._transitionEndHandle);
}
this._transitionEndHandle=_3.connect(_1b,"webkitTransitionEnd",this,"_onPageTurned");
this._catalogNode=_1b;
}
_5.add(_1b,"mblPageTurningCatalog");
_7.set(_1b,this._styleParams.init.catalog);
var _1c=this.getPages();
_2.forEach(_1c,function(_1d){
this.initPage(_1d);
},this);
this.resetCatalog();
};
this._getBaseZIndex=function(){
return this._catalogNode.style.zIndex||0;
};
this.resetCatalog=function(){
var _1e=this.getPages(),len=_1e.length,_1f=this._getBaseZIndex();
for(var i=len-1;i>=0;i--){
var _20=_1e[i];
this.showDogear(_20);
if(this.isPageTurned(_20)){
_20.style.zIndex=_1f+len+1;
}else{
_20.style.zIndex=_1f+len-i;
!this.alwaysDogeared&&this.hideDogear(_20);
this._currentPageNode=_20;
}
}
if(!this.alwaysDogeared&&this._currentPageNode!=_1e[len-1]){
this.showDogear(this._currentPageNode);
}
};
this.initPage=function(_21,dir){
var _22=this.getChildren(_21);
while(_22.length<3){
_21.appendChild(_6.create("div",null));
_22=this.getChildren(_21);
}
var _23=!_5.contains(_21,"mblPageTurningPage");
_5.add(_21,"mblPageTurningPage");
_5.add(_22[0],"mblPageTurningFront");
_5.add(_22[1],"mblPageTurningBack");
_5.add(_22[2],"mblPageTurningShadow");
var p=this._styleParams.init;
_7.set(_21,p.page);
_7.set(_22[0],p.front);
_7.set(_22[1],p.back);
p.shadow&&_7.set(_22[2],p.shadow);
if(!dir){
if(_23&&this._currentPageNode){
var _24=this.getPages();
dir=this.getIndexOfPage(_21)<this.getIndexOfPage(this._currentPageNode)?1:-1;
}else{
dir=this.isPageTurned(_21)?1:-1;
}
}
this._turnPage(_21,dir,0);
};
this.turnToNext=function(_25){
var _26=this.getNextPage(this._currentPageNode);
if(_26){
this._turnPage(this._currentPageNode,1,_25);
this._currentPageNode=_26;
}
};
this.turnToPrev=function(_27){
var _28=this.getPreviousPage(this._currentPageNode);
if(_28){
this._turnPage(_28,-1,_27);
this._currentPageNode=_28;
}
};
this.goTo=function(_29){
var _2a=this.getPages();
if(this._currentPageNode===_2a[_29]||_2a.length<=_29){
return;
}
var _2b=_29<this.getIndexOfPage(this._currentPageNode,_2a);
while(this._currentPageNode!==_2a[_29]){
_2b?this.turnToPrev(0):this.turnToNext(0);
}
};
this._turnPage=function(_2c,dir,_2d){
var _2e=this.getChildren(_2c),d=((typeof _2d!=="undefined")?_2d:this.duration)+"s",p=(dir===1)?this._styleParams.turnForward:this._styleParams.turnBackward;
p.page.webkitTransitionDuration=d;
_7.set(_2c,p.page);
p.front.webkitTransitionDuration=d;
_7.set(_2e[0],p.front);
p.back.webkitTransitionDuration=d;
_7.set(_2e[1],p.back);
var _2f=this.getPages(),_30=this.getNextPage(_2c),len=_2f.length,_31=this._getBaseZIndex();
if(dir===1){
_2c.style.zIndex=_31+len+1;
if(!this.alwaysDogeared&&_30&&this.getNextPage(_30)){
this.showDogear(_30);
}
}else{
if(_30){
_30.style.zIndex=_31+len-this.getIndexOfPage(_30,_2f);
!this.alwaysDogeared&&this.hideDogear(_30);
}
}
};
this.showDogear=function(_32){
var _33=this.getChildren(_32);
_7.set(_32,"overflow","");
_33[1]&&_7.set(_33[1],"display","");
_33[2]&&_7.set(_33[2],"display",this.turnfrom==="bottom"?"none":"");
};
this.hideDogear=function(_34){
if(this.turnfrom==="bottom"){
return;
}
var _35=this.getChildren(_34);
_7.set(_34,"overflow","visible");
_35[1]&&_7.set(_35[1],"display","none");
_35[2]&&_7.set(_35[2],"display","none");
};
};
});
