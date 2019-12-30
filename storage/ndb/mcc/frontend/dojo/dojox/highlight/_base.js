//>>built
define("dojox/highlight/_base",["dojo/_base/lang","dojo/_base/array","dojo/dom","dojo/dom-class"],function(_1,_2,_3,_4){
var dh=_1.getObject("dojox.highlight",true),_5="\\b(0x[A-Za-z0-9]+|\\d+(\\.\\d+)?)";
dh.languages=dh.languages||{};
dh.constants={IDENT_RE:"[a-zA-Z][a-zA-Z0-9_]*",UNDERSCORE_IDENT_RE:"[a-zA-Z_][a-zA-Z0-9_]*",NUMBER_RE:"\\b\\d+(\\.\\d+)?",C_NUMBER_RE:_5,APOS_STRING_MODE:{className:"string",begin:"'",end:"'",illegal:"\\n",contains:["escape"],relevance:0},QUOTE_STRING_MODE:{className:"string",begin:"\"",end:"\"",illegal:"\\n",contains:["escape"],relevance:0},BACKSLASH_ESCAPE:{className:"escape",begin:"\\\\.",end:"^",relevance:0},C_LINE_COMMENT_MODE:{className:"comment",begin:"//",end:"$",relevance:0},C_BLOCK_COMMENT_MODE:{className:"comment",begin:"/\\*",end:"\\*/"},HASH_COMMENT_MODE:{className:"comment",begin:"#",end:"$"},C_NUMBER_MODE:{className:"number",begin:_5,end:"^",relevance:0}};
function _6(_7){
return _7.replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;");
};
function _8(_9){
return _2.every(_9.childNodes,function(_a){
return _a.nodeType==3||String(_a.nodeName).toLowerCase()=="br";
});
};
function _b(_c){
var _d=[];
_2.forEach(_c.childNodes,function(_e){
if(_e.nodeType==3){
_d.push(_e.nodeValue);
}else{
if(String(_e.nodeName).toLowerCase()=="br"){
_d.push("\n");
}else{
throw "Complex markup";
}
}
});
return _d.join("");
};
function _f(_10){
if(!_10.keywordGroups){
for(var key in _10.keywords){
var kw=_10.keywords[key];
if(kw instanceof Object){
_10.keywordGroups=_10.keywords;
}else{
_10.keywordGroups={keyword:_10.keywords};
}
break;
}
}
};
function _11(_12){
if(_12.defaultMode&&_12.modes){
_f(_12.defaultMode);
_2.forEach(_12.modes,_f);
}
};
var _13=function(_14,_15){
this.langName=_14;
this.lang=dh.languages[_14];
this.modes=[this.lang.defaultMode];
this.relevance=0;
this.keywordCount=0;
this.result=[];
if(!this.lang.defaultMode.illegalRe){
this.buildRes();
_11(this.lang);
}
try{
this.highlight(_15);
this.result=this.result.join("");
}
catch(e){
if(e=="Illegal"){
this.relevance=0;
this.keywordCount=0;
this.partialResult=this.result.join("");
this.result=_6(_15);
}else{
throw e;
}
}
};
_1.extend(_13,{buildRes:function(){
_2.forEach(this.lang.modes,function(_16){
if(_16.begin){
_16.beginRe=this.langRe("^"+_16.begin);
}
if(_16.end){
_16.endRe=this.langRe("^"+_16.end);
}
if(_16.illegal){
_16.illegalRe=this.langRe("^(?:"+_16.illegal+")");
}
},this);
this.lang.defaultMode.illegalRe=this.langRe("^(?:"+this.lang.defaultMode.illegal+")");
},subMode:function(_17){
var _18=this.modes[this.modes.length-1].contains;
if(_18){
var _19=this.lang.modes;
for(var i=0;i<_18.length;++i){
var _1a=_18[i];
for(var j=0;j<_19.length;++j){
var _1b=_19[j];
if(_1b.className==_1a&&_1b.beginRe.test(_17)){
return _1b;
}
}
}
}
return null;
},endOfMode:function(_1c){
for(var i=this.modes.length-1;i>=0;--i){
var _1d=this.modes[i];
if(_1d.end&&_1d.endRe.test(_1c)){
return this.modes.length-i;
}
if(!_1d.endsWithParent){
break;
}
}
return 0;
},isIllegal:function(_1e){
var _1f=this.modes[this.modes.length-1].illegalRe;
return _1f&&_1f.test(_1e);
},langRe:function(_20,_21){
var _22="m"+(this.lang.case_insensitive?"i":"")+(_21?"g":"");
return new RegExp(_20,_22);
},buildTerminators:function(){
var _23=this.modes[this.modes.length-1],_24={};
if(_23.contains){
_2.forEach(this.lang.modes,function(_25){
if(_2.indexOf(_23.contains,_25.className)>=0){
_24[_25.begin]=1;
}
});
}
for(var i=this.modes.length-1;i>=0;--i){
var m=this.modes[i];
if(m.end){
_24[m.end]=1;
}
if(!m.endsWithParent){
break;
}
}
if(_23.illegal){
_24[_23.illegal]=1;
}
var t=[];
for(i in _24){
t.push(i);
}
_23.terminatorsRe=this.langRe("("+t.join("|")+")");
},eatModeChunk:function(_26,_27){
var _28=this.modes[this.modes.length-1];
if(!_28.terminatorsRe){
this.buildTerminators();
}
_26=_26.substr(_27);
var _29=_28.terminatorsRe.exec(_26);
if(!_29){
return {buffer:_26,lexeme:"",end:true};
}
return {buffer:_29.index?_26.substr(0,_29.index):"",lexeme:_29[0],end:false};
},keywordMatch:function(_2a,_2b){
var _2c=_2b[0];
if(this.lang.case_insensitive){
_2c=_2c.toLowerCase();
}
for(var _2d in _2a.keywordGroups){
if(_2c in _2a.keywordGroups[_2d]){
return _2d;
}
}
return "";
},buildLexemes:function(_2e){
var _2f={};
_2.forEach(_2e.lexems,function(_30){
_2f[_30]=1;
});
var t=[];
for(var i in _2f){
t.push(i);
}
_2e.lexemsRe=this.langRe("("+t.join("|")+")",true);
},processKeywords:function(_31){
var _32=this.modes[this.modes.length-1];
if(!_32.keywords||!_32.lexems){
return _6(_31);
}
if(!_32.lexemsRe){
this.buildLexemes(_32);
}
_32.lexemsRe.lastIndex=0;
var _33=[],_34=0,_35=_32.lexemsRe.exec(_31);
while(_35){
_33.push(_6(_31.substr(_34,_35.index-_34)));
var _36=this.keywordMatch(_32,_35);
if(_36){
++this.keywordCount;
_33.push("<span class=\""+_36+"\">"+_6(_35[0])+"</span>");
}else{
_33.push(_6(_35[0]));
}
_34=_32.lexemsRe.lastIndex;
_35=_32.lexemsRe.exec(_31);
}
_33.push(_6(_31.substr(_34,_31.length-_34)));
return _33.join("");
},processModeInfo:function(_37,_38,end){
var _39=this.modes[this.modes.length-1];
if(end){
this.result.push(this.processKeywords(_39.buffer+_37));
return;
}
if(this.isIllegal(_38)){
throw "Illegal";
}
var _3a=this.subMode(_38);
if(_3a){
_39.buffer+=_37;
this.result.push(this.processKeywords(_39.buffer));
if(_3a.excludeBegin){
this.result.push(_38+"<span class=\""+_3a.className+"\">");
_3a.buffer="";
}else{
this.result.push("<span class=\""+_3a.className+"\">");
_3a.buffer=_38;
}
this.modes.push(_3a);
this.relevance+=typeof _3a.relevance=="number"?_3a.relevance:1;
return;
}
var _3b=this.endOfMode(_38);
if(_3b){
_39.buffer+=_37;
if(_39.excludeEnd){
this.result.push(this.processKeywords(_39.buffer)+"</span>"+_38);
}else{
this.result.push(this.processKeywords(_39.buffer+_38)+"</span>");
}
while(_3b>1){
this.result.push("</span>");
--_3b;
this.modes.pop();
}
this.modes.pop();
this.modes[this.modes.length-1].buffer="";
return;
}
},highlight:function(_3c){
var _3d=0;
this.lang.defaultMode.buffer="";
do{
var _3e=this.eatModeChunk(_3c,_3d);
this.processModeInfo(_3e.buffer,_3e.lexeme,_3e.end);
_3d+=_3e.buffer.length+_3e.lexeme.length;
}while(!_3e.end);
if(this.modes.length>1){
throw "Illegal";
}
}});
function _3f(_40,_41,_42){
if(String(_40.tagName).toLowerCase()=="code"&&String(_40.parentNode.tagName).toLowerCase()=="pre"){
var _43=document.createElement("div"),_44=_40.parentNode.parentNode;
_43.innerHTML="<pre><code class=\""+_41+"\">"+_42+"</code></pre>";
_44.replaceChild(_43.firstChild,_40.parentNode);
}else{
_40.className=_41;
_40.innerHTML=_42;
}
};
function _45(_46,str){
var _47=new _13(_46,str);
return {result:_47.result,langName:_46,partialResult:_47.partialResult};
};
function _48(_49,_4a){
var _4b=_45(_4a,_b(_49));
_3f(_49,_49.className,_4b.result);
};
function _4c(str){
var _4d="",_4e="",_4f=2,_50=str;
for(var key in dh.languages){
if(!dh.languages[key].defaultMode){
continue;
}
var _51=new _13(key,_50),_52=_51.keywordCount+_51.relevance,_53=0;
if(!_4d||_52>_53){
_53=_52;
_4d=_51.result;
_4e=_51.langName;
}
}
return {result:_4d,langName:_4e};
};
function _54(_55){
var _56=_4c(_b(_55));
if(_56.result){
_3f(_55,_56.langName,_56.result);
}
};
dojox.highlight.processString=function(str,_57){
return _57?_45(_57,str):_4c(str);
};
dojox.highlight.init=function(_58){
_58=_3.byId(_58);
if(_4.contains(_58,"no-highlight")){
return;
}
if(!_8(_58)){
return;
}
var _59=_58.className.split(/\s+/),_5a=_2.some(_59,function(_5b){
if(_5b.charAt(0)!="_"&&dh.languages[_5b]){
_48(_58,_5b);
return true;
}
return false;
});
if(!_5a){
_54(_58);
}
};
dh.Code=function(_5c,_5d){
dh.init(_5d);
};
return dh;
});
