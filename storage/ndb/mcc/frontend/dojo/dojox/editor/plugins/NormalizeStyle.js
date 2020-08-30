//>>built
define("dojox/editor/plugins/NormalizeStyle",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/_editor/html","dojo/_base/connect","dojo/_base/declare"],function(_1,_2,_3,_4,_5){
var _6=_1.declare("dojox.editor.plugins.NormalizeStyle",_4,{mode:"semantic",condenseSpans:true,setEditor:function(_7){
this.editor=_7;
_7.customUndo=true;
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
},_convertToSemantic:function(_8){
if(_8){
var _9=this.editor.document;
var _a=this;
var _b=function(_c){
if(_c.nodeType==1){
if(_c.id!=="dijitEditorBody"){
var _d=_c.style;
var _e=_c.tagName?_c.tagName.toLowerCase():"";
var _f;
if(_d&&_e!="table"&&_e!="ul"&&_e!="ol"){
var fw=_d.fontWeight?_d.fontWeight.toLowerCase():"";
var fs=_d.fontStyle?_d.fontStyle.toLowerCase():"";
var td=_d.textDecoration?_d.textDecoration.toLowerCase():"";
var s=_d.fontSize?_d.fontSize.toLowerCase():"";
var bc=_d.backgroundColor?_d.backgroundColor.toLowerCase():"";
var c=_d.color?_d.color.toLowerCase():"";
var _10=function(_11,_12){
if(_11){
while(_12.firstChild){
_11.appendChild(_12.firstChild);
}
if(_e=="span"&&!_12.style.cssText){
_1.place(_11,_12,"before");
_12.parentNode.removeChild(_12);
_12=_11;
}else{
_12.appendChild(_11);
}
}
return _12;
};
switch(fw){
case "bold":
case "bolder":
case "700":
case "800":
case "900":
_f=_9.createElement("b");
_c.style.fontWeight="";
break;
}
_c=_10(_f,_c);
_f=null;
if(fs=="italic"){
_f=_9.createElement("i");
_c.style.fontStyle="";
}
_c=_10(_f,_c);
_f=null;
if(td){
var da=td.split(" ");
var _13=0;
_1.forEach(da,function(s){
switch(s){
case "underline":
_f=_9.createElement("u");
break;
case "line-through":
_f=_9.createElement("strike");
break;
}
_13++;
if(_13==da.length){
_c.style.textDecoration="";
}
_c=_10(_f,_c);
_f=null;
});
}
if(s){
var _14={"xx-small":1,"x-small":2,"small":3,"medium":4,"large":5,"x-large":6,"xx-large":7,"-webkit-xxx-large":7};
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
var _15=_14[s];
if(!_15){
_15=3;
}
_f=_9.createElement("font");
_f.setAttribute("size",_15);
_c.style.fontSize="";
}
_c=_10(_f,_c);
_f=null;
if(bc&&_e!=="font"&&_a._isInline(_e)){
bc=new _1.Color(bc).toHex();
_f=_9.createElement("font");
_f.style.backgroundColor=bc;
_c.style.backgroundColor="";
}
if(c&&_e!=="font"){
c=new _1.Color(c).toHex();
_f=_9.createElement("font");
_f.setAttribute("color",c);
_c.style.color="";
}
_c=_10(_f,_c);
_f=null;
}
}
if(_c.childNodes){
var _16=[];
_1.forEach(_c.childNodes,function(n){
_16.push(n);
});
_1.forEach(_16,_b);
}
}
return _c;
};
return this._normalizeTags(_b(_8));
}
return _8;
},_normalizeTags:function(_17){
var w=this.editor.window;
var doc=this.editor.document;
_1.query("em,s,strong",_17).forEach(function(n){
var tag=n.tagName?n.tagName.toLowerCase():"";
var _18;
switch(tag){
case "s":
_18="strike";
break;
case "em":
_18="i";
break;
case "strong":
_18="b";
break;
}
if(_18){
var _19=doc.createElement(_18);
_1.place("<"+_18+">",n,"before");
while(n.firstChild){
_19.appendChild(n.firstChild);
}
n.parentNode.removeChild(n);
}
});
return _17;
},_convertToCss:function(_1a){
if(_1a){
var doc=this.editor.document;
var _1b=function(_1c){
if(_1c.nodeType==1){
if(_1c.id!=="dijitEditorBody"){
var tag=_1c.tagName?_1c.tagName.toLowerCase():"";
if(tag){
var _1d;
switch(tag){
case "b":
case "strong":
_1d=doc.createElement("span");
_1d.style.fontWeight="bold";
break;
case "i":
case "em":
_1d=doc.createElement("span");
_1d.style.fontStyle="italic";
break;
case "u":
_1d=doc.createElement("span");
_1d.style.textDecoration="underline";
break;
case "strike":
case "s":
_1d=doc.createElement("span");
_1d.style.textDecoration="line-through";
break;
case "font":
var _1e={};
if(_1.attr(_1c,"color")){
_1e.color=_1.attr(_1c,"color");
}
if(_1.attr(_1c,"face")){
_1e.fontFace=_1.attr(_1c,"face");
}
if(_1c.style&&_1c.style.backgroundColor){
_1e.backgroundColor=_1c.style.backgroundColor;
}
if(_1c.style&&_1c.style.color){
_1e.color=_1c.style.color;
}
var _1f={1:"xx-small",2:"x-small",3:"small",4:"medium",5:"large",6:"x-large",7:"xx-large"};
if(_1.attr(_1c,"size")){
_1e.fontSize=_1f[_1.attr(_1c,"size")];
}
_1d=doc.createElement("span");
_1.style(_1d,_1e);
break;
}
if(_1d){
while(_1c.firstChild){
_1d.appendChild(_1c.firstChild);
}
_1.place(_1d,_1c,"before");
_1c.parentNode.removeChild(_1c);
_1c=_1d;
}
}
}
if(_1c.childNodes){
var _20=[];
_1.forEach(_1c.childNodes,function(n){
_20.push(n);
});
_1.forEach(_20,_1b);
}
}
return _1c;
};
_1a=_1b(_1a);
if(this.condenseSpans){
this._condenseSpans(_1a);
}
}
return _1a;
},_condenseSpans:function(_21){
var _22=function(_23){
var _24=function(_25){
var m;
if(_25){
m={};
var _26=_25.toLowerCase().split(";");
_1.forEach(_26,function(s){
if(s){
var ss=s.split(":");
var key=ss[0]?_1.trim(ss[0]):"";
var val=ss[1]?_1.trim(ss[1]):"";
if(key&&val){
var i;
var _27="";
for(i=0;i<key.length;i++){
var ch=key.charAt(i);
if(ch=="-"){
i++;
ch=key.charAt(i);
_27+=ch.toUpperCase();
}else{
_27+=ch;
}
}
m[_27]=val;
}
}
});
}
return m;
};
if(_23&&_23.nodeType==1){
var tag=_23.tagName?_23.tagName.toLowerCase():"";
if(tag==="span"&&_23.childNodes&&_23.childNodes.length===1){
var c=_23.firstChild;
while(c&&c.nodeType==1&&c.tagName&&c.tagName.toLowerCase()=="span"){
if(!_1.attr(c,"class")&&!_1.attr(c,"id")&&c.style){
var s1=_24(_23.style.cssText);
var s2=_24(c.style.cssText);
if(s1&&s2){
var _28={};
var i;
for(i in s1){
if(!s1[i]||!s2[i]||s1[i]==s2[i]){
_28[i]=s1[i];
delete s2[i];
}else{
if(s1[i]!=s2[i]){
if(i=="textDecoration"){
_28[i]=s1[i]+" "+s2[i];
delete s2[i];
}else{
_28=null;
}
break;
}else{
_28=null;
break;
}
}
}
if(_28){
for(i in s2){
_28[i]=s2[i];
}
_1.style(_23,_28);
while(c.firstChild){
_23.appendChild(c.firstChild);
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
if(_23.childNodes&&_23.childNodes.length){
_1.forEach(_23.childNodes,_22);
}
};
_22(_21);
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
},_inserthtmlImpl:function(_29){
if(_29){
var doc=this.editor.document;
var div=doc.createElement("div");
div.innerHTML=_29;
div=this._browserFilter(div);
_29=_5.getChildrenHtml(div);
div.innerHTML="";
if(this.editor._oldInsertHtmlImpl){
return this.editor._oldInsertHtmlImpl(_29);
}else{
return this.editor.execCommand("inserthtml",_29);
}
}
return false;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _2a=o.args.name.toLowerCase();
if(_2a==="normalizestyle"){
o.plugin=new _6({mode:("mode" in o.args)?o.args.mode:"semantic",condenseSpans:("condenseSpans" in o.args)?o.args.condenseSpans:true});
}
});
return _6;
});
