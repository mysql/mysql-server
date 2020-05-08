dojo.provide("dojo.testsDOH._base._loader.hostenv_spidermonkey");

tests.register("testsDOH._base._loader.hostenv_spidermonkey",
	[
		function getText(t){
			var filePath = dojo.moduleUrl("testsDOH._base._loader", "getText.txt");
			var text = readText(filePath);
			t.assertEqual("dojo._getText() test data", text);
		}
	]
);
