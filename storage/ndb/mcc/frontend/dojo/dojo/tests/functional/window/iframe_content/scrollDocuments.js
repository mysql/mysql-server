define([
	'dojo/string',
	'dojo/_base/array',
	'dojo/sniff',
	'dojo/text!tests/functional/window/iframe_content/scrollTemplate.html',
	// test document templates
	'dojo/text!tests/functional/window/iframe_content/htmlPadding.html',
	'dojo/text!tests/functional/window/iframe_content/absoluteTd.html',
	'dojo/text!tests/functional/window/iframe_content/innerScrollable.html',
	'dojo/text!tests/functional/window/iframe_content/table.html',
	'dojo/text!tests/functional/window/iframe_content/noScroll.html',
	'dojo/text!tests/functional/window/iframe_content/oversizedContent.html'
], function (
	string,
	arrayUtil,
	has,
	documentTemplate,
	paddingTemplate,
	absoluteTdTemplate,
	innerScrollableTemplate,
	tableTemplate,
	noScrollTemplate,
	oversizedTemplate
) {
	// Responsible for constructing iFrame source documents for various scroll tests.

	var docTypes = {
		strict: '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">' ,
		loose: '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">',
		quirks: ''
	};

	var directions = [ 'rtl', 'ltr' ];
	var documents = [];

	function defineLayoutScenario(name, label, styles, content) {
		var type;

		for (type in docTypes) {
			arrayUtil.forEach(directions, function (direction) {
				var html = string.substitute(documentTemplate, {
					docType: docTypes[type],
					direction: direction,
					styles: styles,
					content: content
				});

				documents.push({
					label: label + ' ' + type + ' ' + direction,
					html: html,
					id: name + '_' + type + '_' + direction
				});
			});
		}
	}

	// Define different document layout testing senarios.

	defineLayoutScenario(
		'padding',
		'HTML/BODY padding',
		'HTML, BODY { padding:50px 9px; }' +
		'HTML { overflow-x:hidden !important; /*IE6*/ }',
		paddingTemplate);

	defineLayoutScenario(
		'oversized',
		'Oversized Content',
		'',
		oversizedTemplate);

	defineLayoutScenario(
		'absoluteTd',
		'position:absolute TD content',
		'html, body {height: 700px;}',
		absoluteTdTemplate);

	defineLayoutScenario(
		'innerScrollable',
		'Inner scrollable content with scrollbars',
		'HTML { overflow:hidden !important; /*IE6*/ }' +
		'BODY { padding: 10px; }' +
		has('opera') ? 'TABLE {float: left}' : '',
		innerScrollableTemplate);

	defineLayoutScenario(
		'table',
		'Table',
		'HTML { overflow-x:hidden !important; /*IE6*/ }',
		tableTemplate);

	defineLayoutScenario(
		'noScroll',
		'No scroll',
		'HTML { overflow-x:hidden !important; /*IE6*/ }',
		noScrollTemplate);

	return documents;
});
