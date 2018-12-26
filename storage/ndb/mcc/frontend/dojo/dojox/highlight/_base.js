//>>built
define("dojox/highlight/_base",["dojo","dojox/main"],function(_1,_2){
var dh=_1.getObject("dojox.highlight",true),_3="\\b(0x[A-Za-z0-9]+|\\d+(\\.\\d+)?)";
dh.languages=dh.languages||{};
dh.constants={IDENT_RE:"[a-zA-Z][a-zA-Z0-9_]*",UNDERSCORE_IDENT_RE:"[a-zA-Z_][a-zA-Z0-9_]*",NUMBER_RE:"\\b\\d+(\\.\\d+)?",C_NUMBER_RE:_3,APOS_STRING_MODE:{className:"string",begin:"'",end:"'",illegal:"\\n",contains:["escape"],relevance:0},QUOTE_STRING_MODE:{className:"string",begin:"\"",end:"\"",illegal:"\\n",contains:["escape"],relevance:0},BACKSLASH_ESCAPE:{className:"escape",begin:"\\\\.",end:"^",relevance:0},C_LINE_COMMENT_MODE:{className:"comment",begin:"//",end:"$",relevance:0},C_BLOCK_COMMENT_MODE:{className:"comment",begin:"/\\*",end:"\\*/"},HASH_COMMENT_MODE:{className:"comment",begin:"#",end:"$"},C_NUMBER_MODE:{className:"number",begin:_3,end:"^",relevance:0}};
function _4(_5){
return _5.replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;");
};
function _6(_7){
return _1.every(_7.childNodes,function(_8){
return _8.nodeType==3||String(_8.nodeName).toLowerCase()=="br";
});
};
function _9(_a){
var _b=[];
_1.forEach(_a.childNodes,function(_c){
if(_c.nodeType==3){
_b.push(_c.nodeValue);
}else{
if(String(_c.nodeName).toLowerCase()=="br"){
_b.push("\n");
}else{
throw "Complex markup";
}
}
});
return _b.join("");
};
function _d(_e){
if(!_e.keywordGroups){
for(var _f in _e.keywords){
var kw=_e.keywords[_f];
if(kw instanceof Object){
_e.keywordGroups=_e.keywords;
}else{
_e.keywordGroups={keyword:_e.keywords};
}
break;
}
}
};
function _10(_11){
if(_11.defaultMode&&_11.modes){
_d(_11.defaultMode);
_1.forEach(_11.modes,_d);
}
};
var _12=function(_13,_14){
this.langName=_13;
this.lang=dh.languages[_13];
this.modes=[this.lang.defaultMode];
this.relevance=0;
this.keywordCount=0;
this.result=[];
if(!this.lang.defaultMode.illegalRe){
this.buildRes();
_10(this.lang);
}
try{
this.highlight(_14);
this.result=this.result.join("");
}
catch(e){
if(e=="Illegal"){
this.relevance=0;
this.keywordCount=0;
this.partialResult=this.result.join("");
this.result=_4(_14);
}else{
throw e;
}
}
};
_1.extend(_12,{buildRes:function(){
_1.forEach(this.lang.modes,function(_15){
if(_15.begin){
_15.beginRe=this.langRe("^"+_15.begin);
}
if(_15.end){
_15.endRe=this.langRe("^"+_15.end);
}
if(_15.illegal){
_15.illegalRe=this.langRe("^(?:"+_15.illegal+")");
}
},this);
this.lang.defaultMode.illegalRe=this.langRe("^(?:"+this.lang.defaultMode.illegal+")");
},subMode:function(_16){
var _17=this.modes[this.modes.length-1].contains;
if(_17){
var _18=this.lang.modes;
for(var i=0;i<_17.length;++i){
var _19=_17[i];
for(var j=0;j<_18.length;++j){
var _1a=_18[j];
if(_1a.className==_19&&_1a.beginRe.test(_16)){
return _1a;
}
}
}
}
return null;
},endOfMode:function(_1b){
for(var i=this.modes.length-1;i>=0;--i){
var _1c=this.modes[i];
if(_1c.end&&_1c.endRe.test(_1b)){
return this.modes.length-i;
}
if(!_1c.endsWithParent){
break;
}
}
return 0;
},isIllegal:function(_1d){
var _1e=this.modes[this.modes.length-1].illegalRe;
return _1e&&_1e.test(_1d);
},langRe:function(_1f,_20){
var _21="m"+(this.lang.case_insensitive?"i":"")+(_20?"g":"");
return new RegExp(_1f,_21);
},buildTerminators:function(){
var _22=this.modes[this.modes.length-1],_23={};
if(_22.contains){
_1.forEach(this.lang.modes,function(_24){
if(_1.indexOf(_22.contains,_24.className)>=0){
_23[_24.begin]=1;
}
});
}
for(var i=this.modes.length-1;i>=0;--i){
var m=this.modes[i];
if(m.end){
_23[m.end]=1;
}
if(!m.endsWithParent){
break;
}
}
if(_22.illegal){
_23[_22.illegal]=1;
}
var t=[];
for(i in _23){
t.push(i);
}
_22.terminatorsRe=this.langRe("("+t.join("|")+")");
},eatModeChunk:function(_25,_26){
var _27=this.modes[this.modes.length-1];
if(!_27.terminatorsRe){
this.buildTerminators();
}
_25=_25.substr(_26);
var _28=_27.terminatorsRe.exec(_25);
if(!_28){
return {buffer:_25,lexeme:"",end:true};
}
return {buffer:_28.index?_25.substr(0,_28.index):"",lexeme:_28[0],end:false};
},keywordMatch:function(_29,_2a){
var _2b=_2a[0];
if(this.lang.case_insensitive){
_2b=_2b.toLowerCase();
}
for(var _2c in _29.keywordGroups){
if(_2b in _29.keywordGroups[_2c]){
return _2c;
}
}
return "";
},buildLexemes:function(_2d){
var _2e={};
_1.forEach(_2d.lexems,function(_2f){
_2e[_2f]=1;
});
var t=[];
for(var i in _2e){
t.push(i);
}
_2d.lexemsRe=this.langRe("("+t.join("|")+")",true);
},processKeywords:function(_30){
var _31=this.modes[this.modes.length-1];
if(!_31.keywords||!_31.lexems){
return _4(_30);
}
if(!_31.lexemsRe){
this.buildLexemes(_31);
}
_31.lexemsRe.lastIndex=0;
var _32=[],_33=0,_34=_31.lexemsRe.exec(_30);
while(_34){
_32.push(_4(_30.substr(_33,_34.index-_33)));
var _35=this.keywordMatch(_31,_34);
if(_35){
++this.keywordCount;
_32.push("<span class=\""+_35+"\">"+_4(_34[0])+"</span>");
}else{
_32.push(_4(_34[0]));
}
_33=_31.lexemsRe.lastIndex;
_34=_31.lexemsRe.exec(_30);
}
_32.push(_4(_30.substr(_33,_30.length-_33)));
return _32.join("");
},processModeInfo:function(_36,_37,end){
var _38=this.modes[this.modes.length-1];
if(end){
this.result.push(this.processKeywords(_38.buffer+_36));
return;
}
if(this.isIllegal(_37)){
throw "Illegal";
}
var _39=this.subMode(_37);
if(_39){
_38.buffer+=_36;
this.result.push(this.processKeywords(_38.buffer));
if(_39.excludeBegin){
this.result.push(_37+"<span class=\""+_39.className+"\">");
_39.buffer="";
}else{
this.result.push("<span class=\""+_39.className+"\">");
_39.buffer=_37;
}
this.modes.push(_39);
this.relevance+=typeof _39.relevance=="number"?_39.relevance:1;
return;
}
var _3a=this.endOfMode(_37);
if(_3a){
_38.buffer+=_36;
if(_38.excludeEnd){
this.result.push(this.processKeywords(_38.buffer)+"</span>"+_37);
}else{
this.result.push(this.processKeywords(_38.buffer+_37)+"</span>");
}
while(_3a>1){
this.result.push("</span>");
--_3a;
this.modes.pop();
}
this.modes.pop();
this.modes[this.modes.length-1].buffer="";
return;
}
},highlight:function(_3b){
var _3c=0;
this.lang.defaultMode.buffer="";
do{
var _3d=this.eatModeChunk(_3b,_3c);
this.processModeInfo(_3d.buffer,_3d.lexeme,_3d.end);
_3c+=_3d.buffer.length+_3d.lexeme.length;
}while(!_3d.end);
if(this.modes.length>1){
throw "Illegal";
}
}});
function _3e(_3f,_40,_41){
if(String(_3f.tagName).toLowerCase()=="code"&&String(_3f.parentNode.tagName).toLowerCase()=="pre"){
var _42=document.createElement("div"),_43=_3f.parentNode.parentNode;
_42.innerHTML="<pre><code class=\""+_40+"\">"+_41+"</code></pre>";
_43.replaceChild(_42.firstChild,_3f.parentNode);
}else{
_3f.className=_40;
_3f.innerHTML=_41;
}
};
function _44(_45,str){
var _46=new _12(_45,str);
return {result:_46.result,langName:_45,partialResult:_46.partialResult};
};
function _47(_48,_49){
var _4a=_44(_49,_9(_48));
_3e(_48,_48.className,_4a.result);
};
function _4b(str){
var _4c="",_4d="",_4e=2,_4f=str;
for(var key in dh.languages){
if(!dh.languages[key].defaultMode){
continue;
}
var _50=new _12(key,_4f),_51=_50.keywordCount+_50.relevance,_52=0;
if(!_4c||_51>_52){
_52=_51;
_4c=_50.result;
_4d=_50.langName;
}
}
return {result:_4c,langName:_4d};
};
function _53(_54){
var _55=_4b(_9(_54));
if(_55.result){
_3e(_54,_55.langName,_55.result);
}
};
_2.highlight.processString=function(str,_56){
return _56?_44(_56,str):_4b(str);
};
_2.highlight.init=function(_57){
_57=_1.byId(_57);
if(_1.hasClass(_57,"no-highlight")){
return;
}
if(!_6(_57)){
return;
}
var _58=_57.className.split(/\s+/),_59=_1.some(_58,function(_5a){
if(_5a.charAt(0)!="_"&&dh.languages[_5a]){
_47(_57,_5a);
return true;
}
return false;
});
if(!_59){
_53(_57);
}
};
dh.Code=function(_5b,_5c){
dh.init(_5c);
};
return dh;
});
