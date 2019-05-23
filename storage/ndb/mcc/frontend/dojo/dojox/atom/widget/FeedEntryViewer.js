//>>built
require({cache:{"url:dojox/atom/widget/templates/FeedEntryViewer.html":"<div class=\"feedEntryViewer\">\n    <table border=\"0\" width=\"100%\" class=\"feedEntryViewerMenuTable\" dojoAttachPoint=\"feedEntryViewerMenu\" style=\"display: none;\">\n        <tr width=\"100%\"  dojoAttachPoint=\"entryCheckBoxDisplayOptions\">\n            <td align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"displayOptions\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n        </tr>\n        <tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCelltitle\">\n                <input type=\"checkbox\" name=\"title\" value=\"Title\" dojoAttachPoint=\"feedEntryCheckBoxTitle\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelTitle\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellauthors\">\n                <input type=\"checkbox\" name=\"authors\" value=\"Authors\" dojoAttachPoint=\"feedEntryCheckBoxAuthors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelAuthors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontributors\">\n                <input type=\"checkbox\" name=\"contributors\" value=\"Contributors\" dojoAttachPoint=\"feedEntryCheckBoxContributors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContributors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellid\">\n                <input type=\"checkbox\" name=\"id\" value=\"Id\" dojoAttachPoint=\"feedEntryCheckBoxId\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelId\"></label>\n            </td>\n            <td rowspan=\"2\" align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"close\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n\t\t</tr>\n\t\t<tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow2\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCellupdated\">\n                <input type=\"checkbox\" name=\"updated\" value=\"Updated\" dojoAttachPoint=\"feedEntryCheckBoxUpdated\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelUpdated\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellsummary\">\n                <input type=\"checkbox\" name=\"summary\" value=\"Summary\" dojoAttachPoint=\"feedEntryCheckBoxSummary\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelSummary\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontent\">\n                <input type=\"checkbox\" name=\"content\" value=\"Content\" dojoAttachPoint=\"feedEntryCheckBoxContent\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContent\"></label>\n            </td>\n        </tr>\n    </table>\n    \n    <table class=\"feedEntryViewerContainer\" border=\"0\" width=\"100%\">\n        <tr class=\"feedEntryViewerTitle\" dojoAttachPoint=\"entryTitleRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryTitleHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryTitleNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerAuthor\" dojoAttachPoint=\"entryAuthorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryAuthorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryAuthorNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerContributor\" dojoAttachPoint=\"entryContributorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContributorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContributorNode\" class=\"feedEntryViewerContributorNames\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n        \n        <tr class=\"feedEntryViewerId\" dojoAttachPoint=\"entryIdRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryIdHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryIdNode\" class=\"feedEntryViewerIdText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerUpdated\" dojoAttachPoint=\"entryUpdatedRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryUpdatedHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryUpdatedNode\" class=\"feedEntryViewerUpdatedText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerSummary\" dojoAttachPoint=\"entrySummaryRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entrySummaryHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entrySummaryNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerContent\" dojoAttachPoint=\"entryContentRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContentHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContentNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    </table>\n</div>\n","url:dojox/atom/widget/templates/EntryHeader.html":"<span dojoAttachPoint=\"entryHeaderNode\" class=\"entryHeaderNode\"></span>\n"}});
define("dojox/atom/widget/FeedEntryViewer",["dojo/_base/kernel","dojo/_base/connect","dojo/_base/declare","dojo/_base/fx","dojo/_base/array","dojo/dom-style","dojo/dom-construct","dijit/_Widget","dijit/_Templated","dijit/_Container","dijit/layout/ContentPane","../io/Connection","dojo/text!./templates/FeedEntryViewer.html","dojo/text!./templates/EntryHeader.html","dojo/i18n!./nls/FeedEntryViewer"],function(_1,_2,_3,fx,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
_1.experimental("dojox.atom.widget.FeedEntryViewer");
var _f=_3("dojox.atom.widget.FeedEntryViewer",[_7,_8,_9],{entrySelectionTopic:"",_validEntryFields:{},displayEntrySections:"",_displayEntrySections:null,enableMenu:false,enableMenuFade:false,_optionButtonDisplayed:true,templateString:_c,_entry:null,_feed:null,_editMode:false,postCreate:function(){
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
var _19=new _1a({title:_18.title});
_16.appendChild(_19.domNode);
}
},setTitle:function(_1b,_1c,_1d){
if(_1d.title&&_1d.title.value&&_1d.title.value!==null){
if(_1d.title.type=="text"){
var _1e=document.createTextNode(_1d.title.value);
_1b.appendChild(_1e);
}else{
var _1f=document.createElement("span");
var _20=new _a({refreshOnShow:true,executeScripts:false},_1f);
_20.attr("content",_1d.title.value);
_1b.appendChild(_20.domNode);
}
this.setFieldValidity("title",true);
}
},setAuthorsHeader:function(_21,_22){
if(_22.authors&&_22.authors.length>0){
var _23=_e;
var _24=new _1a({title:_23.authors});
_21.appendChild(_24.domNode);
}
},setAuthors:function(_25,_26,_27){
_25.innerHTML="";
if(_27.authors&&_27.authors.length>0){
for(var i in _27.authors){
if(_27.authors[i].name){
var _28=_25;
if(_27.authors[i].uri){
var _29=document.createElement("a");
_28.appendChild(_29);
_29.href=_27.authors[i].uri;
_28=_29;
}
var _2a=_27.authors[i].name;
if(_27.authors[i].email){
_2a=_2a+" ("+_27.authors[i].email+")";
}
var _2b=document.createTextNode(_2a);
_28.appendChild(_2b);
var _2c=document.createElement("br");
_25.appendChild(_2c);
this.setFieldValidity("authors",true);
}
}
}
},setContributorsHeader:function(_2d,_2e){
if(_2e.contributors&&_2e.contributors.length>0){
var _2f=_e;
var _30=new _1a({title:_2f.contributors});
_2d.appendChild(_30.domNode);
}
},setContributors:function(_31,_32,_33){
if(_33.contributors&&_33.contributors.length>0){
for(var i in _33.contributors){
var _34=document.createTextNode(_33.contributors[i].name);
_31.appendChild(_34);
var _35=document.createElement("br");
_31.appendChild(_35);
this.setFieldValidity("contributors",true);
}
}
},setIdHeader:function(_36,_37){
if(_37.id&&_37.id!==null){
var _38=_e;
var _39=new _1a({title:_38.id});
_36.appendChild(_39.domNode);
}
},setId:function(_3a,_3b,_3c){
if(_3c.id&&_3c.id!==null){
var _3d=document.createTextNode(_3c.id);
_3a.appendChild(_3d);
this.setFieldValidity("id",true);
}
},setUpdatedHeader:function(_3e,_3f){
if(_3f.updated&&_3f.updated!==null){
var _40=_e;
var _41=new _1a({title:_40.updated});
_3e.appendChild(_41.domNode);
}
},setUpdated:function(_42,_43,_44){
if(_44.updated&&_44.updated!==null){
var _45=document.createTextNode(_44.updated);
_42.appendChild(_45);
this.setFieldValidity("updated",true);
}
},setSummaryHeader:function(_46,_47){
if(_47.summary&&_47.summary.value&&_47.summary.value!==null){
var _48=_e;
var _49=new _1a({title:_48.summary});
_46.appendChild(_49.domNode);
}
},setSummary:function(_4a,_4b,_4c){
if(_4c.summary&&_4c.summary.value&&_4c.summary.value!==null){
var _4d=document.createElement("span");
var _4e=new _a({refreshOnShow:true,executeScripts:false},_4d);
_4e.attr("content",_4c.summary.value);
_4a.appendChild(_4e.domNode);
this.setFieldValidity("summary",true);
}
},setContentHeader:function(_4f,_50){
if(_50.content&&_50.content.value&&_50.content.value!==null){
var _51=_e;
var _52=new _1a({title:_51.content});
_4f.appendChild(_52.domNode);
}
},setContent:function(_53,_54,_55){
if(_55.content&&_55.content.value&&_55.content.value!==null){
var _56=document.createElement("span");
var _57=new _a({refreshOnShow:true,executeScripts:false},_56);
_57.attr("content",_55.content.value);
_53.appendChild(_57.domNode);
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
var _58=this._displayEntrySections[i].toLowerCase();
if(_58==="title"&&this.isFieldValid("title")){
_5.set(this.entryTitleRow,"display","");
}
if(_58==="authors"&&this.isFieldValid("authors")){
_5.set(this.entryAuthorRow,"display","");
}
if(_58==="contributors"&&this.isFieldValid("contributors")){
_5.set(this.entryContributorRow,"display","");
}
if(_58==="summary"&&this.isFieldValid("summary")){
_5.set(this.entrySummaryRow,"display","");
}
if(_58==="content"&&this.isFieldValid("content")){
_5.set(this.entryContentRow,"display","");
}
if(_58==="id"&&this.isFieldValid("id")){
_5.set(this.entryIdRow,"display","");
}
if(_58==="updated"&&this.isFieldValid("updated")){
_5.set(this.entryUpdatedRow,"display","");
}
}
},setDisplaySections:function(_59){
if(_59!==null){
this._displayEntrySections=_59;
this._displaySections();
}else{
this._displayEntrySections=["title","authors","contributors","summary","content","id","updated"];
}
},_setDisplaySectionsCheckboxes:function(){
var _5a=["title","authors","contributors","summary","content","id","updated"];
for(var i in _5a){
if(_4.indexOf(this._displayEntrySections,_5a[i])==-1){
_5.set(this["feedEntryCell"+_5a[i]],"display","none");
}else{
this["feedEntryCheckBox"+_5a[i].substring(0,1).toUpperCase()+_5a[i].substring(1)].checked=true;
}
}
},_readDisplaySections:function(){
var _5b=[];
if(this.feedEntryCheckBoxTitle.checked){
_5b.push("title");
}
if(this.feedEntryCheckBoxAuthors.checked){
_5b.push("authors");
}
if(this.feedEntryCheckBoxContributors.checked){
_5b.push("contributors");
}
if(this.feedEntryCheckBoxSummary.checked){
_5b.push("summary");
}
if(this.feedEntryCheckBoxContent.checked){
_5b.push("content");
}
if(this.feedEntryCheckBoxId.checked){
_5b.push("id");
}
if(this.feedEntryCheckBoxUpdated.checked){
_5b.push("updated");
}
this._displayEntrySections=_5b;
},_toggleCheckbox:function(_5c){
if(_5c.checked){
_5c.checked=false;
}else{
_5c.checked=true;
}
this._readDisplaySections();
this._displaySections();
},_toggleOptions:function(_5d){
if(this.enableMenu){
var _5e=null;
var _5f;
var _60;
if(this._optionButtonDisplayed){
if(this.enableMenuFade){
_5f=fx.fadeOut({node:this.entryCheckBoxDisplayOptions,duration:250});
_2.connect(_5f,"onEnd",this,function(){
_5.set(this.entryCheckBoxDisplayOptions,"display","none");
_5.set(this.entryCheckBoxRow,"display","");
_5.set(this.entryCheckBoxRow2,"display","");
fx.fadeIn({node:this.entryCheckBoxRow,duration:250}).play();
fx.fadeIn({node:this.entryCheckBoxRow2,duration:250}).play();
});
_5f.play();
}else{
_5.set(this.entryCheckBoxDisplayOptions,"display","none");
_5.set(this.entryCheckBoxRow,"display","");
_5.set(this.entryCheckBoxRow2,"display","");
}
this._optionButtonDisplayed=false;
}else{
if(this.enableMenuFade){
_5f=fx.fadeOut({node:this.entryCheckBoxRow,duration:250});
_60=fx.fadeOut({node:this.entryCheckBoxRow2,duration:250});
_2.connect(_5f,"onEnd",this,function(){
_5.set(this.entryCheckBoxRow,"display","none");
_5.set(this.entryCheckBoxRow2,"display","none");
_5.set(this.entryCheckBoxDisplayOptions,"display","");
fx.fadeIn({node:this.entryCheckBoxDisplayOptions,duration:250}).play();
});
_5f.play();
_60.play();
}else{
_5.set(this.entryCheckBoxRow,"display","none");
_5.set(this.entryCheckBoxRow2,"display","none");
_5.set(this.entryCheckBoxDisplayOptions,"display","");
}
this._optionButtonDisplayed=true;
}
}
},_handleEvent:function(_61){
if(_61.source!=this){
if(_61.action=="set"&&_61.entry){
this.setEntry(_61.entry,_61.feed);
}else{
if(_61.action=="delete"&&_61.entry&&_61.entry==this._entry){
this.clear();
}
}
}
},setFieldValidity:function(_62,_63){
if(_62){
var _64=_62.toLowerCase();
this._validEntryFields[_62]=_63;
}
},isFieldValid:function(_65){
return this._validEntryFields[_65.toLowerCase()];
},getEntry:function(){
return this._entry;
},getFeed:function(){
return this._feed;
},destroy:function(){
this.clear();
_4.forEach(this._subscriptions,_1.unsubscribe);
}});
var _1a=_f.EntryHeader=_3("dojox.atom.widget.EntryHeader",[_7,_8,_9],{title:"",templateString:_d,postCreate:function(){
this.setListHeader();
},setListHeader:function(_66){
this.clear();
if(_66){
this.title=_66;
}
var _67=document.createTextNode(this.title);
this.entryHeaderNode.appendChild(_67);
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
return _f;
});
