//>>built
define("dojox/editor/plugins/NormalizeIndentOutdent",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/_editor/range","dijit/_editor/selection","dojo/_base/connect","dojo/_base/declare"],function(_1,_2,_3,_4){
_1.declare("dojox.editor.plugins.NormalizeIndentOutdent",_4,{indentBy:40,indentUnits:"px",setEditor:function(_5){
this.editor=_5;
_5._indentImpl=_1.hitch(this,this._indentImpl);
_5._outdentImpl=_1.hitch(this,this._outdentImpl);
if(!_5._indentoutdent_queryCommandEnabled){
_5._indentoutdent_queryCommandEnabled=_5.queryCommandEnabled;
}
_5.queryCommandEnabled=_1.hitch(this,this._queryCommandEnabled);
_5.customUndo=true;
},_queryCommandEnabled:function(_6){
var c=_6.toLowerCase();
var ed,_7,_8,_9,_a,_b;
var _c="marginLeft";
if(!this._isLtr()){
_c="marginRight";
}
if(c==="indent"){
ed=this.editor;
_7=_2.range.getSelection(ed.window);
if(_7&&_7.rangeCount>0){
_8=_7.getRangeAt(0);
_9=_8.startContainer;
while(_9&&_9!==ed.document&&_9!==ed.editNode){
_a=this._getTagName(_9);
if(_a==="li"){
_b=_9.previousSibling;
while(_b&&_b.nodeType!==1){
_b=_b.previousSibling;
}
if(_b&&this._getTagName(_b)==="li"){
return true;
}else{
return false;
}
}else{
if(this._isIndentableElement(_a)){
return true;
}
}
_9=_9.parentNode;
}
if(this._isRootInline(_8.startContainer)){
return true;
}
}
}else{
if(c==="outdent"){
ed=this.editor;
_7=_2.range.getSelection(ed.window);
if(_7&&_7.rangeCount>0){
_8=_7.getRangeAt(0);
_9=_8.startContainer;
while(_9&&_9!==ed.document&&_9!==ed.editNode){
_a=this._getTagName(_9);
if(_a==="li"){
return this.editor._indentoutdent_queryCommandEnabled(_6);
}else{
if(this._isIndentableElement(_a)){
var _d=_9.style?_9.style[_c]:"";
if(_d){
_d=this._convertIndent(_d);
if(_d/this.indentBy>=1){
return true;
}
}
return false;
}
}
_9=_9.parentNode;
}
if(this._isRootInline(_8.startContainer)){
return false;
}
}
}else{
return this.editor._indentoutdent_queryCommandEnabled(_6);
}
}
return false;
},_indentImpl:function(_e){
var ed=this.editor;
var _f=_2.range.getSelection(ed.window);
if(_f&&_f.rangeCount>0){
var _10=_f.getRangeAt(0);
var _11=_10.startContainer;
var tag,_12,end,div;
if(_10.startContainer===_10.endContainer){
if(this._isRootInline(_10.startContainer)){
_12=_10.startContainer;
while(_12&&_12.parentNode!==ed.editNode){
_12=_12.parentNode;
}
while(_12&&_12.previousSibling&&(this._isTextElement(_12)||(_12.nodeType===1&&this._isInlineFormat(this._getTagName(_12))))){
_12=_12.previousSibling;
}
if(_12&&_12.nodeType===1&&!this._isInlineFormat(this._getTagName(_12))){
_12=_12.nextSibling;
}
if(_12){
div=ed.document.createElement("div");
_1.place(div,_12,"after");
div.appendChild(_12);
end=div.nextSibling;
while(end&&(this._isTextElement(end)||(end.nodeType===1&&this._isInlineFormat(this._getTagName(end))))){
div.appendChild(end);
end=div.nextSibling;
}
this._indentElement(div);
ed._sCall("selectElementChildren",[div]);
ed._sCall("collapse",[true]);
}
}else{
while(_11&&_11!==ed.document&&_11!==ed.editNode){
tag=this._getTagName(_11);
if(tag==="li"){
this._indentList(_11);
return;
}else{
if(this._isIndentableElement(tag)){
this._indentElement(_11);
return;
}
}
_11=_11.parentNode;
}
}
}else{
var _13;
_12=_10.startContainer;
end=_10.endContainer;
while(_12&&this._isTextElement(_12)&&_12.parentNode!==ed.editNode){
_12=_12.parentNode;
}
while(end&&this._isTextElement(end)&&end.parentNode!==ed.editNode){
end=end.parentNode;
}
if(end===ed.editNode||end===ed.document.body){
_13=_12;
while(_13.nextSibling&&ed._sCall("inSelection",[_13])){
_13=_13.nextSibling;
}
end=_13;
if(end===ed.editNode||end===ed.document.body){
tag=this._getTagName(_12);
if(tag==="li"){
this._indentList(_12);
}else{
if(this._isIndentableElement(tag)){
this._indentElement(_12);
}else{
if(this._isTextElement(_12)||this._isInlineFormat(tag)){
div=ed.document.createElement("div");
_1.place(div,_12,"after");
var _14=_12;
while(_14&&(this._isTextElement(_14)||(_14.nodeType===1&&this._isInlineFormat(this._getTagName(_14))))){
div.appendChild(_14);
_14=div.nextSibling;
}
this._indentElement(div);
}
}
}
return;
}
}
end=end.nextSibling;
_13=_12;
while(_13&&_13!==end){
if(_13.nodeType===1){
tag=this._getTagName(_13);
if(_1.isIE){
if(tag==="p"&&this._isEmpty(_13)){
_13=_13.nextSibling;
continue;
}
}
if(tag==="li"){
if(div){
if(this._isEmpty(div)){
div.parentNode.removeChild(div);
}else{
this._indentElement(div);
}
div=null;
}
this._indentList(_13);
}else{
if(!this._isInlineFormat(tag)&&this._isIndentableElement(tag)){
if(div){
if(this._isEmpty(div)){
div.parentNode.removeChild(div);
}else{
this._indentElement(div);
}
div=null;
}
_13=this._indentElement(_13);
}else{
if(this._isInlineFormat(tag)){
if(!div){
div=ed.document.createElement("div");
_1.place(div,_13,"after");
div.appendChild(_13);
_13=div;
}else{
div.appendChild(_13);
_13=div;
}
}
}
}
}else{
if(this._isTextElement(_13)){
if(!div){
div=ed.document.createElement("div");
_1.place(div,_13,"after");
div.appendChild(_13);
_13=div;
}else{
div.appendChild(_13);
_13=div;
}
}
}
_13=_13.nextSibling;
}
if(div){
if(this._isEmpty(div)){
div.parentNode.removeChild(div);
}else{
this._indentElement(div);
}
div=null;
}
}
}
},_indentElement:function(_15){
var _16="marginLeft";
if(!this._isLtr()){
_16="marginRight";
}
var tag=this._getTagName(_15);
if(tag==="ul"||tag==="ol"){
var div=this.editor.document.createElement("div");
_1.place(div,_15,"after");
div.appendChild(_15);
_15=div;
}
var _17=_15.style?_15.style[_16]:"";
if(_17){
_17=this._convertIndent(_17);
_17=(parseInt(_17,10)+this.indentBy)+this.indentUnits;
}else{
_17=this.indentBy+this.indentUnits;
}
_1.style(_15,_16,_17);
return _15;
},_outdentElement:function(_18){
var _19="marginLeft";
if(!this._isLtr()){
_19="marginRight";
}
var _1a=_18.style?_18.style[_19]:"";
if(_1a){
_1a=this._convertIndent(_1a);
if(_1a-this.indentBy>0){
_1a=(parseInt(_1a,10)-this.indentBy)+this.indentUnits;
}else{
_1a="";
}
_1.style(_18,_19,_1a);
}
},_outdentImpl:function(_1b){
var ed=this.editor;
var sel=_2.range.getSelection(ed.window);
if(sel&&sel.rangeCount>0){
var _1c=sel.getRangeAt(0);
var _1d=_1c.startContainer;
var tag;
if(_1c.startContainer===_1c.endContainer){
while(_1d&&_1d!==ed.document&&_1d!==ed.editNode){
tag=this._getTagName(_1d);
if(tag==="li"){
return this._outdentList(_1d);
}else{
if(this._isIndentableElement(tag)){
return this._outdentElement(_1d);
}
}
_1d=_1d.parentNode;
}
ed.document.execCommand("outdent",false,_1b);
}else{
var _1e=_1c.startContainer;
var end=_1c.endContainer;
while(_1e&&_1e.nodeType===3){
_1e=_1e.parentNode;
}
while(end&&end.nodeType===3){
end=end.parentNode;
}
end=end.nextSibling;
var _1f=_1e;
while(_1f&&_1f!==end){
if(_1f.nodeType===1){
tag=this._getTagName(_1f);
if(tag==="li"){
this._outdentList(_1f);
}else{
if(this._isIndentableElement(tag)){
this._outdentElement(_1f);
}
}
}
_1f=_1f.nextSibling;
}
}
}
return null;
},_indentList:function(_20){
var ed=this.editor;
var _21,li;
var _22=_20.parentNode;
var _23=_20.previousSibling;
while(_23&&_23.nodeType!==1){
_23=_23.previousSibling;
}
var _24=null;
var tg=this._getTagName(_22);
if(tg==="ol"){
_24="ol";
}else{
if(tg==="ul"){
_24="ul";
}
}
if(_24){
if(_23&&_23.tagName.toLowerCase()=="li"){
var _25;
if(_23.childNodes){
var i;
for(i=0;i<_23.childNodes.length;i++){
var n=_23.childNodes[i];
if(n.nodeType===3){
if(_1.trim(n.nodeValue)){
if(_25){
break;
}
}
}else{
if(n.nodeType===1&&!_25){
if(_24===n.tagName.toLowerCase()){
_25=n;
}
}else{
break;
}
}
}
}
if(_25){
_25.appendChild(_20);
}else{
_21=ed.document.createElement(_24);
_1.style(_21,{paddingTop:"0px",paddingBottom:"0px"});
li=ed.document.createElement("li");
_1.style(li,{listStyleImage:"none",listStyleType:"none"});
_23.appendChild(_21);
_21.appendChild(_20);
}
ed._sCall("selectElementChildren",[_20]);
ed._sCall("collapse",[true]);
}
}
},_outdentList:function(_26){
var ed=this.editor;
var _27=_26.parentNode;
var _28=null;
var tg=_27.tagName?_27.tagName.toLowerCase():"";
var li;
if(tg==="ol"){
_28="ol";
}else{
if(tg==="ul"){
_28="ul";
}
}
var _29=_27.parentNode;
var _2a=this._getTagName(_29);
if(_2a==="li"||_2a==="ol"||_2a==="ul"){
if(_2a==="ol"||_2a==="ul"){
var _2b=_27.previousSibling;
while(_2b&&(_2b.nodeType!==1||(_2b.nodeType===1&&this._getTagName(_2b)!=="li"))){
_2b=_2b.previousSibling;
}
if(_2b){
_2b.appendChild(_27);
_29=_2b;
}else{
li=_26;
var _2c=_26;
while(li.previousSibling){
li=li.previousSibling;
if(li.nodeType===1&&this._getTagName(li)==="li"){
_2c=li;
}
}
if(_2c!==_26){
_1.place(_2c,_27,"before");
_2c.appendChild(_27);
_29=_2c;
}else{
li=ed.document.createElement("li");
_1.place(li,_27,"before");
li.appendChild(_27);
_29=li;
}
_1.style(_27,{paddingTop:"0px",paddingBottom:"0px"});
}
}
var _2d=_26.previousSibling;
while(_2d&&_2d.nodeType!==1){
_2d=_2d.previousSibling;
}
var _2e=_26.nextSibling;
while(_2e&&_2e.nodeType!==1){
_2e=_2e.nextSibling;
}
if(!_2d){
_1.place(_26,_29,"after");
_26.appendChild(_27);
}else{
if(!_2e){
_1.place(_26,_29,"after");
}else{
var _2f=ed.document.createElement(_28);
_1.style(_2f,{paddingTop:"0px",paddingBottom:"0px"});
_26.appendChild(_2f);
while(_26.nextSibling){
_2f.appendChild(_26.nextSibling);
}
_1.place(_26,_29,"after");
}
}
if(_27&&this._isEmpty(_27)){
_27.parentNode.removeChild(_27);
}
if(_29&&this._isEmpty(_29)){
_29.parentNode.removeChild(_29);
}
ed._sCall("selectElementChildren",[_26]);
ed._sCall("collapse",[true]);
}else{
ed.document.execCommand("outdent",false,null);
}
},_isEmpty:function(_30){
if(_30.childNodes){
var _31=true;
var i;
for(i=0;i<_30.childNodes.length;i++){
var n=_30.childNodes[i];
if(n.nodeType===1){
if(this._getTagName(n)==="p"){
if(!_1.trim(n.innerHTML)){
continue;
}
}
_31=false;
break;
}else{
if(this._isTextElement(n)){
var nv=_1.trim(n.nodeValue);
if(nv&&nv!=="&nbsp;"&&nv!==" "){
_31=false;
break;
}
}else{
_31=false;
break;
}
}
}
return _31;
}else{
return true;
}
},_isIndentableElement:function(tag){
switch(tag){
case "p":
case "div":
case "h1":
case "h2":
case "h3":
case "center":
case "table":
case "ul":
case "ol":
return true;
default:
return false;
}
},_convertIndent:function(_32){
var _33=12;
_32=_32+"";
_32=_32.toLowerCase();
var _34=(_32.indexOf("px")>0)?"px":(_32.indexOf("em")>0)?"em":"px";
_32=_32.replace(/(px;?|em;?)/gi,"");
if(_34==="px"){
if(this.indentUnits==="em"){
_32=Math.ceil(_32/_33);
}
}else{
if(this.indentUnits==="px"){
_32=_32*_33;
}
}
return _32;
},_isLtr:function(){
var _35=this.editor.document.body;
var cs=_1.getComputedStyle(_35);
return cs?cs.direction=="ltr":true;
},_isInlineFormat:function(tag){
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
},_getTagName:function(_36){
var tag="";
if(_36&&_36.nodeType===1){
tag=_36.tagName?_36.tagName.toLowerCase():"";
}
return tag;
},_isRootInline:function(_37){
var ed=this.editor;
if(this._isTextElement(_37)&&_37.parentNode===ed.editNode){
return true;
}else{
if(_37.nodeType===1&&this._isInlineFormat(_37)&&_37.parentNode===ed.editNode){
return true;
}else{
if(this._isTextElement(_37)&&this._isInlineFormat(this._getTagName(_37.parentNode))){
_37=_37.parentNode;
while(_37&&_37!==ed.editNode&&this._isInlineFormat(this._getTagName(_37))){
_37=_37.parentNode;
}
if(_37===ed.editNode){
return true;
}
}
}
}
return false;
},_isTextElement:function(_38){
if(_38&&_38.nodeType===3||_38.nodeType===4){
return true;
}
return false;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _39=o.args.name.toLowerCase();
if(_39==="normalizeindentoutdent"){
o.plugin=new _3.editor.plugins.NormalizeIndentOutdent({indentBy:("indentBy" in o.args)?(o.args.indentBy>0?o.args.indentBy:40):40,indentUnits:("indentUnits" in o.args)?(o.args.indentUnits.toLowerCase()=="em"?"em":"px"):"px"});
}
});
return _3.editor.plugins.NormalizeIndentOutdent;
});
