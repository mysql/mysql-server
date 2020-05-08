define(['doh', '../../digests/_base', '../../digests/SHA512'], 
		function(doh, ded, SHA512){
	var message="abc";
	var vector = [
		0xddaf35a1, 0x93617aba, 0xcc417349, 0xae204131,
		0x12e6fa4e, 0x89a97ea2, 0x0a9eeee6, 0x4b55d39a, 
		0x2192992a, 0x274fc1a8, 0x36ba3c23, 0xa3feebbd,
		0x454d4423, 0x643ce80e, 0x2a9ac94f, 0xa54ca49f
	];

	var base64="3a81oZNherrMQXNJriBBMRLm+k6JqX6iCp7u5ktV05ohkpkqJ0/BqDa6PCOj/uu9RU1EI2Q86A4qmslPpUyknw==";
	var hex="ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f";
	var hmacKey="Jefe";
	var hmacData="what do ya want for nothing?";
	var hmacHex="164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737";

	doh.register("dojox.encoding.tests.digests.SHA512", [
		function testBase64Compute(t){
			t.assertEqual(base64, SHA512(message));
		},
		function testHexCompute(t){
			t.assertEqual(hex, SHA512(message, ded.outputTypes.Hex));
		},
		function testHmacCompute(t){
			t.assertEqual(hmacHex, SHA512.hmac(hmacData, hmacKey, ded.outputTypes.Hex));
		}
	]);
});
