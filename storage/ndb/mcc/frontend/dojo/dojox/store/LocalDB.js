define(["./db/has!indexeddb?./db/IndexedDB:./db/SQL"],
	function(LocalDB){
	//	summary:
	//		The module defines an object store based on local database access
	//		./db/IndexDB if detected or ./db/SQL otherwise
	return LocalDB;
});
