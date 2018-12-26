//>>built
define("dojox/dtl/dom",["dojo/_base/lang","./_base","dojox/string/tokenize","./Context","dojo/dom","dojo/dom-construct","dojo/_base/html","dojo/_base/array","dojo/_base/connect","dojo/_base/sniff"],function(_1,dd,_2,_3,_4,_5,_6,_7,_8,_9){
dd.BOOLS={checked:1,disabled:1,readonly:1};
dd.TOKEN_CHANGE=-11;
dd.TOKEN_ATTR=-12;
dd.TOKEN_CUSTOM=-13;
dd.TOKEN_NODE=1;
var _a=dd.text;
var _b=dd.dom={_attributes:{},_uppers:{},_re4:/^function anonymous\(\)\s*{\s*(.*)\s*}$/,_reTrim:/(?:^[\n\s]*(\{%)?\s*|\s*(%\})?[\n\s]*$)/g,_reSplit:/\s*%\}[\n\s]*\{%\s*/g,getTemplate:function(_c){
if(typeof this._commentable=="undefined"){
this._commentable=false;
var _d=document.createElement("div"),_e="Test comment handling, and long comments, using comments whenever possible.";
_d.innerHTML="<!--"+_e+"-->";
if(_d.childNodes.length&&_d.firstChild.nodeType==8&&_d.firstChild.data==_e){
this._commentable=true;
}
}
if(!this._commentable){
_c=_c.replace(/<!--({({|%).*?(%|})})-->/g,"$1");
}
if(_9("ie")){
_c=_c.replace(/\b(checked|disabled|readonly|style)="/g,"t$1=\"");
}
_c=_c.replace(/\bstyle="/g,"tstyle=\"");
var _f;
var _10=_9("webkit");
var _11=[[true,"select","option"],[_10,"tr","td|th"],[_10,"thead","tr","th"],[_10,"tbody","tr","td"],[_10,"table","tbody|thead|tr","tr","td"]];
var _12=[];
for(var i=0,_13;_13=_11[i];i++){
if(!_13[0]){
continue;
}
if(_c.indexOf("<"+_13[1])!=-1){
var _14=new RegExp("<"+_13[1]+"(?:.|\n)*?>((?:.|\n)+?)</"+_13[1]+">","ig");
tagLoop:
while(_f=_14.exec(_c)){
var _15=_13[2].split("|");
var _16=[];
for(var j=0,_17;_17=_15[j];j++){
_16.push("<"+_17+"(?:.|\n)*?>(?:.|\n)*?</"+_17+">");
}
var _18=[];
var _19=_2(_f[1],new RegExp("("+_16.join("|")+")","ig"),function(_1a){
var tag=/<(\w+)/.exec(_1a)[1];
if(!_18[tag]){
_18[tag]=true;
_18.push(tag);
}
return {data:_1a};
});
if(_18.length){
var tag=(_18.length==1)?_18[0]:_13[2].split("|")[0];
var _1b=[];
for(var j=0,jl=_19.length;j<jl;j++){
var _1c=_19[j];
if(_1.isObject(_1c)){
_1b.push(_1c.data);
}else{
var _1d=_1c.replace(this._reTrim,"");
if(!_1d){
continue;
}
_1c=_1d.split(this._reSplit);
for(var k=0,kl=_1c.length;k<kl;k++){
var _1e="";
for(var p=2,pl=_13.length;p<pl;p++){
if(p==2){
_1e+="<"+tag+" dtlinstruction=\"{% "+_1c[k].replace("\"","\\\"")+" %}\">";
}else{
if(tag==_13[p]){
continue;
}else{
_1e+="<"+_13[p]+">";
}
}
}
_1e+="DTL";
for(var p=_13.length-1;p>1;p--){
if(p==2){
_1e+="</"+tag+">";
}else{
if(tag==_13[p]){
continue;
}else{
_1e+="</"+_13[p]+">";
}
}
}
_1b.push("ÿ"+_12.length);
_12.push(_1e);
}
}
}
_c=_c.replace(_f[1],_1b.join(""));
}
}
}
}
for(var i=_12.length;i--;){
_c=_c.replace("ÿ"+i,_12[i]);
}
var re=/\b([a-zA-Z_:][a-zA-Z0-9_\-\.:]*)=['"]/g;
while(_f=re.exec(_c)){
var _1f=_f[1].toLowerCase();
if(_1f=="dtlinstruction"){
continue;
}
if(_1f!=_f[1]){
this._uppers[_1f]=_f[1];
}
this._attributes[_1f]=true;
}
var _d=document.createElement("div");
_d.innerHTML=_c;
var _20={nodes:[]};
while(_d.childNodes.length){
_20.nodes.push(_d.removeChild(_d.childNodes[0]));
}
return _20;
},tokenize:function(_21){
var _22=[];
for(var i=0,_23;_23=_21[i++];){
if(_23.nodeType!=1){
this.__tokenize(_23,_22);
}else{
this._tokenize(_23,_22);
}
}
return _22;
},_swallowed:[],_tokenize:function(_24,_25){
var _26=false;
var _27=this._swallowed;
var i,j,tag,_28;
if(!_25.first){
_26=_25.first=true;
var _29=dd.register.getAttributeTags();
for(i=0;tag=_29[i];i++){
try{
(tag[2])({swallowNode:function(){
throw 1;
}},new dd.Token(dd.TOKEN_ATTR,""));
}
catch(e){
_27.push(tag);
}
}
}
for(i=0;tag=_27[i];i++){
var _2a=_24.getAttribute(tag[0]);
if(_2a){
var _27=false;
var _2b=(tag[2])({swallowNode:function(){
_27=true;
return _24;
}},new dd.Token(dd.TOKEN_ATTR,tag[0]+" "+_2a));
if(_27){
if(_24.parentNode&&_24.parentNode.removeChild){
_24.parentNode.removeChild(_24);
}
_25.push([dd.TOKEN_CUSTOM,_2b]);
return;
}
}
}
var _2c=[];
if(_9("ie")&&_24.tagName=="SCRIPT"){
_2c.push({nodeType:3,data:_24.text});
_24.text="";
}else{
for(i=0;_28=_24.childNodes[i];i++){
_2c.push(_28);
}
}
_25.push([dd.TOKEN_NODE,_24]);
var _2d=false;
if(_2c.length){
_25.push([dd.TOKEN_CHANGE,_24]);
_2d=true;
}
for(var key in this._attributes){
var _2e=false;
var _2f="";
if(key=="class"){
_2f=_24.className||_2f;
}else{
if(key=="for"){
_2f=_24.htmlFor||_2f;
}else{
if(key=="value"&&_24.value==_24.innerHTML){
continue;
}else{
if(_24.getAttribute){
_2f=_24.getAttribute(key,2)||_2f;
if(key=="href"||key=="src"){
if(_9("ie")){
var _30=location.href.lastIndexOf(location.hash);
var _31=location.href.substring(0,_30).split("/");
_31.pop();
_31=_31.join("/")+"/";
if(_2f.indexOf(_31)==0){
_2f=_2f.replace(_31,"");
}
_2f=decodeURIComponent(_2f);
}
}else{
if(key=="tstyle"){
_2e=key;
key="style";
}else{
if(dd.BOOLS[key.slice(1)]&&_1.trim(_2f)){
key=key.slice(1);
}else{
if(this._uppers[key]&&_1.trim(_2f)){
_2e=this._uppers[key];
}
}
}
}
}
}
}
}
if(_2e){
_24.setAttribute(_2e,"");
_24.removeAttribute(_2e);
}
if(typeof _2f=="function"){
_2f=_2f.toString().replace(this._re4,"$1");
}
if(!_2d){
_25.push([dd.TOKEN_CHANGE,_24]);
_2d=true;
}
_25.push([dd.TOKEN_ATTR,_24,key,_2f]);
}
for(i=0,_28;_28=_2c[i];i++){
if(_28.nodeType==1){
var _32=_28.getAttribute("dtlinstruction");
if(_32){
_28.parentNode.removeChild(_28);
_28={nodeType:8,data:_32};
}
}
this.__tokenize(_28,_25);
}
if(!_26&&_24.parentNode&&_24.parentNode.tagName){
if(_2d){
_25.push([dd.TOKEN_CHANGE,_24,true]);
}
_25.push([dd.TOKEN_CHANGE,_24.parentNode]);
_24.parentNode.removeChild(_24);
}else{
_25.push([dd.TOKEN_CHANGE,_24,true,true]);
}
},__tokenize:function(_33,_34){
var _35=_33.data;
switch(_33.nodeType){
case 1:
this._tokenize(_33,_34);
return;
case 3:
if(_35.match(/[^\s\n]/)&&(_35.indexOf("{{")!=-1||_35.indexOf("{%")!=-1)){
var _36=_a.tokenize(_35);
for(var j=0,_37;_37=_36[j];j++){
if(typeof _37=="string"){
_34.push([dd.TOKEN_TEXT,_37]);
}else{
_34.push(_37);
}
}
}else{
_34.push([_33.nodeType,_33]);
}
if(_33.parentNode){
_33.parentNode.removeChild(_33);
}
return;
case 8:
if(_35.indexOf("{%")==0){
var _37=_1.trim(_35.slice(2,-2));
if(_37.substr(0,5)=="load "){
var _38=_1.trim(_37).split(/\s+/g);
for(var i=1,_39;_39=_38[i];i++){
if(/\./.test(_39)){
_39=_39.replace(/\./g,"/");
}
require([_39]);
}
}
_34.push([dd.TOKEN_BLOCK,_37]);
}
if(_35.indexOf("{{")==0){
_34.push([dd.TOKEN_VAR,_1.trim(_35.slice(2,-2))]);
}
if(_33.parentNode){
_33.parentNode.removeChild(_33);
}
return;
}
}};
dd.DomTemplate=_1.extend(function(obj){
if(!obj.nodes){
var _3a=_4.byId(obj);
if(_3a&&_3a.nodeType==1){
_7.forEach(["class","src","href","name","value"],function(_3b){
_b._attributes[_3b]=true;
});
obj={nodes:[_3a]};
}else{
if(typeof obj=="object"){
obj=_a.getTemplateString(obj);
}
obj=_b.getTemplate(obj);
}
}
var _3c=_b.tokenize(obj.nodes);
if(dd.tests){
this.tokens=_3c.slice(0);
}
var _3d=new dd._DomParser(_3c);
this.nodelist=_3d.parse();
},{_count:0,_re:/\bdojo:([a-zA-Z0-9_]+)\b/g,setClass:function(str){
this.getRootNode().className=str;
},getRootNode:function(){
return this.buffer.rootNode;
},getBuffer:function(){
return new dd.DomBuffer();
},render:function(_3e,_3f){
_3f=this.buffer=_3f||this.getBuffer();
this.rootNode=null;
var _40=this.nodelist.render(_3e||new dd.Context({}),_3f);
for(var i=0,_41;_41=_3f._cache[i];i++){
if(_41._cache){
_41._cache.length=0;
}
}
return _40;
},unrender:function(_42,_43){
return this.nodelist.unrender(_42,_43);
}});
dd.DomBuffer=_1.extend(function(_44){
this._parent=_44;
this._cache=[];
},{concat:function(_45){
var _46=this._parent;
if(_46&&_45.parentNode&&_45.parentNode===_46&&!_46._dirty){
return this;
}
if(_45.nodeType==1&&!this.rootNode){
this.rootNode=_45||true;
return this;
}
if(!_46){
if(_45.nodeType==3&&_1.trim(_45.data)){
throw new Error("Text should not exist outside of the root node in template");
}
return this;
}
if(this._closed){
if(_45.nodeType==3&&!_1.trim(_45.data)){
return this;
}else{
throw new Error("Content should not exist outside of the root node in template");
}
}
if(_46._dirty){
if(_45._drawn&&_45.parentNode==_46){
var _47=_46._cache;
if(_47){
for(var i=0,_48;_48=_47[i];i++){
this.onAddNode&&this.onAddNode(_48);
_46.insertBefore(_48,_45);
this.onAddNodeComplete&&this.onAddNodeComplete(_48);
}
_47.length=0;
}
}
_46._dirty=false;
}
if(!_46._cache){
_46._cache=[];
this._cache.push(_46);
}
_46._dirty=true;
_46._cache.push(_45);
return this;
},remove:function(obj){
if(typeof obj=="string"){
if(this._parent){
this._parent.removeAttribute(obj);
}
}else{
if(obj.nodeType==1&&!this.getRootNode()&&!this._removed){
this._removed=true;
return this;
}
if(obj.parentNode){
this.onRemoveNode&&this.onRemoveNode(obj);
if(obj.parentNode){
obj.parentNode.removeChild(obj);
}
}
}
return this;
},setAttribute:function(key,_49){
var old=_6.attr(this._parent,key);
if(this.onChangeAttribute&&old!=_49){
this.onChangeAttribute(this._parent,key,old,_49);
}
if(key=="style"){
this._parent.style.cssText=_49;
}else{
_6.attr(this._parent,key,_49);
if(key=="value"){
this._parent.setAttribute(key,_49);
}
}
return this;
},addEvent:function(_4a,_4b,fn,_4c){
if(!_4a.getThis()){
throw new Error("You must use Context.setObject(instance)");
}
this.onAddEvent&&this.onAddEvent(this.getParent(),_4b,fn);
var _4d=fn;
if(_1.isArray(_4c)){
_4d=function(e){
this[fn].apply(this,[e].concat(_4c));
};
}
return _8.connect(this.getParent(),_4b,_4a.getThis(),_4d);
},setParent:function(_4e,up,_4f){
if(!this._parent){
this._parent=this._first=_4e;
}
if(up&&_4f&&_4e===this._first){
this._closed=true;
}
if(up){
var _50=this._parent;
var _51="";
var ie=_9("ie")&&_50.tagName=="SCRIPT";
if(ie){
_50.text="";
}
if(_50._dirty){
var _52=_50._cache;
var _53=(_50.tagName=="SELECT"&&!_50.options.length);
for(var i=0,_54;_54=_52[i];i++){
if(_54!==_50){
this.onAddNode&&this.onAddNode(_54);
if(ie){
_51+=_54.data;
}else{
_50.appendChild(_54);
if(_53&&_54.defaultSelected&&i){
_53=i;
}
}
this.onAddNodeComplete&&this.onAddNodeComplete(_54);
}
}
if(_53){
_50.options.selectedIndex=(typeof _53=="number")?_53:0;
}
_52.length=0;
_50._dirty=false;
}
if(ie){
_50.text=_51;
}
}
this._parent=_4e;
this.onSetParent&&this.onSetParent(_4e,up,_4f);
return this;
},getParent:function(){
return this._parent;
},getRootNode:function(){
return this.rootNode;
}});
dd._DomNode=_1.extend(function(_55){
this.contents=_55;
},{render:function(_56,_57){
this._rendered=true;
return _57.concat(this.contents);
},unrender:function(_58,_59){
if(!this._rendered){
return _59;
}
this._rendered=false;
return _59.remove(this.contents);
},clone:function(_5a){
return new this.constructor(this.contents);
}});
dd._DomNodeList=_1.extend(function(_5b){
this.contents=_5b||[];
},{push:function(_5c){
this.contents.push(_5c);
},unshift:function(_5d){
this.contents.unshift(_5d);
},render:function(_5e,_5f,_60){
_5f=_5f||dd.DomTemplate.prototype.getBuffer();
if(_60){
var _61=_5f.getParent();
}
for(var i=0;i<this.contents.length;i++){
_5f=this.contents[i].render(_5e,_5f);
if(!_5f){
throw new Error("Template node render functions must return their buffer");
}
}
if(_61){
_5f.setParent(_61);
}
return _5f;
},dummyRender:function(_62,_63,_64){
var div=document.createElement("div");
var _65=_63.getParent();
var old=_65._clone;
_65._clone=div;
var _66=this.clone(_63,div);
if(old){
_65._clone=old;
}else{
_65._clone=null;
}
_63=dd.DomTemplate.prototype.getBuffer();
_66.unshift(new dd.ChangeNode(div));
_66.unshift(new dd._DomNode(div));
_66.push(new dd.ChangeNode(div,true));
_66.render(_62,_63);
if(_64){
return _63.getRootNode();
}
var _67=div.innerHTML;
return (_9("ie"))?_5.replace(/\s*_(dirty|clone)="[^"]*"/g,""):_67;
},unrender:function(_68,_69,_6a){
if(_6a){
var _6b=_69.getParent();
}
for(var i=0;i<this.contents.length;i++){
_69=this.contents[i].unrender(_68,_69);
if(!_69){
throw new Error("Template node render functions must return their buffer");
}
}
if(_6b){
_69.setParent(_6b);
}
return _69;
},clone:function(_6c){
var _6d=_6c.getParent();
var _6e=this.contents;
var _6f=new dd._DomNodeList();
var _70=[];
for(var i=0;i<_6e.length;i++){
var _71=_6e[i].clone(_6c);
if(_71 instanceof dd.ChangeNode||_71 instanceof dd._DomNode){
var _72=_71.contents._clone;
if(_72){
_71.contents=_72;
}else{
if(_6d!=_71.contents&&_71 instanceof dd._DomNode){
var _73=_71.contents;
_71.contents=_71.contents.cloneNode(false);
_6c.onClone&&_6c.onClone(_73,_71.contents);
_70.push(_73);
_73._clone=_71.contents;
}
}
}
_6f.push(_71);
}
for(var i=0,_71;_71=_70[i];i++){
_71._clone=null;
}
return _6f;
},rtrim:function(){
while(1){
var i=this.contents.length-1;
if(this.contents[i] instanceof dd._DomTextNode&&this.contents[i].isEmpty()){
this.contents.pop();
}else{
break;
}
}
return this;
}});
dd._DomVarNode=_1.extend(function(str){
this.contents=new dd._Filter(str);
},{render:function(_74,_75){
var str=this.contents.resolve(_74);
var _76="text";
if(str){
if(str.render&&str.getRootNode){
_76="injection";
}else{
if(str.safe){
if(str.nodeType){
_76="node";
}else{
if(str.toString){
str=str.toString();
_76="html";
}
}
}
}
}
if(this._type&&_76!=this._type){
this.unrender(_74,_75);
}
this._type=_76;
switch(_76){
case "text":
this._rendered=true;
this._txt=this._txt||document.createTextNode(str);
if(this._txt.data!=str){
var old=this._txt.data;
this._txt.data=str;
_75.onChangeData&&_75.onChangeData(this._txt,old,this._txt.data);
}
return _75.concat(this._txt);
case "injection":
var _77=str.getRootNode();
if(this._rendered&&_77!=this._root){
_75=this.unrender(_74,_75);
}
this._root=_77;
var _78=this._injected=new dd._DomNodeList();
_78.push(new dd.ChangeNode(_75.getParent()));
_78.push(new dd._DomNode(_77));
_78.push(str);
_78.push(new dd.ChangeNode(_75.getParent()));
this._rendered=true;
return _78.render(_74,_75);
case "node":
this._rendered=true;
if(this._node&&this._node!=str&&this._node.parentNode&&this._node.parentNode===_75.getParent()){
this._node.parentNode.removeChild(this._node);
}
this._node=str;
return _75.concat(str);
case "html":
if(this._rendered&&this._src!=str){
_75=this.unrender(_74,_75);
}
this._src=str;
if(!this._rendered){
this._rendered=true;
this._html=this._html||[];
var div=(this._div=this._div||document.createElement("div"));
div.innerHTML=str;
var _79=div.childNodes;
while(_79.length){
var _7a=div.removeChild(_79[0]);
this._html.push(_7a);
_75=_75.concat(_7a);
}
}
return _75;
default:
return _75;
}
},unrender:function(_7b,_7c){
if(!this._rendered){
return _7c;
}
this._rendered=false;
switch(this._type){
case "text":
return _7c.remove(this._txt);
case "injection":
return this._injection.unrender(_7b,_7c);
case "node":
if(this._node.parentNode===_7c.getParent()){
return _7c.remove(this._node);
}
return _7c;
case "html":
for(var i=0,l=this._html.length;i<l;i++){
_7c=_7c.remove(this._html[i]);
}
return _7c;
default:
return _7c;
}
},clone:function(){
return new this.constructor(this.contents.getExpression());
}});
dd.ChangeNode=_1.extend(function(_7d,up,_7e){
this.contents=_7d;
this.up=up;
this.root=_7e;
},{render:function(_7f,_80){
return _80.setParent(this.contents,this.up,this.root);
},unrender:function(_81,_82){
if(!_82.getParent()){
return _82;
}
return _82.setParent(this.contents);
},clone:function(){
return new this.constructor(this.contents,this.up,this.root);
}});
dd.AttributeNode=_1.extend(function(key,_83){
this.key=key;
this.value=_83;
this.contents=_83;
if(this._pool[_83]){
this.nodelist=this._pool[_83];
}else{
if(!(this.nodelist=dd.quickFilter(_83))){
this.nodelist=(new dd.Template(_83,true)).nodelist;
}
this._pool[_83]=this.nodelist;
}
this.contents="";
},{_pool:{},render:function(_84,_85){
var key=this.key;
var _86=this.nodelist.dummyRender(_84);
if(dd.BOOLS[key]){
_86=!(_86=="false"||_86=="undefined"||!_86);
}
if(_86!==this.contents){
this.contents=_86;
return _85.setAttribute(key,_86);
}
return _85;
},unrender:function(_87,_88){
this.contents="";
return _88.remove(this.key);
},clone:function(_89){
return new this.constructor(this.key,this.value);
}});
dd._DomTextNode=_1.extend(function(str){
this.contents=document.createTextNode(str);
this.upcoming=str;
},{set:function(_8a){
this.upcoming=_8a;
return this;
},render:function(_8b,_8c){
if(this.contents.data!=this.upcoming){
var old=this.contents.data;
this.contents.data=this.upcoming;
_8c.onChangeData&&_8c.onChangeData(this.contents,old,this.upcoming);
}
return _8c.concat(this.contents);
},unrender:function(_8d,_8e){
return _8e.remove(this.contents);
},isEmpty:function(){
return !_1.trim(this.contents.data);
},clone:function(){
return new this.constructor(this.contents.data);
}});
dd._DomParser=_1.extend(function(_8f){
this.contents=_8f;
},{i:0,parse:function(_90){
var _91={};
var _92=this.contents;
if(!_90){
_90=[];
}
for(var i=0;i<_90.length;i++){
_91[_90[i]]=true;
}
var _93=new dd._DomNodeList();
while(this.i<_92.length){
var _94=_92[this.i++];
var _95=_94[0];
var _96=_94[1];
if(_95==dd.TOKEN_CUSTOM){
_93.push(_96);
}else{
if(_95==dd.TOKEN_CHANGE){
var _97=new dd.ChangeNode(_96,_94[2],_94[3]);
_96[_97.attr]=_97;
_93.push(_97);
}else{
if(_95==dd.TOKEN_ATTR){
var fn=_a.getTag("attr:"+_94[2],true);
if(fn&&_94[3]){
if(_94[3].indexOf("{%")!=-1||_94[3].indexOf("{{")!=-1){
_96.setAttribute(_94[2],"");
}
_93.push(fn(null,new dd.Token(_95,_94[2]+" "+_94[3])));
}else{
if(_1.isString(_94[3])){
if(_94[2]=="style"||_94[3].indexOf("{%")!=-1||_94[3].indexOf("{{")!=-1){
_93.push(new dd.AttributeNode(_94[2],_94[3]));
}else{
if(_1.trim(_94[3])){
try{
_6.attr(_96,_94[2],_94[3]);
}
catch(e){
}
}
}
}
}
}else{
if(_95==dd.TOKEN_NODE){
var fn=_a.getTag("node:"+_96.tagName.toLowerCase(),true);
if(fn){
_93.push(fn(null,new dd.Token(_95,_96),_96.tagName.toLowerCase()));
}
_93.push(new dd._DomNode(_96));
}else{
if(_95==dd.TOKEN_VAR){
_93.push(new dd._DomVarNode(_96));
}else{
if(_95==dd.TOKEN_TEXT){
_93.push(new dd._DomTextNode(_96.data||_96));
}else{
if(_95==dd.TOKEN_BLOCK){
if(_91[_96]){
--this.i;
return _93;
}
var cmd=_96.split(/\s+/g);
if(cmd.length){
cmd=cmd[0];
var fn=_a.getTag(cmd);
if(typeof fn!="function"){
throw new Error("Function not found for "+cmd);
}
var tpl=fn(this,new dd.Token(_95,_96));
if(tpl){
_93.push(tpl);
}
}
}
}
}
}
}
}
}
}
if(_90.length){
throw new Error("Could not find closing tag(s): "+_90.toString());
}
return _93;
},next_token:function(){
var _98=this.contents[this.i++];
return new dd.Token(_98[0],_98[1]);
},delete_first_token:function(){
this.i++;
},skip_past:function(_99){
return dd._Parser.prototype.skip_past.call(this,_99);
},create_variable_node:function(_9a){
return new dd._DomVarNode(_9a);
},create_text_node:function(_9b){
return new dd._DomTextNode(_9b||"");
},getTemplate:function(loc){
return new dd.DomTemplate(_b.getTemplate(loc));
}});
return dojox.dtl.dom;
});
