//>>built
define("dojox/editor/plugins/NormalizeStyle",["dojo","dijit","dojox","dijit/_editor/html","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.NormalizeStyle",_2._editor._Plugin,{mode:"semantic",condenseSpans:true,setEditor:function(_4){
this.editor=_4;
_4.customUndo=true;
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
},_convertToSemantic:function(_5){
if(_5){
var w=this.editor.window;
var _6=this;
var _7=function(_8){
if(_8.nodeType==1){
if(_8.id!=="dijitEditorBody"){
var _9=_8.style;
var _a=_8.tagName?_8.tagName.toLowerCase():"";
var _b;
if(_9&&_a!="table"&&_a!="ul"&&_a!="ol"){
var fw=_9.fontWeight?_9.fontWeight.toLowerCase():"";
var fs=_9.fontStyle?_9.fontStyle.toLowerCase():"";
var td=_9.textDecoration?_9.textDecoration.toLowerCase():"";
var s=_9.fontSize?_9.fontSize.toLowerCase():"";
var bc=_9.backgroundColor?_9.backgroundColor.toLowerCase():"";
var c=_9.color?_9.color.toLowerCase():"";
var _c=function(_d,_e){
if(_d){
while(_e.firstChild){
_d.appendChild(_e.firstChild);
}
if(_a=="span"&&!_e.style.cssText){
_1.place(_d,_e,"before");
_e.parentNode.removeChild(_e);
_e=_d;
}else{
_e.appendChild(_d);
}
}
return _e;
};
switch(fw){
case "bold":
case "bolder":
case "700":
case "800":
case "900":
_b=_1.withGlobal(w,"create",_1,["b",{}]);
_8.style.fontWeight="";
break;
}
_8=_c(_b,_8);
_b=null;
if(fs=="italic"){
_b=_1.withGlobal(w,"create",_1,["i",{}]);
_8.style.fontStyle="";
}
_8=_c(_b,_8);
_b=null;
if(td){
var da=td.split(" ");
var _f=0;
_1.forEach(da,function(s){
switch(s){
case "underline":
_b=_1.withGlobal(w,"create",_1,["u",{}]);
break;
case "line-through":
_b=_1.withGlobal(w,"create",_1,["strike",{}]);
break;
}
_f++;
if(_f==da.length){
_8.style.textDecoration="";
}
_8=_c(_b,_8);
_b=null;
});
}
if(s){
var _10={"xx-small":1,"x-small":2,"small":3,"medium":4,"large":5,"x-large":6,"xx-large":7,"-webkit-xxx-large":7};
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
var _11=_10[s];
if(!_11){
_11=3;
}
_b=_1.withGlobal(w,"create",_1,["font",{size:_11}]);
_8.style.fontSize="";
}
_8=_c(_b,_8);
_b=null;
if(bc&&_a!=="font"&&_6._isInline(_a)){
bc=new _1.Color(bc).toHex();
_b=_1.withGlobal(w,"create",_1,["font",{style:{backgroundColor:bc}}]);
_8.style.backgroundColor="";
}
if(c&&_a!=="font"){
c=new _1.Color(c).toHex();
_b=_1.withGlobal(w,"create",_1,["font",{color:c}]);
_8.style.color="";
}
_8=_c(_b,_8);
_b=null;
}
}
if(_8.childNodes){
var _12=[];
_1.forEach(_8.childNodes,function(n){
_12.push(n);
});
_1.forEach(_12,_7);
}
}
return _8;
};
return this._normalizeTags(_7(_5));
}
return _5;
},_normalizeTags:function(_13){
var w=this.editor.window;
var _14=_1.withGlobal(w,function(){
return _1.query("em,s,strong",_13);
});
if(_14&&_14.length){
_1.forEach(_14,function(n){
if(n){
var tag=n.tagName?n.tagName.toLowerCase():"";
var _15;
switch(tag){
case "s":
_15="strike";
break;
case "em":
_15="i";
break;
case "strong":
_15="b";
break;
}
if(_15){
var _16=_1.withGlobal(w,"create",_1,[_15,null,n,"before"]);
while(n.firstChild){
_16.appendChild(n.firstChild);
}
n.parentNode.removeChild(n);
}
}
});
}
return _13;
},_convertToCss:function(_17){
if(_17){
var w=this.editor.window;
var _18=function(_19){
if(_19.nodeType==1){
if(_19.id!=="dijitEditorBody"){
var tag=_19.tagName?_19.tagName.toLowerCase():"";
if(tag){
var _1a;
switch(tag){
case "b":
case "strong":
_1a=_1.withGlobal(w,"create",_1,["span",{style:{"fontWeight":"bold"}}]);
break;
case "i":
case "em":
_1a=_1.withGlobal(w,"create",_1,["span",{style:{"fontStyle":"italic"}}]);
break;
case "u":
_1a=_1.withGlobal(w,"create",_1,["span",{style:{"textDecoration":"underline"}}]);
break;
case "strike":
case "s":
_1a=_1.withGlobal(w,"create",_1,["span",{style:{"textDecoration":"line-through"}}]);
break;
case "font":
var _1b={};
if(_1.attr(_19,"color")){
_1b.color=_1.attr(_19,"color");
}
if(_1.attr(_19,"face")){
_1b.fontFace=_1.attr(_19,"face");
}
if(_19.style&&_19.style.backgroundColor){
_1b.backgroundColor=_19.style.backgroundColor;
}
if(_19.style&&_19.style.color){
_1b.color=_19.style.color;
}
var _1c={1:"xx-small",2:"x-small",3:"small",4:"medium",5:"large",6:"x-large",7:"xx-large"};
if(_1.attr(_19,"size")){
_1b.fontSize=_1c[_1.attr(_19,"size")];
}
_1a=_1.withGlobal(w,"create",_1,["span",{style:_1b}]);
break;
}
if(_1a){
while(_19.firstChild){
_1a.appendChild(_19.firstChild);
}
_1.place(_1a,_19,"before");
_19.parentNode.removeChild(_19);
_19=_1a;
}
}
}
if(_19.childNodes){
var _1d=[];
_1.forEach(_19.childNodes,function(n){
_1d.push(n);
});
_1.forEach(_1d,_18);
}
}
return _19;
};
_17=_18(_17);
if(this.condenseSpans){
this._condenseSpans(_17);
}
}
return _17;
},_condenseSpans:function(_1e){
var _1f=function(_20){
var _21=function(_22){
var m;
if(_22){
m={};
var _23=_22.toLowerCase().split(";");
_1.forEach(_23,function(s){
if(s){
var ss=s.split(":");
var key=ss[0]?_1.trim(ss[0]):"";
var val=ss[1]?_1.trim(ss[1]):"";
if(key&&val){
var i;
var _24="";
for(i=0;i<key.length;i++){
var ch=key.charAt(i);
if(ch=="-"){
i++;
ch=key.charAt(i);
_24+=ch.toUpperCase();
}else{
_24+=ch;
}
}
m[_24]=val;
}
}
});
}
return m;
};
if(_20&&_20.nodeType==1){
var tag=_20.tagName?_20.tagName.toLowerCase():"";
if(tag==="span"&&_20.childNodes&&_20.childNodes.length===1){
var c=_20.firstChild;
while(c&&c.nodeType==1&&c.tagName&&c.tagName.toLowerCase()=="span"){
if(!_1.attr(c,"class")&&!_1.attr(c,"id")&&c.style){
var s1=_21(_20.style.cssText);
var s2=_21(c.style.cssText);
if(s1&&s2){
var _25={};
var i;
for(i in s1){
if(!s1[i]||!s2[i]||s1[i]==s2[i]){
_25[i]=s1[i];
delete s2[i];
}else{
if(s1[i]!=s2[i]){
if(i=="textDecoration"){
_25[i]=s1[i]+" "+s2[i];
delete s2[i];
}else{
_25=null;
}
break;
}else{
_25=null;
break;
}
}
}
if(_25){
for(i in s2){
_25[i]=s2[i];
}
_1.style(_20,_25);
while(c.firstChild){
_20.appendChild(c.firstChild);
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
if(_20.childNodes&&_20.childNodes.length){
_1.forEach(_20.childNodes,_1f);
}
};
_1f(_1e);
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
},_inserthtmlImpl:function(_26){
if(_26){
var div=this.editor.document.createElement("div");
div.innerHTML=_26;
div=this._browserFilter(div);
_26=_2._editor.getChildrenHtml(div);
div.innerHTML="";
if(this.editor._oldInsertHtmlImpl){
return this.editor._oldInsertHtmlImpl(_26);
}else{
return this.editor.execCommand("inserthtml",_26);
}
}
return false;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _27=o.args.name.toLowerCase();
if(_27==="normalizestyle"){
o.plugin=new _3.editor.plugins.NormalizeStyle({mode:("mode" in o.args)?o.args.mode:"semantic",condenseSpans:("condenseSpans" in o.args)?o.args.condenseSpans:true});
}
});
return _3.editor.plugins.NormalizeStyle;
});
