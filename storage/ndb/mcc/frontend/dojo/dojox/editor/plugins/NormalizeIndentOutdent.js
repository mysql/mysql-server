//>>built
define("dojox/editor/plugins/NormalizeIndentOutdent",["dojo","dijit","dojox","dijit/_editor/range","dijit/_editor/selection","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.NormalizeIndentOutdent",_2._editor._Plugin,{indentBy:40,indentUnits:"px",setEditor:function(_4){
this.editor=_4;
_4._indentImpl=_1.hitch(this,this._indentImpl);
_4._outdentImpl=_1.hitch(this,this._outdentImpl);
if(!_4._indentoutdent_queryCommandEnabled){
_4._indentoutdent_queryCommandEnabled=_4.queryCommandEnabled;
}
_4.queryCommandEnabled=_1.hitch(this,this._queryCommandEnabled);
_4.customUndo=true;
},_queryCommandEnabled:function(_5){
var c=_5.toLowerCase();
var ed,_6,_7,_8,_9,_a;
var _b="marginLeft";
if(!this._isLtr()){
_b="marginRight";
}
if(c==="indent"){
ed=this.editor;
_6=_2.range.getSelection(ed.window);
if(_6&&_6.rangeCount>0){
_7=_6.getRangeAt(0);
_8=_7.startContainer;
while(_8&&_8!==ed.document&&_8!==ed.editNode){
_9=this._getTagName(_8);
if(_9==="li"){
_a=_8.previousSibling;
while(_a&&_a.nodeType!==1){
_a=_a.previousSibling;
}
if(_a&&this._getTagName(_a)==="li"){
return true;
}else{
return false;
}
}else{
if(this._isIndentableElement(_9)){
return true;
}
}
_8=_8.parentNode;
}
if(this._isRootInline(_7.startContainer)){
return true;
}
}
}else{
if(c==="outdent"){
ed=this.editor;
_6=_2.range.getSelection(ed.window);
if(_6&&_6.rangeCount>0){
_7=_6.getRangeAt(0);
_8=_7.startContainer;
while(_8&&_8!==ed.document&&_8!==ed.editNode){
_9=this._getTagName(_8);
if(_9==="li"){
return this.editor._indentoutdent_queryCommandEnabled(_5);
}else{
if(this._isIndentableElement(_9)){
var _c=_8.style?_8.style[_b]:"";
if(_c){
_c=this._convertIndent(_c);
if(_c/this.indentBy>=1){
return true;
}
}
return false;
}
}
_8=_8.parentNode;
}
if(this._isRootInline(_7.startContainer)){
return false;
}
}
}else{
return this.editor._indentoutdent_queryCommandEnabled(_5);
}
}
return false;
},_indentImpl:function(_d){
var ed=this.editor;
var _e=_2.range.getSelection(ed.window);
if(_e&&_e.rangeCount>0){
var _f=_e.getRangeAt(0);
var _10=_f.startContainer;
var tag,_11,end,div;
if(_f.startContainer===_f.endContainer){
if(this._isRootInline(_f.startContainer)){
_11=_f.startContainer;
while(_11&&_11.parentNode!==ed.editNode){
_11=_11.parentNode;
}
while(_11&&_11.previousSibling&&(this._isTextElement(_11)||(_11.nodeType===1&&this._isInlineFormat(this._getTagName(_11))))){
_11=_11.previousSibling;
}
if(_11&&_11.nodeType===1&&!this._isInlineFormat(this._getTagName(_11))){
_11=_11.nextSibling;
}
if(_11){
div=ed.document.createElement("div");
_1.place(div,_11,"after");
div.appendChild(_11);
end=div.nextSibling;
while(end&&(this._isTextElement(end)||(end.nodeType===1&&this._isInlineFormat(this._getTagName(end))))){
div.appendChild(end);
end=div.nextSibling;
}
this._indentElement(div);
_1.withGlobal(ed.window,"selectElementChildren",_2._editor.selection,[div]);
_1.withGlobal(ed.window,"collapse",_2._editor.selection,[true]);
}
}else{
while(_10&&_10!==ed.document&&_10!==ed.editNode){
tag=this._getTagName(_10);
if(tag==="li"){
this._indentList(_10);
return;
}else{
if(this._isIndentableElement(tag)){
this._indentElement(_10);
return;
}
}
_10=_10.parentNode;
}
}
}else{
var _12;
_11=_f.startContainer;
end=_f.endContainer;
while(_11&&this._isTextElement(_11)&&_11.parentNode!==ed.editNode){
_11=_11.parentNode;
}
while(end&&this._isTextElement(end)&&end.parentNode!==ed.editNode){
end=end.parentNode;
}
if(end===ed.editNode||end===ed.document.body){
_12=_11;
while(_12.nextSibling&&_1.withGlobal(ed.window,"inSelection",_2._editor.selection,[_12])){
_12=_12.nextSibling;
}
end=_12;
if(end===ed.editNode||end===ed.document.body){
tag=this._getTagName(_11);
if(tag==="li"){
this._indentList(_11);
}else{
if(this._isIndentableElement(tag)){
this._indentElement(_11);
}else{
if(this._isTextElement(_11)||this._isInlineFormat(tag)){
div=ed.document.createElement("div");
_1.place(div,_11,"after");
var _13=_11;
while(_13&&(this._isTextElement(_13)||(_13.nodeType===1&&this._isInlineFormat(this._getTagName(_13))))){
div.appendChild(_13);
_13=div.nextSibling;
}
this._indentElement(div);
}
}
}
return;
}
}
end=end.nextSibling;
_12=_11;
while(_12&&_12!==end){
if(_12.nodeType===1){
tag=this._getTagName(_12);
if(_1.isIE){
if(tag==="p"&&this._isEmpty(_12)){
_12=_12.nextSibling;
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
this._indentList(_12);
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
_12=this._indentElement(_12);
}else{
if(this._isInlineFormat(tag)){
if(!div){
div=ed.document.createElement("div");
_1.place(div,_12,"after");
div.appendChild(_12);
_12=div;
}else{
div.appendChild(_12);
_12=div;
}
}
}
}
}else{
if(this._isTextElement(_12)){
if(!div){
div=ed.document.createElement("div");
_1.place(div,_12,"after");
div.appendChild(_12);
_12=div;
}else{
div.appendChild(_12);
_12=div;
}
}
}
_12=_12.nextSibling;
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
},_indentElement:function(_14){
var _15="marginLeft";
if(!this._isLtr()){
_15="marginRight";
}
var tag=this._getTagName(_14);
if(tag==="ul"||tag==="ol"){
var div=this.editor.document.createElement("div");
_1.place(div,_14,"after");
div.appendChild(_14);
_14=div;
}
var _16=_14.style?_14.style[_15]:"";
if(_16){
_16=this._convertIndent(_16);
_16=(parseInt(_16,10)+this.indentBy)+this.indentUnits;
}else{
_16=this.indentBy+this.indentUnits;
}
_1.style(_14,_15,_16);
return _14;
},_outdentElement:function(_17){
var _18="marginLeft";
if(!this._isLtr()){
_18="marginRight";
}
var _19=_17.style?_17.style[_18]:"";
if(_19){
_19=this._convertIndent(_19);
if(_19-this.indentBy>0){
_19=(parseInt(_19,10)-this.indentBy)+this.indentUnits;
}else{
_19="";
}
_1.style(_17,_18,_19);
}
},_outdentImpl:function(_1a){
var ed=this.editor;
var sel=_2.range.getSelection(ed.window);
if(sel&&sel.rangeCount>0){
var _1b=sel.getRangeAt(0);
var _1c=_1b.startContainer;
var tag;
if(_1b.startContainer===_1b.endContainer){
while(_1c&&_1c!==ed.document&&_1c!==ed.editNode){
tag=this._getTagName(_1c);
if(tag==="li"){
return this._outdentList(_1c);
}else{
if(this._isIndentableElement(tag)){
return this._outdentElement(_1c);
}
}
_1c=_1c.parentNode;
}
ed.document.execCommand("outdent",false,_1a);
}else{
var _1d=_1b.startContainer;
var end=_1b.endContainer;
while(_1d&&_1d.nodeType===3){
_1d=_1d.parentNode;
}
while(end&&end.nodeType===3){
end=end.parentNode;
}
end=end.nextSibling;
var _1e=_1d;
while(_1e&&_1e!==end){
if(_1e.nodeType===1){
tag=this._getTagName(_1e);
if(tag==="li"){
this._outdentList(_1e);
}else{
if(this._isIndentableElement(tag)){
this._outdentElement(_1e);
}
}
}
_1e=_1e.nextSibling;
}
}
}
return null;
},_indentList:function(_1f){
var ed=this.editor;
var _20,li;
var _21=_1f.parentNode;
var _22=_1f.previousSibling;
while(_22&&_22.nodeType!==1){
_22=_22.previousSibling;
}
var _23=null;
var tg=this._getTagName(_21);
if(tg==="ol"){
_23="ol";
}else{
if(tg==="ul"){
_23="ul";
}
}
if(_23){
if(_22&&_22.tagName.toLowerCase()=="li"){
var _24;
if(_22.childNodes){
var i;
for(i=0;i<_22.childNodes.length;i++){
var n=_22.childNodes[i];
if(n.nodeType===3){
if(_1.trim(n.nodeValue)){
if(_24){
break;
}
}
}else{
if(n.nodeType===1&&!_24){
if(_23===n.tagName.toLowerCase()){
_24=n;
}
}else{
break;
}
}
}
}
if(_24){
_24.appendChild(_1f);
}else{
_20=ed.document.createElement(_23);
_1.style(_20,{paddingTop:"0px",paddingBottom:"0px"});
li=ed.document.createElement("li");
_1.style(li,{listStyleImage:"none",listStyleType:"none"});
_22.appendChild(_20);
_20.appendChild(_1f);
}
_1.withGlobal(ed.window,"selectElementChildren",_2._editor.selection,[_1f]);
_1.withGlobal(ed.window,"collapse",_2._editor.selection,[true]);
}
}
},_outdentList:function(_25){
var ed=this.editor;
var _26=_25.parentNode;
var _27=null;
var tg=_26.tagName?_26.tagName.toLowerCase():"";
var li;
if(tg==="ol"){
_27="ol";
}else{
if(tg==="ul"){
_27="ul";
}
}
var _28=_26.parentNode;
var _29=this._getTagName(_28);
if(_29==="li"||_29==="ol"||_29==="ul"){
if(_29==="ol"||_29==="ul"){
var _2a=_26.previousSibling;
while(_2a&&(_2a.nodeType!==1||(_2a.nodeType===1&&this._getTagName(_2a)!=="li"))){
_2a=_2a.previousSibling;
}
if(_2a){
_2a.appendChild(_26);
_28=_2a;
}else{
li=_25;
var _2b=_25;
while(li.previousSibling){
li=li.previousSibling;
if(li.nodeType===1&&this._getTagName(li)==="li"){
_2b=li;
}
}
if(_2b!==_25){
_1.place(_2b,_26,"before");
_2b.appendChild(_26);
_28=_2b;
}else{
li=ed.document.createElement("li");
_1.place(li,_26,"before");
li.appendChild(_26);
_28=li;
}
_1.style(_26,{paddingTop:"0px",paddingBottom:"0px"});
}
}
var _2c=_25.previousSibling;
while(_2c&&_2c.nodeType!==1){
_2c=_2c.previousSibling;
}
var _2d=_25.nextSibling;
while(_2d&&_2d.nodeType!==1){
_2d=_2d.nextSibling;
}
if(!_2c){
_1.place(_25,_28,"after");
_25.appendChild(_26);
}else{
if(!_2d){
_1.place(_25,_28,"after");
}else{
var _2e=ed.document.createElement(_27);
_1.style(_2e,{paddingTop:"0px",paddingBottom:"0px"});
_25.appendChild(_2e);
while(_25.nextSibling){
_2e.appendChild(_25.nextSibling);
}
_1.place(_25,_28,"after");
}
}
if(_26&&this._isEmpty(_26)){
_26.parentNode.removeChild(_26);
}
if(_28&&this._isEmpty(_28)){
_28.parentNode.removeChild(_28);
}
_1.withGlobal(ed.window,"selectElementChildren",_2._editor.selection,[_25]);
_1.withGlobal(ed.window,"collapse",_2._editor.selection,[true]);
}else{
ed.document.execCommand("outdent",false,null);
}
},_isEmpty:function(_2f){
if(_2f.childNodes){
var _30=true;
var i;
for(i=0;i<_2f.childNodes.length;i++){
var n=_2f.childNodes[i];
if(n.nodeType===1){
if(this._getTagName(n)==="p"){
if(!_1.trim(n.innerHTML)){
continue;
}
}
_30=false;
break;
}else{
if(this._isTextElement(n)){
var nv=_1.trim(n.nodeValue);
if(nv&&nv!=="&nbsp;"&&nv!==" "){
_30=false;
break;
}
}else{
_30=false;
break;
}
}
}
return _30;
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
},_convertIndent:function(_31){
var _32=12;
_31=_31+"";
_31=_31.toLowerCase();
var _33=(_31.indexOf("px")>0)?"px":(_31.indexOf("em")>0)?"em":"px";
_31=_31.replace(/(px;?|em;?)/gi,"");
if(_33==="px"){
if(this.indentUnits==="em"){
_31=Math.ceil(_31/_32);
}
}else{
if(this.indentUnits==="px"){
_31=_31*_32;
}
}
return _31;
},_isLtr:function(){
var _34=this.editor.document.body;
return _1.withGlobal(this.editor.window,function(){
var cs=_1.getComputedStyle(_34);
return cs?cs.direction=="ltr":true;
});
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
},_getTagName:function(_35){
var tag="";
if(_35&&_35.nodeType===1){
tag=_35.tagName?_35.tagName.toLowerCase():"";
}
return tag;
},_isRootInline:function(_36){
var ed=this.editor;
if(this._isTextElement(_36)&&_36.parentNode===ed.editNode){
return true;
}else{
if(_36.nodeType===1&&this._isInlineFormat(_36)&&_36.parentNode===ed.editNode){
return true;
}else{
if(this._isTextElement(_36)&&this._isInlineFormat(this._getTagName(_36.parentNode))){
_36=_36.parentNode;
while(_36&&_36!==ed.editNode&&this._isInlineFormat(this._getTagName(_36))){
_36=_36.parentNode;
}
if(_36===ed.editNode){
return true;
}
}
}
}
return false;
},_isTextElement:function(_37){
if(_37&&_37.nodeType===3||_37.nodeType===4){
return true;
}
return false;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _38=o.args.name.toLowerCase();
if(_38==="normalizeindentoutdent"){
o.plugin=new _3.editor.plugins.NormalizeIndentOutdent({indentBy:("indentBy" in o.args)?(o.args.indentBy>0?o.args.indentBy:40):40,indentUnits:("indentUnits" in o.args)?(o.args.indentUnits.toLowerCase()=="em"?"em":"px"):"px"});
}
});
return _3.editor.plugins.NormalizeIndentOutdent;
});
