//>>built
define("dojox/highlight/languages/xml", ["dojox/main", "../_base"], function(dojox){

	var XML_COMMENT = {
		className: 'comment',
		begin: '<!--', end: '-->'
	};
	
	var XML_ATTR = {
		className: 'attribute',
		begin: ' [a-zA-Z-]+\\s*=\\s*', end: '^',
		contains: ['value']
	};
	
	var XML_VALUE = {
		className: 'value',
		begin: '"', end: '"'
	};
	
	var dh = dojox.highlight, dhc = dh.constants;
	dh.languages.xml = {
		defaultMode: {
			contains: ['pi', 'comment', 'cdata', 'tag']
		},
		case_insensitive: true,
		modes: [
			{
				className: 'pi',
				begin: '<\\?', end: '\\?>',
				relevance: 10
			},
			XML_COMMENT,
			{
				className: 'cdata',
				begin: '<\\!\\[CDATA\\[', end: '\\]\\]>'
			},
			{
				className: 'tag',
				begin: '</?', end: '>',
				contains: ['title', 'tag_internal'],
				relevance: 1.5
			},
			{
				className: 'title',
				begin: '[A-Za-z:_][A-Za-z0-9\\._:-]+', end: '^',
				relevance: 0
			},
			{
				className: 'tag_internal',
				begin: '^', endsWithParent: true,
				contains: ['attribute'],
				relevance: 0,
				illegal: '[\\+\\.]'
			},
			XML_ATTR,
			XML_VALUE
		],
		// exporting constants
		XML_COMMENT: XML_COMMENT,
		XML_ATTR: XML_ATTR,
		XML_VALUE: XML_VALUE
	};

	return dh.languages.xml;

});
