//>>built
define("dojox/html/styles",["dojo/_base/lang","dojo/_base/array","dojo/_base/window","dojo/_base/sniff"],function(_1,_2,_3,_4){
var dh=_1.getObject("dojox.html",true);
var _5={};
var _6={};
var _7=[];
dh.insertCssRule=function(_8,_9,_a){
var ss=dh.getDynamicStyleSheet(_a);
var _b=_8+" {"+_9+"}";
if(_4("ie")){
ss.cssText+=_b;
}else{
if(ss.sheet){
ss.sheet.insertRule(_b,ss._indicies.length);
}else{
ss.appendChild(_3.doc.createTextNode(_b));
}
}
ss._indicies.push(_8+" "+_9);
return _8;
};
dh.removeCssRule=function(_c,_d,_e){
var ss;
var _f=-1;
var nm;
var i;
for(nm in _5){
if(_e&&_e!==nm){
continue;
}
ss=_5[nm];
for(i=0;i<ss._indicies.length;i++){
if(_c+" "+_d===ss._indicies[i]){
_f=i;
break;
}
}
if(_f>-1){
break;
}
}
if(!ss){
console.warn("No dynamic style sheet has been created from which to remove a rule.");
return false;
}
if(_f===-1){
console.warn("The css rule was not found and could not be removed.");
return false;
}
ss._indicies.splice(_f,1);
if(_4("ie")){
ss.removeRule(_f);
}else{
if(ss.sheet){
ss.sheet.deleteRule(_f);
}
}
return true;
};
dh.modifyCssRule=function(_10,_11,_12){
};
dh.getStyleSheet=function(_13){
if(_5[_13||"default"]){
return _5[_13||"default"];
}
if(!_13){
return false;
}
var _14=dh.getStyleSheets();
if(_14[_13]){
return dh.getStyleSheets()[_13];
}
var nm;
for(nm in _14){
if(_14[nm].href&&_14[nm].href.indexOf(_13)>-1){
return _14[nm];
}
}
return false;
};
dh.getDynamicStyleSheet=function(_15){
if(!_15){
_15="default";
}
if(!_5[_15]){
if(_3.doc.createStyleSheet){
_5[_15]=_3.doc.createStyleSheet();
if(_4("ie")<9){
_5[_15].title=_15;
}
}else{
_5[_15]=_3.doc.createElement("style");
_5[_15].setAttribute("type","text/css");
_3.doc.getElementsByTagName("head")[0].appendChild(_5[_15]);
}
_5[_15]._indicies=[];
}
return _5[_15];
};
dh.enableStyleSheet=function(_16){
var ss=dh.getStyleSheet(_16);
if(ss){
if(ss.sheet){
ss.sheet.disabled=false;
}else{
ss.disabled=false;
}
}
};
dh.disableStyleSheet=function(_17){
var ss=dh.getStyleSheet(_17);
if(ss){
if(ss.sheet){
ss.sheet.disabled=true;
}else{
ss.disabled=true;
}
}
};
dh.activeStyleSheet=function(_18){
var _19=dh.getToggledStyleSheets();
var i;
if(arguments.length===1){
_2.forEach(_19,function(s){
s.disabled=(s.title===_18)?false:true;
});
}else{
for(i=0;i<_19.length;i++){
if(_19[i].disabled===false){
return _19[i];
}
}
}
return true;
};
dh.getPreferredStyleSheet=function(){
};
dh.getToggledStyleSheets=function(){
var nm;
if(!_7.length){
var _1a=dh.getStyleSheets();
for(nm in _1a){
if(_1a[nm].title){
_7.push(_1a[nm]);
}
}
}
return _7;
};
dh.getStyleSheets=function(){
if(_6.collected){
return _6;
}
var _1b=_3.doc.styleSheets;
_2.forEach(_1b,function(n){
var s=(n.sheet)?n.sheet:n;
var _1c=s.title||s.href;
if(_4("ie")){
if(s.cssText.indexOf("#default#VML")===-1){
if(s.href){
_6[_1c]=s;
}else{
if(s.imports.length){
_2.forEach(s.imports,function(si){
_6[si.title||si.href]=si;
});
}else{
_6[_1c]=s;
}
}
}
}else{
_6[_1c]=s;
_6[_1c].id=s.ownerNode.id;
_2.forEach(s.cssRules,function(r){
if(r.href){
_6[r.href]=r.styleSheet;
_6[r.href].id=s.ownerNode.id;
}
});
}
});
_6.collected=true;
return _6;
};
return dh;
});
