//>>built
define("dijit/form/_ExpandingTextAreaMixin",["dojo/_base/declare","dojo/dom-construct","dojo/_base/lang","dojo/_base/window"],function(_1,_2,_3,_4){
var _5;
return _1("dijit.form._ExpandingTextAreaMixin",null,{_setValueAttr:function(){
this.inherited(arguments);
this.resize();
},postCreate:function(){
this.inherited(arguments);
var _6=this.textbox;
if(_5==undefined){
var te=_2.create("textarea",{rows:"5",cols:"20",value:" ",style:{zoom:1,overflow:"hidden",visibility:"hidden",position:"absolute",border:"0px solid black",padding:"0px"}},_4.body(),"last");
_5=te.scrollHeight>=te.clientHeight;
_4.body().removeChild(te);
}
this.connect(_6,"onscroll","_resizeLater");
this.connect(_6,"onresize","_resizeLater");
this.connect(_6,"onfocus","_resizeLater");
_6.style.overflowY="hidden";
this._estimateHeight();
this._resizeLater();
},_onInput:function(e){
this.inherited(arguments);
this.resize();
},_estimateHeight:function(){
var _7=this.textbox;
_7.style.height="auto";
_7.rows=(_7.value.match(/\n/g)||[]).length+2;
},_resizeLater:function(){
setTimeout(_3.hitch(this,"resize"),0);
},resize:function(){
function _8(){
var _9=false;
if(_a.value===""){
_a.value=" ";
_9=true;
}
var sh=_a.scrollHeight;
if(_9){
_a.value="";
}
return sh;
};
var _a=this.textbox;
if(_a.style.overflowY=="hidden"){
_a.scrollTop=0;
}
if(this.resizeTimer){
clearTimeout(this.resizeTimer);
}
this.resizeTimer=null;
if(this.busyResizing){
return;
}
this.busyResizing=true;
if(_8()||_a.offsetHeight){
var _b=_a.style.height;
if(!(/px/.test(_b))){
_b=_8();
_a.rows=1;
_a.style.height=_b+"px";
}
var _c=Math.max(parseInt(_b)-_a.clientHeight,0)+_8();
var _d=_c+"px";
if(_d!=_a.style.height){
_a.rows=1;
_a.style.height=_d;
}
if(_5){
var _e=_8();
_a.style.height="auto";
if(_8()<_e){
_d=_c-_e+_8()+"px";
}
_a.style.height=_d;
}
_a.style.overflowY=_8()>_a.clientHeight?"auto":"hidden";
}else{
this._estimateHeight();
}
this.busyResizing=false;
},destroy:function(){
if(this.resizeTimer){
clearTimeout(this.resizeTimer);
}
if(this.shrinkTimer){
clearTimeout(this.shrinkTimer);
}
this.inherited(arguments);
}});
});
