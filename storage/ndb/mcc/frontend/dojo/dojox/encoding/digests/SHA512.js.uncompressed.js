define("dojox/encoding/digests/SHA512", ["./_sha-64", "./_sha2"], function(sha64, sha2){
	//	The 512-bit implementation of SHA-2
	
	//	Note that for 64-bit hashes, we're actually doing high-order, low-order, high-order, low-order.
	//	The 64-bit functions will assemble them into actual 64-bit "words".
	var hash = [
		0x6a09e667, 0xf3bcc908, 0xbb67ae85, 0x84caa73b, 0x3c6ef372, 0xfe94f82b, 0xa54ff53a, 0x5f1d36f1,
		0x510e527f, 0xade682d1, 0x9b05688c, 0x2b3e6c1f, 0x1f83d9ab, 0xfb41bd6b, 0x5be0cd19, 0x137e2179
	];

	return sha2(sha64, 512, 1024, hash);
});
