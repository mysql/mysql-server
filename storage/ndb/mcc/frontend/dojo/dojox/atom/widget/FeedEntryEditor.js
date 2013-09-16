//>>built
require({cache:{"url:dojox/atom/widget/templates/FeedEntryEditor.html":"<div class=\"feedEntryViewer\">\n    <table border=\"0\" width=\"100%\" class=\"feedEntryViewerMenuTable\" dojoAttachPoint=\"feedEntryViewerMenu\" style=\"display: none;\">\n        <tr width=\"100%\"  dojoAttachPoint=\"entryCheckBoxDisplayOptions\">\n        \t<td align=\"left\" dojoAttachPoint=\"entryNewButton\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"doNew\" dojoAttachEvent=\"onclick:_toggleNew\"></span>\n        \t</td>\n            <td align=\"left\" dojoAttachPoint=\"entryEditButton\" style=\"display: none;\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"edit\" dojoAttachEvent=\"onclick:_toggleEdit\"></span>\n            </td>\n            <td align=\"left\" dojoAttachPoint=\"entrySaveCancelButtons\" style=\"display: none;\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"save\" dojoAttachEvent=\"onclick:saveEdits\"></span>\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"cancel\" dojoAttachEvent=\"onclick:cancelEdits\"></span>\n            </td>\n            <td align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"displayOptions\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n        </tr>\n        <tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCelltitle\">\n                <input type=\"checkbox\" name=\"title\" value=\"Title\" dojoAttachPoint=\"feedEntryCheckBoxTitle\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelTitle\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellauthors\">\n                <input type=\"checkbox\" name=\"authors\" value=\"Authors\" dojoAttachPoint=\"feedEntryCheckBoxAuthors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelAuthors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontributors\">\n                <input type=\"checkbox\" name=\"contributors\" value=\"Contributors\" dojoAttachPoint=\"feedEntryCheckBoxContributors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContributors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellid\">\n                <input type=\"checkbox\" name=\"id\" value=\"Id\" dojoAttachPoint=\"feedEntryCheckBoxId\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelId\"></label>\n            </td>\n            <td rowspan=\"2\" align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"close\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n\t\t</tr>\n\t\t<tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow2\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCellupdated\">\n                <input type=\"checkbox\" name=\"updated\" value=\"Updated\" dojoAttachPoint=\"feedEntryCheckBoxUpdated\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelUpdated\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellsummary\">\n                <input type=\"checkbox\" name=\"summary\" value=\"Summary\" dojoAttachPoint=\"feedEntryCheckBoxSummary\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelSummary\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontent\">\n                <input type=\"checkbox\" name=\"content\" value=\"Content\" dojoAttachPoint=\"feedEntryCheckBoxContent\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContent\"></label>\n            </td>\n        </tr>\n    </table>\n    \n    <table class=\"feedEntryViewerContainer\" border=\"0\" width=\"100%\">\n        <tr class=\"feedEntryViewerTitle\" dojoAttachPoint=\"entryTitleRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryTitleHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td>\n                        \t<select dojoAttachPoint=\"entryTitleSelect\" dojoAttachEvent=\"onchange:_switchEditor\" style=\"display: none\">\n                        \t\t<option value=\"text\">Text</option>\n\t\t\t\t\t\t\t\t<option value=\"html\">HTML</option>\n\t\t\t\t\t\t\t\t<option value=\"xhtml\">XHTML</option>\n                        \t</select>\n                        </td>\n                    </tr>\n                    <tr>\n                        <td colspan=\"2\" dojoAttachPoint=\"entryTitleNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerAuthor\" dojoAttachPoint=\"entryAuthorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryAuthorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryAuthorNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerContributor\" dojoAttachPoint=\"entryContributorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContributorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContributorNode\" class=\"feedEntryViewerContributorNames\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n        \n        <tr class=\"feedEntryViewerId\" dojoAttachPoint=\"entryIdRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryIdHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryIdNode\" class=\"feedEntryViewerIdText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerUpdated\" dojoAttachPoint=\"entryUpdatedRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryUpdatedHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryUpdatedNode\" class=\"feedEntryViewerUpdatedText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerSummary\" dojoAttachPoint=\"entrySummaryRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\" colspan=\"2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entrySummaryHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td>\n                        \t<select dojoAttachPoint=\"entrySummarySelect\" dojoAttachEvent=\"onchange:_switchEditor\" style=\"display: none\">\n                        \t\t<option value=\"text\">Text</option>\n\t\t\t\t\t\t\t\t<option value=\"html\">HTML</option>\n\t\t\t\t\t\t\t\t<option value=\"xhtml\">XHTML</option>\n                        \t</select>\n                        </td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entrySummaryNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerContent\" dojoAttachPoint=\"entryContentRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContentHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td>\n                        \t<select dojoAttachPoint=\"entryContentSelect\" dojoAttachEvent=\"onchange:_switchEditor\" style=\"display: none\">\n                        \t\t<option value=\"text\">Text</option>\n\t\t\t\t\t\t\t\t<option value=\"html\">HTML</option>\n\t\t\t\t\t\t\t\t<option value=\"xhtml\">XHTML</option>\n                        \t</select>\n                        </td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContentNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    </table>\n</div>\n","url:dojox/atom/widget/templates/PeopleEditor.html":"<div class=\"peopleEditor\">\n\t<table style=\"width: 100%\">\n\t\t<tbody dojoAttachPoint=\"peopleEditorEditors\"></tbody>\n\t</table>\n\t<span class=\"peopleEditorButton\" dojoAttachPoint=\"peopleEditorButton\" dojoAttachEvent=\"onclick:_add\"></span>\n</div>"}});
define("dojox/atom/widget/FeedEntryEditor",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/connect","dojo/_base/fx","dojo/_base/sniff","dojo/dom","dojo/dom-style","dojo/dom-construct","dijit/_Widget","dijit/_Templated","dijit/_Container","dijit/Editor","dijit/form/TextBox","dijit/form/SimpleTextarea","./FeedEntryViewer","../io/model","dojo/text!./templates/FeedEntryEditor.html","dojo/text!./templates/PeopleEditor.html","dojo/i18n!./nls/FeedEntryViewer","dojo/i18n!./nls/FeedEntryEditor","dojo/i18n!./nls/PeopleEditor","dojo/_base/declare"],function(_1,_2,_3,fx,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14){
_1.experimental("dojox.atom.widget.FeedEntryEditor");
var _15=_1.getObject("dojox.atom.widget",true);
_15.FeedEntryEditor=_1.declare(_e,{_contentEditor:null,_oldContent:null,_setObject:null,enableEdit:false,_contentEditorCreator:null,_editors:{},entryNewButton:null,_editable:false,templateString:_10,postCreate:function(){
if(this.entrySelectionTopic!==""){
this._subscriptions=[_1.subscribe(this.entrySelectionTopic,this,"_handleEvent")];
}
var _16=_12;
this.displayOptions.innerHTML=_16.displayOptions;
this.feedEntryCheckBoxLabelTitle.innerHTML=_16.title;
this.feedEntryCheckBoxLabelAuthors.innerHTML=_16.authors;
this.feedEntryCheckBoxLabelContributors.innerHTML=_16.contributors;
this.feedEntryCheckBoxLabelId.innerHTML=_16.id;
this.close.innerHTML=_16.close;
this.feedEntryCheckBoxLabelUpdated.innerHTML=_16.updated;
this.feedEntryCheckBoxLabelSummary.innerHTML=_16.summary;
this.feedEntryCheckBoxLabelContent.innerHTML=_16.content;
_16=_13;
this.doNew.innerHTML=_16.doNew;
this.edit.innerHTML=_16.edit;
this.save.innerHTML=_16.save;
this.cancel.innerHTML=_16.cancel;
},setEntry:function(_17,_18,_19){
if(this._entry!==_17){
this._editMode=false;
_19=false;
}else{
_19=true;
}
_15.FeedEntryEditor.superclass.setEntry.call(this,_17,_18);
this._editable=this._isEditable(_17);
if(!_19&&!this._editable){
_6.set(this.entryEditButton,"display","none");
_6.set(this.entrySaveCancelButtons,"display","none");
}
if(this._editable&&this.enableEdit){
if(!_19){
_6.set(this.entryEditButton,"display","");
if(this.enableMenuFade&&this.entrySaveCancelButton){
fx.fadeOut({node:this.entrySaveCancelButton,duration:250}).play();
}
}
}
},_toggleEdit:function(){
if(this._editable&&this.enableEdit){
_6.set(this.entryEditButton,"display","none");
_6.set(this.entrySaveCancelButtons,"display","");
this._editMode=true;
this.setEntry(this._entry,this._feed,true);
}
},_handleEvent:function(_1a){
if(_1a.source!=this&&_1a.action=="delete"&&_1a.entry&&_1a.entry==this._entry){
_6.set(this.entryEditButton,"display","none");
}
_15.FeedEntryEditor.superclass._handleEvent.call(this,_1a);
},_isEditable:function(_1b){
var _1c=false;
if(_1b&&_1b!==null&&_1b.links&&_1b.links!==null){
for(var x in _1b.links){
if(_1b.links[x].rel&&_1b.links[x].rel=="edit"){
_1c=true;
break;
}
}
}
return _1c;
},setTitle:function(_1d,_1e,_1f){
if(!_1e){
_15.FeedEntryEditor.superclass.setTitle.call(this,_1d,_1e,_1f);
if(_1f.title&&_1f.title.value&&_1f.title.value!==null){
this.setFieldValidity("title",true);
}
}else{
if(_1f.title&&_1f.title.value&&_1f.title.value!==null){
if(!this._toLoad){
this._toLoad=[];
}
this.entryTitleSelect.value=_1f.title.type;
var _20=this._createEditor(_1d,_1f.title,true,_1f.title.type==="html"||_1f.title.type==="xhtml");
_20.name="title";
this._toLoad.push(_20);
this.setFieldValidity("titleedit",true);
this.setFieldValidity("title",true);
}
}
},setAuthors:function(_21,_22,_23){
if(!_22){
_15.FeedEntryEditor.superclass.setAuthors.call(this,_21,_22,_23);
if(_23.authors&&_23.authors.length>0){
this.setFieldValidity("authors",true);
}
}else{
if(_23.authors&&_23.authors.length>0){
this._editors.authors=this._createPeopleEditor(this.entryAuthorNode,{data:_23.authors,name:"Author"});
this.setFieldValidity("authors",true);
}
}
},setContributors:function(_24,_25,_26){
if(!_25){
_15.FeedEntryEditor.superclass.setContributors.call(this,_24,_25,_26);
if(_26.contributors&&_26.contributors.length>0){
this.setFieldValidity("contributors",true);
}
}else{
if(_26.contributors&&_26.contributors.length>0){
this._editors.contributors=this._createPeopleEditor(this.entryContributorNode,{data:_26.contributors,name:"Contributor"});
this.setFieldValidity("contributors",true);
}
}
},setId:function(_27,_28,_29){
if(!_28){
_15.FeedEntryEditor.superclass.setId.call(this,_27,_28,_29);
if(_29.id&&_29.id!==null){
this.setFieldValidity("id",true);
}
}else{
if(_29.id&&_29.id!==null){
this._editors.id=this._createEditor(_27,_29.id);
this.setFieldValidity("id",true);
}
}
},setUpdated:function(_2a,_2b,_2c){
if(!_2b){
_15.FeedEntryEditor.superclass.setUpdated.call(this,_2a,_2b,_2c);
if(_2c.updated&&_2c.updated!==null){
this.setFieldValidity("updated",true);
}
}else{
if(_2c.updated&&_2c.updated!==null){
this._editors.updated=this._createEditor(_2a,_2c.updated);
this.setFieldValidity("updated",true);
}
}
},setSummary:function(_2d,_2e,_2f){
if(!_2e){
_15.FeedEntryEditor.superclass.setSummary.call(this,_2d,_2e,_2f);
if(_2f.summary&&_2f.summary.value&&_2f.summary.value!==null){
this.setFieldValidity("summary",true);
}
}else{
if(_2f.summary&&_2f.summary.value&&_2f.summary.value!==null){
if(!this._toLoad){
this._toLoad=[];
}
this.entrySummarySelect.value=_2f.summary.type;
var _30=this._createEditor(_2d,_2f.summary,true,_2f.summary.type==="html"||_2f.summary.type==="xhtml");
_30.name="summary";
this._toLoad.push(_30);
this.setFieldValidity("summaryedit",true);
this.setFieldValidity("summary",true);
}
}
},setContent:function(_31,_32,_33){
if(!_32){
_15.FeedEntryEditor.superclass.setContent.call(this,_31,_32,_33);
if(_33.content&&_33.content.value&&_33.content.value!==null){
this.setFieldValidity("content",true);
}
}else{
if(_33.content&&_33.content.value&&_33.content.value!==null){
if(!this._toLoad){
this._toLoad=[];
}
this.entryContentSelect.value=_33.content.type;
var _34=this._createEditor(_31,_33.content,true,_33.content.type==="html"||_33.content.type==="xhtml");
_34.name="content";
this._toLoad.push(_34);
this.setFieldValidity("contentedit",true);
this.setFieldValidity("content",true);
}
}
},_createEditor:function(_35,_36,_37,rte){
var _38;
var box;
if(!_36){
if(rte){
return {anchorNode:_35,entryValue:"",editor:null,generateEditor:function(){
var _39=document.createElement("div");
_39.innerHTML=this.entryValue;
this.anchorNode.appendChild(_39);
var _3a=new _b({},_39);
this.editor=_3a;
return _3a;
}};
}
if(_37){
_38=document.createElement("textarea");
_35.appendChild(_38);
_6.set(_38,"width","90%");
box=new _d({},_38);
}else{
_38=document.createElement("input");
_35.appendChild(_38);
_6.set(_38,"width","95%");
box=new _c({},_38);
}
box.attr("value","");
return box;
}
var _3b;
if(_36.value!==undefined){
_3b=_36.value;
}else{
if(_36.attr){
_3b=_36.attr("value");
}else{
_3b=_36;
}
}
if(rte){
if(_3b.indexOf("<")!=-1){
_3b=_3b.replace(/</g,"&lt;");
}
return {anchorNode:_35,entryValue:_3b,editor:null,generateEditor:function(){
var _3c=document.createElement("div");
_3c.innerHTML=this.entryValue;
this.anchorNode.appendChild(_3c);
var _3d=new _b({},_3c);
this.editor=_3d;
return _3d;
}};
}
if(_37){
_38=document.createElement("textarea");
_35.appendChild(_38);
_6.set(_38,"width","90%");
box=new _d({},_38);
}else{
_38=document.createElement("input");
_35.appendChild(_38);
_6.set(_38,"width","95%");
box=new _c({},_38);
}
box.attr("value",_3b);
return box;
},_switchEditor:function(_3e){
var _3f=null;
var _40=null;
var _41=null;
if(_4("ie")){
_40=_3e.srcElement;
}else{
_40=_3e.target;
}
if(_40===this.entryTitleSelect){
_41=this.entryTitleNode;
_3f="title";
}else{
if(_40===this.entrySummarySelect){
_41=this.entrySummaryNode;
_3f="summary";
}else{
_41=this.entryContentNode;
_3f="content";
}
}
var _42=this._editors[_3f];
var _43;
var _44;
if(_40.value==="text"){
if(_42.isInstanceOf(_b)){
_44=_42.attr("value",false);
_42.close(false,true);
_42.destroy();
while(_41.firstChild){
_7.destroy(_41.firstChild);
}
_43=this._createEditor(_41,{value:_44},true,false);
this._editors[_3f]=_43;
}
}else{
if(!_42.isInstanceOf(_b)){
_44=_42.attr("value");
_42.destroy();
while(_41.firstChild){
_7.destroy(_41.firstChild);
}
_43=this._createEditor(_41,{value:_44},true,true);
_43=_2.hitch(_43,_43.generateEditor)();
this._editors[_3f]=_43;
}
}
},_createPeopleEditor:function(_45,_46){
var _47=document.createElement("div");
_45.appendChild(_47);
return new _15.PeopleEditor(_46,_47);
},saveEdits:function(){
_6.set(this.entrySaveCancelButtons,"display","none");
_6.set(this.entryEditButton,"display","");
_6.set(this.entryNewButton,"display","");
var _48=false;
var _49;
var i;
var _4a;
var _4b;
var _4c;
var _4d;
if(!this._new){
_4b=this.getEntry();
if(this._editors.title&&(this._editors.title.attr("value")!=_4b.title.value||this.entryTitleSelect.value!=_4b.title.type)){
_49=this._editors.title.attr("value");
if(this.entryTitleSelect.value==="xhtml"){
_49=this._enforceXhtml(_49);
if(_49.indexOf("<div xmlns=\"http://www.w3.org/1999/xhtml\">")!==0){
_49="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_49+"</div>";
}
}
_4b.title=new _f.Content("title",_49,null,this.entryTitleSelect.value);
_48=true;
}
if(this._editors.id.attr("value")!=_4b.id){
_4b.id=this._editors.id.attr("value");
_48=true;
}
if(this._editors.summary&&(this._editors.summary.attr("value")!=_4b.summary.value||this.entrySummarySelect.value!=_4b.summary.type)){
_49=this._editors.summary.attr("value");
if(this.entrySummarySelect.value==="xhtml"){
_49=this._enforceXhtml(_49);
if(_49.indexOf("<div xmlns=\"http://www.w3.org/1999/xhtml\">")!==0){
_49="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_49+"</div>";
}
}
_4b.summary=new _f.Content("summary",_49,null,this.entrySummarySelect.value);
_48=true;
}
if(this._editors.content&&(this._editors.content.attr("value")!=_4b.content.value||this.entryContentSelect.value!=_4b.content.type)){
_49=this._editors.content.attr("value");
if(this.entryContentSelect.value==="xhtml"){
_49=this._enforceXhtml(_49);
if(_49.indexOf("<div xmlns=\"http://www.w3.org/1999/xhtml\">")!==0){
_49="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_49+"</div>";
}
}
_4b.content=new _f.Content("content",_49,null,this.entryContentSelect.value);
_48=true;
}
if(this._editors.authors){
if(_48){
_4b.authors=[];
_4c=this._editors.authors.getValues();
for(i in _4c){
if(_4c[i].name||_4c[i].email||_4c[i].uri){
_4b.addAuthor(_4c[i].name,_4c[i].email,_4c[i].uri);
}
}
}else{
var _4e=_4b.authors;
var _4f=function(_50,_51,uri){
for(i in _4e){
if(_4e[i].name===_50&&_4e[i].email===_51&&_4e[i].uri===uri){
return true;
}
}
return false;
};
_4c=this._editors.authors.getValues();
_4a=false;
for(i in _4c){
if(!_4f(_4c[i].name,_4c[i].email,_4c[i].uri)){
_4a=true;
break;
}
}
if(_4a){
_4b.authors=[];
for(i in _4c){
if(_4c[i].name||_4c[i].email||_4c[i].uri){
_4b.addAuthor(_4c[i].name,_4c[i].email,_4c[i].uri);
}
}
_48=true;
}
}
}
if(this._editors.contributors){
if(_48){
_4b.contributors=[];
_4d=this._editors.contributors.getValues();
for(i in _4d){
if(_4d[i].name||_4d[i].email||_4d[i].uri){
_4b.addAuthor(_4d[i].name,_4d[i].email,_4d[i].uri);
}
}
}else{
var _52=_4b.contributors;
var _53=function(_54,_55,uri){
for(i in _52){
if(_52[i].name===_54&&_52[i].email===_55&&_52[i].uri===uri){
return true;
}
}
return false;
};
_4d=this._editors.contributors.getValues();
_4a=false;
for(i in _4d){
if(_53(_4d[i].name,_4d[i].email,_4d[i].uri)){
_4a=true;
break;
}
}
if(_4a){
_4b.contributors=[];
for(i in _4d){
if(_4d[i].name||_4d[i].email||_4d[i].uri){
_4b.addContributor(_4d[i].name,_4d[i].email,_4d[i].uri);
}
}
_48=true;
}
}
}
if(_48){
_1.publish(this.entrySelectionTopic,[{action:"update",source:this,entry:_4b,callback:this._handleSave}]);
}
}else{
this._new=false;
_4b=new _f.Entry();
_49=this._editors.title.attr("value");
if(this.entryTitleSelect.value==="xhtml"){
_49=this._enforceXhtml(_49);
_49="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_49+"</div>";
}
_4b.setTitle(_49,this.entryTitleSelect.value);
_4b.id=this._editors.id.attr("value");
_4c=this._editors.authors.getValues();
for(i in _4c){
if(_4c[i].name||_4c[i].email||_4c[i].uri){
_4b.addAuthor(_4c[i].name,_4c[i].email,_4c[i].uri);
}
}
_4d=this._editors.contributors.getValues();
for(i in _4d){
if(_4d[i].name||_4d[i].email||_4d[i].uri){
_4b.addContributor(_4d[i].name,_4d[i].email,_4d[i].uri);
}
}
_49=this._editors.summary.attr("value");
if(this.entrySummarySelect.value==="xhtml"){
_49=this._enforceXhtml(_49);
_49="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_49+"</div>";
}
_4b.summary=new _f.Content("summary",_49,null,this.entrySummarySelect.value);
_49=this._editors.content.attr("value");
if(this.entryContentSelect.value==="xhtml"){
_49=this._enforceXhtml(_49);
_49="<div xmlns=\"http://www.w3.org/1999/xhtml\">"+_49+"</div>";
}
_4b.content=new _f.Content("content",_49,null,this.entryContentSelect.value);
_6.set(this.entryNewButton,"display","");
_1.publish(this.entrySelectionTopic,[{action:"post",source:this,entry:_4b}]);
}
this._editMode=false;
this.setEntry(_4b,this._feed,true);
},_handleSave:function(_56,_57){
this._editMode=false;
this.clear();
this.setEntry(_56,this.getFeed(),true);
},cancelEdits:function(){
this._new=false;
_6.set(this.entrySaveCancelButtons,"display","none");
if(this._editable){
_6.set(this.entryEditButton,"display","");
}
_6.set(this.entryNewButton,"display","");
this._editMode=false;
this.clearEditors();
this.setEntry(this.getEntry(),this.getFeed(),true);
},clear:function(){
this._editable=false;
this.clearEditors();
_15.FeedEntryEditor.superclass.clear.apply(this);
if(this._contentEditor){
this._contentEditor=this._setObject=this._oldContent=this._contentEditorCreator=null;
this._editors={};
}
},clearEditors:function(){
for(var key in this._editors){
if(this._editors[key].isInstanceOf(_b)){
this._editors[key].close(false,true);
}
this._editors[key].destroy();
}
this._editors={};
},_enforceXhtml:function(_58){
var _59=null;
if(_58){
var _5a=/<br>/g;
_59=_58.replace(_5a,"<br/>");
_59=this._closeTag(_59,"hr");
_59=this._closeTag(_59,"img");
}
return _59;
},_closeTag:function(_5b,tag){
var _5c="<"+tag;
var _5d=_5b.indexOf(_5c);
if(_5d!==-1){
while(_5d!==-1){
var _5e="";
var _5f=false;
for(var i=0;i<_5b.length;i++){
var c=_5b.charAt(i);
if(i<=_5d||_5f){
_5e+=c;
}else{
if(c===">"){
_5e+="/";
_5f=true;
}
_5e+=c;
}
}
_5b=_5e;
_5d=_5b.indexOf(_5c,_5d+1);
}
}
return _5b;
},_toggleNew:function(){
_6.set(this.entryNewButton,"display","none");
_6.set(this.entryEditButton,"display","none");
_6.set(this.entrySaveCancelButtons,"display","");
this.entrySummarySelect.value="text";
this.entryContentSelect.value="text";
this.entryTitleSelect.value="text";
this.clearNodes();
this._new=true;
var _60=_12;
var _61=new _15.EntryHeader({title:_60.title});
this.entryTitleHeader.appendChild(_61.domNode);
this._editors.title=this._createEditor(this.entryTitleNode,null);
this.setFieldValidity("title",true);
var _62=new _15.EntryHeader({title:_60.authors});
this.entryAuthorHeader.appendChild(_62.domNode);
this._editors.authors=this._createPeopleEditor(this.entryAuthorNode,{name:"Author"});
this.setFieldValidity("authors",true);
var _63=new _15.EntryHeader({title:_60.contributors});
this.entryContributorHeader.appendChild(_63.domNode);
this._editors.contributors=this._createPeopleEditor(this.entryContributorNode,{name:"Contributor"});
this.setFieldValidity("contributors",true);
var _64=new _15.EntryHeader({title:_60.id});
this.entryIdHeader.appendChild(_64.domNode);
this._editors.id=this._createEditor(this.entryIdNode,null);
this.setFieldValidity("id",true);
var _65=new _15.EntryHeader({title:_60.updated});
this.entryUpdatedHeader.appendChild(_65.domNode);
this._editors.updated=this._createEditor(this.entryUpdatedNode,null);
this.setFieldValidity("updated",true);
var _66=new _15.EntryHeader({title:_60.summary});
this.entrySummaryHeader.appendChild(_66.domNode);
this._editors.summary=this._createEditor(this.entrySummaryNode,null,true);
this.setFieldValidity("summaryedit",true);
this.setFieldValidity("summary",true);
var _67=new _15.EntryHeader({title:_60.content});
this.entryContentHeader.appendChild(_67.domNode);
this._editors.content=this._createEditor(this.entryContentNode,null,true);
this.setFieldValidity("contentedit",true);
this.setFieldValidity("content",true);
this._displaySections();
},_displaySections:function(){
_6.set(this.entrySummarySelect,"display","none");
_6.set(this.entryContentSelect,"display","none");
_6.set(this.entryTitleSelect,"display","none");
if(this.isFieldValid("contentedit")){
_6.set(this.entryContentSelect,"display","");
}
if(this.isFieldValid("summaryedit")){
_6.set(this.entrySummarySelect,"display","");
}
if(this.isFieldValid("titleedit")){
_6.set(this.entryTitleSelect,"display","");
}
_15.FeedEntryEditor.superclass._displaySections.apply(this);
if(this._toLoad){
for(var i in this._toLoad){
var _68;
if(this._toLoad[i].generateEditor){
_68=_2.hitch(this._toLoad[i],this._toLoad[i].generateEditor)();
}else{
_68=this._toLoad[i];
}
this._editors[this._toLoad[i].name]=_68;
this._toLoad[i]=null;
}
this._toLoad=null;
}
}});
_15.PeopleEditor=_1.declare([_8,_9,_a],{templateString:_11,_rows:[],_editors:[],_index:0,_numRows:0,postCreate:function(){
var _69=_14;
if(this.name){
if(this.name=="Author"){
this.peopleEditorButton.appendChild(document.createTextNode("["+_69.addAuthor+"]"));
}else{
if(this.name=="Contributor"){
this.peopleEditorButton.appendChild(document.createTextNode("["+_69.addContributor+"]"));
}
}
}else{
this.peopleEditorButton.appendChild(document.createTextNode("["+_69.add+"]"));
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
for(var _6a in this._editors[key]){
this._editors[key][_6a].destroy();
}
}
this._editors=[];
},_createEditors:function(_6b,_6c,uri,_6d,_6e){
var row=document.createElement("tr");
this.peopleEditorEditors.appendChild(row);
row.id="removeRow"+_6d;
var _6f=document.createElement("td");
_6f.setAttribute("align","right");
row.appendChild(_6f);
_6f.colSpan=2;
if(this._numRows>0){
var hr=document.createElement("hr");
_6f.appendChild(hr);
hr.id="hr"+_6d;
}
row=document.createElement("span");
_6f.appendChild(row);
row.className="peopleEditorButton";
_6.set(row,"font-size","x-small");
_3.connect(row,"onclick",this,"_removeEditor");
row.id="remove"+_6d;
_6f=document.createTextNode("[X]");
row.appendChild(_6f);
row=document.createElement("tr");
this.peopleEditorEditors.appendChild(row);
row.id="editorsRow"+_6d;
var _70=document.createElement("td");
row.appendChild(_70);
_6.set(_70,"width","20%");
_6f=document.createElement("td");
row.appendChild(_6f);
row=document.createElement("table");
_70.appendChild(row);
_6.set(row,"width","100%");
_70=document.createElement("tbody");
row.appendChild(_70);
row=document.createElement("table");
_6f.appendChild(row);
_6.set(row,"width","100%");
_6f=document.createElement("tbody");
row.appendChild(_6f);
this._editors[_6d]=[];
this._editors[_6d].push(this._createEditor(_6b,_6e+"name"+_6d,"Name:",_70,_6f));
this._editors[_6d].push(this._createEditor(_6c,_6e+"email"+_6d,"Email:",_70,_6f));
this._editors[_6d].push(this._createEditor(uri,_6e+"uri"+_6d,"URI:",_70,_6f));
},_createEditor:function(_71,id,_72,_73,_74){
var row=document.createElement("tr");
_73.appendChild(row);
var _75=document.createElement("label");
_75.setAttribute("for",id);
_75.appendChild(document.createTextNode(_72));
_73=document.createElement("td");
_73.appendChild(_75);
row.appendChild(_73);
row=document.createElement("tr");
_74.appendChild(row);
_74=document.createElement("td");
row.appendChild(_74);
var _76=document.createElement("input");
_76.setAttribute("id",id);
_74.appendChild(_76);
_6.set(_76,"width","95%");
var box=new _c({},_76);
box.attr("value",_71);
return box;
},_removeEditor:function(_77){
var _78=null;
if(_4("ie")){
_78=_77.srcElement;
}else{
_78=_77.target;
}
var id=_78.id;
id=id.substring(6);
for(var key in this._editors[id]){
this._editors[id][key].destroy();
}
var _79=_5.byId("editorsRow"+id);
var _7a=_79.parentNode;
_7a.removeChild(_79);
_79=_5.byId("removeRow"+id);
_7a=_79.parentNode;
_7a.removeChild(_79);
this._numRows--;
if(this._numRows===1&&_7a.firstChild.firstChild.firstChild.tagName.toLowerCase()==="hr"){
_79=_7a.firstChild.firstChild;
_79.removeChild(_79.firstChild);
}
this._editors[id]=null;
},_add:function(){
this._createEditors(null,null,null,this._index);
this._index++;
this._numRows++;
},getValues:function(){
var _7b=[];
for(var i in this._editors){
if(this._editors[i]){
_7b.push({name:this._editors[i][0].attr("value"),email:this._editors[i][1].attr("value"),uri:this._editors[i][2].attr("value")});
}
}
return _7b;
}});
return _15.FeedEntryEditor;
});
