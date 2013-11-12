//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/widget/Portlet,dijit/Tooltip,dijit/form/TextBox,dijit/form/Button,dojox/data/GoogleFeedStore"],function(_1,_2,_3){
_2.provide("dojox.widget.FeedPortlet");
_2.require("dojox.widget.Portlet");
_2.require("dijit.Tooltip");
_2.require("dijit.form.TextBox");
_2.require("dijit.form.Button");
_2.require("dojox.data.GoogleFeedStore");
_2.declare("dojox.widget.FeedPortlet",_3.widget.Portlet,{local:false,maxResults:5,url:"",openNew:true,showFeedTitle:true,postCreate:function(){
this.inherited(arguments);
if(this.local&&!_3.data.AtomReadStore){
throw Error(this.declaredClass+": To use local feeds, you must include dojox.data.AtomReadStore on the page.");
}
},onFeedError:function(){
this.containerNode.innerHTML="Error accessing the feed.";
},addChild:function(_4){
this.inherited(arguments);
var _5=_4.attr("feedPortletUrl");
if(_5){
this.set("url",_5);
}
},_getTitle:function(_6){
var t=this.store.getValue(_6,"title");
return this.local?t.text:t;
},_getLink:function(_7){
var l=this.store.getValue(_7,"link");
return this.local?l.href:l;
},_getContent:function(_8){
var c=this.store.getValue(_8,"summary");
if(!c){
return null;
}
if(this.local){
c=c.text;
}
c=c.split("<script").join("<!--").split("</script>").join("-->");
c=c.split("<iframe").join("<!--").split("</iframe>").join("-->");
return c;
},_setUrlAttr:function(_9){
this.url=_9;
if(this._started){
this.load();
}
},startup:function(){
if(this.started||this._started){
return;
}
this.inherited(arguments);
if(!this.url||this.url==""){
throw new Error(this.id+": A URL must be specified for the feed portlet");
}
if(this.url&&this.url!=""){
this.load();
}
},load:function(){
if(this._resultList){
_2.destroy(this._resultList);
}
var _a,_b;
if(this.local){
_a=new _3.data.AtomReadStore({url:this.url});
_b={};
}else{
_a=new _3.data.GoogleFeedStore();
_b={url:this.url};
}
var _c={query:_b,count:this.maxResults,onComplete:_2.hitch(this,function(_d){
if(this.showFeedTitle&&_a.getFeedValue){
var _e=this.store.getFeedValue("title");
if(_e){
this.set("title",_e.text?_e.text:_e);
}
}
this.generateResults(_d);
}),onError:_2.hitch(this,"onFeedError")};
this.store=_a;
_a.fetch(_c);
},generateResults:function(_f){
var _10=this.store;
var _11;
var ul=(this._resultList=_2.create("ul",{"class":"dojoxFeedPortletList"},this.containerNode));
_2.forEach(_f,_2.hitch(this,function(_12){
var li=_2.create("li",{innerHTML:"<a href=\""+this._getLink(_12)+"\""+(this.openNew?" target=\"_blank\"":"")+">"+this._getTitle(_12)+"</a>"},ul);
_2.connect(li,"onmouseover",_2.hitch(this,function(evt){
if(_11){
clearTimeout(_11);
}
_11=setTimeout(_2.hitch(this,function(){
_11=null;
var _13=this._getContent(_12);
if(!_13){
return;
}
var _14="<div class=\"dojoxFeedPortletPreview\">"+_13+"</div>";
_2.query("li",ul).forEach(function(_15){
if(_15!=evt.target){
_1.hideTooltip(_15);
}
});
_1.showTooltip(_14,li.firstChild,!this.isLeftToRight());
}),500);
}));
_2.connect(li,"onmouseout",function(){
if(_11){
clearTimeout(_11);
_11=null;
}
_1.hideTooltip(li.firstChild);
});
}));
this.resize();
}});
_2.declare("dojox.widget.ExpandableFeedPortlet",_3.widget.FeedPortlet,{onlyOpenOne:false,generateResults:function(_16){
var _17=this.store;
var _18="dojoxPortletToggleIcon";
var _19="dojoxPortletItemCollapsed";
var _1a="dojoxPortletItemOpen";
var _1b;
var ul=(this._resultList=_2.create("ul",{"class":"dojoxFeedPortletExpandableList"},this.containerNode));
_2.forEach(_16,_2.hitch(this,_2.hitch(this,function(_1c){
var li=_2.create("li",{"class":_19},ul);
var _1d=_2.create("div",{style:"width: 100%;"},li);
var _1e=_2.create("div",{"class":"dojoxPortletItemSummary",innerHTML:this._getContent(_1c)},li);
_2.create("span",{"class":_18,innerHTML:"<img src='"+_2.config.baseUrl+"/resources/blank.gif'>"},_1d);
var a=_2.create("a",{href:this._getLink(_1c),innerHTML:this._getTitle(_1c)},_1d);
if(this.openNew){
_2.attr(a,"target","_blank");
}
})));
_2.connect(ul,"onclick",_2.hitch(this,function(evt){
if(_2.hasClass(evt.target,_18)||_2.hasClass(evt.target.parentNode,_18)){
_2.stopEvent(evt);
var li=evt.target.parentNode;
while(li.tagName!="LI"){
li=li.parentNode;
}
if(this.onlyOpenOne){
_2.query("li",ul).filter(function(_1f){
return _1f!=li;
}).removeClass(_1a).addClass(_19);
}
var _20=_2.hasClass(li,_1a);
_2.toggleClass(li,_1a,!_20);
_2.toggleClass(li,_19,_20);
}
}));
}});
_2.declare("dojox.widget.PortletFeedSettings",_3.widget.PortletSettings,{"class":"dojoxPortletFeedSettings",urls:null,selectedIndex:0,buildRendering:function(){
var s;
if(this.urls&&this.urls.length>0){
s=_2.create("select");
if(this.srcNodeRef){
_2.place(s,this.srcNodeRef,"before");
_2.destroy(this.srcNodeRef);
}
this.srcNodeRef=s;
_2.forEach(this.urls,function(url){
_2.create("option",{value:url.url||url,innerHTML:url.label||url},s);
});
}
if(this.srcNodeRef.tagName=="SELECT"){
this.text=this.srcNodeRef;
var div=_2.create("div",{},this.srcNodeRef,"before");
div.appendChild(this.text);
this.srcNodeRef=div;
_2.query("option",this.text).filter("return !item.value;").forEach("item.value = item.innerHTML");
if(!this.text.value){
if(this.content&&this.text.options.length==0){
this.text.appendChild(this.content);
}
_2.attr(s||this.text,"value",this.text.options[this.selectedIndex].value);
}
}
this.inherited(arguments);
},_setContentAttr:function(){
},postCreate:function(){
if(!this.text){
var _21=this.text=new _1.form.TextBox({});
_2.create("span",{innerHTML:"Choose Url: "},this.domNode);
this.addChild(_21);
}
this.addChild(new _1.form.Button({label:"Load",onClick:_2.hitch(this,function(){
this.portlet.attr("url",(this.text.tagName=="SELECT")?this.text.value:this.text.attr("value"));
if(this.text.tagName=="SELECT"){
_2.some(this.text.options,_2.hitch(this,function(opt,idx){
if(opt.selected){
this.set("selectedIndex",idx);
return true;
}
return false;
}));
}
this.toggle();
})}));
this.addChild(new _1.form.Button({label:"Cancel",onClick:_2.hitch(this,"toggle")}));
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(!this.portlet){
throw Error(this.declaredClass+": A PortletFeedSettings widget cannot exist without a Portlet.");
}
if(this.text.tagName=="SELECT"){
_2.forEach(this.text.options,_2.hitch(this,function(opt,_22){
_2.attr(opt,"selected",_22==this.selectedIndex);
}));
}
var url=this.portlet.attr("url");
if(url){
if(this.text.tagName=="SELECT"){
if(!this.urls&&_2.query("option[value='"+url+"']",this.text).length<1){
_2.place(_2.create("option",{value:url,innerHTML:url,selected:"true"}),this.text,"first");
}
}else{
this.text.attr("value",url);
}
}else{
this.portlet.attr("url",this.get("feedPortletUrl"));
}
},_getFeedPortletUrlAttr:function(){
return this.text.value;
}});
});
