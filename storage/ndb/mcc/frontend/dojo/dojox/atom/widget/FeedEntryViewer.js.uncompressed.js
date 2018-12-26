//>>built
require({cache:{
'url:dojox/atom/widget/templates/FeedEntryViewer.html':"<div class=\"feedEntryViewer\">\n    <table border=\"0\" width=\"100%\" class=\"feedEntryViewerMenuTable\" dojoAttachPoint=\"feedEntryViewerMenu\" style=\"display: none;\">\n        <tr width=\"100%\"  dojoAttachPoint=\"entryCheckBoxDisplayOptions\">\n            <td align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"displayOptions\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n        </tr>\n        <tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCelltitle\">\n                <input type=\"checkbox\" name=\"title\" value=\"Title\" dojoAttachPoint=\"feedEntryCheckBoxTitle\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelTitle\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellauthors\">\n                <input type=\"checkbox\" name=\"authors\" value=\"Authors\" dojoAttachPoint=\"feedEntryCheckBoxAuthors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelAuthors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontributors\">\n                <input type=\"checkbox\" name=\"contributors\" value=\"Contributors\" dojoAttachPoint=\"feedEntryCheckBoxContributors\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContributors\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellid\">\n                <input type=\"checkbox\" name=\"id\" value=\"Id\" dojoAttachPoint=\"feedEntryCheckBoxId\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelId\"></label>\n            </td>\n            <td rowspan=\"2\" align=\"right\">\n                <span class=\"feedEntryViewerMenu\" dojoAttachPoint=\"close\" dojoAttachEvent=\"onclick:_toggleOptions\"></span>\n            </td>\n\t\t</tr>\n\t\t<tr class=\"feedEntryViewerDisplayCheckbox\" dojoAttachPoint=\"entryCheckBoxRow2\" width=\"100%\" style=\"display: none;\">\n            <td dojoAttachPoint=\"feedEntryCellupdated\">\n                <input type=\"checkbox\" name=\"updated\" value=\"Updated\" dojoAttachPoint=\"feedEntryCheckBoxUpdated\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelUpdated\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellsummary\">\n                <input type=\"checkbox\" name=\"summary\" value=\"Summary\" dojoAttachPoint=\"feedEntryCheckBoxSummary\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelSummary\"></label>\n            </td>\n            <td dojoAttachPoint=\"feedEntryCellcontent\">\n                <input type=\"checkbox\" name=\"content\" value=\"Content\" dojoAttachPoint=\"feedEntryCheckBoxContent\" dojoAttachEvent=\"onclick:_toggleCheckbox\"/>\n\t\t\t\t<label for=\"title\" dojoAttachPoint=\"feedEntryCheckBoxLabelContent\"></label>\n            </td>\n        </tr>\n    </table>\n    \n    <table class=\"feedEntryViewerContainer\" border=\"0\" width=\"100%\">\n        <tr class=\"feedEntryViewerTitle\" dojoAttachPoint=\"entryTitleRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryTitleHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryTitleNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerAuthor\" dojoAttachPoint=\"entryAuthorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryAuthorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryAuthorNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n\n        <tr class=\"feedEntryViewerContributor\" dojoAttachPoint=\"entryContributorRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContributorHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContributorNode\" class=\"feedEntryViewerContributorNames\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n        \n        <tr class=\"feedEntryViewerId\" dojoAttachPoint=\"entryIdRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryIdHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryIdNode\" class=\"feedEntryViewerIdText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerUpdated\" dojoAttachPoint=\"entryUpdatedRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryUpdatedHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryUpdatedNode\" class=\"feedEntryViewerUpdatedText\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerSummary\" dojoAttachPoint=\"entrySummaryRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entrySummaryHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entrySummaryNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    \n        <tr class=\"feedEntryViewerContent\" dojoAttachPoint=\"entryContentRow\" style=\"display: none;\">\n            <td>\n                <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n                    <tr class=\"graphic-tab-lgray\">\n\t\t\t\t\t\t<td class=\"lp2\">\n\t\t\t\t\t\t\t<span class=\"lp\" dojoAttachPoint=\"entryContentHeader\"></span>\n\t\t\t\t\t\t</td>\n                    </tr>\n                    <tr>\n                        <td dojoAttachPoint=\"entryContentNode\">\n                        </td>\n                    </tr>\n                </table>\n            </td>\n        </tr>\n    </table>\n</div>\n",
'url:dojox/atom/widget/templates/EntryHeader.html':"<span dojoAttachPoint=\"entryHeaderNode\" class=\"entryHeaderNode\"></span>\n"}});
define("dojox/atom/widget/FeedEntryViewer", [
	"dojo/_base/kernel",
	"dojo/_base/connect",
	"dojo/_base/declare",
	"dojo/_base/fx",
	"dojo/_base/array",
	"dojo/dom-style",
	"dojo/dom-construct",
	"dijit/_Widget",
	"dijit/_Templated",
	"dijit/_Container",
	"dijit/layout/ContentPane",
	"../io/Connection",
	"dojo/text!./templates/FeedEntryViewer.html",
	"dojo/text!./templates/EntryHeader.html",
	"dojo/i18n!./nls/FeedEntryViewer"
], function (dojo, connect, declare, fx, arrayUtil, domStyle, domConstruct, _Widget, _Templated, _Container, ContentPane, Connection, template, headerTemplate, i18nViewer) {

dojo.experimental("dojox.atom.widget.FeedEntryViewer");

var widget = dojo.getObject("dojox.atom.widget", true);

widget.FeedEntryViewer = dojo.declare(/*===== "dojox.atom.widget.FeedEntryViewer", =====*/ [_Widget, _Templated, _Container],{
	//	summary:
	//		An ATOM feed entry editor for publishing updated ATOM entries, or viewing non-editable entries.
	//	description:
	//		An ATOM feed entry editor for publishing updated ATOM entries, or viewing non-editable entries.
	entrySelectionTopic: "",	//The topic to listen on for entries to edit.

	_validEntryFields: {},		//The entry fields that were present on the entry and are being displayed.
								//This works in conjuntion with what is selected to be displayed.
	displayEntrySections: "", //What current sections of the entries to display as a comma separated list.
	_displayEntrySections: null,
	
	//Control options for the display options menu.
	enableMenu: false,
	enableMenuFade: false,
	_optionButtonDisplayed: true,

	//Templates for the HTML rendering.  Need to figure these out better, admittedly.
	templateString: template,
	
	_entry: null, //The entry that is being viewed/edited.
	_feed: null, //The feed the entry came from.

	_editMode: false, //Flag denoting the state of the widget, in edit mode or not.
	
	postCreate: function(){
		if(this.entrySelectionTopic !== ""){
			this._subscriptions = [dojo.subscribe(this.entrySelectionTopic, this, "_handleEvent")];
		}
		var _nlsResources = i18nViewer;
		this.displayOptions.innerHTML = _nlsResources.displayOptions;
		this.feedEntryCheckBoxLabelTitle.innerHTML = _nlsResources.title;
		this.feedEntryCheckBoxLabelAuthors.innerHTML = _nlsResources.authors;
		this.feedEntryCheckBoxLabelContributors.innerHTML = _nlsResources.contributors;
		this.feedEntryCheckBoxLabelId.innerHTML = _nlsResources.id;
		this.close.innerHTML = _nlsResources.close;
		this.feedEntryCheckBoxLabelUpdated.innerHTML = _nlsResources.updated;
		this.feedEntryCheckBoxLabelSummary.innerHTML = _nlsResources.summary;
		this.feedEntryCheckBoxLabelContent.innerHTML = _nlsResources.content;
	},

	startup: function(){
		if(this.displayEntrySections === ""){
			this._displayEntrySections = ["title","authors","contributors","summary","content","id","updated"];
		}else{
			this._displayEntrySections = this.displayEntrySections.split(",");
		}
		this._setDisplaySectionsCheckboxes();

		if(this.enableMenu){
			domStyle.set(this.feedEntryViewerMenu, 'display', '');
			if(this.entryCheckBoxRow && this.entryCheckBoxRow2){
				if(this.enableMenuFade){
					fx.fadeOut({node: this.entryCheckBoxRow,duration: 250}).play();
					fx.fadeOut({node: this.entryCheckBoxRow2,duration: 250}).play();
				}
			}
		}
	},

	clear: function(){
		//	summary:
		//		Function to clear the state of the widget.
		//	description:
		//		Function to clear the state of the widget.
		this.destroyDescendants();
		this._entry=null;
		this._feed=null;
		this.clearNodes();
	},
	
	clearNodes: function(){
		//	summary:
		//		Function to clear all the display nodes for the ATOM entry from the viewer.
		//	description:
		//		Function to clear all the display nodes for the ATOM entry from the viewer.

		arrayUtil.forEach([
			"entryTitleRow", "entryAuthorRow", "entryContributorRow", "entrySummaryRow", "entryContentRow",
			"entryIdRow", "entryUpdatedRow"
			], function(node){
				domStyle.set(this[node], "display", "none");
			}, this);

		arrayUtil.forEach([
			"entryTitleNode", "entryTitleHeader", "entryAuthorHeader", "entryContributorHeader",
			"entryContributorNode", "entrySummaryHeader", "entrySummaryNode", "entryContentHeader",
			"entryContentNode", "entryIdNode", "entryIdHeader", "entryUpdatedHeader", "entryUpdatedNode"
			], function(part){
				while(this[part].firstChild){
					domConstruct.destroy(this[part].firstChild);
				}
			}
		,this);
		
	},

	setEntry: function(/*object*/entry, /*object*/feed, /*boolean*/leaveMenuState){
		//	summary:
		//		Function to set the current entry that is being edited.
		//	description:
		//		Function to set the current entry that is being edited.
		//
		//	entry:
		//		Instance of dojox.atom.io.model.Entry to display for reading/editing.
		this.clear();
		this._validEntryFields = {};
		this._entry = entry;
		this._feed = feed;

		if(entry !== null){
			// Handle the title.
			if(this.entryTitleHeader){
				this.setTitleHeader(this.entryTitleHeader, entry);
			}
			
			if(this.entryTitleNode){
				this.setTitle(this.entryTitleNode, this._editMode, entry);
			}

			if(this.entryAuthorHeader){
				this.setAuthorsHeader(this.entryAuthorHeader, entry);
			}

			if(this.entryAuthorNode){
				this.setAuthors(this.entryAuthorNode, this._editMode, entry);
			}
			
			if(this.entryContributorHeader){
				this.setContributorsHeader(this.entryContributorHeader, entry);
			}

			if(this.entryContributorNode){
				this.setContributors(this.entryContributorNode, this._editMode, entry);
			}

			if(this.entryIdHeader){
				this.setIdHeader(this.entryIdHeader, entry);
			}

			if(this.entryIdNode){
				this.setId(this.entryIdNode, this._editMode, entry);
			}

			if(this.entryUpdatedHeader){
				this.setUpdatedHeader(this.entryUpdatedHeader, entry);
			}

			if(this.entryUpdatedNode){
				this.setUpdated(this.entryUpdatedNode, this._editMode, entry);
			}

			if(this.entrySummaryHeader){
				this.setSummaryHeader(this.entrySummaryHeader, entry);
			}

			if(this.entrySummaryNode){
				this.setSummary(this.entrySummaryNode, this._editMode, entry);
			}

			if(this.entryContentHeader){
				this.setContentHeader(this.entryContentHeader, entry);
			}

			if(this.entryContentNode){
				this.setContent(this.entryContentNode, this._editMode, entry);
			}
		}
		this._displaySections();
	},

	setTitleHeader: function(/*DOM node*/titleHeaderNode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the title header node in the template to some value.
		//	description:
		//		Function to set the contents of the title header node in the template to some value.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	titleAchorNode:
		//		The DOM node to attach the title data to.
		//	editMode:
		//		Boolean to indicate if the display should be in edit mode or not.
		//	entry:
		//		The Feed Entry to work with.
		//
		if(entry.title && entry.title.value && entry.title.value !== null){
			var _nlsResources = i18nViewer;
			var titleHeader = new widget.EntryHeader({title: _nlsResources.title});
			titleHeaderNode.appendChild(titleHeader.domNode);
		}
	},

	setTitle: function(titleAnchorNode, editMode, entry){
		//	summary:
		//		Function to set the contents of the title node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the title node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	titleAchorNode:
		//		The DOM node to attach the title data to.
		//	editMode:
		//		Boolean to indicate if the display should be in edit mode or not.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.title && entry.title.value && entry.title.value !== null){
			if(entry.title.type == "text"){
				var titleNode = document.createTextNode(entry.title.value);
				titleAnchorNode.appendChild(titleNode);
			}else{
				var titleViewNode = document.createElement("span");
				var titleView = new ContentPane({refreshOnShow: true, executeScripts: false}, titleViewNode);
				titleView.attr('content', entry.title.value);
				titleAnchorNode.appendChild(titleView.domNode);
			}
			this.setFieldValidity("title", true);
		}
	},

	setAuthorsHeader: function(/*DOM node*/authorHeaderNode, /*object*/entry){
		//	summary:
		//		Function to set the title format for the authors section of the author row in the template to some value from the entry.
		//	description:
		//		Function to set the title format for the authors section of the author row in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the author data is filled out from an entry.
		//
		//	authorHeaderNode:
		//		The DOM node to attach the author section header data to.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.authors && entry.authors.length > 0){
			var _nlsResources = i18nViewer;
			var authorHeader = new widget.EntryHeader({title: _nlsResources.authors});
			authorHeaderNode.appendChild(authorHeader.domNode);
		}
	},

	setAuthors: function(/*DOM node*/authorsAnchorNode, /*boolean*/editMode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the author node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the author node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	authorsAchorNode:
		//		The DOM node to attach the author data to.
		//	editMode:
		//		Boolean to indicate if the display should be in edit mode or not.
		//	entry:
		//		The Feed Entry to work with.
		authorsAnchorNode.innerHTML = "";
		if(entry.authors && entry.authors.length > 0){
			for(var i in entry.authors){
				if(entry.authors[i].name){
					var anchor = authorsAnchorNode;
					if(entry.authors[i].uri){
						var link = document.createElement("a");
						anchor.appendChild(link);
						link.href = entry.authors[i].uri;
						anchor = link;
					}
					var name = entry.authors[i].name;
					if(entry.authors[i].email){
						name = name + " (" + entry.authors[i].email + ")";
					}
					var authorNode = document.createTextNode(name);
					anchor.appendChild(authorNode);
					var breakNode = document.createElement("br");
					authorsAnchorNode.appendChild(breakNode);
					this.setFieldValidity("authors", true);
				}
			}
		}
	},

	setContributorsHeader: function(/*DOM node*/contributorsHeaderNode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the contributor header node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the contributor header node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	contributorsHeaderNode:
		//		The DOM node to attach the contributor title to.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.contributors && entry.contributors.length > 0){
			var _nlsResources = i18nViewer;
			var contributorHeader = new widget.EntryHeader({title: _nlsResources.contributors});
			contributorsHeaderNode.appendChild(contributorHeader.domNode);
		}
	},


	setContributors: function(/*DOM node*/contributorsAnchorNode, /*boolean*/editMode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the contributor node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the contributor node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	contributorsAnchorNode:
		//		The DOM node to attach the contributor data to.
		//	editMode:
		//		Boolean to indicate if the display should be in edit mode or not.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.contributors && entry.contributors.length > 0){
			for(var i in entry.contributors){
				var contributorNode = document.createTextNode(entry.contributors[i].name);
				contributorsAnchorNode.appendChild(contributorNode);
				var breakNode = document.createElement("br");
				contributorsAnchorNode.appendChild(breakNode);
				this.setFieldValidity("contributors", true);
			}
		}
	},

				 
	setIdHeader: function(/*DOM node*/idHeaderNode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the ID  node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the ID node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	idAnchorNode:
		//		The DOM node to attach the ID data to.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.id && entry.id !== null){
			var _nlsResources = i18nViewer;
			var idHeader = new widget.EntryHeader({title: _nlsResources.id});
			idHeaderNode.appendChild(idHeader.domNode);
		}
	},


	setId: function(/*DOM node*/idAnchorNode, /*boolean*/editMode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the ID  node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the ID node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	idAnchorNode:
		//		The DOM node to attach the ID data to.
		// 	editMode:
		//		Boolean to indicate if the display should be in edit mode or not.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.id && entry.id !== null){
			var idNode = document.createTextNode(entry.id);
			idAnchorNode.appendChild(idNode);
			this.setFieldValidity("id", true);
		}
	},
	
	setUpdatedHeader: function(/*DOM node*/updatedHeaderNode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the updated header node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the updated header node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	updatedHeaderNode:
		//		The DOM node to attach the updated header data to.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.updated && entry.updated !== null){
			var _nlsResources = i18nViewer;
			var updatedHeader = new widget.EntryHeader({title: _nlsResources.updated});
			updatedHeaderNode.appendChild(updatedHeader.domNode);
		}
	},

	setUpdated: function(/*DOM node*/updatedAnchorNode, /*boolean*/editMode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the updated  node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the updated node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	updatedAnchorNode:
		//		The DOM node to attach the udpated data to.
		//	editMode:
		//		Boolean to indicate if the display should be in edit mode or not.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.updated && entry.updated !== null){
			var updatedNode = document.createTextNode(entry.updated);
			updatedAnchorNode.appendChild(updatedNode);
			this.setFieldValidity("updated", true);
		}
	},

	setSummaryHeader: function(/*DOM node*/summaryHeaderNode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the summary  node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the summary node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	summaryHeaderNode:
		//		The DOM node to attach the summary title to.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.summary && entry.summary.value && entry.summary.value !== null){
			var _nlsResources = i18nViewer;
			var summaryHeader = new widget.EntryHeader({title: _nlsResources.summary});
			summaryHeaderNode.appendChild(summaryHeader.domNode);
		}
	},


	setSummary: function(/*DOM node*/summaryAnchorNode, /*boolean*/editMode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the summary  node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the summary node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	summaryAnchorNode:
		//		The DOM node to attach the summary data to.
		//	editMode:
		//		Boolean to indicate if the display should be in edit mode or not.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.summary && entry.summary.value && entry.summary.value !== null){
			var summaryViewNode = document.createElement("span");
			var summaryView = new ContentPane({refreshOnShow: true, executeScripts: false}, summaryViewNode);
			summaryView.attr('content', entry.summary.value);
			summaryAnchorNode.appendChild(summaryView.domNode);
			this.setFieldValidity("summary", true);
		}
	},

	setContentHeader: function(/*DOM node*/contentHeaderNode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the content node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the content node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	contentHeaderNode:
		//		The DOM node to attach the content data to.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.content && entry.content.value && entry.content.value !== null){
			var _nlsResources = i18nViewer;
			var contentHeader = new widget.EntryHeader({title: _nlsResources.content});
			contentHeaderNode.appendChild(contentHeader.domNode);
		}
	},

	setContent: function(/*DOM node*/contentAnchorNode, /*boolean*/editMode, /*object*/entry){
		//	summary:
		//		Function to set the contents of the content node in the template to some value from the entry.
		//	description:
		//		Function to set the contents of the content node in the template to some value from the entry.
		//		This exists specifically so users can over-ride how the title data is filled out from an entry.
		//
		//	contentAnchorNode:
		//		The DOM node to attach the content data to.
		//	editMode:
		//		Boolean to indicate if the display should be in edit mode or not.
		//	entry:
		//		The Feed Entry to work with.
		if(entry.content && entry.content.value && entry.content.value !== null){
			var contentViewNode = document.createElement("span");
			var contentView = new ContentPane({refreshOnShow: true, executeScripts: false},contentViewNode);
			contentView.attr('content', entry.content.value);
			contentAnchorNode.appendChild(contentView.domNode);
			this.setFieldValidity("content", true);
		}
	},


	_displaySections: function(){
		//	summary:
		//		Internal function for determining which sections of the view to actually display.
		//	description:
		//		Internal function for determining which sections of the view to actually display.
		//
		//	returns:
		//		Nothing.
		domStyle.set(this.entryTitleRow, 'display', 'none');
		domStyle.set(this.entryAuthorRow, 'display', 'none');
		domStyle.set(this.entryContributorRow, 'display', 'none');
		domStyle.set(this.entrySummaryRow, 'display', 'none');
		domStyle.set(this.entryContentRow, 'display', 'none');
		domStyle.set(this.entryIdRow, 'display', 'none');
		domStyle.set(this.entryUpdatedRow, 'display', 'none');

		for(var i in this._displayEntrySections){
			var section = this._displayEntrySections[i].toLowerCase();
			if(section === "title" && this.isFieldValid("title")){
				domStyle.set(this.entryTitleRow, 'display', '');
			}
			if(section === "authors" && this.isFieldValid("authors")){
				domStyle.set(this.entryAuthorRow, 'display', '');
			}
			if(section === "contributors" && this.isFieldValid("contributors")){
				domStyle.set(this.entryContributorRow, 'display', '');
			}
			if(section === "summary" && this.isFieldValid("summary")){
				domStyle.set(this.entrySummaryRow, 'display', '');
			}
			if(section === "content" && this.isFieldValid("content")){
				domStyle.set(this.entryContentRow, 'display', '');
			}
			if(section === "id" && this.isFieldValid("id")){
				domStyle.set(this.entryIdRow, 'display', '');
			}
			if(section === "updated" && this.isFieldValid("updated")){
				domStyle.set(this.entryUpdatedRow, 'display', '');
			}

		}
	},

	setDisplaySections: function(/*array*/sectionsArray){
		//	summary:
		//		Function for setting which sections of the entry should be displayed.
		//	description:
		//		Function for setting which sections of the entry should be displayed.
		//
		//	sectionsArray:
		//		Array of string names that indicate which sections to display.
		//
		//	returns:
		//		Nothing.
		if(sectionsArray !== null){
			this._displayEntrySections = sectionsArray;
			this._displaySections();
		}else{
			this._displayEntrySections = ["title","authors","contributors","summary","content","id","updated"];
		}
	},

	_setDisplaySectionsCheckboxes: function(){
		//	summary:
		//		Internal function for setting which checkboxes on the display are selected.
		//	description:
		//		Internal function for setting which checkboxes on the display are selected.
		//
		//	returns:
		//		Nothing.
		var items = ["title","authors","contributors","summary","content","id","updated"];
		for(var i in items){
			if(arrayUtil.indexOf(this._displayEntrySections, items[i]) == -1){
				domStyle.set(this["feedEntryCell"+items[i]], 'display', 'none');
			}else{
				this["feedEntryCheckBox"+items[i].substring(0,1).toUpperCase()+items[i].substring(1)].checked=true;
			}
		}
	},

	_readDisplaySections: function(){
		//	summary:
		//		Internal function for reading what is currently checked for display and generating the display list from it.
		//	description:
		//		Internal function for reading what is currently checked for display and generating the display list from it.
		//
		//	returns:
		//		Nothing.
		var checkedList = [];

		if(this.feedEntryCheckBoxTitle.checked){
			checkedList.push("title");
		}
		if(this.feedEntryCheckBoxAuthors.checked){
			checkedList.push("authors");
		}
		if(this.feedEntryCheckBoxContributors.checked){
			checkedList.push("contributors");
		}
		if(this.feedEntryCheckBoxSummary.checked){
			checkedList.push("summary");
		}
		if(this.feedEntryCheckBoxContent.checked){
			checkedList.push("content");
		}
		if(this.feedEntryCheckBoxId.checked){
			checkedList.push("id");
		}
		if(this.feedEntryCheckBoxUpdated.checked){
			checkedList.push("updated");
		}
		this._displayEntrySections = checkedList;
	},

	_toggleCheckbox: function(/*object*/checkBox){
		//	summary:
		//		Internal function for determining of a particular entry is editable.
		//	description:
		//		Internal function for determining of a particular entry is editable.
		//		This is used for determining if the delete action should be displayed or not.
		//
		//	checkBox:
		//		The checkbox object to toggle the selection on.
		//
		//	returns:
		//		Nothing
		if(checkBox.checked){
			checkBox.checked=false;
		}else{
			checkBox.checked=true;
		}
		this._readDisplaySections();
		this._displaySections();
	},

	_toggleOptions: function(/*object*/checkBox){
		//	summary:
		//		Internal function for determining of a particular entry is editable.
		//	description:
		//		Internal function for determining of a particular entry is editable.
		//		This is used for determining if the delete action should be displayed or not.
		//
		//	checkBox:
		//		The checkbox object to toggle the selection on.
		//
		//	returns:
		//		Nothing
		if(this.enableMenu){
			var fade = null;
			var anim;
			var anim2;
			if(this._optionButtonDisplayed){
				if(this.enableMenuFade){
					anim = fx.fadeOut({node: this.entryCheckBoxDisplayOptions,duration: 250});
					connect.connect(anim, "onEnd", this, function(){
						domStyle.set(this.entryCheckBoxDisplayOptions, 'display', 'none');
						domStyle.set(this.entryCheckBoxRow, 'display', '');
						domStyle.set(this.entryCheckBoxRow2, 'display', '');
						fx.fadeIn({node: this.entryCheckBoxRow, duration: 250}).play();
						fx.fadeIn({node: this.entryCheckBoxRow2, duration: 250}).play();
					});
					anim.play();
				}else{
					domStyle.set(this.entryCheckBoxDisplayOptions, 'display', 'none');
					domStyle.set(this.entryCheckBoxRow, 'display', '');
					domStyle.set(this.entryCheckBoxRow2, 'display', '');
				}
				this._optionButtonDisplayed=false;
			}else{
				if(this.enableMenuFade){
					anim = fx.fadeOut({node: this.entryCheckBoxRow,duration: 250});
					anim2 = fx.fadeOut({node: this.entryCheckBoxRow2,duration: 250});
					connect.connect(anim, "onEnd", this, function(){
						domStyle.set(this.entryCheckBoxRow, 'display', 'none');
						domStyle.set(this.entryCheckBoxRow2, 'display', 'none');
						domStyle.set(this.entryCheckBoxDisplayOptions, 'display', '');
						fx.fadeIn({node: this.entryCheckBoxDisplayOptions, duration: 250}).play();
					});
					anim.play();
					anim2.play();
				}else{
					domStyle.set(this.entryCheckBoxRow, 'display', 'none');
					domStyle.set(this.entryCheckBoxRow2, 'display', 'none');
					domStyle.set(this.entryCheckBoxDisplayOptions, 'display', '');
				}
				this._optionButtonDisplayed=true;
			}
		}
	},

	_handleEvent: function(/*object*/entrySelectionEvent){
		//	summary:
		//		Internal function for listening to a topic that will handle entry notification.
		//	description:
		//		Internal function for listening to a topic that will handle entry notification.
		//
		//	entrySelectionEvent:
		//		The topic message containing the entry that was selected for view.
		//
		//	returns:
		//		Nothing.
		if(entrySelectionEvent.source != this){
			if(entrySelectionEvent.action == "set" && entrySelectionEvent.entry){
				this.setEntry(entrySelectionEvent.entry, entrySelectionEvent.feed);
			}else if(entrySelectionEvent.action == "delete" && entrySelectionEvent.entry && entrySelectionEvent.entry == this._entry){
				this.clear();
			}
		}
	},

	setFieldValidity: function(/*string*/field, /*boolean*/isValid){
		//	summary:
		//		Function to set whether a field in the view is valid and displayable.
		//	description:
		//		Function to set whether a field in the view is valid and displayable.
		//		This is needed for over-riding of the set* functions and customization of how data is displayed in the attach point.
		//		So if custom implementations use their own display logic, they can still enable the field.
		//
		//	field:
		//		The field name to set the valid parameter on.  Such as 'content', 'id', etc.
		//	isValid:
		//		Flag denoting if the field is valid or not.
		//
		//	returns:
		//		Nothing.
		if(field){
			var lowerField = field.toLowerCase();
			this._validEntryFields[field] = isValid;
		}
	},
	
	isFieldValid: function(/*string*/field){
		//	summary:
		//		Function to return if a displayable field is valid or not
		//	description:
		//		Function to return if a displayable field is valid or not
		//
		//	field:
		//		The field name to get the valid parameter of.  Such as 'content', 'id', etc.
		//
		//	returns:
		//		boolean denoting if the field is valid and set.
		return this._validEntryFields[field.toLowerCase()];
	},

	getEntry: function(){
		return this._entry;
	},

	getFeed: function(){
		 return this._feed;
	},

	destroy: function(){
		this.clear();
		arrayUtil.forEach(this._subscriptions, dojo.unsubscribe);
	}
});

widget.EntryHeader = dojo.declare(/*===== "dojox.atom.widget.EntryHeader", =====*/ [_Widget, _Templated, _Container],{
	//	summary:
	//		Widget representing a header in a FeedEntryViewer/Editor
	//	description:
	//		Widget representing a header in a FeedEntryViewer/Editor
	title: "",
	templateString: headerTemplate,

	postCreate: function(){
		this.setListHeader();
	},

	setListHeader: function(/*string*/title){
		this.clear();
		if(title){
			this.title = title;
		}
		var textNode = document.createTextNode(this.title);
		this.entryHeaderNode.appendChild(textNode);
	},

	clear: function(){
		this.destroyDescendants();
		 if(this.entryHeaderNode){
			 for(var i = 0; i < this.entryHeaderNode.childNodes.length; i++){
				 this.entryHeaderNode.removeChild(this.entryHeaderNode.childNodes[i]);
			 }
		 }
	},

	destroy: function(){
		this.clear();
	}
});

return widget.FeedEntryViewer;
});
