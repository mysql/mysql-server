define(['dojo/has', 'dojo/sniff'], function(has){
	//	summary:
	//		has() test for indexeddb. 
	has.add('indexeddb', !!(window.indexedDB || window.webkitIndexedDB || window.mozIndexedDB));
	return has;
});