//>>built
define("dojox/editor/plugins/NormalizeStyle",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/_editor/html","dojo/_base/connect","dojo/_base/declare"],function(_1,_2,_3,_4,_5){
_1.declare("dojox.editor.plugins.NormalizeStyle",_4,{mode:"semantic",condenseSpans:true,setEditor:function(_6){
this.editor=_6;
_6.customUndo=true;
if(this.mode==="semantic"){
this.editor.contentDomPostFilters.push(_1.hitch(this,this._convertToSemantic));
}else{
if(this.mode==="css"){
this.editor.contentDomPostFilters.push(_1.hitch(this,this._convertToCss));
}
}
if(_1.isIE){
this.editor.contentDomPreFilters.push(_1.hitch(this,this._convertToSemantic));
this._browserFilter=this._convertToSemantic;
}else{
if(_1.isWebKit){
this.editor.contentDomPreFilters.push(_1.hitch(this,this._convertToCss));
this._browserFilter=this._convertToCss;
}else{
if(_1.isMoz){
this.editor.contentDomPreFilters.push(_1.hitch(this,this._convertToSemantic));
this._browserFilter=this._convertToSemantic;
}else{
this.editor.contentDomPreFilters.push(_1.hitch(this,this._convertToSemantic));
this._browserFilter=this._convertToSemantic;
}
}
}
if(this.editor._inserthtmlImpl){
this.editor._oldInsertHtmlImpl=this.editor._inserthtmlImpl;
}
this.editor._inserthtmlImpl=_1.hitch(this,this._inserthtmlImpl);
},_convertToSemantic:function(_7){
if(_7){
var _8=this.editor.document;
var _9=this;
var _a=function(_b){
if(_b.nodeType==1){
if(_b.id!=="dijitEditorBody"){
var _c=_b.style;
var _d=_b.tagName?_b.tagName.toLowerCase():"";
var _e;
if(_c&&_d!="table"&&_d!="ul"&&_d!="ol"){
var fw=_c.fontWeight?_c.fontWeight.toLowerCase():"";
var fs=_c.fontStyle?_c.fontStyle.toLowerCase():"";
var td=_c.textDecoration?_c.textDecoration.toLowerCase():"";
var s=_c.fontSize?_c.fontSize.toLowerCase():"";
var bc=_c.backgroundColor?_c.backgroundColor.toLowerCase():"";
var c=_c.color?_c.color.toLowerCase():"";
var _f=function(_10,_11){
if(_10){
while(_11.firstChild){
_10.appendChild(_11.firstChild);
}
if(_d=="span"&&!_11.style.cssText){
_1.place(_10,_11,"before");
_11.parentNode.removeChild(_11);
_11=_10;
}else{
_11.appendChild(_10);
}
}
return _11;
};
switch(fw){
case "bold":
case "bolder":
case "700":
case "800":
case "900":
_e=_8.createElement("b");
_b.style.fontWeight="";
break;
}
_b=_f(_e,_b);
_e=null;
if(fs=="italic"){
_e=_8.createElement("i");
_b.style.fontStyle="";
}
_b=_f(_e,_b);
_e=null;
if(td){
var da=td.split(" ");
var _12=0;
_1.forEach(da,function(s){
switch(s){
case "underline":
_e=_8.createElement("u");
break;
case "line-through":
_e=_8.createElement("strike");
break;
}
_12++;
if(_12==da.length){
_b.style.textDecoration="";
}
_b=_f(_e,_b);
_e=null;
});
}
if(s){
var _13={"xx-small":1,"x-small":2,"small":3,"medium":4,"large":5,"x-large":6,"xx-large":7,"-webkit-xxx-large":7};
if(s.indexOf("pt")>0){
s=s.substring(0,s.indexOf("pt"));
s=parseInt(s);
if(s<5){
s="xx-small";
}else{
if(s<10){
s="x-small";
}else{
if(s<15){
s="small";
}else{
if(s<20){
s="medium";
}else{
if(s<25){
s="large";
}else{
if(s<30){
s="x-large";
}else{
if(s>30){
s="xx-large";
}
}
}
}
}
}
}
}else{
if(s.indexOf("px")>0){
s=s.substring(0,s.indexOf("px"));
s=parseInt(s);
if(s<5){
s="xx-small";
}else{
if(s<10){
s="x-small";
}else{
if(s<15){
s="small";
}else{
if(s<20){
s="medium";
}else{
if(s<25){
s="large";
}else{
if(s<30){
s="x-large";
}else{
if(s>30){
s="xx-large";
}
}
}
}
}
}
}
}
}
var _14=_13[s];
if(!_14){
_14=3;
}
_e=_8.createElement("font");
_e.setAttribute("size",_14);
_b.style.fontSize="";
}
_b=_f(_e,_b);
_e=null;
if(bc&&_d!=="font"&&_9._isInline(_d)){
bc=new _1.Color(bc).toHex();
_e=_8.createElement("font");
_e.style.backgroundColor=bc;
_b.style.backgroundColor="";
}
if(c&&_d!=="font"){
c=new _1.Color(c).toHex();
_e=_8.createElement("font");
_e.setAttribute("color",c);
_b.style.color="";
}
_b=_f(_e,_b);
_e=null;
}
}
if(_b.childNodes){
var _15=[];
_1.forEach(_b.childNodes,function(n){
_15.push(n);
});
_1.forEach(_15,_a);
}
}
return _b;
};
return this._normalizeTags(_a(_7));
}
return _7;
},_normalizeTags:function(_16){
var w=this.editor.window;
var doc=this.editor.document;
_1.query("em,s,strong",_16).forEach(function(n){
var tag=n.tagName?n.tagName.toLowerCase():"";
var _17;
switch(tag){
case "s":
_17="strike";
break;
case "em":
_17="i";
break;
case "strong":
_17="b";
break;
}
if(_17){
var _18=doc.createElement(_17);
_1.place("<"+_17+">",n,"before");
while(n.firstChild){
_18.appendChild(n.firstChild);
}
n.parentNode.removeChild(n);
}
});
return _16;
},_convertToCss:function(_19){
if(_19){
var doc=this.editor.document;
var _1a=function(_1b){
if(_1b.nodeType==1){
if(_1b.id!=="dijitEditorBody"){
var tag=_1b.tagName?_1b.tagName.toLowerCase():"";
if(tag){
var _1c;
switch(tag){
case "b":
case "strong":
_1c=doc.createElement("span");
_1c.style.fontWeight="bold";
break;
case "i":
case "em":
_1c=doc.createElement("span");
_1c.style.fontStyle="italic";
break;
case "u":
_1c=doc.createElement("span");
_1c.style.textDecoration="underline";
break;
case "strike":
case "s":
_1c=doc.createElement("span");
_1c.style.textDecoration="line-through";
break;
case "font":
var _1d={};
if(_1.attr(_1b,"color")){
_1d.color=_1.attr(_1b,"color");
}
if(_1.attr(_1b,"face")){
_1d.fontFace=_1.attr(_1b,"face");
}
if(_1b.style&&_1b.style.backgroundColor){
_1d.backgroundColor=_1b.style.backgroundColor;
}
if(_1b.style&&_1b.style.color){
_1d.color=_1b.style.color;
}
var _1e={1:"xx-small",2:"x-small",3:"small",4:"medium",5:"large",6:"x-large",7:"xx-large"};
if(_1.attr(_1b,"size")){
_1d.fontSize=_1e[_1.attr(_1b,"size")];
}
_1c=doc.createElement("span");
_1.style(_1c,_1d);
break;
}
if(_1c){
while(_1b.firstChild){
_1c.appendChild(_1b.firstChild);
}
_1.place(_1c,_1b,"before");
_1b.parentNode.removeChild(_1b);
_1b=_1c;
}
}
}
if(_1b.childNodes){
var _1f=[];
_1.forEach(_1b.childNodes,function(n){
_1f.push(n);
});
_1.forEach(_1f,_1a);
}
}
return _1b;
};
_19=_1a(_19);
if(this.condenseSpans){
this._condenseSpans(_19);
}
}
return _19;
},_condenseSpans:function(_20){
var _21=function(_22){
var _23=function(_24){
var m;
if(_24){
m={};
var _25=_24.toLowerCase().split(";");
_1.forEach(_25,function(s){
if(s){
var ss=s.split(":");
var key=ss[0]?_1.trim(ss[0]):"";
var val=ss[1]?_1.trim(ss[1]):"";
if(key&&val){
var i;
var _26="";
for(i=0;i<key.length;i++){
var ch=key.charAt(i);
if(ch=="-"){
i++;
ch=key.charAt(i);
_26+=ch.toUpperCase();
}else{
_26+=ch;
}
}
m[_26]=val;
}
}
});
}
return m;
};
if(_22&&_22.nodeType==1){
var tag=_22.tagName?_22.tagName.toLowerCase():"";
if(tag==="span"&&_22.childNodes&&_22.childNodes.length===1){
var c=_22.firstChild;
while(c&&c.nodeType==1&&c.tagName&&c.tagName.toLowerCase()=="span"){
if(!_1.attr(c,"class")&&!_1.attr(c,"id")&&c.style){
var s1=_23(_22.style.cssText);
var s2=_23(c.style.cssText);
if(s1&&s2){
var _27={};
var i;
for(i in s1){
if(!s1[i]||!s2[i]||s1[i]==s2[i]){
_27[i]=s1[i];
delete s2[i];
}else{
if(s1[i]!=s2[i]){
if(i=="textDecoration"){
_27[i]=s1[i]+" "+s2[i];
delete s2[i];
}else{
_27=null;
}
break;
}else{
_27=null;
break;
}
}
}
if(_27){
for(i in s2){
_27[i]=s2[i];
}
_1.style(_22,_27);
while(c.firstChild){
_22.appendChild(c.firstChild);
}
var t=c.nextSibling;
c.parentNode.removeChild(c);
c=t;
}else{
c=c.nextSibling;
}
}else{
c=c.nextSibling;
}
}else{
c=c.nextSibling;
}
}
}
}
if(_22.childNodes&&_22.childNodes.length){
_1.forEach(_22.childNodes,_21);
}
};
_21(_20);
},_isInline:function(tag){
switch(tag){
case "a":
case "b":
case "strong":
case "s":
case "strike":
case "i":
case "u":
case "em":
case "sup":
case "sub":
case "span":
case "font":
case "big":
case "cite":
case "q":
case "img":
case "small":
return true;
default:
return false;
}
},_inserthtmlImpl:function(_28){
if(_28){
var doc=this.editor.document;
var div=doc.createElement("div");
div.innerHTML=_28;
div=this._browserFilter(div);
_28=_5.getChildrenHtml(div);
div.innerHTML="";
if(this.editor._oldInsertHtmlImpl){
return this.editor._oldInsertHtmlImpl(_28);
}else{
return this.editor.execCommand("inserthtml",_28);
}
}
return false;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _29=o.args.name.toLowerCase();
if(_29==="normalizestyle"){
o.plugin=new _3.editor.plugins.NormalizeStyle({mode:("mode" in o.args)?o.args.mode:"semantic",condenseSpans:("condenseSpans" in o.args)?o.args.condenseSpans:true});
}
});
return _3.editor.plugins.NormalizeStyle;
});
