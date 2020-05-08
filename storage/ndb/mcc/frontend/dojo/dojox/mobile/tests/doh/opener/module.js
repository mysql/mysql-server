define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.Opener", require.toUrl("./OpenerTests1.html"),999999);
	}
});