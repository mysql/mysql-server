//>>built
require({cache:{"url:dojox/atom/widget/templates/FeedViewer.html":"<div class=\"feedViewerContainer\" dojoAttachPoint=\"feedViewerContainerNode\">\n\t<table cellspacing=\"0\" cellpadding=\"0\" class=\"feedViewerTable\">\n\t\t<tbody dojoAttachPoint=\"feedViewerTableBody\" class=\"feedViewerTableBody\">\n\t\t</tbody>\n\t</table>\n</div>\n","url:dojox/atom/widget/templates/FeedViewerEntry.html":"<tr class=\"feedViewerEntry\" dojoAttachPoint=\"entryNode\" dojoAttachEvent=\"onclick:onClick\">\n    <td class=\"feedViewerEntryUpdated\" dojoAttachPoint=\"timeNode\">\n    </td>\n    <td>\n        <table border=\"0\" width=\"100%\" dojoAttachPoint=\"titleRow\">\n            <tr padding=\"0\" border=\"0\">\n                <td class=\"feedViewerEntryTitle\" dojoAttachPoint=\"titleNode\">\n                </td>\n                <td class=\"feedViewerEntryDelete\" align=\"right\">\n                    <span dojoAttachPoint=\"deleteButton\" dojoAttachEvent=\"onclick:deleteEntry\" class=\"feedViewerDeleteButton\" style=\"display:none;\">[delete]</span>\n                </td>\n            <tr>\n        </table>\n    </td>\n</tr>","url:dojox/atom/widget/templates/FeedViewerGrouping.html":"<tr dojoAttachPoint=\"groupingNode\" class=\"feedViewerGrouping\">\n\t<td colspan=\"2\" dojoAttachPoint=\"titleNode\" class=\"feedViewerGroupingTitle\">\n\t</td>\n</tr>"}});
define("dojox/atom/widget/FeedViewer",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/connect","dojo/dom-class","dijit/_Widget","dijit/_Templated","dijit/_Container","../io/Connection","dojo/text!./templates/FeedViewer.html","dojo/text!./templates/FeedViewerEntry.html","dojo/text!./templates/FeedViewerGrouping.html","dojo/i18n!./nls/FeedViewerEntry","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
_1.experimental("dojox.atom.widget.FeedViewer");
var _e=_1.getObject("dojox.atom.widget",true);
_e.FeedViewer=_1.declare([_6,_7,_8],{feedViewerTableBody:null,feedViewerTable:null,entrySelectionTopic:"",url:"",xmethod:false,localSaveOnly:false,templateString:_a,_feed:null,_currentSelection:null,_includeFilters:null,alertsEnabled:false,postCreate:function(){
this._includeFilters=[];
if(this.entrySelectionTopic!==""){
this._subscriptions=[_1.subscribe(this.entrySelectionTopic,this,"_handleEvent")];
}
this.atomIO=new _9();
this.childWidgets=[];
},startup:function(){
this.containerNode=this.feedViewerTableBody;
var _f=this.getDescendants();
for(var i in _f){
var _10=_f[i];
if(_10&&_10.isFilter){
this._includeFilters.push(new _e.FeedViewer.CategoryIncludeFilter(_10.scheme,_10.term,_10.label));
_10.destroy();
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
var _11="";
if(url.charAt(0)!=="/"){
_11=this._calculateBaseURL(window.location.href,true);
}else{
_11=this._calculateBaseURL(window.location.href,false);
}
this.url=_11+url;
}
this.atomIO.getFeed(url,_2.hitch(this,this.setFeed));
}
},setFeed:function(_12){
this._feed=_12;
this.clear();
var _13=function(a,b){
var _14=this._displayDateForEntry(a);
var _15=this._displayDateForEntry(b);
if(_14>_15){
return -1;
}
if(_14<_15){
return 1;
}
return 0;
};
var _16=function(_17){
var _18=_17.split(",");
_18.pop();
return _18.join(",");
};
var _19=_12.entries.sort(_2.hitch(this,_13));
if(_12){
var _1a=null;
for(var i=0;i<_19.length;i++){
var _1b=_19[i];
if(this._isFilterAccepted(_1b)){
var _1c=this._displayDateForEntry(_1b);
var _1d="";
if(_1c!==null){
_1d=_16(_1c.toLocaleString());
if(_1d===""){
_1d=""+(_1c.getMonth()+1)+"/"+_1c.getDate()+"/"+_1c.getFullYear();
}
}
if((_1a===null)||(_1a!=_1d)){
this.appendGrouping(_1d);
_1a=_1d;
}
this.appendEntry(_1b);
}
}
}
},_displayDateForEntry:function(_1e){
if(_1e.updated){
return _1e.updated;
}
if(_1e.modified){
return _1e.modified;
}
if(_1e.issued){
return _1e.issued;
}
return new Date();
},appendGrouping:function(_1f){
var _20=new _e.FeedViewerGrouping({});
_20.setText(_1f);
this.addChild(_20);
this.childWidgets.push(_20);
},appendEntry:function(_21){
var _22=new _e.FeedViewerEntry({"xmethod":this.xmethod});
_22.setTitle(_21.title.value);
_22.setTime(this._displayDateForEntry(_21).toLocaleTimeString());
_22.entrySelectionTopic=this.entrySelectionTopic;
_22.feed=this;
this.addChild(_22);
this.childWidgets.push(_22);
this.connect(_22,"onClick","_rowSelected");
_21.domNode=_22.entryNode;
_21._entryWidget=_22;
_22.entry=_21;
},deleteEntry:function(_23){
if(!this.localSaveOnly){
this.atomIO.deleteEntry(_23.entry,_2.hitch(this,this._removeEntry,_23),null,this.xmethod);
}else{
this._removeEntry(_23,true);
}
_1.publish(this.entrySelectionTopic,[{action:"delete",source:this,entry:_23.entry}]);
},_removeEntry:function(_24,_25){
if(_25){
var idx=_3.indexOf(this.childWidgets,_24);
var _26=this.childWidgets[idx-1];
var _27=this.childWidgets[idx+1];
if(_26.isInstanceOf(_e.FeedViewerGrouping)&&(_27===undefined||_27.isInstanceOf(_e.FeedViewerGrouping))){
_26.destroy();
}
_24.destroy();
}else{
}
},_rowSelected:function(evt){
var _28=evt.target;
while(_28){
if(_5.contains(_28,"feedViewerEntry")){
break;
}
_28=_28.parentNode;
}
for(var i=0;i<this._feed.entries.length;i++){
var _29=this._feed.entries[i];
if((_28===_29.domNode)&&(this._currentSelection!==_29)){
_5.add(_29.domNode,"feedViewerEntrySelected");
_5.remove(_29._entryWidget.timeNode,"feedViewerEntryUpdated");
_5.add(_29._entryWidget.timeNode,"feedViewerEntryUpdatedSelected");
this.onEntrySelected(_29);
if(this.entrySelectionTopic!==""){
_1.publish(this.entrySelectionTopic,[{action:"set",source:this,feed:this._feed,entry:_29}]);
}
if(this._isEditable(_29)){
_29._entryWidget.enableDelete();
}
this._deselectCurrentSelection();
this._currentSelection=_29;
break;
}else{
if((_28===_29.domNode)&&(this._currentSelection===_29)){
_1.publish(this.entrySelectionTopic,[{action:"delete",source:this,entry:_29}]);
this._deselectCurrentSelection();
break;
}
}
}
},_deselectCurrentSelection:function(){
if(this._currentSelection){
_5.add(this._currentSelection._entryWidget.timeNode,"feedViewerEntryUpdated");
_5.remove(this._currentSelection.domNode,"feedViewerEntrySelected");
_5.remove(this._currentSelection._entryWidget.timeNode,"feedViewerEntryUpdatedSelected");
this._currentSelection._entryWidget.disableDelete();
this._currentSelection=null;
}
},_isEditable:function(_2a){
var _2b=false;
if(_2a&&_2a!==null&&_2a.links&&_2a.links!==null){
for(var x in _2a.links){
if(_2a.links[x].rel&&_2a.links[x].rel=="edit"){
_2b=true;
break;
}
}
}
return _2b;
},onEntrySelected:function(_2c){
},_isRelativeURL:function(url){
var _2d=function(url){
var _2e=false;
if(url.indexOf("file://")===0){
_2e=true;
}
return _2e;
};
var _2f=function(url){
var _30=false;
if(url.indexOf("http://")===0){
_30=true;
}
return _30;
};
var _31=false;
if(url!==null){
if(!_2d(url)&&!_2f(url)){
_31=true;
}
}
return _31;
},_calculateBaseURL:function(_32,_33){
var _34=null;
if(_32!==null){
var _35=_32.indexOf("?");
if(_35!=-1){
_32=_32.substring(0,_35);
}
if(_33){
_35=_32.lastIndexOf("/");
if((_35>0)&&(_35<_32.length)&&(_35!==(_32.length-1))){
_34=_32.substring(0,(_35+1));
}else{
_34=_32;
}
}else{
_35=_32.indexOf("://");
if(_35>0){
_35=_35+3;
var _36=_32.substring(0,_35);
var _37=_32.substring(_35,_32.length);
_35=_37.indexOf("/");
if((_35<_37.length)&&(_35>0)){
_34=_36+_37.substring(0,_35);
}else{
_34=_36+_37;
}
}
}
}
return _34;
},_isFilterAccepted:function(_38){
var _39=false;
if(this._includeFilters&&(this._includeFilters.length>0)){
for(var i=0;i<this._includeFilters.length;i++){
var _3a=this._includeFilters[i];
if(_3a.match(_38)){
_39=true;
break;
}
}
}else{
_39=true;
}
return _39;
},addCategoryIncludeFilter:function(_3b){
if(_3b){
var _3c=_3b.scheme;
var _3d=_3b.term;
var _3e=_3b.label;
var _3f=true;
if(!_3c){
_3c=null;
}
if(!_3d){
_3c=null;
}
if(!_3e){
_3c=null;
}
if(this._includeFilters&&this._includeFilters.length>0){
for(var i=0;i<this._includeFilters.length;i++){
var _40=this._includeFilters[i];
if((_40.term===_3d)&&(_40.scheme===_3c)&&(_40.label===_3e)){
_3f=false;
break;
}
}
}
if(_3f){
this._includeFilters.push(_e.FeedViewer.CategoryIncludeFilter(_3c,_3d,_3e));
}
}
},removeCategoryIncludeFilter:function(_41){
if(_41){
var _42=_41.scheme;
var _43=_41.term;
var _44=_41.label;
if(!_42){
_42=null;
}
if(!_43){
_42=null;
}
if(!_44){
_42=null;
}
var _45=[];
if(this._includeFilters&&this._includeFilters.length>0){
for(var i=0;i<this._includeFilters.length;i++){
var _46=this._includeFilters[i];
if(!((_46.term===_43)&&(_46.scheme===_42)&&(_46.label===_44))){
_45.push(_46);
}
}
this._includeFilters=_45;
}
}
},_handleEvent:function(_47){
if(_47.source!=this){
if(_47.action=="update"&&_47.entry){
var evt=_47;
if(!this.localSaveOnly){
this.atomIO.updateEntry(evt.entry,_2.hitch(evt.source,evt.callback),null,true);
}
this._currentSelection._entryWidget.setTime(this._displayDateForEntry(evt.entry).toLocaleTimeString());
this._currentSelection._entryWidget.setTitle(evt.entry.title.value);
}else{
if(_47.action=="post"&&_47.entry){
if(!this.localSaveOnly){
this.atomIO.addEntry(_47.entry,this.url,_2.hitch(this,this._addEntry));
}else{
this._addEntry(_47.entry);
}
}
}
}
},_addEntry:function(_48){
this._feed.addEntry(_48);
this.setFeed(this._feed);
_1.publish(this.entrySelectionTopic,[{action:"set",source:this,feed:this._feed,entry:_48}]);
},destroy:function(){
this.clear();
_3.forEach(this._subscriptions,_1.unsubscribe);
}});
_e.FeedViewerEntry=_1.declare([_6,_7],{templateString:_b,entryNode:null,timeNode:null,deleteButton:null,entry:null,feed:null,postCreate:function(){
var _49=_d;
this.deleteButton.innerHTML=_49.deleteButton;
},setTitle:function(_4a){
if(this.titleNode.lastChild){
this.titleNode.removeChild(this.titleNode.lastChild);
}
var _4b=document.createElement("div");
_4b.innerHTML=_4a;
this.titleNode.appendChild(_4b);
},setTime:function(_4c){
if(this.timeNode.lastChild){
this.timeNode.removeChild(this.timeNode.lastChild);
}
var _4d=document.createTextNode(_4c);
this.timeNode.appendChild(_4d);
},enableDelete:function(){
if(this.deleteButton!==null){
this.deleteButton.style.display="inline";
}
},disableDelete:function(){
if(this.deleteButton!==null){
this.deleteButton.style.display="none";
}
},deleteEntry:function(_4e){
_4e.preventDefault();
_4e.stopPropagation();
this.feed.deleteEntry(this);
},onClick:function(e){
}});
_e.FeedViewerGrouping=_1.declare([_6,_7],{templateString:_c,groupingNode:null,titleNode:null,setText:function(_4f){
if(this.titleNode.lastChild){
this.titleNode.removeChild(this.titleNode.lastChild);
}
var _50=document.createTextNode(_4f);
this.titleNode.appendChild(_50);
}});
_e.AtomEntryCategoryFilter=_1.declare([_6,_7],{scheme:"",term:"",label:"",isFilter:true});
_e.FeedViewer.CategoryIncludeFilter=_1.declare(null,{constructor:function(_51,_52,_53){
this.scheme=_51;
this.term=_52;
this.label=_53;
},match:function(_54){
var _55=false;
if(_54!==null){
var _56=_54.categories;
if(_56!==null){
for(var i=0;i<_56.length;i++){
var _57=_56[i];
if(this.scheme!==""){
if(this.scheme!==_57.scheme){
break;
}
}
if(this.term!==""){
if(this.term!==_57.term){
break;
}
}
if(this.label!==""){
if(this.label!==_57.label){
break;
}
}
_55=true;
}
}
}
return _55;
}});
return _e.FeedViewer;
});
