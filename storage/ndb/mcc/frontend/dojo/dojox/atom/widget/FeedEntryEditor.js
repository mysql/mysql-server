//>>built
require({cache:{"url:dojox/atom/widget/templates/FeedEntryEditor.html":"<div class=\"feedEntryViewer\">\n    <table border=\"0\" width=\"100%\" class=\"feedEntryViewerMenuTable\" dojoAttachPoint=\"feedEntryViewerMenu\" style=\"display: none;\">\n        <tr width=\"100%\"  dojoAttachPoint=\"entryCheckBoxDisplayOptions\">\n        \t<td align=\"left\" dojoAttachPoint=\"entryNewButton\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"doNew\" dojoAttachEvent=\"onclick:_toggleNew\"></span>\n        \t</td>\n            <td align=\"left\" dojoAttachPoint=\"entryEditButton\" style=\"display: none;\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"edit\" dojoAttachEvent=\"onclick:_toggleEdit\"></span>\n            </td>\n            <td align=\"left\" dojoAttachPoint=\"entrySaveCancelButtons\" style=\"display: none;\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"save\" dojoAttachEvent=\"onclick:saveEdits\"></span>\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"cancel\" dojoAttachEvent=\"onclick:cancelEdits\"></span>\n            </td>\n            <td align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"displayOptions\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n        </tr>\n        <tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCelltitle\">\n                <input type=\"checkbox\" name=\"title\" value=\"Title\" dojoAttachPoint=\"feedEntryCheckBoxTitle\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelTitle\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellauthors\">\n                <input type=\"checkbox\" name=\"authors\" value=\"Authors\" dojoAttachPoint=\"feedEntryCheckBoxAuthors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelAuthors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontributors\">\n                <input type=\"checkbox\" name=\"contributors\" value=\"Contributors\" dojoAttachPoint=\"feedEntryCheckBoxContributors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContributors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellid\">\n                <input type=\"checkbox\" name=\"id\" value=\"Id\" dojoAttachPoint=\"feedEntryCheckBoxId\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelId\"></label>\n            </td>\n            <td rowspan=\"2\" align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"close\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n\t\t</tr>\n\t\t<tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow2\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCellupdated\">\n                <input type=\"checkbox\" name=\"updated\" value=\"Updated\" dojoAttachPoint=\"feedEntryCheckBoxUpdated\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelUpdated\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellsummary\">\n                <input type=\"checkbox\" name=\"summary\" value=\"Summary\" dojoAttachPoint=\"feedEntryCheckBoxSummary\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelSummary\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontent\">\n                <input type=\"checkbox\" name=\"content\" value=\"Content\" dojoAttachPoint=\"feedEntryCheckBoxContent\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContent\"></label>\n            </td>\n        </tr>\n    </table>\n    \n    <table class=\"feedEntryViewerContainer\" border=\"0\" width=\"100%\">\n        <tr class=\"feedEntryViewerTitle\" dojoAttachPoint=\"entryTitleRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryTitleHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td>\n                        \t<select dojoAttachPoint=\"entryTitleSelect\" dojoAttachEvent=\"onchange:_switchEditor\" style=\"display: none\">\n                        \t\t<option value=\"text\">Text</option>\n\t\t\t\t\t\t\t\t<option value=\"html\">HTML</option>\n\t\t\t\t\t\t\t\t<option value=\"xhtml\">XHTML</option>\n                        \t</select>\n                        </td>\n                    </tr>\n                    <tr>\n                        <td colspan=\"2\" dojoAttachPoint=\"entryTitleNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerAuthor\" dojoAttachPoint=\"entryAuthorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryAuthorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryAuthorNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerContributor\" dojoAttachPoint=\"entryContributorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContributorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContributorNode\" class=\"feedEntryViewerContributorNames\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n        \n        <tr class=\"feedEntryViewerId\" dojoAttachPoint=\"entryIdRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryIdHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryIdNode\" class=\"feedEntryViewerIdText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerUpdated\" dojoAttachPoint=\"entryUpdatedRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryUpdatedHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryUpdatedNode\" class=\"feedEntryViewerUpdatedText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerSummary\" dojoAttachPoint=\"entrySummaryRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\" colspan=\"2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entrySummaryHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td>\n                        \t<select dojoAttachPoint=\"entrySummarySelect\" dojoAttachEvent=\"onchange:_switchEditor\" style=\"display: none\">\n                        \t\t<option value=\"text\">Text</option>\n\t\t\t\t\t\t\t\t<option value=\"html\">HTML</option>\n\t\t\t\t\t\t\t\t<option value=\"xhtml\">XHTML</option>\n                        \t</select>\n                        </td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entrySummaryNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerContent\" dojoAttachPoint=\"entryContentRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContentHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td>\n                        \t<select dojoAttachPoint=\"entryContentSelect\" dojoAttachEvent=\"onchange:_switchEditor\" style=\"display: none\">\n                        \t\t<option value=\"text\">Text</option>\n\t\t\t\t\t\t\t\t<option value=\"html\">HTML</option>\n\t\t\t\t\t\t\t\t<option value=\"xhtml\">XHTML</option>\n                        \t</select>\n                        </td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContentNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    </table>\n</div>\n","url:dojox/atom/widget/templates/PeopleEditor.html":"<div class=\"peopleEditor\">\n\t<table style=\"width: 100%\">\n\t\t<tbody dojoAttachPoint=\"peopleEditorEditors\"></tbody>\n\t</table>\n\t<span class=\"peopleEditorButton\" dojoAttachPoint=\"peopleEditorButton\" dojoAttachEvent=\"onclick:_add\"></span>\n</div>"}});
define("dojox/atom/widget/FeedEntryEditor",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/connect","dojo/_base/declare","dojo/_base/fx","dojo/_base/sniff","dojo/dom","dojo/dom-style","dojo/dom-construct","dijit/_Widget","dijit/_Templated","dijit/_Container","dijit/Editor","dijit/form/TextBox","dijit/form/SimpleTextarea","./FeedEntryViewer","../io/model","dojo/text!./templates/FeedEntryEditor.html","dojo/text!./templates/PeopleEditor.html","dojo/i18n!./nls/FeedEntryViewer","dojo/i18n!./nls/FeedEntryEditor","dojo/i18n!./nls/PeopleEditor"],function(_1,_2,_3,_4,fx,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15){
_1.experimental("dojox.atom.widget.FeedEntryEditor");
var _16=_4("dojox.atom.widget.FeedEntryEditor",_f,{_contentEditor:null,_oldContent:null,_setObject:null,enableEdit:false,_contentEditorCreator:null,_editors:{},entryNewButton:null,_editable:false,templateString:_11,postCreate:function(){
if(this.entrySelectionTopic!==""){
this._subscriptions=[_1.subscribe(this.entrySelectionTopic,this,"_handleEvent")];
}
var _17=_13;
this.displayOptions.innerHTML=_17.displayOptions;
this.feedEntryCheckBoxLabelTitle.innerHTML=_17.title;
this.feedEntryCheckBoxLabelAuthors.innerHTML=_17.authors;
this.feedEntryCheckBoxLabelContributors.innerHTML=_17.contributors;
this.feedEntryCheckBoxLabelId.innerHTML=_17.id;
this.close.innerHTML=_17.close;
this.feedEntryCheckBoxLabelUpdated.innerHTML=_17.updated;
this.feedEntryCheckBoxLabelSummary.innerHTML=_17.summary;
this.feedEntryCheckBoxLabelContent.innerHTML=_17.content;
_17=_14;
this.doNew.innerHTML=_17.doNew;
this.edit.innerHTML=_17.edit;
this.save.innerHTML=_17.save;
this.cancel.innerHTML=_17.cancel;
},setEntry:function(_18,_19,_1a){
if(this._entry!==_18){
this._editMode=false;
_1a=false;
}else{
_1a=true;
}
_16.superclass.setEntry.call(this,_18,_19);
this._editable=this._isEditable(_18);
if(!_1a&&!this._editable){
_7.set(this.entryEditButton,"display","none");
_7.set(this.entrySaveCancelButtons,"display","none");
}
if(this._editable&&this.enableEdit){
if(!_1a){
_7.set(this.entryEditButton,"display","");
if(this.enableMenuFade&&this.entrySaveCancelButton){
fx.fadeOut({node:this.entrySaveCancelButton,duration:250}).play();
}
}
}
},_toggleEdit:function(){
if(this._editable&&this.enableEdit){
_7.set(this.entryEditButton,"display","none");
_7.set(this.entrySaveCancelButtons,"display","");
this._editMode=true;
this.setEntry(this._entry,this._feed,true);
}
},_handleEvent:function(_1b){
if(_1b.source!=this&&_1b.action=="delete"&&_1b.entry&&_1b.entry==this._entry){
_7.set(this.entryEditButton,"display","none");
}
_16.superclass._handleEvent.call(this,_1b);
},_isEditable:function(_1c){
var _1d=false;
if(_1c&&_1c!==null&&_1c.links&&_1c.links!==null){
for(var x in _1c.links){
if(_1c.links[x].rel&&_1c.links[x].rel=="edit"){
_1d=true;
break;
}
}
}
return _1d;
},setTitle:function(_1e,_1f,_20){
if(!_1f){
_16.superclass.setTitle.call(this,_1e,_1f,_20);
if(_20.title&&_20.title.value&&_20.title.value!==null){
this.setFieldValidity("title",true);
}
}else{
if(_20.title&&_20.title.value&&_20.title.value!==null){
if(!this._toLoad){
this._toLoad=[];
}
this.entryTitleSelect.value=_20.title.type;
var _21=this._createEditor(_1e,_20.title,true,_20.title.type==="html"||_20.title.type==="xhtml");
_21.name="title";
this._toLoad.push(_21);
this.setFieldValidity("titleedit",true);
this.setFieldValidity("title",true);
}
}
},setAuthors:function(_22,_23,_24){
if(!_23){
_16.superclass.setAuthors.call(this,_22,_23,_24);
if(_24.authors&&_24.authors.length>0){
this.setFieldValidity("authors",true);
}
}else{
if(_24.authors&&_24.authors.length>0){
this._editors.authors=this._createPeopleEditor(this.entryAuthorNode,{data:_24.authors,name:"Author"});
this.setFieldValidity("authors",true);
}
}
},setContributors:function(_25,_26,_27){
if(!_26){
_16.superclass.setContributors.call(this,_25,_26,_27);
if(_27.contributors&&_27.contributors.length>0){
this.setFieldValidity("contributors",true);
}
}else{
if(_27.contributors&&_27.contributors.length>0){
this._editors.contributors=this._createPeopleEditor(this.entryContributorNode,{data:_27.contributors,name:"Contributor"});
this.setFieldValidity("contributors",true);
}
}
},setId:function(_28,_29,_2a){
if(!_29){
_16.superclass.setId.call(this,_28,_29,_2a);
if(_2a.id&&_2a.id!==null){
this.setFieldValidity("id",true);
}
}else{
if(_2a.id&&_2a.id!==null){
this._editors.id=this._createEditor(_28,_2a.id);
this.setFieldValidity("id",true);
}
}
},setUpdated:function(_2b,_2c,_2d){
if(!_2c){
_16.superclass.setUpdated.call(this,_2b,_2c,_2d);
if(_2d.updated&&_2d.updated!==null){
this.setFieldValidity("updated",true);
}
}else{
if(_2d.updated&&_2d.updated!==null){
this._editors.updated=this._createEditor(_2b,_2d.updated);
this.setFieldValidity("updated",true);
}
}
},setSummary:function(_2e,_2f,_30){
if(!_2f){
_16.superclass.setSummary.call(this,_2e,_2f,_30);
if(_30.summary&&_30.summary.value&&_30.summary.value!==null){
this.setFieldValidity("summary",true);
}
}else{
if(_30.summary&&_30.summary.value&&_30.summary.value!==null){
if(!this._toLoad){
this._toLoad=[];
}
this.entrySummarySelect.value=_30.summary.type;
var _31=this._createEditor(_2e,_30.summary,true,_30.summary.type==="html"||_30.summary.type==="xhtml");
_31.name="summary";
this._toLoad.push(_31);
this.setFieldValidity("summaryedit",true);
this.setFieldValidity("summary",true);
}
}
},setContent:function(_32,_33,_34){
if(!_33){
_16.superclass.setContent.call(this,_32,_33,_34);
if(_34.content&&_34.content.value&&_34.content.value!==null){
this.setFieldValidity("content",true);
}
}else{
if(_34.content&&_34.content.value&&_34.content.value!==null){
if(!this._toLoad){
this._toLoad=[];
}
this.entryContentSelect.value=_34.content.type;
var _35=this._createEditor(_32,_34.content,true,_34.content.type==="html"||_34.content.type==="xhtml");
_35.name="content";
this._toLoad.push(_35);
this.setFieldValidity("contentedit",true);
this.setFieldValidity("content",true);
}
}
},_createEditor:function(_36,_37,_38,rte){
var _39;
var box;
if(!_37){
if(rte){
return {anchorNode:_36,entryValue:"",editor:null,generateEditor:function(){
var _3a=document.createElement("div");
_3a.innerHTML=this.entryValue;
this.anchorNode.appendChild(_3a);
var _3b=new _c({},_3a);
this.editor=_3b;
return _3b;
}};
}
if(_38){
_39=document.createElement("textarea");
_36.appendChild(_39);
_7.set(_39,"width","90%");
box=new _e({},_39);
}else{
_39=document.createElement("input");
_36.appendChild(_39);
_7.set(_39,"width","95%");
box=new _d({},_39);
}
box.attr("value","");
return box;
}
var _3c;
if(_37.value!==undefined){
_3c=_37.value;
}else{
if(_37.attr){
_3c=_37.attr("value");
}else{
_3c=_37;
}
}
if(rte){
if(_3c.indexOf("<")!=-1){
_3c=_3c.replace(/</g,"&lt;");
}
return {anchorNode:_36,entryValue:_3c,editor:null,generateEditor:function(){
var _3d=document.createElement("div");
_3d.innerHTML=this.entryValue;
this.anchorNode.appendChild(_3d);
var _3e=new _c({},_3d);
this.editor=_3e;
return _3e;
}};
}
if(_38){
_39=document.createElement("textarea");
_36.appendChild(_39);
_7.set(_39,"width","90%");
box=new _e({},_39);
}else{
_39=document.createElement("input");
_36.appendChild(_39);
_7.set(_39,"width","95%");
box=new _d({},_39);
}
box.attr("value",_3c);
return box;
},_switchEditor:function(_3f){
var _40=null;
var _41=null;
var _42=null;
if(_5("ie")){
_41=_3f.srcElement;
}else{
_41=_3f.target;
}
if(_41===this.entryTitleSelect){
_42=this.entryTitleNode;
_40="title";
}else{
if(_41===this.entrySummarySelect){
_42=this.entrySummaryNode;
_40="summary";
}else{
_42=this.entryContentNode;
_40="content";
}
}
var _43=this._editors[_40];
var _44;
var _45;
if(_41.value==="text"){
if(_43.isInstanceOf(_c)){
_45=_43.attr("value",false);
_43.close(false,true);
_43.destroy();
while(_42.firstChild){
_8.destroy(_42.firstChild);
}
_44=this._createEditor(_42,{value:_45},true,false);
this._editors[_40]=_44;
}
}else{
if(!_43.isInstanceOf(_c)){
_45=_43.attr("value");
_43.destroy();
while(_42.firstChild){
_8.destroy(_42.firstChild);
}
_44=this._createEditor(_42,{value:_45},true,true);
_44=_2.hitch(_44,_44.generateEditor)();
this._editors[_40]=_44;
}
}
},_createPeopleEditor:function(_46,_47){
var _48=document.createElement("div");
_46.appendChild(_48);
return new _49(_47,_48);
},saveEdits:function(){
_7.set(this.entrySaveCancelButtons,"display","none");
_7.set(this.entryEditButton,"display","");
_7.set(this.entryNewButton,"display","");
var _4a=false;
var _4b;
var i;
var _4c;
var _4d;
var _4e;
var _4f;
if(!this._new){
_4d=this.getEntry();
if(this._editors.title&&(this._editors.title.attr("value")!=_4d.title.value||this.entryTitleSelect.value!=_4d.title.type)){
_4b=this._editors.title.attr("value");
if(this.entryTitleSelect.value==="xhtml"){
_4b=this._enforceXhtml(_4b);
if(_4b.indexOf("<div xmlns=\"http://www.w3.org/1999/xhtml\">")!==0){
_4b="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_4b+"</div>";
}
}
_4d.title=new _10.Content("title",_4b,null,this.entryTitleSelect.value);
_4a=true;
}
if(this._editors.id.attr("value")!=_4d.id){
_4d.id=this._editors.id.attr("value");
_4a=true;
}
if(this._editors.summary&&(this._editors.summary.attr("value")!=_4d.summary.value||this.entrySummarySelect.value!=_4d.summary.type)){
_4b=this._editors.summary.attr("value");
if(this.entrySummarySelect.value==="xhtml"){
_4b=this._enforceXhtml(_4b);
if(_4b.indexOf("<div xmlns=\"http://www.w3.org/1999/xhtml\">")!==0){
_4b="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_4b+"</div>";
}
}
_4d.summary=new _10.Content("summary",_4b,null,this.entrySummarySelect.value);
_4a=true;
}
if(this._editors.content&&(this._editors.content.attr("value")!=_4d.content.value||this.entryContentSelect.value!=_4d.content.type)){
_4b=this._editors.content.attr("value");
if(this.entryContentSelect.value==="xhtml"){
_4b=this._enforceXhtml(_4b);
if(_4b.indexOf("<div xmlns=\"http://www.w3.org/1999/xhtml\">")!==0){
_4b="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_4b+"</div>";
}
}
_4d.content=new _10.Content("content",_4b,null,this.entryContentSelect.value);
_4a=true;
}
if(this._editors.authors){
if(_4a){
_4d.authors=[];
_4e=this._editors.authors.getValues();
for(i in _4e){
if(_4e[i].name||_4e[i].email||_4e[i].uri){
_4d.addAuthor(_4e[i].name,_4e[i].email,_4e[i].uri);
}
}
}else{
var _50=_4d.authors;
var _51=function(_52,_53,uri){
for(i in _50){
if(_50[i].name===_52&&_50[i].email===_53&&_50[i].uri===uri){
return true;
}
}
return false;
};
_4e=this._editors.authors.getValues();
_4c=false;
for(i in _4e){
if(!_51(_4e[i].name,_4e[i].email,_4e[i].uri)){
_4c=true;
break;
}
}
if(_4c){
_4d.authors=[];
for(i in _4e){
if(_4e[i].name||_4e[i].email||_4e[i].uri){
_4d.addAuthor(_4e[i].name,_4e[i].email,_4e[i].uri);
}
}
_4a=true;
}
}
}
if(this._editors.contributors){
if(_4a){
_4d.contributors=[];
_4f=this._editors.contributors.getValues();
for(i in _4f){
if(_4f[i].name||_4f[i].email||_4f[i].uri){
_4d.addAuthor(_4f[i].name,_4f[i].email,_4f[i].uri);
}
}
}else{
var _54=_4d.contributors;
var _55=function(_56,_57,uri){
for(i in _54){
if(_54[i].name===_56&&_54[i].email===_57&&_54[i].uri===uri){
return true;
}
}
return false;
};
_4f=this._editors.contributors.getValues();
_4c=false;
for(i in _4f){
if(_55(_4f[i].name,_4f[i].email,_4f[i].uri)){
_4c=true;
break;
}
}
if(_4c){
_4d.contributors=[];
for(i in _4f){
if(_4f[i].name||_4f[i].email||_4f[i].uri){
_4d.addContributor(_4f[i].name,_4f[i].email,_4f[i].uri);
}
}
_4a=true;
}
}
}
if(_4a){
_1.publish(this.entrySelectionTopic,[{action:"update",source:this,entry:_4d,callback:this._handleSave}]);
}
}else{
this._new=false;
_4d=new _10.Entry();
_4b=this._editors.title.attr("value");
if(this.entryTitleSelect.value==="xhtml"){
_4b=this._enforceXhtml(_4b);
_4b="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_4b+"</div>";
}
_4d.setTitle(_4b,this.entryTitleSelect.value);
_4d.id=this._editors.id.attr("value");
_4e=this._editors.authors.getValues();
for(i in _4e){
if(_4e[i].name||_4e[i].email||_4e[i].uri){
_4d.addAuthor(_4e[i].name,_4e[i].email,_4e[i].uri);
}
}
_4f=this._editors.contributors.getValues();
for(i in _4f){
if(_4f[i].name||_4f[i].email||_4f[i].uri){
_4d.addContributor(_4f[i].name,_4f[i].email,_4f[i].uri);
}
}
_4b=this._editors.summary.attr("value");
if(this.entrySummarySelect.value==="xhtml"){
_4b=this._enforceXhtml(_4b);
_4b="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_4b+"</div>";
}
_4d.summary=new _10.Content("summary",_4b,null,this.entrySummarySelect.value);
_4b=this._editors.content.attr("value");
if(this.entryContentSelect.value==="xhtml"){
_4b=this._enforceXhtml(_4b);
_4b="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_4b+"</div>";
}
_4d.content=new _10.Content("content",_4b,null,this.entryContentSelect.value);
_7.set(this.entryNewButton,"display","");
_1.publish(this.entrySelectionTopic,[{action:"post",source:this,entry:_4d}]);
}
this._editMode=false;
this.setEntry(_4d,this._feed,true);
},_handleSave:function(_58,_59){
this._editMode=false;
this.clear();
this.setEntry(_58,this.getFeed(),true);
},cancelEdits:function(){
this._new=false;
_7.set(this.entrySaveCancelButtons,"display","none");
if(this._editable){
_7.set(this.entryEditButton,"display","");
}
_7.set(this.entryNewButton,"display","");
this._editMode=false;
this.clearEditors();
this.setEntry(this.getEntry(),this.getFeed(),true);
},clear:function(){
this._editable=false;
this.clearEditors();
_16.superclass.clear.apply(this);
if(this._contentEditor){
this._contentEditor=this._setObject=this._oldContent=this._contentEditorCreator=null;
this._editors={};
}
},clearEditors:function(){
for(var key in this._editors){
if(this._editors[key].isInstanceOf(_c)){
this._editors[key].close(false,true);
}
this._editors[key].destroy();
}
this._editors={};
},_enforceXhtml:function(_5a){
var _5b=null;
if(_5a){
var _5c=/<br>/g;
_5b=_5a.replace(_5c,"<br/>");
_5b=this._closeTag(_5b,"hr");
_5b=this._closeTag(_5b,"img");
}
return _5b;
},_closeTag:function(_5d,tag){
var _5e="<"+tag;
var _5f=_5d.indexOf(_5e);
if(_5f!==-1){
while(_5f!==-1){
var _60="";
var _61=false;
for(var i=0;i<_5d.length;i++){
var c=_5d.charAt(i);
if(i<=_5f||_61){
_60+=c;
}else{
if(c===">"){
_60+="/";
_61=true;
}
_60+=c;
}
}
_5d=_60;
_5f=_5d.indexOf(_5e,_5f+1);
}
}
return _5d;
},_toggleNew:function(){
_7.set(this.entryNewButton,"display","none");
_7.set(this.entryEditButton,"display","none");
_7.set(this.entrySaveCancelButtons,"display","");
this.entrySummarySelect.value="text";
this.entryContentSelect.value="text";
this.entryTitleSelect.value="text";
this.clearNodes();
this._new=true;
var _62=_13;
var _63=new _f.EntryHeader({title:_62.title});
this.entryTitleHeader.appendChild(_63.domNode);
this._editors.title=this._createEditor(this.entryTitleNode,null);
this.setFieldValidity("title",true);
var _64=new _f.EntryHeader({title:_62.authors});
this.entryAuthorHeader.appendChild(_64.domNode);
this._editors.authors=this._createPeopleEditor(this.entryAuthorNode,{name:"Author"});
this.setFieldValidity("authors",true);
var _65=new _f.EntryHeader({title:_62.contributors});
this.entryContributorHeader.appendChild(_65.domNode);
this._editors.contributors=this._createPeopleEditor(this.entryContributorNode,{name:"Contributor"});
this.setFieldValidity("contributors",true);
var _66=new _f.EntryHeader({title:_62.id});
this.entryIdHeader.appendChild(_66.domNode);
this._editors.id=this._createEditor(this.entryIdNode,null);
this.setFieldValidity("id",true);
var _67=new _f.EntryHeader({title:_62.updated});
this.entryUpdatedHeader.appendChild(_67.domNode);
this._editors.updated=this._createEditor(this.entryUpdatedNode,null);
this.setFieldValidity("updated",true);
var _68=new _f.EntryHeader({title:_62.summary});
this.entrySummaryHeader.appendChild(_68.domNode);
this._editors.summary=this._createEditor(this.entrySummaryNode,null,true);
this.setFieldValidity("summaryedit",true);
this.setFieldValidity("summary",true);
var _69=new _f.EntryHeader({title:_62.content});
this.entryContentHeader.appendChild(_69.domNode);
this._editors.content=this._createEditor(this.entryContentNode,null,true);
this.setFieldValidity("contentedit",true);
this.setFieldValidity("content",true);
this._displaySections();
},_displaySections:function(){
_7.set(this.entrySummarySelect,"display","none");
_7.set(this.entryContentSelect,"display","none");
_7.set(this.entryTitleSelect,"display","none");
if(this.isFieldValid("contentedit")){
_7.set(this.entryContentSelect,"display","");
}
if(this.isFieldValid("summaryedit")){
_7.set(this.entrySummarySelect,"display","");
}
if(this.isFieldValid("titleedit")){
_7.set(this.entryTitleSelect,"display","");
}
_16.superclass._displaySections.apply(this);
if(this._toLoad){
for(var i in this._toLoad){
var _6a;
if(this._toLoad[i].generateEditor){
_6a=_2.hitch(this._toLoad[i],this._toLoad[i].generateEditor)();
}else{
_6a=this._toLoad[i];
}
this._editors[this._toLoad[i].name]=_6a;
this._toLoad[i]=null;
}
this._toLoad=null;
}
}});
var _49=_4("dojox.atom.widget.PeopleEditor",[_9,_a,_b],{templateString:_12,_rows:[],_editors:[],_index:0,_numRows:0,postCreate:function(){
var _6b=_15;
if(this.name){
if(this.name=="Author"){
this.peopleEditorButton.appendChild(document.createTextNode("["+_6b.addAuthor+"]"));
}else{
if(this.name=="Contributor"){
this.peopleEditorButton.appendChild(document.createTextNode("["+_6b.addContributor+"]"));
}
}
}else{
this.peopleEditorButton.appendChild(document.createTextNode("["+_6b.add+"]"));
}
this._editors=[];
if(!this.data||this.data.length===0){
this._createEditors(null,null,null,0,this.name);
this._index=1;
}else{
for(var i in this.data){
this._createEditors(this.data[i].name,this.data[i].email,this.data[i].uri,i);
this._index++;
this._numRows++;
}
}
},destroy:function(){
for(var key in this._editors){
for(var _6c in this._editors[key]){
this._editors[key][_6c].destroy();
}
}
this._editors=[];
},_createEditors:function(_6d,_6e,uri,_6f,_70){
var row=document.createElement("tr");
this.peopleEditorEditors.appendChild(row);
row.id="removeRow"+_6f;
var _71=document.createElement("td");
_71.setAttribute("align","right");
row.appendChild(_71);
_71.colSpan=2;
if(this._numRows>0){
var hr=document.createElement("hr");
_71.appendChild(hr);
hr.id="hr"+_6f;
}
row=document.createElement("span");
_71.appendChild(row);
row.className="peopleEditorButton";
_7.set(row,"font-size","x-small");
_3.connect(row,"onclick",this,"_removeEditor");
row.id="remove"+_6f;
_71=document.createTextNode("[X]");
row.appendChild(_71);
row=document.createElement("tr");
this.peopleEditorEditors.appendChild(row);
row.id="editorsRow"+_6f;
var _72=document.createElement("td");
row.appendChild(_72);
_7.set(_72,"width","20%");
_71=document.createElement("td");
row.appendChild(_71);
row=document.createElement("table");
_72.appendChild(row);
_7.set(row,"width","100%");
_72=document.createElement("tbody");
row.appendChild(_72);
row=document.createElement("table");
_71.appendChild(row);
_7.set(row,"width","100%");
_71=document.createElement("tbody");
row.appendChild(_71);
this._editors[_6f]=[];
this._editors[_6f].push(this._createEditor(_6d,_70+"name"+_6f,"Name:",_72,_71));
this._editors[_6f].push(this._createEditor(_6e,_70+"email"+_6f,"Email:",_72,_71));
this._editors[_6f].push(this._createEditor(uri,_70+"uri"+_6f,"URI:",_72,_71));
},_createEditor:function(_73,id,_74,_75,_76){
var row=document.createElement("tr");
_75.appendChild(row);
var _77=document.createElement("label");
_77.setAttribute("for",id);
_77.appendChild(document.createTextNode(_74));
_75=document.createElement("td");
_75.appendChild(_77);
row.appendChild(_75);
row=document.createElement("tr");
_76.appendChild(row);
_76=document.createElement("td");
row.appendChild(_76);
var _78=document.createElement("input");
_78.setAttribute("id",id);
_76.appendChild(_78);
_7.set(_78,"width","95%");
var box=new _d({},_78);
box.attr("value",_73);
return box;
},_removeEditor:function(_79){
var _7a=null;
if(_5("ie")){
_7a=_79.srcElement;
}else{
_7a=_79.target;
}
var id=_7a.id;
id=id.substring(6);
for(var key in this._editors[id]){
this._editors[id][key].destroy();
}
var _7b=_6.byId("editorsRow"+id);
var _7c=_7b.parentNode;
_7c.removeChild(_7b);
_7b=_6.byId("removeRow"+id);
_7c=_7b.parentNode;
_7c.removeChild(_7b);
this._numRows--;
if(this._numRows===1&&_7c.firstChild.firstChild.firstChild.tagName.toLowerCase()==="hr"){
_7b=_7c.firstChild.firstChild;
_7b.removeChild(_7b.firstChild);
}
this._editors[id]=null;
},_add:function(){
this._createEditors(null,null,null,this._index);
this._index++;
this._numRows++;
},getValues:function(){
var _7d=[];
for(var i in this._editors){
if(this._editors[i]){
_7d.push({name:this._editors[i][0].attr("value"),email:this._editors[i][1].attr("value"),uri:this._editors[i][2].attr("value")});
}
}
return _7d;
}});
return _16;
});
