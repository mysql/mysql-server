//>>built
define("dojox/atom/io/model",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/date/stamp","dojox/xml/parser"],function(_1,_2,_3,_4,_5){
var _6={};
_1.setObject("dojox.atom.io.model",_6);
_6._Constants={"ATOM_URI":"http://www.w3.org/2005/Atom","ATOM_NS":"http://www.w3.org/2005/Atom","PURL_NS":"http://purl.org/atom/app#","APP_NS":"http://www.w3.org/2007/app"};
_6._actions={"link":function(_7,_8){
if(_7.links===null){
_7.links=[];
}
var _9=new _6.Link();
_9.buildFromDom(_8);
_7.links.push(_9);
},"author":function(_a,_b){
if(_a.authors===null){
_a.authors=[];
}
var _c=new _6.Person("author");
_c.buildFromDom(_b);
_a.authors.push(_c);
},"contributor":function(_d,_e){
if(_d.contributors===null){
_d.contributors=[];
}
var _f=new _6.Person("contributor");
_f.buildFromDom(_e);
_d.contributors.push(_f);
},"category":function(obj,_10){
if(obj.categories===null){
obj.categories=[];
}
var cat=new _6.Category();
cat.buildFromDom(_10);
obj.categories.push(cat);
},"icon":function(obj,_11){
obj.icon=_5.textContent(_11);
},"id":function(obj,_12){
obj.id=_5.textContent(_12);
},"rights":function(obj,_13){
obj.rights=_5.textContent(_13);
},"subtitle":function(obj,_14){
var cnt=new _6.Content("subtitle");
cnt.buildFromDom(_14);
obj.subtitle=cnt;
},"title":function(obj,_15){
var cnt=new _6.Content("title");
cnt.buildFromDom(_15);
obj.title=cnt;
},"updated":function(obj,_16){
obj.updated=_6.util.createDate(_16);
},"issued":function(obj,_17){
obj.issued=_6.util.createDate(_17);
},"modified":function(obj,_18){
obj.modified=_6.util.createDate(_18);
},"published":function(obj,_19){
obj.published=_6.util.createDate(_19);
},"entry":function(obj,_1a){
if(obj.entries===null){
obj.entries=[];
}
var _1b=obj.createEntry?obj.createEntry():new _6.Entry();
_1b.buildFromDom(_1a);
obj.entries.push(_1b);
},"content":function(obj,_1c){
var cnt=new _6.Content("content");
cnt.buildFromDom(_1c);
obj.content=cnt;
},"summary":function(obj,_1d){
var _1e=new _6.Content("summary");
_1e.buildFromDom(_1d);
obj.summary=_1e;
},"name":function(obj,_1f){
obj.name=_5.textContent(_1f);
},"email":function(obj,_20){
obj.email=_5.textContent(_20);
},"uri":function(obj,_21){
obj.uri=_5.textContent(_21);
},"generator":function(obj,_22){
obj.generator=new _6.Generator();
obj.generator.buildFromDom(_22);
}};
_6.util={createDate:function(_23){
var _24=_5.textContent(_23);
if(_24){
return _4.fromISOString(_3.trim(_24));
}
return null;
},escapeHtml:function(str){
return str.replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;").replace(/"/gm,"&quot;").replace(/'/gm,"&#39;");
},unEscapeHtml:function(str){
return str.replace(/&lt;/gm,"<").replace(/&gt;/gm,">").replace(/&quot;/gm,"\"").replace(/&#39;/gm,"'").replace(/&amp;/gm,"&");
},getNodename:function(_25){
var _26=null;
if(_25!==null){
_26=_25.localName?_25.localName:_25.nodeName;
if(_26!==null){
var _27=_26.indexOf(":");
if(_27!==-1){
_26=_26.substring((_27+1),_26.length);
}
}
}
return _26;
}};
_6.Node=_2(null,{constructor:function(_28,_29,_2a,_2b,_2c){
this.name_space=_28;
this.name=_29;
this.attributes=[];
if(_2a){
this.attributes=_2a;
}
this.content=[];
this.rawNodes=[];
this.textContent=null;
if(_2b){
this.content.push(_2b);
}
this.shortNs=_2c;
this._objName="Node";
this.nodeType="Node";
},buildFromDom:function(_2d){
this._saveAttributes(_2d);
this.name_space=_2d.namespaceURI;
this.shortNs=_2d.prefix;
this.name=_6.util.getNodename(_2d);
for(var x=0;x<_2d.childNodes.length;x++){
var c=_2d.childNodes[x];
if(_6.util.getNodename(c)!="#text"){
this.rawNodes.push(c);
var n=new _6.Node();
n.buildFromDom(c,true);
this.content.push(n);
}else{
this.content.push(c.nodeValue);
}
}
this.textContent=_5.textContent(_2d);
},_saveAttributes:function(_2e){
if(!this.attributes){
this.attributes=[];
}
var _2f=function(_30){
var _31=_30.attributes;
if(_31===null){
return false;
}
return (_31.length!==0);
};
if(_2f(_2e)&&this._getAttributeNames){
var _32=this._getAttributeNames(_2e);
if(_32&&_32.length>0){
for(var x in _32){
var _33=_2e.getAttribute(_32[x]);
if(_33){
this.attributes[_32[x]]=_33;
}
}
}
}
},addAttribute:function(_34,_35){
this.attributes[_34]=_35;
},getAttribute:function(_36){
return this.attributes[_36];
},_getAttributeNames:function(_37){
var _38=[];
for(var i=0;i<_37.attributes.length;i++){
_38.push(_37.attributes[i].nodeName);
}
return _38;
},toString:function(){
var xml=[];
var x;
var _39=(this.shortNs?this.shortNs+":":"")+this.name;
var _3a=(this.name=="#cdata-section");
if(_3a){
xml.push("<![CDATA[");
xml.push(this.textContent);
xml.push("]]>");
}else{
xml.push("<");
xml.push(_39);
if(this.name_space){
xml.push(" xmlns='"+this.name_space+"'");
}
if(this.attributes){
for(x in this.attributes){
xml.push(" "+x+"='"+this.attributes[x]+"'");
}
}
if(this.content){
xml.push(">");
for(x in this.content){
xml.push(this.content[x]);
}
xml.push("</"+_39+">\n");
}else{
xml.push("/>\n");
}
}
return xml.join("");
},addContent:function(_3b){
this.content.push(_3b);
}});
_6.AtomItem=_2(_6.Node,{constructor:function(_3c){
this.ATOM_URI=_6._Constants.ATOM_URI;
this.links=null;
this.authors=null;
this.categories=null;
this.contributors=null;
this.icon=this.id=this.logo=this.xmlBase=this.rights=null;
this.subtitle=this.title=null;
this.updated=this.published=null;
this.issued=this.modified=null;
this.content=null;
this.extensions=null;
this.entries=null;
this.name_spaces={};
this._objName="AtomItem";
this.nodeType="AtomItem";
},_getAttributeNames:function(){
return null;
},_accepts:{},accept:function(tag){
return Boolean(this._accepts[tag]);
},_postBuild:function(){
},buildFromDom:function(_3d){
var i,c,n;
for(i=0;i<_3d.attributes.length;i++){
c=_3d.attributes.item(i);
n=_6.util.getNodename(c);
if(c.prefix=="xmlns"&&c.prefix!=n){
this.addNamespace(c.nodeValue,n);
}
}
c=_3d.childNodes;
for(i=0;i<c.length;i++){
if(c[i].nodeType==1){
var _3e=_6.util.getNodename(c[i]);
if(!_3e){
continue;
}
if(c[i].namespaceURI!=_6._Constants.ATOM_NS&&_3e!="#text"){
if(!this.extensions){
this.extensions=[];
}
var _3f=new _6.Node();
_3f.buildFromDom(c[i]);
this.extensions.push(_3f);
}
if(!this.accept(_3e.toLowerCase())){
continue;
}
var fn=_6._actions[_3e];
if(fn){
fn(this,c[i]);
}
}
}
this._saveAttributes(_3d);
if(this._postBuild){
this._postBuild();
}
},addNamespace:function(_40,_41){
if(_40&&_41){
this.name_spaces[_41]=_40;
}
},addAuthor:function(_42,_43,uri){
if(!this.authors){
this.authors=[];
}
this.authors.push(new _6.Person("author",_42,_43,uri));
},addContributor:function(_44,_45,uri){
if(!this.contributors){
this.contributors=[];
}
this.contributors.push(new _6.Person("contributor",_44,_45,uri));
},addLink:function(_46,rel,_47,_48,_49){
if(!this.links){
this.links=[];
}
this.links.push(new _6.Link(_46,rel,_47,_48,_49));
},removeLink:function(_4a,rel){
if(!this.links||!_3.isArray(this.links)){
return;
}
var _4b=0;
for(var i=0;i<this.links.length;i++){
if((!_4a||this.links[i].href===_4a)&&(!rel||this.links[i].rel===rel)){
this.links.splice(i,1);
_4b++;
}
}
return _4b;
},removeBasicLinks:function(){
if(!this.links){
return;
}
var _4c=0;
for(var i=0;i<this.links.length;i++){
if(!this.links[i].rel){
this.links.splice(i,1);
_4c++;
i--;
}
}
return _4c;
},addCategory:function(_4d,_4e,_4f){
if(!this.categories){
this.categories=[];
}
this.categories.push(new _6.Category(_4d,_4e,_4f));
},getCategories:function(_50){
if(!_50){
return this.categories;
}
var arr=[];
for(var x in this.categories){
if(this.categories[x].scheme===_50){
arr.push(this.categories[x]);
}
}
return arr;
},removeCategories:function(_51,_52){
if(!this.categories){
return;
}
var _53=0;
for(var i=0;i<this.categories.length;i++){
if((!_51||this.categories[i].scheme===_51)&&(!_52||this.categories[i].term===_52)){
this.categories.splice(i,1);
_53++;
i--;
}
}
return _53;
},setTitle:function(str,_54){
if(!str){
return;
}
this.title=new _6.Content("title");
this.title.value=str;
if(_54){
this.title.type=_54;
}
},addExtension:function(_55,_56,_57,_58,_59){
if(!this.extensions){
this.extensions=[];
}
this.extensions.push(new _6.Node(_55,_56,_57,_58,_59||"ns"+this.extensions.length));
},getExtensions:function(_5a,_5b){
var arr=[];
if(!this.extensions){
return arr;
}
for(var x in this.extensions){
if((this.extensions[x].name_space===_5a||this.extensions[x].shortNs===_5a)&&(!_5b||this.extensions[x].name===_5b)){
arr.push(this.extensions[x]);
}
}
return arr;
},removeExtensions:function(_5c,_5d){
if(!this.extensions){
return;
}
for(var i=0;i<this.extensions.length;i++){
if((this.extensions[i].name_space==_5c||this.extensions[i].shortNs===_5c)&&this.extensions[i].name===_5d){
this.extensions.splice(i,1);
i--;
}
}
},destroy:function(){
this.links=null;
this.authors=null;
this.categories=null;
this.contributors=null;
this.icon=this.id=this.logo=this.xmlBase=this.rights=null;
this.subtitle=this.title=null;
this.updated=this.published=null;
this.issued=this.modified=null;
this.content=null;
this.extensions=null;
this.entries=null;
}});
_6.Category=_2(_6.Node,{constructor:function(_5e,_5f,_60){
this.scheme=_5e;
this.term=_5f;
this.label=_60;
this._objName="Category";
this.nodeType="Category";
},_postBuild:function(){
},_getAttributeNames:function(){
return ["label","scheme","term"];
},toString:function(){
var s=[];
s.push("<category ");
if(this.label){
s.push(" label=\""+this.label+"\" ");
}
if(this.scheme){
s.push(" scheme=\""+this.scheme+"\" ");
}
if(this.term){
s.push(" term=\""+this.term+"\" ");
}
s.push("/>\n");
return s.join("");
},buildFromDom:function(_61){
this._saveAttributes(_61);
this.label=this.attributes.label;
this.scheme=this.attributes.scheme;
this.term=this.attributes.term;
if(this._postBuild){
this._postBuild();
}
}});
_6.Content=_2(_6.Node,{constructor:function(_62,_63,src,_64,_65){
this.tagName=_62;
this.value=_63;
this.src=src;
this.type=_64;
this.xmlLang=_65;
this.HTML="html";
this.TEXT="text";
this.XHTML="xhtml";
this.XML="xml";
this._useTextContent="true";
this.nodeType="Content";
},_getAttributeNames:function(){
return ["type","src"];
},_postBuild:function(){
},buildFromDom:function(_66){
var _67=_66.getAttribute("type");
if(_67){
_67=_67.toLowerCase();
if(_67=="xml"||"text/xml"){
_67=this.XML;
}
}else{
_67="text";
}
if(_67===this.XML){
if(_66.firstChild){
var i;
this.value="";
for(i=0;i<_66.childNodes.length;i++){
var c=_66.childNodes[i];
if(c){
this.value+=_5.innerXML(c);
}
}
}
}else{
if(_66.innerHTML){
this.value=_66.innerHTML;
}else{
this.value=_5.textContent(_66);
}
}
this._saveAttributes(_66);
if(this.attributes){
this.type=this.attributes.type;
this.scheme=this.attributes.scheme;
this.term=this.attributes.term;
}
if(!this.type){
this.type="text";
}
var _68=this.type.toLowerCase();
if(_68==="html"||_68==="text/html"||_68==="xhtml"||_68==="text/xhtml"){
this.value=this.value?_6.util.unEscapeHtml(this.value):"";
}
if(this._postBuild){
this._postBuild();
}
},toString:function(){
var s=[];
s.push("<"+this.tagName+" ");
if(!this.type){
this.type="text";
}
if(this.type){
s.push(" type=\""+this.type+"\" ");
}
if(this.xmlLang){
s.push(" xml:lang=\""+this.xmlLang+"\" ");
}
if(this.xmlBase){
s.push(" xml:base=\""+this.xmlBase+"\" ");
}
if(this.type.toLowerCase()==this.HTML){
s.push(">"+_6.util.escapeHtml(this.value)+"</"+this.tagName+">\n");
}else{
s.push(">"+this.value+"</"+this.tagName+">\n");
}
var ret=s.join("");
return ret;
}});
_6.Link=_2(_6.Node,{constructor:function(_69,rel,_6a,_6b,_6c){
this.href=_69;
this.hrefLang=_6a;
this.rel=rel;
this.title=_6b;
this.type=_6c;
this.nodeType="Link";
},_getAttributeNames:function(){
return ["href","jrefLang","rel","title","type"];
},_postBuild:function(){
},buildFromDom:function(_6d){
this._saveAttributes(_6d);
this.href=this.attributes.href;
this.hrefLang=this.attributes.hreflang;
this.rel=this.attributes.rel;
this.title=this.attributes.title;
this.type=this.attributes.type;
if(this._postBuild){
this._postBuild();
}
},toString:function(){
var s=[];
s.push("<link ");
if(this.href){
s.push(" href=\""+this.href+"\" ");
}
if(this.hrefLang){
s.push(" hrefLang=\""+this.hrefLang+"\" ");
}
if(this.rel){
s.push(" rel=\""+this.rel+"\" ");
}
if(this.title){
s.push(" title=\""+this.title+"\" ");
}
if(this.type){
s.push(" type = \""+this.type+"\" ");
}
s.push("/>\n");
return s.join("");
}});
_6.Person=_2(_6.Node,{constructor:function(_6e,_6f,_70,uri){
this.author="author";
this.contributor="contributor";
if(!_6e){
_6e=this.author;
}
this.personType=_6e;
this.name=_6f||"";
this.email=_70||"";
this.uri=uri||"";
this._objName="Person";
this.nodeType="Person";
},_getAttributeNames:function(){
return null;
},_postBuild:function(){
},accept:function(tag){
return Boolean(this._accepts[tag]);
},buildFromDom:function(_71){
var c=_71.childNodes;
for(var i=0;i<c.length;i++){
var _72=_6.util.getNodename(c[i]);
if(!_72){
continue;
}
if(c[i].namespaceURI!=_6._Constants.ATOM_NS&&_72!="#text"){
if(!this.extensions){
this.extensions=[];
}
var _73=new _6.Node();
_73.buildFromDom(c[i]);
this.extensions.push(_73);
}
if(!this.accept(_72.toLowerCase())){
continue;
}
var fn=_6._actions[_72];
if(fn){
fn(this,c[i]);
}
}
this._saveAttributes(_71);
if(this._postBuild){
this._postBuild();
}
},_accepts:{"name":true,"uri":true,"email":true},toString:function(){
var s=[];
s.push("<"+this.personType+">\n");
if(this.name){
s.push("\t<name>"+this.name+"</name>\n");
}
if(this.email){
s.push("\t<email>"+this.email+"</email>\n");
}
if(this.uri){
s.push("\t<uri>"+this.uri+"</uri>\n");
}
s.push("</"+this.personType+">\n");
return s.join("");
}});
_6.Generator=_2(_6.Node,{constructor:function(uri,_74,_75){
this.uri=uri;
this.version=_74;
this.value=_75;
},_postBuild:function(){
},buildFromDom:function(_76){
this.value=_5.textContent(_76);
this._saveAttributes(_76);
this.uri=this.attributes.uri;
this.version=this.attributes.version;
if(this._postBuild){
this._postBuild();
}
},toString:function(){
var s=[];
s.push("<generator ");
if(this.uri){
s.push(" uri=\""+this.uri+"\" ");
}
if(this.version){
s.push(" version=\""+this.version+"\" ");
}
s.push(">"+this.value+"</generator>\n");
var ret=s.join("");
return ret;
}});
_6.Entry=_2(_6.AtomItem,{constructor:function(id){
this.id=id;
this._objName="Entry";
this.feedUrl=null;
},_getAttributeNames:function(){
return null;
},_accepts:{"author":true,"content":true,"category":true,"contributor":true,"created":true,"id":true,"link":true,"published":true,"rights":true,"summary":true,"title":true,"updated":true,"xmlbase":true,"issued":true,"modified":true},toString:function(_77){
var s=[];
var i;
if(_77){
s.push("<?xml version='1.0' encoding='UTF-8'?>");
s.push("<entry xmlns='"+_6._Constants.ATOM_URI+"'");
}else{
s.push("<entry");
}
if(this.xmlBase){
s.push(" xml:base=\""+this.xmlBase+"\" ");
}
for(i in this.name_spaces){
s.push(" xmlns:"+i+"=\""+this.name_spaces[i]+"\"");
}
s.push(">\n");
s.push("<id>"+(this.id?this.id:"")+"</id>\n");
if(this.issued&&!this.published){
this.published=this.issued;
}
if(this.published){
s.push("<published>"+_4.toISOString(this.published)+"</published>\n");
}
if(this.created){
s.push("<created>"+_4.toISOString(this.created)+"</created>\n");
}
if(this.issued){
s.push("<issued>"+_4.toISOString(this.issued)+"</issued>\n");
}
if(this.modified){
s.push("<modified>"+_4.toISOString(this.modified)+"</modified>\n");
}
if(this.modified&&!this.updated){
this.updated=this.modified;
}
if(this.updated){
s.push("<updated>"+_4.toISOString(this.updated)+"</updated>\n");
}
if(this.rights){
s.push("<rights>"+this.rights+"</rights>\n");
}
if(this.title){
s.push(this.title.toString());
}
if(this.summary){
s.push(this.summary.toString());
}
var _78=[this.authors,this.categories,this.links,this.contributors,this.extensions];
for(var x in _78){
if(_78[x]){
for(var y in _78[x]){
s.push(_78[x][y]);
}
}
}
if(this.content){
s.push(this.content.toString());
}
s.push("</entry>\n");
return s.join("");
},getEditHref:function(){
if(this.links===null||this.links.length===0){
return null;
}
for(var x in this.links){
if(this.links[x].rel&&this.links[x].rel=="edit"){
return this.links[x].href;
}
}
return null;
},setEditHref:function(url){
if(this.links===null){
this.links=[];
}
for(var x in this.links){
if(this.links[x].rel&&this.links[x].rel=="edit"){
this.links[x].href=url;
return;
}
}
this.addLink(url,"edit");
}});
_6.Feed=_2(_6.AtomItem,{_accepts:{"author":true,"content":true,"category":true,"contributor":true,"created":true,"id":true,"link":true,"published":true,"rights":true,"summary":true,"title":true,"updated":true,"xmlbase":true,"entry":true,"logo":true,"issued":true,"modified":true,"icon":true,"subtitle":true},addEntry:function(_79){
if(!_79.id){
throw new Error("The entry object must be assigned an ID attribute.");
}
if(!this.entries){
this.entries=[];
}
_79.feedUrl=this.getSelfHref();
this.entries.push(_79);
},getFirstEntry:function(){
if(!this.entries||this.entries.length===0){
return null;
}
return this.entries[0];
},getEntry:function(_7a){
if(!this.entries){
return null;
}
for(var x in this.entries){
if(this.entries[x].id==_7a){
return this.entries[x];
}
}
return null;
},removeEntry:function(_7b){
if(!this.entries){
return;
}
var _7c=0;
for(var i=0;i<this.entries.length;i++){
if(this.entries[i]===_7b){
this.entries.splice(i,1);
_7c++;
}
}
return _7c;
},setEntries:function(_7d){
for(var x in _7d){
this.addEntry(_7d[x]);
}
},toString:function(){
var s=[];
var i;
s.push("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
s.push("<feed xmlns=\""+_6._Constants.ATOM_URI+"\"");
if(this.xmlBase){
s.push(" xml:base=\""+this.xmlBase+"\"");
}
for(i in this.name_spaces){
s.push(" xmlns:"+i+"=\""+this.name_spaces[i]+"\"");
}
s.push(">\n");
s.push("<id>"+(this.id?this.id:"")+"</id>\n");
if(this.title){
s.push(this.title);
}
if(this.copyright&&!this.rights){
this.rights=this.copyright;
}
if(this.rights){
s.push("<rights>"+this.rights+"</rights>\n");
}
if(this.issued){
s.push("<issued>"+_4.toISOString(this.issued)+"</issued>\n");
}
if(this.modified){
s.push("<modified>"+_4.toISOString(this.modified)+"</modified>\n");
}
if(this.modified&&!this.updated){
this.updated=this.modified;
}
if(this.updated){
s.push("<updated>"+_4.toISOString(this.updated)+"</updated>\n");
}
if(this.published){
s.push("<published>"+_4.toISOString(this.published)+"</published>\n");
}
if(this.icon){
s.push("<icon>"+this.icon+"</icon>\n");
}
if(this.language){
s.push("<language>"+this.language+"</language>\n");
}
if(this.logo){
s.push("<logo>"+this.logo+"</logo>\n");
}
if(this.subtitle){
s.push(this.subtitle.toString());
}
if(this.tagline){
s.push(this.tagline.toString());
}
var _7e=[this.alternateLinks,this.authors,this.categories,this.contributors,this.otherLinks,this.extensions,this.entries];
for(i in _7e){
if(_7e[i]){
for(var x in _7e[i]){
s.push(_7e[i][x]);
}
}
}
s.push("</feed>");
return s.join("");
},createEntry:function(){
var _7f=new _6.Entry();
_7f.feedUrl=this.getSelfHref();
return _7f;
},getSelfHref:function(){
if(this.links===null||this.links.length===0){
return null;
}
for(var x in this.links){
if(this.links[x].rel&&this.links[x].rel=="self"){
return this.links[x].href;
}
}
return null;
}});
_6.Service=_2(_6.AtomItem,{constructor:function(_80){
this.href=_80;
},buildFromDom:function(_81){
var i;
this.workspaces=[];
if(_81.tagName!="service"){
return;
}
if(_81.namespaceURI!=_6._Constants.PURL_NS&&_81.namespaceURI!=_6._Constants.APP_NS){
return;
}
var ns=_81.namespaceURI;
this.name_space=_81.namespaceURI;
var _82;
if(typeof (_81.getElementsByTagNameNS)!="undefined"){
_82=_81.getElementsByTagNameNS(ns,"workspace");
}else{
_82=[];
var _83=_81.getElementsByTagName("workspace");
for(i=0;i<_83.length;i++){
if(_83[i].namespaceURI==ns){
_82.push(_83[i]);
}
}
}
if(_82&&_82.length>0){
var _84=0;
var _85;
for(i=0;i<_82.length;i++){
_85=(typeof (_82.item)==="undefined"?_82[i]:_82.item(i));
var _86=new _6.Workspace();
_86.buildFromDom(_85);
this.workspaces[_84++]=_86;
}
}
},getCollection:function(url){
for(var i=0;i<this.workspaces.length;i++){
var _87=this.workspaces[i].collections;
for(var j=0;j<_87.length;j++){
if(_87[j].href==url){
return _87;
}
}
}
return null;
}});
_6.Workspace=_2(_6.AtomItem,{constructor:function(_88){
this.title=_88;
this.collections=[];
},buildFromDom:function(_89){
var _8a=_6.util.getNodename(_89);
if(_8a!="workspace"){
return;
}
var c=_89.childNodes;
var len=0;
for(var i=0;i<c.length;i++){
var _8b=c[i];
if(_8b.nodeType===1){
_8a=_6.util.getNodename(_8b);
if(_8b.namespaceURI==_6._Constants.PURL_NS||_8b.namespaceURI==_6._Constants.APP_NS){
if(_8a==="collection"){
var _8c=new _6.Collection();
_8c.buildFromDom(_8b);
this.collections[len++]=_8c;
}
}else{
if(_8b.namespaceURI===_6._Constants.ATOM_NS){
if(_8a==="title"){
this.title=_5.textContent(_8b);
}
}
}
}
}
}});
_6.Collection=_2(_6.AtomItem,{constructor:function(_8d,_8e){
this.href=_8d;
this.title=_8e;
this.attributes=[];
this.features=[];
this.children=[];
this.memberType=null;
this.id=null;
},buildFromDom:function(_8f){
this.href=_8f.getAttribute("href");
var c=_8f.childNodes;
for(var i=0;i<c.length;i++){
var _90=c[i];
if(_90.nodeType===1){
var _91=_6.util.getNodename(_90);
if(_90.namespaceURI==_6._Constants.PURL_NS||_90.namespaceURI==_6._Constants.APP_NS){
if(_91==="member-type"){
this.memberType=_5.textContent(_90);
}else{
if(_91=="feature"){
if(_90.getAttribute("id")){
this.features.push(_90.getAttribute("id"));
}
}else{
var _92=new _6.Node();
_92.buildFromDom(_90);
this.children.push(_92);
}
}
}else{
if(_90.namespaceURI===_6._Constants.ATOM_NS){
if(_91==="id"){
this.id=_5.textContent(_90);
}else{
if(_91==="title"){
this.title=_5.textContent(_90);
}
}
}
}
}
}
}});
return _6;
});
