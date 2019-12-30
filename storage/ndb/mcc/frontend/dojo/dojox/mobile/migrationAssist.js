//>>built
define("dojox/mobile/migrationAssist",["dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/ready","dijit/_Container","dijit/_WidgetBase","./_ItemBase","./common","./FixedSplitterPane","./Heading","./iconUtils","./ListItem","./RoundRect","./SpinWheel","./SpinWheelSlot","./SwapView","./TabBarButton","./ToolBarButton","./View"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16){
var _17;
var _18=function(){
var get=function(w,key){
return w[key]||w.srcNodeRef&&w.srcNodeRef.getAttribute(key);
};
this.dispatch=function(cls,w){
var _19=cls.replace(/.*\./,"");
this["check"+_19]&&this["check"+_19](w);
};
this.checkCarousel=function(w){
};
this.checkFixedSplitter=function(w){
if(!this._fixedSplitter_css_checked){
this._fixedSplitter_css_checked=true;
var _1a=_5.create("div",{className:"mblFixedSplitter"},_3.body());
if(_6.get(_1a,"height")==0){
_5.create("link",{href:"../themes/android/FixedSplitter.css",type:"text/css",rel:"stylesheet"},_3.doc.getElementsByTagName("head")[0]);
}
_3.body().removeChild(_1a);
setTimeout(function(){
w.resize();
},1000);
}
};
this.checkFixedSplitterPane=function(w){
};
this.checkFixedSplitter=function(w){
if(!this._fixedSplitter_css_checked){
this._fixedSplitter_css_checked=true;
var _1b=_5.create("div",{className:"mblFixedSplitter"},_3.body());
if(_6.get(_1b,"height")==0){
_5.create("link",{href:"../themes/android/FixedSplitter.css",type:"text/css",rel:"stylesheet"},_3.doc.getElementsByTagName("head")[0]);
}
_3.body().removeChild(_1b);
setTimeout(function(){
w.resize();
},1000);
}
};
this.checkListItem=function(w){
if(w.sync!==undefined||w.srcNodeRef&&w.srcNodeRef.getAttribute("sync")){
}
if(w.btnClass!==undefined||w.srcNodeRef&&w.srcNodeRef.getAttribute("btnClass")){
w.rightIcon=w.btnClass||w.srcNodeRef&&w.srcNodeRef.getAttribute("btnClass");
}
if(w.btnClass2!==undefined||w.srcNodeRef&&w.srcNodeRef.getAttribute("btnClass2")){
w.rightIcon2=w.btnClass2||w.srcNodeRef&&w.srcNodeRef.getAttribute("btnClass2");
}
};
this.checkSpinWheelSlot=function(w){
if(w.labels&&w.labels[0]&&w.labels[0].charAt(0)==="["){
for(var i=0;i<w.labels.length;i++){
w.labels[i]=w.labels[i].replace(/^\[*[\'\"]*/,"");
w.labels[i]=w.labels[i].replace(/[\'\"]*\]*$/,"");
}
}
};
this.checkSwapView=function(w){
var n=w.srcNodeRef;
if(n){
var _1c=n.getAttribute("dojoType")||n.getAttribute("data-dojo-type");
if(_1c==="dojox.mobile.FlippableView"){
}
}
};
this.checkSwitch=function(w){
if(w["class"]==="mblItemSwitch"){
}
};
this.checkTabBar=function(w){
if(get(w,"barType")==="segmentedControl"){
_5.create("style",{innerHTML:".iphone_theme .mblTabBarSegmentedControl .mblTabBarButtonIconArea { display: none; }"},_3.doc.getElementsByTagName("head")[0]);
}
};
this.checkTabBarButton=function(w){
if((w["class"]||"").indexOf("mblDomButton")===0){
w.icon=w["class"];
w["class"]="";
if(w.srcNodeRef){
w.srcNodeRef.className="";
}
}
};
this.checkToolBarButton=function(w){
if((w["class"]||"").indexOf("mblColor")===0){
w.defaultColor=w["class"];
w["class"]="";
if(w.srcNodeRef){
w.srcNodeRef.className="";
}
}
if((w["class"]||"").indexOf("mblDomButton")===0){
w.icon=w["class"];
w["class"]="";
if(w.srcNodeRef){
w.srcNodeRef.className="";
}
}
};
};
dojox.mobile.FlippableView=_13;
var _1d=new _18();
_9.prototype.postMixInProperties=function(){
_1d.dispatch(this.declaredClass,this);
dojo.forEach([_c,_d,_10,_11,_14,_15,_16],function(_1e){
if(this.declaredClass!==_1e.prototype.declaredClass&&this instanceof _1e){
_1d.dispatch(_1e.prototype.declaredClass,this);
}
},this);
};
extendSelectFunction=function(obj){
_2.extend(obj,{select:function(){
obj.prototype.set.apply(this,["selected",!arguments[0]]);
},deselect:function(){
this.select(true);
}});
};
extendSelectFunction(_15);
extendSelectFunction(_14);
_2.extend(_f,{set:function(key,_1f){
if(key==="btnClass"){
key="rightIcon";
}else{
if(key==="btnClass2"){
key="rightIcon2";
}
}
_9.prototype.set.apply(this,[key,_1f]);
}});
_2.extend(_11,{getValue:function(){
return this.get("values");
},setValue:function(_20){
return this.set("values",_20);
}});
_2.extend(_12,{getValue:function(){
return this.get("value");
},getKey:function(){
return this.get("key");
},setValue:function(_21){
return this.set("value",_21);
}});
_2.mixin(_b,{createDomButton:function(){
return _e.createDomButton.apply(this,arguments);
}});
var _22=[],i,j;
var s=_3.doc.styleSheets;
for(i=0;i<s.length;i++){
if(s[i].href){
continue;
}
var r=s[i].cssRules||s[i].imports;
if(!r){
continue;
}
for(j=0;j<r.length;j++){
if(r[j].href){
_22.push(r[j].href);
}
}
}
var _23=_3.doc.getElementsByTagName("link");
for(i=0;i<_23.length;i++){
_22.push(_23[i].href);
}
for(i=0;i<_22.length;i++){
if(_22[i].indexOf("/iphone/")!==-1){
_17="iphone";
}else{
if(_22[i].indexOf("/android/")!==-1){
_17="android";
}else{
if(_22[i].indexOf("/blackberry/")!==-1){
_17="blackberry";
}else{
if(_22[i].indexOf("/custom/")!==-1){
_17="custom";
}
}
}
}
_4.add(_3.doc.documentElement,_17+"_theme");
if(_22[i].match(/themes\/common\/(FixedSplitter.css)|themes\/common\/(SpinWheel.css)/)){
}
}
_7(function(){
if(dojo.hash){
if(dojo.require){
dojo["require"]("dojox.mobile.bookmarkable");
}else{
require(["dojox/mobile/bookmarkable"]);
}
}
});
return _1d;
});
