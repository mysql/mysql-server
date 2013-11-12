//>>built
require({cache:{"url:dojox/atom/widget/templates/FeedEntryViewer.html":"<div class=\"feedEntryViewer\">\n    <table border=\"0\" width=\"100%\" class=\"feedEntryViewerMenuTable\" dojoAttachPoint=\"feedEntryViewerMenu\" style=\"display: none;\">\n        <tr width=\"100%\"  dojoAttachPoint=\"entryCheckBoxDisplayOptions\">\n            <td align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"displayOptions\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n        </tr>\n        <tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCelltitle\">\n                <input type=\"checkbox\" name=\"title\" value=\"Title\" dojoAttachPoint=\"feedEntryCheckBoxTitle\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelTitle\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellauthors\">\n                <input type=\"checkbox\" name=\"authors\" value=\"Authors\" dojoAttachPoint=\"feedEntryCheckBoxAuthors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelAuthors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontributors\">\n                <input type=\"checkbox\" name=\"contributors\" value=\"Contributors\" dojoAttachPoint=\"feedEntryCheckBoxContributors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContributors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellid\">\n                <input type=\"checkbox\" name=\"id\" value=\"Id\" dojoAttachPoint=\"feedEntryCheckBoxId\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelId\"></label>\n            </td>\n            <td rowspan=\"2\" align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"close\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n\t\t</tr>\n\t\t<tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow2\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCellupdated\">\n                <input type=\"checkbox\" name=\"updated\" value=\"Updated\" dojoAttachPoint=\"feedEntryCheckBoxUpdated\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelUpdated\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellsummary\">\n                <input type=\"checkbox\" name=\"summary\" value=\"Summary\" dojoAttachPoint=\"feedEntryCheckBoxSummary\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelSummary\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontent\">\n                <input type=\"checkbox\" name=\"content\" value=\"Content\" dojoAttachPoint=\"feedEntryCheckBoxContent\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContent\"></label>\n            </td>\n        </tr>\n    </table>\n    \n    <table class=\"feedEntryViewerContainer\" border=\"0\" width=\"100%\">\n        <tr class=\"feedEntryViewerTitle\" dojoAttachPoint=\"entryTitleRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryTitleHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryTitleNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerAuthor\" dojoAttachPoint=\"entryAuthorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryAuthorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryAuthorNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerContributor\" dojoAttachPoint=\"entryContributorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContributorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContributorNode\" class=\"feedEntryViewerContributorNames\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n        \n        <tr class=\"feedEntryViewerId\" dojoAttachPoint=\"entryIdRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryIdHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryIdNode\" class=\"feedEntryViewerIdText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerUpdated\" dojoAttachPoint=\"entryUpdatedRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryUpdatedHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryUpdatedNode\" class=\"feedEntryViewerUpdatedText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerSummary\" dojoAttachPoint=\"entrySummaryRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entrySummaryHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entrySummaryNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerContent\" dojoAttachPoint=\"entryContentRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContentHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContentNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    </table>\n</div>\n","url:dojox/atom/widget/templates/EntryHeader.html":"<span dojoAttachPoint=\"entryHeaderNode\" class=\"entryHeaderNode\"></span>\n"}});
define("dojox/atom/widget/FeedEntryViewer",["dojo/_base/kernel","dojo/_base/connect","dojo/_base/declare","dojo/_base/fx","dojo/_base/array","dojo/dom-style","dojo/dom-construct","dijit/_Widget","dijit/_Templated","dijit/_Container","dijit/layout/ContentPane","../io/Connection","dojo/text!./templates/FeedEntryViewer.html","dojo/text!./templates/EntryHeader.html","dojo/i18n!./nls/FeedEntryViewer"],function(_1,_2,_3,fx,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
_1.experimental("dojox.atom.widget.FeedEntryViewer");
var _f=_1.getObject("dojox.atom.widget",true);
_f.FeedEntryViewer=_1.declare([_7,_8,_9],{entrySelectionTopic:"",_validEntryFields:{},displayEntrySections:"",_displayEntrySections:null,enableMenu:false,enableMenuFade:false,_optionButtonDisplayed:true,templateString:_c,_entry:null,_feed:null,_editMode:false,postCreate:function(){
if(this.entrySelectionTopic!==""){
this._subscriptions=[_1.subscribe(this.entrySelectionTopic,this,"_handleEvent")];
}
var _10=_e;
this.displayOptions.innerHTML=_10.displayOptions;
this.feedEntryCheckBoxLabelTitle.innerHTML=_10.title;
this.feedEntryCheckBoxLabelAuthors.innerHTML=_10.authors;
this.feedEntryCheckBoxLabelContributors.innerHTML=_10.contributors;
this.feedEntryCheckBoxLabelId.innerHTML=_10.id;
this.close.innerHTML=_10.close;
this.feedEntryCheckBoxLabelUpdated.innerHTML=_10.updated;
this.feedEntryCheckBoxLabelSummary.innerHTML=_10.summary;
this.feedEntryCheckBoxLabelContent.innerHTML=_10.content;
},startup:function(){
if(this.displayEntrySections===""){
this._displayEntrySections=["title","authors","contributors","summary","content","id","updated"];
}else{
this._displayEntrySections=this.displayEntrySections.split(",");
}
this._setDisplaySectionsCheckboxes();
if(this.enableMenu){
_5.set(this.feedEntryViewerMenu,"display","");
if(this.entryCheckBoxRow&&this.entryCheckBoxRow2){
if(this.enableMenuFade){
fx.fadeOut({node:this.entryCheckBoxRow,duration:250}).play();
fx.fadeOut({node:this.entryCheckBoxRow2,duration:250}).play();
}
}
}
},clear:function(){
this.destroyDescendants();
this._entry=null;
this._feed=null;
this.clearNodes();
},clearNodes:function(){
_4.forEach(["entryTitleRow","entryAuthorRow","entryContributorRow","entrySummaryRow","entryContentRow","entryIdRow","entryUpdatedRow"],function(_11){
_5.set(this[_11],"display","none");
},this);
_4.forEach(["entryTitleNode","entryTitleHeader","entryAuthorHeader","entryContributorHeader","entryContributorNode","entrySummaryHeader","entrySummaryNode","entryContentHeader","entryContentNode","entryIdNode","entryIdHeader","entryUpdatedHeader","entryUpdatedNode"],function(_12){
while(this[_12].firstChild){
_6.destroy(this[_12].firstChild);
}
},this);
},setEntry:function(_13,_14,_15){
this.clear();
this._validEntryFields={};
this._entry=_13;
this._feed=_14;
if(_13!==null){
if(this.entryTitleHeader){
this.setTitleHeader(this.entryTitleHeader,_13);
}
if(this.entryTitleNode){
this.setTitle(this.entryTitleNode,this._editMode,_13);
}
if(this.entryAuthorHeader){
this.setAuthorsHeader(this.entryAuthorHeader,_13);
}
if(this.entryAuthorNode){
this.setAuthors(this.entryAuthorNode,this._editMode,_13);
}
if(this.entryContributorHeader){
this.setContributorsHeader(this.entryContributorHeader,_13);
}
if(this.entryContributorNode){
this.setContributors(this.entryContributorNode,this._editMode,_13);
}
if(this.entryIdHeader){
this.setIdHeader(this.entryIdHeader,_13);
}
if(this.entryIdNode){
this.setId(this.entryIdNode,this._editMode,_13);
}
if(this.entryUpdatedHeader){
this.setUpdatedHeader(this.entryUpdatedHeader,_13);
}
if(this.entryUpdatedNode){
this.setUpdated(this.entryUpdatedNode,this._editMode,_13);
}
if(this.entrySummaryHeader){
this.setSummaryHeader(this.entrySummaryHeader,_13);
}
if(this.entrySummaryNode){
this.setSummary(this.entrySummaryNode,this._editMode,_13);
}
if(this.entryContentHeader){
this.setContentHeader(this.entryContentHeader,_13);
}
if(this.entryContentNode){
this.setContent(this.entryContentNode,this._editMode,_13);
}
}
this._displaySections();
},setTitleHeader:function(_16,_17){
if(_17.title&&_17.title.value&&_17.title.value!==null){
var _18=_e;
var _19=new _f.EntryHeader({title:_18.title});
_16.appendChild(_19.domNode);
}
},setTitle:function(_1a,_1b,_1c){
if(_1c.title&&_1c.title.value&&_1c.title.value!==null){
if(_1c.title.type=="text"){
var _1d=document.createTextNode(_1c.title.value);
_1a.appendChild(_1d);
}else{
var _1e=document.createElement("span");
var _1f=new _a({refreshOnShow:true,executeScripts:false},_1e);
_1f.attr("content",_1c.title.value);
_1a.appendChild(_1f.domNode);
}
this.setFieldValidity("title",true);
}
},setAuthorsHeader:function(_20,_21){
if(_21.authors&&_21.authors.length>0){
var _22=_e;
var _23=new _f.EntryHeader({title:_22.authors});
_20.appendChild(_23.domNode);
}
},setAuthors:function(_24,_25,_26){
_24.innerHTML="";
if(_26.authors&&_26.authors.length>0){
for(var i in _26.authors){
if(_26.authors[i].name){
var _27=_24;
if(_26.authors[i].uri){
var _28=document.createElement("a");
_27.appendChild(_28);
_28.href=_26.authors[i].uri;
_27=_28;
}
var _29=_26.authors[i].name;
if(_26.authors[i].email){
_29=_29+" ("+_26.authors[i].email+")";
}
var _2a=document.createTextNode(_29);
_27.appendChild(_2a);
var _2b=document.createElement("br");
_24.appendChild(_2b);
this.setFieldValidity("authors",true);
}
}
}
},setContributorsHeader:function(_2c,_2d){
if(_2d.contributors&&_2d.contributors.length>0){
var _2e=_e;
var _2f=new _f.EntryHeader({title:_2e.contributors});
_2c.appendChild(_2f.domNode);
}
},setContributors:function(_30,_31,_32){
if(_32.contributors&&_32.contributors.length>0){
for(var i in _32.contributors){
var _33=document.createTextNode(_32.contributors[i].name);
_30.appendChild(_33);
var _34=document.createElement("br");
_30.appendChild(_34);
this.setFieldValidity("contributors",true);
}
}
},setIdHeader:function(_35,_36){
if(_36.id&&_36.id!==null){
var _37=_e;
var _38=new _f.EntryHeader({title:_37.id});
_35.appendChild(_38.domNode);
}
},setId:function(_39,_3a,_3b){
if(_3b.id&&_3b.id!==null){
var _3c=document.createTextNode(_3b.id);
_39.appendChild(_3c);
this.setFieldValidity("id",true);
}
},setUpdatedHeader:function(_3d,_3e){
if(_3e.updated&&_3e.updated!==null){
var _3f=_e;
var _40=new _f.EntryHeader({title:_3f.updated});
_3d.appendChild(_40.domNode);
}
},setUpdated:function(_41,_42,_43){
if(_43.updated&&_43.updated!==null){
var _44=document.createTextNode(_43.updated);
_41.appendChild(_44);
this.setFieldValidity("updated",true);
}
},setSummaryHeader:function(_45,_46){
if(_46.summary&&_46.summary.value&&_46.summary.value!==null){
var _47=_e;
var _48=new _f.EntryHeader({title:_47.summary});
_45.appendChild(_48.domNode);
}
},setSummary:function(_49,_4a,_4b){
if(_4b.summary&&_4b.summary.value&&_4b.summary.value!==null){
var _4c=document.createElement("span");
var _4d=new _a({refreshOnShow:true,executeScripts:false},_4c);
_4d.attr("content",_4b.summary.value);
_49.appendChild(_4d.domNode);
this.setFieldValidity("summary",true);
}
},setContentHeader:function(_4e,_4f){
if(_4f.content&&_4f.content.value&&_4f.content.value!==null){
var _50=_e;
var _51=new _f.EntryHeader({title:_50.content});
_4e.appendChild(_51.domNode);
}
},setContent:function(_52,_53,_54){
if(_54.content&&_54.content.value&&_54.content.value!==null){
var _55=document.createElement("span");
var _56=new _a({refreshOnShow:true,executeScripts:false},_55);
_56.attr("content",_54.content.value);
_52.appendChild(_56.domNode);
this.setFieldValidity("content",true);
}
},_displaySections:function(){
_5.set(this.entryTitleRow,"display","none");
_5.set(this.entryAuthorRow,"display","none");
_5.set(this.entryContributorRow,"display","none");
_5.set(this.entrySummaryRow,"display","none");
_5.set(this.entryContentRow,"display","none");
_5.set(this.entryIdRow,"display","none");
_5.set(this.entryUpdatedRow,"display","none");
for(var i in this._displayEntrySections){
var _57=this._displayEntrySections[i].toLowerCase();
if(_57==="title"&&this.isFieldValid("title")){
_5.set(this.entryTitleRow,"display","");
}
if(_57==="authors"&&this.isFieldValid("authors")){
_5.set(this.entryAuthorRow,"display","");
}
if(_57==="contributors"&&this.isFieldValid("contributors")){
_5.set(this.entryContributorRow,"display","");
}
if(_57==="summary"&&this.isFieldValid("summary")){
_5.set(this.entrySummaryRow,"display","");
}
if(_57==="content"&&this.isFieldValid("content")){
_5.set(this.entryContentRow,"display","");
}
if(_57==="id"&&this.isFieldValid("id")){
_5.set(this.entryIdRow,"display","");
}
if(_57==="updated"&&this.isFieldValid("updated")){
_5.set(this.entryUpdatedRow,"display","");
}
}
},setDisplaySections:function(_58){
if(_58!==null){
this._displayEntrySections=_58;
this._displaySections();
}else{
this._displayEntrySections=["title","authors","contributors","summary","content","id","updated"];
}
},_setDisplaySectionsCheckboxes:function(){
var _59=["title","authors","contributors","summary","content","id","updated"];
for(var i in _59){
if(_4.indexOf(this._displayEntrySections,_59[i])==-1){
_5.set(this["feedEntryCell"+_59[i]],"display","none");
}else{
this["feedEntryCheckBox"+_59[i].substring(0,1).toUpperCase()+_59[i].substring(1)].checked=true;
}
}
},_readDisplaySections:function(){
var _5a=[];
if(this.feedEntryCheckBoxTitle.checked){
_5a.push("title");
}
if(this.feedEntryCheckBoxAuthors.checked){
_5a.push("authors");
}
if(this.feedEntryCheckBoxContributors.checked){
_5a.push("contributors");
}
if(this.feedEntryCheckBoxSummary.checked){
_5a.push("summary");
}
if(this.feedEntryCheckBoxContent.checked){
_5a.push("content");
}
if(this.feedEntryCheckBoxId.checked){
_5a.push("id");
}
if(this.feedEntryCheckBoxUpdated.checked){
_5a.push("updated");
}
this._displayEntrySections=_5a;
},_toggleCheckbox:function(_5b){
if(_5b.checked){
_5b.checked=false;
}else{
_5b.checked=true;
}
this._readDisplaySections();
this._displaySections();
},_toggleOptions:function(_5c){
if(this.enableMenu){
var _5d=null;
var _5e;
var _5f;
if(this._optionButtonDisplayed){
if(this.enableMenuFade){
_5e=fx.fadeOut({node:this.entryCheckBoxDisplayOptions,duration:250});
_2.connect(_5e,"onEnd",this,function(){
_5.set(this.entryCheckBoxDisplayOptions,"display","none");
_5.set(this.entryCheckBoxRow,"display","");
_5.set(this.entryCheckBoxRow2,"display","");
fx.fadeIn({node:this.entryCheckBoxRow,duration:250}).play();
fx.fadeIn({node:this.entryCheckBoxRow2,duration:250}).play();
});
_5e.play();
}else{
_5.set(this.entryCheckBoxDisplayOptions,"display","none");
_5.set(this.entryCheckBoxRow,"display","");
_5.set(this.entryCheckBoxRow2,"display","");
}
this._optionButtonDisplayed=false;
}else{
if(this.enableMenuFade){
_5e=fx.fadeOut({node:this.entryCheckBoxRow,duration:250});
_5f=fx.fadeOut({node:this.entryCheckBoxRow2,duration:250});
_2.connect(_5e,"onEnd",this,function(){
_5.set(this.entryCheckBoxRow,"display","none");
_5.set(this.entryCheckBoxRow2,"display","none");
_5.set(this.entryCheckBoxDisplayOptions,"display","");
fx.fadeIn({node:this.entryCheckBoxDisplayOptions,duration:250}).play();
});
_5e.play();
_5f.play();
}else{
_5.set(this.entryCheckBoxRow,"display","none");
_5.set(this.entryCheckBoxRow2,"display","none");
_5.set(this.entryCheckBoxDisplayOptions,"display","");
}
this._optionButtonDisplayed=true;
}
}
},_handleEvent:function(_60){
if(_60.source!=this){
if(_60.action=="set"&&_60.entry){
this.setEntry(_60.entry,_60.feed);
}else{
if(_60.action=="delete"&&_60.entry&&_60.entry==this._entry){
this.clear();
}
}
}
},setFieldValidity:function(_61,_62){
if(_61){
var _63=_61.toLowerCase();
this._validEntryFields[_61]=_62;
}
},isFieldValid:function(_64){
return this._validEntryFields[_64.toLowerCase()];
},getEntry:function(){
return this._entry;
},getFeed:function(){
return this._feed;
},destroy:function(){
this.clear();
_4.forEach(this._subscriptions,_1.unsubscribe);
}});
_f.EntryHeader=_1.declare([_7,_8,_9],{title:"",templateString:_d,postCreate:function(){
this.setListHeader();
},setListHeader:function(_65){
this.clear();
if(_65){
this.title=_65;
}
var _66=document.createTextNode(this.title);
this.entryHeaderNode.appendChild(_66);
},clear:function(){
this.destroyDescendants();
if(this.entryHeaderNode){
for(var i=0;i<this.entryHeaderNode.childNodes.length;i++){
this.entryHeaderNode.removeChild(this.entryHeaderNode.childNodes[i]);
}
}
},destroy:function(){
this.clear();
}});
return _f.FeedEntryViewer;
});
