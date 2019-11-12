//>>built
require({cache:{"url:dojox/atom/widget/templates/FeedViewer.html":"<div class=\"feedViewerContainer\" dojoAttachPoint=\"feedViewerContainerNode\">\n\t<table cellspacing=\"0\" cellpadding=\"0\" class=\"feedViewerTable\">\n\t\t<tbody dojoAttachPoint=\"feedViewerTableBody\" class=\"feedViewerTableBody\">\n\t\t</tbody>\n\t</table>\n</div>\n","url:dojox/atom/widget/templates/FeedViewerEntry.html":"<tr class=\"feedViewerEntry\" dojoAttachPoint=\"entryNode\" dojoAttachEvent=\"onclick:onClick\">\n    <td class=\"feedViewerEntryUpdated\" dojoAttachPoint=\"timeNode\">\n    </td>\n    <td>\n        <table border=\"0\" width=\"100%\" dojoAttachPoint=\"titleRow\">\n            <tr padding=\"0\" border=\"0\">\n                <td class=\"feedViewerEntryTitle\" dojoAttachPoint=\"titleNode\">\n                </td>\n                <td class=\"feedViewerEntryDelete\" align=\"right\">\n                    <span dojoAttachPoint=\"deleteButton\" dojoAttachEvent=\"onclick:deleteEntry\" class=\"feedViewerDeleteButton\" style=\"display:none;\">[delete]</span>\n                </td>\n            <tr>\n        </table>\n    </td>\n</tr>","url:dojox/atom/widget/templates/FeedViewerGrouping.html":"<tr dojoAttachPoint=\"groupingNode\" class=\"feedViewerGrouping\">\n\t<td colspan=\"2\" dojoAttachPoint=\"titleNode\" class=\"feedViewerGroupingTitle\">\n\t</td>\n</tr>"}});
define("dojox/atom/widget/FeedViewer",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/dom-class","dijit/_Widget","dijit/_Templated","dijit/_Container","../io/Connection","dojo/text!./templates/FeedViewer.html","dojo/text!./templates/FeedViewerEntry.html","dojo/text!./templates/FeedViewerGrouping.html","dojo/i18n!./nls/FeedViewerEntry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
_1.experimental("dojox.atom.widget.FeedViewer");
var _f=_5("dojox.atom.widget.FeedViewer",[_7,_8,_9],{feedViewerTableBody:null,feedViewerTable:null,entrySelectionTopic:"",url:"",xmethod:false,localSaveOnly:false,templateString:_b,_feed:null,_currentSelection:null,_includeFilters:null,alertsEnabled:false,postCreate:function(){
this._includeFilters=[];
if(this.entrySelectionTopic!==""){
this._subscriptions=[_1.subscribe(this.entrySelectionTopic,this,"_handleEvent")];
}
this.atomIO=new _a();
this.childWidgets=[];
},startup:function(){
this.containerNode=this.feedViewerTableBody;
var _10=this.getDescendants();
for(var i in _10){
var _11=_10[i];
if(_11&&_11.isFilter){
this._includeFilters.push(new _f.CategoryIncludeFilter(_11.scheme,_11.term,_11.label));
_11.destroy();
}
}
if(this.url!==""){
this.setFeedFromUrl(this.url);
}
},clear:function(){
this.destroyDescendants();
},setFeedFromUrl:function(url){
if(url!==""){
if(this._isRelativeURL(url)){
var _12="";
if(url.charAt(0)!=="/"){
_12=this._calculateBaseURL(window.location.href,true);
}else{
_12=this._calculateBaseURL(window.location.href,false);
}
this.url=_12+url;
}
this.atomIO.getFeed(url,_2.hitch(this,this.setFeed));
}
},setFeed:function(_13){
this._feed=_13;
this.clear();
var _14=function(a,b){
var _15=this._displayDateForEntry(a);
var _16=this._displayDateForEntry(b);
if(_15>_16){
return -1;
}
if(_15<_16){
return 1;
}
return 0;
};
var _17=function(_18){
var _19=_18.split(",");
_19.pop();
return _19.join(",");
};
var _1a=_13.entries.sort(_2.hitch(this,_14));
if(_13){
var _1b=null;
for(var i=0;i<_1a.length;i++){
var _1c=_1a[i];
if(this._isFilterAccepted(_1c)){
var _1d=this._displayDateForEntry(_1c);
var _1e="";
if(_1d!==null){
_1e=_17(_1d.toLocaleString());
if(_1e===""){
_1e=""+(_1d.getMonth()+1)+"/"+_1d.getDate()+"/"+_1d.getFullYear();
}
}
if((_1b===null)||(_1b!=_1e)){
this.appendGrouping(_1e);
_1b=_1e;
}
this.appendEntry(_1c);
}
}
}
},_displayDateForEntry:function(_1f){
if(_1f.updated){
return _1f.updated;
}
if(_1f.modified){
return _1f.modified;
}
if(_1f.issued){
return _1f.issued;
}
return new Date();
},appendGrouping:function(_20){
var _21=new _22({});
_21.setText(_20);
this.addChild(_21);
this.childWidgets.push(_21);
},appendEntry:function(_23){
var _24=new _25({"xmethod":this.xmethod});
_24.setTitle(_23.title.value);
_24.setTime(this._displayDateForEntry(_23).toLocaleTimeString());
_24.entrySelectionTopic=this.entrySelectionTopic;
_24.feed=this;
this.addChild(_24);
this.childWidgets.push(_24);
this.connect(_24,"onClick","_rowSelected");
_23.domNode=_24.entryNode;
_23._entryWidget=_24;
_24.entry=_23;
},deleteEntry:function(_26){
if(!this.localSaveOnly){
this.atomIO.deleteEntry(_26.entry,_2.hitch(this,this._removeEntry,_26),null,this.xmethod);
}else{
this._removeEntry(_26,true);
}
_1.publish(this.entrySelectionTopic,[{action:"delete",source:this,entry:_26.entry}]);
},_removeEntry:function(_27,_28){
if(_28){
var idx=_3.indexOf(this.childWidgets,_27);
var _29=this.childWidgets[idx-1];
var _2a=this.childWidgets[idx+1];
if(_29.isInstanceOf(widget.FeedViewerGrouping)&&(_2a===undefined||_2a.isInstanceOf(widget.FeedViewerGrouping))){
_29.destroy();
}
_27.destroy();
}else{
}
},_rowSelected:function(evt){
var _2b=evt.target;
while(_2b){
if(_6.contains(_2b,"feedViewerEntry")){
break;
}
_2b=_2b.parentNode;
}
for(var i=0;i<this._feed.entries.length;i++){
var _2c=this._feed.entries[i];
if((_2b===_2c.domNode)&&(this._currentSelection!==_2c)){
_6.add(_2c.domNode,"feedViewerEntrySelected");
_6.remove(_2c._entryWidget.timeNode,"feedViewerEntryUpdated");
_6.add(_2c._entryWidget.timeNode,"feedViewerEntryUpdatedSelected");
this.onEntrySelected(_2c);
if(this.entrySelectionTopic!==""){
_1.publish(this.entrySelectionTopic,[{action:"set",source:this,feed:this._feed,entry:_2c}]);
}
if(this._isEditable(_2c)){
_2c._entryWidget.enableDelete();
}
this._deselectCurrentSelection();
this._currentSelection=_2c;
break;
}else{
if((_2b===_2c.domNode)&&(this._currentSelection===_2c)){
_1.publish(this.entrySelectionTopic,[{action:"delete",source:this,entry:_2c}]);
this._deselectCurrentSelection();
break;
}
}
}
},_deselectCurrentSelection:function(){
if(this._currentSelection){
_6.add(this._currentSelection._entryWidget.timeNode,"feedViewerEntryUpdated");
_6.remove(this._currentSelection.domNode,"feedViewerEntrySelected");
_6.remove(this._currentSelection._entryWidget.timeNode,"feedViewerEntryUpdatedSelected");
this._currentSelection._entryWidget.disableDelete();
this._currentSelection=null;
}
},_isEditable:function(_2d){
var _2e=false;
if(_2d&&_2d!==null&&_2d.links&&_2d.links!==null){
for(var x in _2d.links){
if(_2d.links[x].rel&&_2d.links[x].rel=="edit"){
_2e=true;
break;
}
}
}
return _2e;
},onEntrySelected:function(_2f){
},_isRelativeURL:function(url){
var _30=function(url){
var _31=false;
if(url.indexOf("file://")===0){
_31=true;
}
return _31;
};
var _32=function(url){
var _33=false;
if(url.indexOf("http://")===0){
_33=true;
}
return _33;
};
var _34=false;
if(url!==null){
if(!_30(url)&&!_32(url)){
_34=true;
}
}
return _34;
},_calculateBaseURL:function(_35,_36){
var _37=null;
if(_35!==null){
var _38=_35.indexOf("?");
if(_38!=-1){
_35=_35.substring(0,_38);
}
if(_36){
_38=_35.lastIndexOf("/");
if((_38>0)&&(_38<_35.length)&&(_38!==(_35.length-1))){
_37=_35.substring(0,(_38+1));
}else{
_37=_35;
}
}else{
_38=_35.indexOf("://");
if(_38>0){
_38=_38+3;
var _39=_35.substring(0,_38);
var _3a=_35.substring(_38,_35.length);
_38=_3a.indexOf("/");
if((_38<_3a.length)&&(_38>0)){
_37=_39+_3a.substring(0,_38);
}else{
_37=_39+_3a;
}
}
}
}
return _37;
},_isFilterAccepted:function(_3b){
var _3c=false;
if(this._includeFilters&&(this._includeFilters.length>0)){
for(var i=0;i<this._includeFilters.length;i++){
var _3d=this._includeFilters[i];
if(_3d.match(_3b)){
_3c=true;
break;
}
}
}else{
_3c=true;
}
return _3c;
},addCategoryIncludeFilter:function(_3e){
if(_3e){
var _3f=_3e.scheme;
var _40=_3e.term;
var _41=_3e.label;
var _42=true;
if(!_3f){
_3f=null;
}
if(!_40){
_3f=null;
}
if(!_41){
_3f=null;
}
if(this._includeFilters&&this._includeFilters.length>0){
for(var i=0;i<this._includeFilters.length;i++){
var _43=this._includeFilters[i];
if((_43.term===_40)&&(_43.scheme===_3f)&&(_43.label===_41)){
_42=false;
break;
}
}
}
if(_42){
this._includeFilters.push(widget.FeedViewer.CategoryIncludeFilter(_3f,_40,_41));
}
}
},removeCategoryIncludeFilter:function(_44){
if(_44){
var _45=_44.scheme;
var _46=_44.term;
var _47=_44.label;
if(!_45){
_45=null;
}
if(!_46){
_45=null;
}
if(!_47){
_45=null;
}
var _48=[];
if(this._includeFilters&&this._includeFilters.length>0){
for(var i=0;i<this._includeFilters.length;i++){
var _49=this._includeFilters[i];
if(!((_49.term===_46)&&(_49.scheme===_45)&&(_49.label===_47))){
_48.push(_49);
}
}
this._includeFilters=_48;
}
}
},_handleEvent:function(_4a){
if(_4a.source!=this){
if(_4a.action=="update"&&_4a.entry){
var evt=_4a;
if(!this.localSaveOnly){
this.atomIO.updateEntry(evt.entry,_2.hitch(evt.source,evt.callback),null,true);
}
this._currentSelection._entryWidget.setTime(this._displayDateForEntry(evt.entry).toLocaleTimeString());
this._currentSelection._entryWidget.setTitle(evt.entry.title.value);
}else{
if(_4a.action=="post"&&_4a.entry){
if(!this.localSaveOnly){
this.atomIO.addEntry(_4a.entry,this.url,_2.hitch(this,this._addEntry));
}else{
this._addEntry(_4a.entry);
}
}
}
}
},_addEntry:function(_4b){
this._feed.addEntry(_4b);
this.setFeed(this._feed);
_1.publish(this.entrySelectionTopic,[{action:"set",source:this,feed:this._feed,entry:_4b}]);
},destroy:function(){
this.clear();
_3.forEach(this._subscriptions,_1.unsubscribe);
}});
var _25=_f.FeedViewerEntry=_5("dojox.atom.widget.FeedViewerEntry",[_7,_8],{templateString:_c,entryNode:null,timeNode:null,deleteButton:null,entry:null,feed:null,postCreate:function(){
var _4c=_e;
this.deleteButton.innerHTML=_4c.deleteButton;
},setTitle:function(_4d){
if(this.titleNode.lastChild){
this.titleNode.removeChild(this.titleNode.lastChild);
}
var _4e=document.createElement("div");
_4e.innerHTML=_4d;
this.titleNode.appendChild(_4e);
},setTime:function(_4f){
if(this.timeNode.lastChild){
this.timeNode.removeChild(this.timeNode.lastChild);
}
var _50=document.createTextNode(_4f);
this.timeNode.appendChild(_50);
},enableDelete:function(){
if(this.deleteButton!==null){
this.deleteButton.style.display="inline";
}
},disableDelete:function(){
if(this.deleteButton!==null){
this.deleteButton.style.display="none";
}
},deleteEntry:function(_51){
_51.preventDefault();
_51.stopPropagation();
this.feed.deleteEntry(this);
},onClick:function(e){
}});
var _22=_f.FeedViewerGrouping=_5("dojox.atom.widget.FeedViewerGrouping",[_7,_8],{templateString:_d,groupingNode:null,titleNode:null,setText:function(_52){
if(this.titleNode.lastChild){
this.titleNode.removeChild(this.titleNode.lastChild);
}
var _53=document.createTextNode(_52);
this.titleNode.appendChild(_53);
}});
_f.AtomEntryCategoryFilter=_5("dojox.atom.widget.AtomEntryCategoryFilter",null,{scheme:"",term:"",label:"",isFilter:true});
_f.CategoryIncludeFilter=_5("dojox.atom.widget.FeedViewer.CategoryIncludeFilter",null,{constructor:function(_54,_55,_56){
this.scheme=_54;
this.term=_55;
this.label=_56;
},match:function(_57){
var _58=false;
if(_57!==null){
var _59=_57.categories;
if(_59!==null){
for(var i=0;i<_59.length;i++){
var _5a=_59[i];
if(this.scheme!==""){
if(this.scheme!==_5a.scheme){
break;
}
}
if(this.term!==""){
if(this.term!==_5a.term){
break;
}
}
if(this.label!==""){
if(this.label!==_5a.label){
break;
}
}
_58=true;
}
}
}
return _58;
}});
return _f;
});
