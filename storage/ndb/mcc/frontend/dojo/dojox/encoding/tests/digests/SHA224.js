define(['doh', '../../digests/_base', '../../digests/SHA224', "../../digests/_sha-32"], function(doh, ded, SHA224, sha32){
	var message="abc";
	//	test vector, from http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/SHA224.pdf
	var wa = [
		0x23097d22, 0x3405d822, 0x8642a477, 0xbda255b3, 0x2aadbce4, 0xbda0b3f7, 0xe36c9da7
	];
	var base64="Iwl9IjQF2CKGQqR3vaJVsyqtvOS9oLP342ydpw==";
	var hex="23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7";
	var hmacKey="Jefe";
	var hmacData="what do ya want for nothing?";
	var hmacHex="a30e01098bc6dbbf45690f3a7e9e6d0f8bbea2a39e6148008fd05e44";

	doh.register("dojox.encoding.tests.digests.SHA224", [
		function testBase64Compute(t){
			t.assertEqual(base64, SHA224(message));
		},
		function testHexCompute(t){
			t.assertEqual(hex, SHA224(message, ded.outputTypes.Hex));
		},
		function testHmacCompute(t){
			t.assertEqual(hmacHex, SHA224.hmac(hmacData, hmacKey, ded.outputTypes.Hex));
		}
	]);
});
