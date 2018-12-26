//>>built
define("dojox/dtl/_base",["dojo/_base/kernel","dojo/_base/lang","dojox/string/tokenize","dojo/_base/json","dojo/dom","dojo/_base/xhr","dojox/string/Builder","dojo/_base/Deferred"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.experimental("dojox.dtl");
var dd=_2.getObject("dojox.dtl",true);
dd._base={};
dd.TOKEN_BLOCK=-1;
dd.TOKEN_VAR=-2;
dd.TOKEN_COMMENT=-3;
dd.TOKEN_TEXT=3;
dd._Context=_2.extend(function(_9){
if(_9){
_2._mixin(this,_9);
if(_9.get){
this._getter=_9.get;
delete this.get;
}
}
},{push:function(){
var _a=this;
var _b=_2.delegate(this);
_b.pop=function(){
return _a;
};
return _b;
},pop:function(){
throw new Error("pop() called on empty Context");
},get:function(_c,_d){
var n=this._normalize;
if(this._getter){
var _e=this._getter(_c);
if(_e!==undefined){
return n(_e);
}
}
if(this[_c]!==undefined){
return n(this[_c]);
}
return _d;
},_normalize:function(_f){
if(_f instanceof Date){
_f.year=_f.getFullYear();
_f.month=_f.getMonth()+1;
_f.day=_f.getDate();
_f.date=_f.year+"-"+("0"+_f.month).slice(-2)+"-"+("0"+_f.day).slice(-2);
_f.hour=_f.getHours();
_f.minute=_f.getMinutes();
_f.second=_f.getSeconds();
_f.microsecond=_f.getMilliseconds();
}
return _f;
},update:function(_10){
var _11=this.push();
if(_10){
_2._mixin(this,_10);
}
return _11;
}});
var _12=/("(?:[^"\\]*(?:\\.[^"\\]*)*)"|'(?:[^'\\]*(?:\\.[^'\\]*)*)'|[^\s]+)/g;
var _13=/\s+/g;
var _14=function(_15,_16){
_15=_15||_13;
if(!(_15 instanceof RegExp)){
_15=new RegExp(_15,"g");
}
if(!_15.global){
throw new Error("You must use a globally flagged RegExp with split "+_15);
}
_15.exec("");
var _17,_18=[],_19=0,i=0;
while((_17=_15.exec(this))){
_18.push(this.slice(_19,_15.lastIndex-_17[0].length));
_19=_15.lastIndex;
if(_16&&(++i>_16-1)){
break;
}
}
_18.push(this.slice(_19));
return _18;
};
dd.Token=function(_1a,_1b){
this.token_type=_1a;
this.contents=new String(_2.trim(_1b));
this.contents.split=_14;
this.split=function(){
return String.prototype.split.apply(this.contents,arguments);
};
};
dd.Token.prototype.split_contents=function(_1c){
var bit,_1d=[],i=0;
_1c=_1c||999;
while(i++<_1c&&(bit=_12.exec(this.contents))){
bit=bit[0];
if(bit.charAt(0)=="\""&&bit.slice(-1)=="\""){
_1d.push("\""+bit.slice(1,-1).replace("\\\"","\"").replace("\\\\","\\")+"\"");
}else{
if(bit.charAt(0)=="'"&&bit.slice(-1)=="'"){
_1d.push("'"+bit.slice(1,-1).replace("\\'","'").replace("\\\\","\\")+"'");
}else{
_1d.push(bit);
}
}
}
return _1d;
};
var ddt=dd.text={_get:function(_1e,_1f,_20){
var _21=dd.register.get(_1e,_1f.toLowerCase(),_20);
if(!_21){
if(!_20){
throw new Error("No tag found for "+_1f);
}
return null;
}
var fn=_21[1];
var _22=_21[2];
var _23;
if(fn.indexOf(":")!=-1){
_23=fn.split(":");
fn=_23.pop();
}
var mod=_22;
if(/\./.test(_22)){
_22=_22.replace(/\./g,"/");
}
require([_22],function(){
});
var _24=_2.getObject(mod);
return _24[fn||_1f]||_24[_1f+"_"]||_24[fn+"_"];
},getTag:function(_25,_26){
return ddt._get("tag",_25,_26);
},getFilter:function(_27,_28){
return ddt._get("filter",_27,_28);
},getTemplate:function(_29){
return new dd.Template(ddt.getTemplateString(_29));
},getTemplateString:function(_2a){
return _6._getText(_2a.toString())||"";
},_resolveLazy:function(_2b,_2c,_2d){
if(_2c){
if(_2d){
return _2d.fromJson(_6._getText(_2b))||{};
}else{
return dd.text.getTemplateString(_2b);
}
}else{
return _6.get({handleAs:_2d?"json":"text",url:_2b});
}
},_resolveTemplateArg:function(arg,_2e){
if(ddt._isTemplate(arg)){
if(!_2e){
var d=new _8();
d.callback(arg);
return d;
}
return arg;
}
return ddt._resolveLazy(arg,_2e);
},_isTemplate:function(arg){
return (arg===undefined)||(typeof arg=="string"&&(arg.match(/^\s*[<{]/)||arg.indexOf(" ")!=-1));
},_resolveContextArg:function(arg,_2f){
if(arg.constructor==Object){
if(!_2f){
var d=new _8;
d.callback(arg);
return d;
}
return arg;
}
return ddt._resolveLazy(arg,_2f,true);
},_re:/(?:\{\{\s*(.+?)\s*\}\}|\{%\s*(load\s*)?(.+?)\s*%\})/g,tokenize:function(str){
return _3(str,ddt._re,ddt._parseDelims);
},_parseDelims:function(_30,_31,tag){
if(_30){
return [dd.TOKEN_VAR,_30];
}else{
if(_31){
var _32=_2.trim(tag).split(/\s+/g);
for(var i=0,_33;_33=_32[i];i++){
if(/\./.test(_33)){
_33=_33.replace(/\./g,"/");
}
require([_33]);
}
}else{
return [dd.TOKEN_BLOCK,tag];
}
}
}};
dd.Template=_2.extend(function(_34,_35){
var str=_35?_34:ddt._resolveTemplateArg(_34,true)||"";
var _36=ddt.tokenize(str);
var _37=new dd._Parser(_36);
this.nodelist=_37.parse();
},{update:function(_38,_39){
return ddt._resolveContextArg(_39).addCallback(this,function(_3a){
var _3b=this.render(new dd._Context(_3a));
if(_38.forEach){
_38.forEach(function(_3c){
_3c.innerHTML=_3b;
});
}else{
_5.byId(_38).innerHTML=_3b;
}
return this;
});
},render:function(_3d,_3e){
_3e=_3e||this.getBuffer();
_3d=_3d||new dd._Context({});
return this.nodelist.render(_3d,_3e)+"";
},getBuffer:function(){
return new _7();
}});
var _3f=/\{\{\s*(.+?)\s*\}\}/g;
dd.quickFilter=function(str){
if(!str){
return new dd._NodeList();
}
if(str.indexOf("{%")==-1){
return new dd._QuickNodeList(_3(str,_3f,function(_40){
return new dd._Filter(_40);
}));
}
};
dd._QuickNodeList=_2.extend(function(_41){
this.contents=_41;
},{render:function(_42,_43){
for(var i=0,l=this.contents.length;i<l;i++){
if(this.contents[i].resolve){
_43=_43.concat(this.contents[i].resolve(_42));
}else{
_43=_43.concat(this.contents[i]);
}
}
return _43;
},dummyRender:function(_44){
return this.render(_44,dd.Template.prototype.getBuffer()).toString();
},clone:function(_45){
return this;
}});
dd._Filter=_2.extend(function(_46){
if(!_46){
throw new Error("Filter must be called with variable name");
}
this.contents=_46;
var _47=this._cache[_46];
if(_47){
this.key=_47[0];
this.filters=_47[1];
}else{
this.filters=[];
_3(_46,this._re,this._tokenize,this);
this._cache[_46]=[this.key,this.filters];
}
},{_cache:{},_re:/(?:^_\("([^\\"]*(?:\\.[^\\"])*)"\)|^"([^\\"]*(?:\\.[^\\"]*)*)"|^([a-zA-Z0-9_.]+)|\|(\w+)(?::(?:_\("([^\\"]*(?:\\.[^\\"])*)"\)|"([^\\"]*(?:\\.[^\\"]*)*)"|([a-zA-Z0-9_.]+)|'([^\\']*(?:\\.[^\\']*)*)'))?|^'([^\\']*(?:\\.[^\\']*)*)')/g,_values:{0:"\"",1:"\"",2:"",8:"\""},_args:{4:"\"",5:"\"",6:"",7:"'"},_tokenize:function(){
var pos,arg;
for(var i=0,has=[];i<arguments.length;i++){
has[i]=(arguments[i]!==undefined&&typeof arguments[i]=="string"&&arguments[i]);
}
if(!this.key){
for(pos in this._values){
if(has[pos]){
this.key=this._values[pos]+arguments[pos]+this._values[pos];
break;
}
}
}else{
for(pos in this._args){
if(has[pos]){
var _48=arguments[pos];
if(this._args[pos]=="'"){
_48=_48.replace(/\\'/g,"'");
}else{
if(this._args[pos]=="\""){
_48=_48.replace(/\\"/g,"\"");
}
}
arg=[!this._args[pos],_48];
break;
}
}
var fn=ddt.getFilter(arguments[3]);
if(!_2.isFunction(fn)){
throw new Error(arguments[3]+" is not registered as a filter");
}
this.filters.push([fn,arg]);
}
},getExpression:function(){
return this.contents;
},resolve:function(_49){
if(this.key===undefined){
return "";
}
var str=this.resolvePath(this.key,_49);
for(var i=0,_4a;_4a=this.filters[i];i++){
if(_4a[1]){
if(_4a[1][0]){
str=_4a[0](str,this.resolvePath(_4a[1][1],_49));
}else{
str=_4a[0](str,_4a[1][1]);
}
}else{
str=_4a[0](str);
}
}
return str;
},resolvePath:function(_4b,_4c){
var _4d,_4e;
var _4f=_4b.charAt(0);
var _50=_4b.slice(-1);
if(!isNaN(parseInt(_4f))){
_4d=(_4b.indexOf(".")==-1)?parseInt(_4b):parseFloat(_4b);
}else{
if(_4f=="\""&&_4f==_50){
_4d=_4b.slice(1,-1);
}else{
if(_4b=="true"){
return true;
}
if(_4b=="false"){
return false;
}
if(_4b=="null"||_4b=="None"){
return null;
}
_4e=_4b.split(".");
_4d=_4c.get(_4e[0]);
if(_2.isFunction(_4d)){
var _51=_4c.getThis&&_4c.getThis();
if(_4d.alters_data){
_4d="";
}else{
if(_51){
_4d=_4d.call(_51);
}else{
_4d="";
}
}
}
for(var i=1;i<_4e.length;i++){
var _52=_4e[i];
if(_4d){
var _53=_4d;
if(_2.isObject(_4d)&&_52=="items"&&_4d[_52]===undefined){
var _54=[];
for(var key in _4d){
_54.push([key,_4d[key]]);
}
_4d=_54;
continue;
}
if(_4d.get&&_2.isFunction(_4d.get)&&_4d.get.safe){
_4d=_4d.get(_52);
}else{
if(_4d[_52]===undefined){
_4d=_4d[_52];
break;
}else{
_4d=_4d[_52];
}
}
if(_2.isFunction(_4d)){
if(_4d.alters_data){
_4d="";
}else{
_4d=_4d.call(_53);
}
}else{
if(_4d instanceof Date){
_4d=dd._Context.prototype._normalize(_4d);
}
}
}else{
return "";
}
}
}
}
return _4d;
}});
dd._TextNode=dd._Node=_2.extend(function(obj){
this.contents=obj;
},{set:function(_55){
this.contents=_55;
return this;
},render:function(_56,_57){
return _57.concat(this.contents);
},isEmpty:function(){
return !_2.trim(this.contents);
},clone:function(){
return this;
}});
dd._NodeList=_2.extend(function(_58){
this.contents=_58||[];
this.last="";
},{push:function(_59){
this.contents.push(_59);
return this;
},concat:function(_5a){
this.contents=this.contents.concat(_5a);
return this;
},render:function(_5b,_5c){
for(var i=0;i<this.contents.length;i++){
_5c=this.contents[i].render(_5b,_5c);
if(!_5c){
throw new Error("Template must return buffer");
}
}
return _5c;
},dummyRender:function(_5d){
return this.render(_5d,dd.Template.prototype.getBuffer()).toString();
},unrender:function(){
return arguments[1];
},clone:function(){
return this;
},rtrim:function(){
while(1){
i=this.contents.length-1;
if(this.contents[i] instanceof dd._TextNode&&this.contents[i].isEmpty()){
this.contents.pop();
}else{
break;
}
}
return this;
}});
dd._VarNode=_2.extend(function(str){
this.contents=new dd._Filter(str);
},{render:function(_5e,_5f){
var str=this.contents.resolve(_5e);
if(!str.safe){
str=dd._base.escape(""+str);
}
return _5f.concat(str);
}});
dd._noOpNode=new function(){
this.render=this.unrender=function(){
return arguments[1];
};
this.clone=function(){
return this;
};
};
dd._Parser=_2.extend(function(_60){
this.contents=_60;
},{i:0,parse:function(_61){
var _62={},_63;
_61=_61||[];
for(var i=0;i<_61.length;i++){
_62[_61[i]]=true;
}
var _64=new dd._NodeList();
while(this.i<this.contents.length){
_63=this.contents[this.i++];
if(typeof _63=="string"){
_64.push(new dd._TextNode(_63));
}else{
var _65=_63[0];
var _66=_63[1];
if(_65==dd.TOKEN_VAR){
_64.push(new dd._VarNode(_66));
}else{
if(_65==dd.TOKEN_BLOCK){
if(_62[_66]){
--this.i;
return _64;
}
var cmd=_66.split(/\s+/g);
if(cmd.length){
cmd=cmd[0];
var fn=ddt.getTag(cmd);
if(fn){
_64.push(fn(this,new dd.Token(_65,_66)));
}
}
}
}
}
}
if(_61.length){
throw new Error("Could not find closing tag(s): "+_61.toString());
}
this.contents.length=0;
return _64;
},next_token:function(){
var _67=this.contents[this.i++];
return new dd.Token(_67[0],_67[1]);
},delete_first_token:function(){
this.i++;
},skip_past:function(_68){
while(this.i<this.contents.length){
var _69=this.contents[this.i++];
if(_69[0]==dd.TOKEN_BLOCK&&_69[1]==_68){
return;
}
}
throw new Error("Unclosed tag found when looking for "+_68);
},create_variable_node:function(_6a){
return new dd._VarNode(_6a);
},create_text_node:function(_6b){
return new dd._TextNode(_6b||"");
},getTemplate:function(_6c){
return new dd.Template(_6c);
}});
dd.register={_registry:{attributes:[],tags:[],filters:[]},get:function(_6d,_6e){
var _6f=dd.register._registry[_6d+"s"];
for(var i=0,_70;_70=_6f[i];i++){
if(typeof _70[0]=="string"){
if(_70[0]==_6e){
return _70;
}
}else{
if(_6e.match(_70[0])){
return _70;
}
}
}
},getAttributeTags:function(){
var _71=[];
var _72=dd.register._registry.attributes;
for(var i=0,_73;_73=_72[i];i++){
if(_73.length==3){
_71.push(_73);
}else{
var fn=_2.getObject(_73[1]);
if(fn&&_2.isFunction(fn)){
_73.push(fn);
_71.push(_73);
}
}
}
return _71;
},_any:function(_74,_75,_76){
for(var _77 in _76){
for(var i=0,fn;fn=_76[_77][i];i++){
var key=fn;
if(_2.isArray(fn)){
key=fn[0];
fn=fn[1];
}
if(typeof key=="string"){
if(key.substr(0,5)=="attr:"){
var _78=fn;
if(_78.substr(0,5)=="attr:"){
_78=_78.slice(5);
}
dd.register._registry.attributes.push([_78.toLowerCase(),_75+"."+_77+"."+_78]);
}
key=key.toLowerCase();
}
dd.register._registry[_74].push([key,fn,_75+"."+_77]);
}
}
},tags:function(_79,_7a){
dd.register._any("tags",_79,_7a);
},filters:function(_7b,_7c){
dd.register._any("filters",_7b,_7c);
}};
var _7d=/&/g;
var _7e=/</g;
var _7f=/>/g;
var _80=/'/g;
var _81=/"/g;
dd._base.escape=function(_82){
return dd.mark_safe(_82.replace(_7d,"&amp;").replace(_7e,"&lt;").replace(_7f,"&gt;").replace(_81,"&quot;").replace(_80,"&#39;"));
};
dd._base.safe=function(_83){
if(typeof _83=="string"){
_83=new String(_83);
}
if(typeof _83=="object"){
_83.safe=true;
}
return _83;
};
dd.mark_safe=dd._base.safe;
dd.register.tags("dojox.dtl.tag",{"date":["now"],"logic":["if","for","ifequal","ifnotequal"],"loader":["extends","block","include","load","ssi"],"misc":["comment","debug","filter","firstof","spaceless","templatetag","widthratio","with"],"loop":["cycle","ifchanged","regroup"]});
dd.register.filters("dojox.dtl.filter",{"dates":["date","time","timesince","timeuntil"],"htmlstrings":["linebreaks","linebreaksbr","removetags","striptags"],"integers":["add","get_digit"],"lists":["dictsort","dictsortreversed","first","join","length","length_is","random","slice","unordered_list"],"logic":["default","default_if_none","divisibleby","yesno"],"misc":["filesizeformat","pluralize","phone2numeric","pprint"],"strings":["addslashes","capfirst","center","cut","fix_ampersands","floatformat","iriencode","linenumbers","ljust","lower","make_list","rjust","slugify","stringformat","title","truncatewords","truncatewords_html","upper","urlencode","urlize","urlizetrunc","wordcount","wordwrap"]});
dd.register.filters("dojox.dtl",{"_base":["escape","safe"]});
return dd;
});
