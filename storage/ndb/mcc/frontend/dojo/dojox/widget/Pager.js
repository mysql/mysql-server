//>>built
require({cache:{"url:dojox/widget/Pager/Pager.html":"<div dojoAttachPoint=\"pagerContainer\" tabIndex=\"0\" dojoAttachEvent=\"onkeypress: _handleKey, onfocus: _a11yStyle, onblur:_a11yStyle\" class=\"${orientation}PagerContainer\">\n    <div class=\"pagerContainer\">\n\t\t<div dojoAttachPoint=\"pagerContainerStatus\" class=\"${orientation}PagerStatus\"></div>\n\t\t<div dojoAttachPoint=\"pagerContainerView\" class=\"${orientation}PagerView\">\n\t\t    <div dojoAttachPoint=\"pagerItemContainer\"><ul dojoAttachPoint=\"pagerItems\" class=\"pagerItems\"></ul></div>\n\t\t</div>\n\t\t<div dojoAttachPoint=\"pagerContainerPager\" class=\"${orientation}PagerPager\">\n\t\t\t<div tabIndex=\"0\" dojoAttachPoint=\"pagerNext\" class=\"pagerIconContainer\" dojoAttachEvent=\"onclick: _next\"><img dojoAttachPoint=\"pagerIconNext\" src=\"${iconNext}\" alt=\"Next\" /></div>\n\t\t\t<div tabIndex=\"0\" dojoAttachPoint=\"pagerPrevious\" class=\"pagerIconContainer\" dojoAttachEvent=\"onclick: _previous\"><img dojoAttachPoint=\"pagerIconPrevious\" src=\"${iconPrevious}\" alt=\"Previous\" /></div>\n\t\t</div>\n    </div>\n\t<div dojoAttachPoint=\"containerNode\" style=\"display:none\"></div>\n</div>"}});
define("dojox/widget/Pager",["dojo/aspect","dojo/_base/array","dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/fx","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/on","dijit/_WidgetBase","dijit/_TemplatedMixin","./PagerItem","dojo/text!./Pager/Pager.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,fx,_a,_b,_c,on,_d,_e,_f,_10){
_a.experimental("dojox.widget.Pager");
return _3("dojox.widget.Pager",[_d,_e],{templateString:_10,iconPrevious:"",iconNext:"",iconPage:require.toUrl("dojox/widget/Pager/images/pageInactive.png"),iconPageActive:require.toUrl("dojox/widget/Pager/images/pageActive.png"),store:null,orientation:"horizontal",statusPos:"leading",pagerPos:"center",duration:500,itemSpace:2,resizeChildren:true,itemClass:_f,itemsPage:3,postMixInProperties:function(){
var h=(this.orientation=="horizontal");
_c.mixin(this,{_totalPages:0,_currentPage:1,dirClass:"pager"+(h?"Horizontal":"Vertical"),iconNext:require.toUrl("dojox/widget/Pager/images/"+(h?"h":"v")+"Next.png"),iconPrevious:require.toUrl("dojox/widget/Pager/images/"+(h?"h":"v")+"Previous.png")});
},_next:function(){
if(!this.isLeftToRight()){
this._pagerPrevious();
}else{
this._pagerNext();
}
},_previous:function(){
if(!this.isLeftToRight()){
this._pagerNext();
}else{
this._pagerPrevious();
}
},postCreate:function(){
this.inherited(arguments);
this.store.fetch({onComplete:_c.hitch(this,"_init")});
},_a11yStyle:function(e){
_6.toggle(e.target,"pagerFocus",(e.type=="focus"));
},_handleKey:function(e){
var key=(e.charCode==_b.SPACE?_b.SPACE:e.keyCode);
switch(key){
case _b.UP_ARROW:
case _b.RIGHT_ARROW:
case 110:
case 78:
e.preventDefault();
this._next();
break;
case _b.DOWN_ARROW:
case _b.LEFT_ARROW:
case 112:
case 80:
e.preventDefault();
this._previous();
break;
case _b.ENTER:
switch(e.target){
case this.pagerNext:
this._next();
break;
case this.pagerPrevious:
this._previous();
break;
}
break;
}
},_init:function(_11){
this.items=_11;
this._renderPages();
this._renderStatus();
this._renderPager();
},generatePagerItem:function(_12,cnt){
var _13=this.itemClass,_14=(typeof _13=="string"?_c.getObject(_13):_13);
var _15=_7.create("div",{innerHTML:_12.content});
return new _14({id:this.id+"-item-"+(cnt+1)},_15);
},_renderPages:function(){
var pcv=this.pagerContainerView,_16=(this.orientation=="horizontal");
if(_16){
var _17=_8.getMarginBox(this.pagerContainerPager).h,_18=_8.getMarginBox(this.pagerContainerStatus).h;
if(this.pagerPos!="center"){
var _19=_17+_18;
}else{
var _19=_18;
var _1a=this.pagerIconNext.width,_1b=_9.get(pcv,"width"),_1c=_1b-(2*_1a);
_9.set(pcv,{width:_1c+"px",marginLeft:this.pagerIconNext.width+"px",marginRight:this.pagerIconNext.width+"px"});
}
var _1d=_9.get(this.pagerContainer,"height")-_19;
_9.set(this.pagerContainerView,"height",_1d+"px");
var _1e=Math.floor(_9.get(pcv,"width")/this.itemsPage);
if(this.statusPos=="trailing"){
if(this.pagerPos!="center"){
_9.set(pcv,"marginTop",_17+"px");
}
_9.set(pcv,"marginBottom",_18+"px");
}else{
_9.set(pcv,"marginTop",_18+"px");
if(this.pagerPos!="center"){
_9.set(pcv,"marginTop",_17+"px");
}
}
}else{
var _1f=_8.getMarginBox(this.pagerContainerPager).w,_20=_8.getMarginBox(this.pagerContainerStatus).w,_21=_9.get(this.pagerContainer,"width");
if(this.pagerPos!="center"){
var _22=_1f+_20;
}else{
var _22=_20,_23=this.pagerIconNext.height,_24=_9.get(pcv,"height"),_25=_24-(2*_23);
_9.set(pcv,{height:_25+"px",marginTop:this.pagerIconNext.height+"px",marginBottom:this.pagerIconNext.height+"px"});
}
var _26=_9.get(this.pagerContainer,"width")-_22;
_9.set(pcv,"width",_26+"px");
var _1e=Math.floor(_9.get(pcv,"height")/this.itemsPage);
if(this.statusPos=="trailing"){
if(this.pagerPos!="center"){
_9.set(pcv,"marginLeft",_1f+"px");
}
_9.set(pcv,"marginRight",_20+"px");
}else{
_9.set(pcv,"marginLeft",_20+"px");
if(this.pagerPos!="center"){
_9.set(pcv,"marginRight",_1f+"px");
}
}
}
var _27="padding"+(_16?"Left":"Top"),_28="padding"+(_16?"Right":"Bottom");
_2.forEach(this.items,function(_29,cnt){
var _2a=this.generatePagerItem(_29,cnt),_2b={};
this.pagerItems.appendChild(_2a.domNode);
_2b[(_16?"width":"height")]=(_1e-this.itemSpace)+"px";
var p=(_16?"height":"width");
_2b[p]=_9.get(pcv,p)+"px";
_9.set(_2a.containerNode,_2b);
if(this.resizeChildren){
_2a.resizeChildren();
}
_2a.parseChildren();
_9.set(_2a.domNode,"position","absolute");
if(cnt<this.itemsPage){
var pos=(cnt)*_1e,_2c=(_16?"left":"top"),dir=(_16?"top":"left");
_9.set(_2a.domNode,dir,"0px");
_9.set(_2a.domNode,_2c,pos+"px");
}else{
_9.set(_2a.domNode,"top","-1000px");
_9.set(_2a.domNode,"left","-1000px");
}
_9.set(_2a.domNode,_28,(this.itemSpace/2)+"px");
_9.set(_2a.domNode,_27,(this.itemSpace/2)+"px");
},this);
},_renderPager:function(){
var tcp=this.pagerContainerPager,_2d="0px",_2e=(this.orientation=="horizontal");
if(_2e){
if(this.statusPos=="center"){
}else{
if(this.statusPos=="trailing"){
_9.set(tcp,"top",_2d);
}else{
_9.set(tcp,"bottom",_2d);
}
}
_9.set(this.pagerNext,"right",_2d);
_9.set(this.pagerPrevious,"left",_2d);
}else{
if(this.statusPos=="trailing"){
_9.set(tcp,"left",_2d);
}else{
_9.set(tcp,"right",_2d);
}
_9.set(this.pagerNext,"bottom",_2d);
_9.set(this.pagerPrevious,"top",_2d);
}
},_renderStatus:function(){
this._totalPages=Math.ceil(this.items.length/this.itemsPage);
this.iconWidth=0;
this.iconHeight=0;
this.iconsLoaded=0;
this._iconConnects=[];
for(var i=1;i<=this._totalPages;i++){
var _2f=new Image();
var _30=i;
on(_2f,"click",_c.hitch(this,"_pagerSkip",_30));
this._iconConnects[_30]=on(_2f,"load",_c.hitch(this,function(_31){
this.iconWidth+=_2f.width;
this.iconHeight+=_2f.height;
this.iconsLoaded++;
if(this._totalPages==this.iconsLoaded){
if(this.orientation=="horizontal"){
if(this.statusPos=="trailing"){
if(this.pagerPos=="center"){
var _32=_9.get(this.pagerContainer,"height"),_33=_9.get(this.pagerContainerStatus,"height");
_9.set(this.pagerContainerPager,"top",((_32/2)-(_33/2))+"px");
}
_9.set(this.pagerContainerStatus,"bottom","0px");
}else{
if(this.pagerPos=="center"){
var _32=_9.get(this.pagerContainer,"height"),_33=_9.get(this.pagerContainerStatus,"height");
_9.set(this.pagerContainerPager,"bottom",((_32/2)-(_33/2))+"px");
}
_9.set(this.pagerContainerStatus,"top","0px");
}
var _34=(_9.get(this.pagerContainer,"width")/2)-(this.iconWidth/2);
_9.set(this.pagerContainerStatus,this.isLeftToRight()?"paddingLeft":"paddingRight",_34+"px");
}else{
if(this.statusPos=="trailing"){
if(this.pagerPos=="center"){
var _35=_9.get(this.pagerContainer,"width"),_36=_9.get(this.pagerContainerStatus,"width");
_9.set(this.pagerContainerPager,"left",((_35/2)-(_36/2))+"px");
}
_9.set(this.pagerContainerStatus,"right","0px");
}else{
if(this.pagerPos=="center"){
var _35=_9.get(this.pagerContainer,"width"),_36=_9.get(this.pagerContainerStatus,"width");
_9.set(this.pagerContainerPager,"right",((_35/2)-(_36/2))+"px");
}
_9.set(this.pagerContainerStatus,"left","0px");
}
var _34=(_9.get(this.pagerContainer,"height")/2)-(this.iconHeight/2);
_9.set(this.pagerContainerStatus,"paddingTop",_34+"px");
}
}
this._iconConnects[_31].remove();
},_30));
if(i==this._currentPage){
_2f.src=this.iconPageActive;
}else{
_2f.src=this.iconPage;
}
var _30=i;
_6.add(_2f,this.orientation+"PagerIcon");
_5.set(_2f,"id",this.id+"-status-"+i);
this.pagerContainerStatus.appendChild(_2f);
if(this.orientation=="vertical"){
_9.set(_2f,"display","block");
}
}
},_pagerSkip:function(_37){
if(this._currentPage==_37){
return;
}else{
var _38;
var _39;
if(_37<this._currentPage){
_38=this._currentPage-_37;
_39=(this._totalPages+_37)-this._currentPage;
}else{
_38=(this._totalPages+this._currentPage)-_37;
_39=_37-this._currentPage;
}
var b=(_39>_38);
this._toScroll=(b?_38:_39);
var cmd=(b?"_pagerPrevious":"_pagerNext"),_3a=_1.after(this,"onScrollEnd",_c.hitch(this,function(){
this._toScroll--;
if(this._toScroll<1){
_3a.remove();
}else{
this[cmd]();
}
}),true);
this[cmd]();
}
},_pagerNext:function(){
if(this._anim){
return;
}
var _3b=[];
for(var i=this._currentPage*this.itemsPage;i>(this._currentPage-1)*this.itemsPage;i--){
if(!_4.byId(this.id+"-item-"+i)){
continue;
}
var _3c=_4.byId(this.id+"-item-"+i);
var _3d=_8.getMarginBox(_3c);
if(this.orientation=="horizontal"){
var _3e=_3d.l-(this.itemsPage*_3d.w);
_3b.push(fx.slideTo({node:_3c,left:_3e,duration:this.duration}));
}else{
var _3e=_3d.t-(this.itemsPage*_3d.h);
_3b.push(fx.slideTo({node:_3c,top:_3e,duration:this.duration}));
}
}
var _3f=this._currentPage;
if(this._currentPage==this._totalPages){
this._currentPage=1;
}else{
this._currentPage++;
}
var cnt=this.itemsPage;
for(var i=this._currentPage*this.itemsPage;i>(this._currentPage-1)*this.itemsPage;i--){
if(_4.byId(this.id+"-item-"+i)){
var _3c=_4.byId(this.id+"-item-"+i);
var _3d=_8.getMarginBox(_3c);
if(this.orientation=="horizontal"){
var _40=(_9.get(this.pagerContainerView,"width")+((cnt-1)*_3d.w))-1;
_9.set(_3c,"left",_40+"px");
_9.set(_3c,"top","0px");
var _3e=_40-(this.itemsPage*_3d.w);
_3b.push(fx.slideTo({node:_3c,left:_3e,duration:this.duration}));
}else{
_40=(_9.get(this.pagerContainerView,"height")+((cnt-1)*_3d.h))-1;
_9.set(_3c,"top",_40+"px");
_9.set(_3c,"left","0px");
var _3e=_40-(this.itemsPage*_3d.h);
_3b.push(fx.slideTo({node:_3c,top:_3e,duration:this.duration}));
}
}
cnt--;
}
this._anim=fx.combine(_3b);
var _41=_1.after(this._anim,"onEnd",_c.hitch(this,function(){
delete this._anim;
this.onScrollEnd();
_41.remove();
}),true);
this._anim.play();
_4.byId(this.id+"-status-"+_3f).src=this.iconPage;
_4.byId(this.id+"-status-"+this._currentPage).src=this.iconPageActive;
},_pagerPrevious:function(){
if(this._anim){
return;
}
var _42=[];
for(var i=this._currentPage*this.itemsPage;i>(this._currentPage-1)*this.itemsPage;i--){
if(!_4.byId(this.id+"-item-"+i)){
continue;
}
var _43=_4.byId(this.id+"-item-"+i);
var _44=_8.getMarginBox(_43);
if(this.orientation=="horizontal"){
var _45=_9.get(_43,"left")+(this.itemsPage*_44.w);
_42.push(fx.slideTo({node:_43,left:_45,duration:this.duration}));
}else{
var _45=_9.get(_43,"top")+(this.itemsPage*_44.h);
_42.push(fx.slideTo({node:_43,top:_45,duration:this.duration}));
}
}
var _46=this._currentPage;
if(this._currentPage==1){
this._currentPage=this._totalPages;
}else{
this._currentPage--;
}
var cnt=this.itemsPage;
var j=1;
for(var i=this._currentPage*this.itemsPage;i>(this._currentPage-1)*this.itemsPage;i--){
if(_4.byId(this.id+"-item-"+i)){
var _43=_4.byId(this.id+"-item-"+i),_44=_8.getMarginBox(_43);
if(this.orientation=="horizontal"){
var _47=-(j*_44.w)+1;
_9.set(_43,"left",_47+"px");
_9.set(_43,"top","0px");
var _45=((cnt-1)*_44.w);
_42.push(fx.slideTo({node:_43,left:_45,duration:this.duration}));
var _45=_47+(this.itemsPage*_44.w);
_42.push(fx.slideTo({node:_43,left:_45,duration:this.duration}));
}else{
_47=-((j*_44.h)+1);
_9.set(_43,"top",_47+"px");
_9.set(_43,"left","0px");
var _45=((cnt-1)*_44.h);
_42.push(fx.slideTo({node:_43,top:_45,duration:this.duration}));
}
}
cnt--;
j++;
}
this._anim=fx.combine(_42);
var _48=_1.after(this._anim,"onEnd",_c.hitch(this,function(){
delete this._anim;
this.onScrollEnd();
_48.remove();
}),true);
this._anim.play();
_4.byId(this.id+"-status-"+_46).src=this.iconPage;
_4.byId(this.id+"-status-"+this._currentPage).src=this.iconPageActive;
},onScrollEnd:function(){
}});
});
