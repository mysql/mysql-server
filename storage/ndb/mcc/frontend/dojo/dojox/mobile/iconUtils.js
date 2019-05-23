//>>built
define("dojox/mobile/iconUtils",["dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","./sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var dm=_5.getObject("dojox.mobile",true);
var _b=function(){
this.setupSpriteIcon=function(_c,_d){
if(_c&&_d){
var _e=_1.map(_d.split(/[ ,]/),function(_f){
return _f-0;
});
var t=_e[0];
var r=_e[1]+_e[2];
var b=_e[0]+_e[3];
var l=_e[1];
_9.set(_c,{clip:"rect("+t+"px "+r+"px "+b+"px "+l+"px)",top:(_c.parentNode?_9.get(_c,"top"):0)-t+"px",left:-l+"px"});
_7.add(_c,"mblSpriteIcon");
}
};
this.createDomButton=function(_10,_11,_12){
if(!this._domButtons){
if(_a("webkit")){
var _13=function(_14,dic){
var i,j;
if(!_14){
var _15={};
var ss=_6.doc.styleSheets;
for(i=0;i<ss.length;i++){
ss[i]&&_13(ss[i],_15);
}
return _15;
}
var _16=_14.cssRules||[];
for(i=0;i<_16.length;i++){
var _17=_16[i];
if(_17.href&&_17.styleSheet){
_13(_17.styleSheet,dic);
}else{
if(_17.selectorText){
var _18=_17.selectorText.split(/,/);
for(j=0;j<_18.length;j++){
var sel=_18[j];
var n=sel.split(/>/).length-1;
if(sel.match(/(mblDomButton\w+)/)){
var cls=RegExp.$1;
if(!dic[cls]||n>dic[cls]){
dic[cls]=n;
}
}
}
}
}
}
return dic;
};
this._domButtons=_13();
}else{
this._domButtons={};
}
}
var s=_10.className;
var _19=_12||_10;
if(s.match(/(mblDomButton\w+)/)&&s.indexOf("/")===-1){
var _1a=RegExp.$1;
var _1b=4;
if(s.match(/(mblDomButton\w+_(\d+))/)){
_1b=RegExp.$2-0;
}else{
if(this._domButtons[_1a]!==undefined){
_1b=this._domButtons[_1a];
}
}
var _1c=null;
if(_a("bb")&&_2["mblBBBoxShadowWorkaround"]!==false){
_1c={style:"-webkit-box-shadow:none"};
}
for(var i=0,p=_19;i<_1b;i++){
p=p.firstChild||_8.create("div",_1c,p);
}
if(_12){
setTimeout(function(){
_7.remove(_10,_1a);
},0);
_7.add(_12,_1a);
}
}else{
if(s.indexOf(".")!==-1){
_8.create("img",{src:s},_19);
}else{
return null;
}
}
_7.add(_19,"mblDomButton");
!!_11&&_9.set(_19,_11);
return _19;
};
this.createIcon=function(_1d,_1e,_1f,_20,_21,_22,pos){
_20=_20||"";
if(_1d&&_1d.indexOf("mblDomButton")===0){
if(!_1f){
_1f=_8.create("div",null,_22||_21,pos);
}else{
if(_1f.className.match(/(mblDomButton\w+)/)){
_7.remove(_1f,RegExp.$1);
}
}
_1f.title=_20;
_7.add(_1f,_1d);
this.createDomButton(_1f);
}else{
if(_1d&&_1d!=="none"){
if(!_1f||_1f.nodeName!=="IMG"){
_1f=_8.create("img",{alt:_20},_22||_21,pos);
}
_1f.src=(_1d||"").replace("${theme}",dm.currentTheme);
this.setupSpriteIcon(_1f,_1e);
if(_1e&&_21){
var arr=_1e.split(/[ ,]/);
_9.set(_21,{width:arr[2]+"px",height:arr[3]+"px"});
_7.add(_21,"mblSpriteIconParent");
}
_3.connect(_1f,"ondragstart",_4,"stop");
}
}
return _1f;
};
this.iconWrapper=false;
this.setIcon=function(_23,_24,_25,alt,_26,_27,pos){
if(!_26||!_23&&!_25){
return null;
}
if(_23&&_23!=="none"){
if(!this.iconWrapper&&_23.indexOf("mblDomButton")!==0&&!_24){
if(_25&&_25.tagName==="DIV"){
_8.destroy(_25);
_25=null;
}
_25=this.createIcon(_23,null,_25,alt,_26,_27,pos);
_7.add(_25,"mblImageIcon");
}else{
if(_25&&_25.tagName==="IMG"){
_8.destroy(_25);
_25=null;
}
_25&&_8.empty(_25);
if(!_25){
_25=_8.create("div",null,_27||_26,pos);
}
this.createIcon(_23,_24,null,null,_25);
if(alt){
_25.title=alt;
}
}
_7.remove(_26,"mblNoIcon");
return _25;
}else{
_8.destroy(_25);
_7.add(_26,"mblNoIcon");
return null;
}
};
};
return new _b();
});
