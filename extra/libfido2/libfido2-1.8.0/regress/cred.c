/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <assert.h>
#include <cbor.h>
#include <fido.h>
#include <string.h>

#define FAKE_DEV_HANDLE	((void *)0xdeadbeef)

static const unsigned char cdh[32] = {
	0xf9, 0x64, 0x57, 0xe7, 0x2d, 0x97, 0xf6, 0xbb,
	0xdd, 0xd7, 0xfb, 0x06, 0x37, 0x62, 0xea, 0x26,
	0x20, 0x44, 0x8e, 0x69, 0x7c, 0x03, 0xf2, 0x31,
	0x2f, 0x99, 0xdc, 0xaf, 0x3e, 0x8a, 0x91, 0x6b,
};

static const unsigned char authdata[198] = {
	0x58, 0xc4, 0x49, 0x96, 0x0d, 0xe5, 0x88, 0x0e,
	0x8c, 0x68, 0x74, 0x34, 0x17, 0x0f, 0x64, 0x76,
	0x60, 0x5b, 0x8f, 0xe4, 0xae, 0xb9, 0xa2, 0x86,
	0x32, 0xc7, 0x99, 0x5c, 0xf3, 0xba, 0x83, 0x1d,
	0x97, 0x63, 0x41, 0x00, 0x00, 0x00, 0x00, 0xf8,
	0xa0, 0x11, 0xf3, 0x8c, 0x0a, 0x4d, 0x15, 0x80,
	0x06, 0x17, 0x11, 0x1f, 0x9e, 0xdc, 0x7d, 0x00,
	0x40, 0x53, 0xfb, 0xdf, 0xaa, 0xce, 0x63, 0xde,
	0xc5, 0xfe, 0x47, 0xe6, 0x52, 0xeb, 0xf3, 0x5d,
	0x53, 0xa8, 0xbf, 0x9d, 0xd6, 0x09, 0x6b, 0x5e,
	0x7f, 0xe0, 0x0d, 0x51, 0x30, 0x85, 0x6a, 0xda,
	0x68, 0x70, 0x85, 0xb0, 0xdb, 0x08, 0x0b, 0x83,
	0x2c, 0xef, 0x44, 0xe2, 0x36, 0x88, 0xee, 0x76,
	0x90, 0x6e, 0x7b, 0x50, 0x3e, 0x9a, 0xa0, 0xd6,
	0x3c, 0x34, 0xe3, 0x83, 0xe7, 0xd1, 0xbd, 0x9f,
	0x25, 0xa5, 0x01, 0x02, 0x03, 0x26, 0x20, 0x01,
	0x21, 0x58, 0x20, 0x17, 0x5b, 0x27, 0xa6, 0x56,
	0xb2, 0x26, 0x0c, 0x26, 0x0c, 0x55, 0x42, 0x78,
	0x17, 0x5d, 0x4c, 0xf8, 0xa2, 0xfd, 0x1b, 0xb9,
	0x54, 0xdf, 0xd5, 0xeb, 0xbf, 0x22, 0x64, 0xf5,
	0x21, 0x9a, 0xc6, 0x22, 0x58, 0x20, 0x87, 0x5f,
	0x90, 0xe6, 0xfd, 0x71, 0x27, 0x9f, 0xeb, 0xe3,
	0x03, 0x44, 0xbc, 0x8d, 0x49, 0xc6, 0x1c, 0x31,
	0x3b, 0x72, 0xae, 0xd4, 0x53, 0xb1, 0xfe, 0x5d,
	0xe1, 0x30, 0xfc, 0x2b, 0x1e, 0xd2,
};

static const unsigned char authdata_dupkeys[200] = {
	0x58, 0xc6, 0x49, 0x96, 0x0d, 0xe5, 0x88, 0x0e,
	0x8c, 0x68, 0x74, 0x34, 0x17, 0x0f, 0x64, 0x76,
	0x60, 0x5b, 0x8f, 0xe4, 0xae, 0xb9, 0xa2, 0x86,
	0x32, 0xc7, 0x99, 0x5c, 0xf3, 0xba, 0x83, 0x1d,
	0x97, 0x63, 0x41, 0x00, 0x00, 0x00, 0x00, 0xf8,
	0xa0, 0x11, 0xf3, 0x8c, 0x0a, 0x4d, 0x15, 0x80,
	0x06, 0x17, 0x11, 0x1f, 0x9e, 0xdc, 0x7d, 0x00,
	0x40, 0x53, 0xfb, 0xdf, 0xaa, 0xce, 0x63, 0xde,
	0xc5, 0xfe, 0x47, 0xe6, 0x52, 0xeb, 0xf3, 0x5d,
	0x53, 0xa8, 0xbf, 0x9d, 0xd6, 0x09, 0x6b, 0x5e,
	0x7f, 0xe0, 0x0d, 0x51, 0x30, 0x85, 0x6a, 0xda,
	0x68, 0x70, 0x85, 0xb0, 0xdb, 0x08, 0x0b, 0x83,
	0x2c, 0xef, 0x44, 0xe2, 0x36, 0x88, 0xee, 0x76,
	0x90, 0x6e, 0x7b, 0x50, 0x3e, 0x9a, 0xa0, 0xd6,
	0x3c, 0x34, 0xe3, 0x83, 0xe7, 0xd1, 0xbd, 0x9f,
	0x25, 0xa6, 0x01, 0x02, 0x01, 0x02, 0x03, 0x26,
	0x20, 0x01, 0x21, 0x58, 0x20, 0x17, 0x5b, 0x27,
	0xa6, 0x56, 0xb2, 0x26, 0x0c, 0x26, 0x0c, 0x55,
	0x42, 0x78, 0x17, 0x5d, 0x4c, 0xf8, 0xa2, 0xfd,
	0x1b, 0xb9, 0x54, 0xdf, 0xd5, 0xeb, 0xbf, 0x22,
	0x64, 0xf5, 0x21, 0x9a, 0xc6, 0x22, 0x58, 0x20,
	0x87, 0x5f, 0x90, 0xe6, 0xfd, 0x71, 0x27, 0x9f,
	0xeb, 0xe3, 0x03, 0x44, 0xbc, 0x8d, 0x49, 0xc6,
	0x1c, 0x31, 0x3b, 0x72, 0xae, 0xd4, 0x53, 0xb1,
	0xfe, 0x5d, 0xe1, 0x30, 0xfc, 0x2b, 0x1e, 0xd2,
};

static const unsigned char authdata_unsorted_keys[198] = {
	0x58, 0xc4, 0x49, 0x96, 0x0d, 0xe5, 0x88, 0x0e,
	0x8c, 0x68, 0x74, 0x34, 0x17, 0x0f, 0x64, 0x76,
	0x60, 0x5b, 0x8f, 0xe4, 0xae, 0xb9, 0xa2, 0x86,
	0x32, 0xc7, 0x99, 0x5c, 0xf3, 0xba, 0x83, 0x1d,
	0x97, 0x63, 0x41, 0x00, 0x00, 0x00, 0x00, 0xf8,
	0xa0, 0x11, 0xf3, 0x8c, 0x0a, 0x4d, 0x15, 0x80,
	0x06, 0x17, 0x11, 0x1f, 0x9e, 0xdc, 0x7d, 0x00,
	0x40, 0x53, 0xfb, 0xdf, 0xaa, 0xce, 0x63, 0xde,
	0xc5, 0xfe, 0x47, 0xe6, 0x52, 0xeb, 0xf3, 0x5d,
	0x53, 0xa8, 0xbf, 0x9d, 0xd6, 0x09, 0x6b, 0x5e,
	0x7f, 0xe0, 0x0d, 0x51, 0x30, 0x85, 0x6a, 0xda,
	0x68, 0x70, 0x85, 0xb0, 0xdb, 0x08, 0x0b, 0x83,
	0x2c, 0xef, 0x44, 0xe2, 0x36, 0x88, 0xee, 0x76,
	0x90, 0x6e, 0x7b, 0x50, 0x3e, 0x9a, 0xa0, 0xd6,
	0x3c, 0x34, 0xe3, 0x83, 0xe7, 0xd1, 0xbd, 0x9f,
	0x25, 0xa5, 0x03, 0x26, 0x01, 0x02, 0x20, 0x01,
	0x21, 0x58, 0x20, 0x17, 0x5b, 0x27, 0xa6, 0x56,
	0xb2, 0x26, 0x0c, 0x26, 0x0c, 0x55, 0x42, 0x78,
	0x17, 0x5d, 0x4c, 0xf8, 0xa2, 0xfd, 0x1b, 0xb9,
	0x54, 0xdf, 0xd5, 0xeb, 0xbf, 0x22, 0x64, 0xf5,
	0x21, 0x9a, 0xc6, 0x22, 0x58, 0x20, 0x87, 0x5f,
	0x90, 0xe6, 0xfd, 0x71, 0x27, 0x9f, 0xeb, 0xe3,
	0x03, 0x44, 0xbc, 0x8d, 0x49, 0xc6, 0x1c, 0x31,
	0x3b, 0x72, 0xae, 0xd4, 0x53, 0xb1, 0xfe, 0x5d,
	0xe1, 0x30, 0xfc, 0x2b, 0x1e, 0xd2,
};

static const unsigned char x509[742] = {
	0x30, 0x82, 0x02, 0xe2, 0x30, 0x81, 0xcb, 0x02,
	0x01, 0x01, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86,
	0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05,
	0x00, 0x30, 0x1d, 0x31, 0x1b, 0x30, 0x19, 0x06,
	0x03, 0x55, 0x04, 0x03, 0x13, 0x12, 0x59, 0x75,
	0x62, 0x69, 0x63, 0x6f, 0x20, 0x55, 0x32, 0x46,
	0x20, 0x54, 0x65, 0x73, 0x74, 0x20, 0x43, 0x41,
	0x30, 0x1e, 0x17, 0x0d, 0x31, 0x34, 0x30, 0x35,
	0x31, 0x35, 0x31, 0x32, 0x35, 0x38, 0x35, 0x34,
	0x5a, 0x17, 0x0d, 0x31, 0x34, 0x30, 0x36, 0x31,
	0x34, 0x31, 0x32, 0x35, 0x38, 0x35, 0x34, 0x5a,
	0x30, 0x1d, 0x31, 0x1b, 0x30, 0x19, 0x06, 0x03,
	0x55, 0x04, 0x03, 0x13, 0x12, 0x59, 0x75, 0x62,
	0x69, 0x63, 0x6f, 0x20, 0x55, 0x32, 0x46, 0x20,
	0x54, 0x65, 0x73, 0x74, 0x20, 0x45, 0x45, 0x30,
	0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48,
	0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86,
	0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42,
	0x00, 0x04, 0xdb, 0x0a, 0xdb, 0xf5, 0x21, 0xc7,
	0x5c, 0xce, 0x63, 0xdc, 0xa6, 0xe1, 0xe8, 0x25,
	0x06, 0x0d, 0x94, 0xe6, 0x27, 0x54, 0x19, 0x4f,
	0x9d, 0x24, 0xaf, 0x26, 0x1a, 0xbe, 0xad, 0x99,
	0x44, 0x1f, 0x95, 0xa3, 0x71, 0x91, 0x0a, 0x3a,
	0x20, 0xe7, 0x3e, 0x91, 0x5e, 0x13, 0xe8, 0xbe,
	0x38, 0x05, 0x7a, 0xd5, 0x7a, 0xa3, 0x7e, 0x76,
	0x90, 0x8f, 0xaf, 0xe2, 0x8a, 0x94, 0xb6, 0x30,
	0xeb, 0x9d, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86,
	0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05,
	0x00, 0x03, 0x82, 0x02, 0x01, 0x00, 0x95, 0x40,
	0x6b, 0x50, 0x61, 0x7d, 0xad, 0x84, 0xa3, 0xb4,
	0xeb, 0x88, 0x0f, 0xe3, 0x30, 0x0f, 0x2d, 0xa2,
	0x0a, 0x00, 0xd9, 0x25, 0x04, 0xee, 0x72, 0xfa,
	0x67, 0xdf, 0x58, 0x51, 0x0f, 0x0b, 0x47, 0x02,
	0x9c, 0x3e, 0x41, 0x29, 0x4a, 0x93, 0xac, 0x29,
	0x85, 0x89, 0x2d, 0xa4, 0x7a, 0x81, 0x32, 0x28,
	0x57, 0x71, 0x01, 0xef, 0xa8, 0x42, 0x88, 0x16,
	0x96, 0x37, 0x91, 0xd5, 0xdf, 0xe0, 0x8f, 0xc9,
	0x3c, 0x8d, 0xb0, 0xcd, 0x89, 0x70, 0x82, 0xec,
	0x79, 0xd3, 0xc6, 0x78, 0x73, 0x29, 0x32, 0xe5,
	0xab, 0x6c, 0xbd, 0x56, 0x9f, 0xd5, 0x45, 0x91,
	0xce, 0xc1, 0xdd, 0x8d, 0x64, 0xdc, 0xe9, 0x9c,
	0x1f, 0x5e, 0x3c, 0xd2, 0xaf, 0x51, 0xa5, 0x82,
	0x18, 0xaf, 0xe0, 0x37, 0xe7, 0x32, 0x9e, 0x76,
	0x05, 0x77, 0x02, 0x7b, 0xe6, 0x24, 0xa0, 0x31,
	0x56, 0x1b, 0xfd, 0x19, 0xc5, 0x71, 0xd3, 0xf0,
	0x9e, 0xc0, 0x73, 0x05, 0x4e, 0xbc, 0x85, 0xb8,
	0x53, 0x9e, 0xef, 0xc5, 0xbc, 0x9c, 0x56, 0xa3,
	0xba, 0xd9, 0x27, 0x6a, 0xbb, 0xa9, 0x7a, 0x40,
	0xd7, 0x47, 0x8b, 0x55, 0x72, 0x6b, 0xe3, 0xfe,
	0x28, 0x49, 0x71, 0x24, 0xf4, 0x8f, 0xf4, 0x20,
	0x81, 0xea, 0x38, 0xff, 0x7c, 0x0a, 0x4f, 0xdf,
	0x02, 0x82, 0x39, 0x81, 0x82, 0x3b, 0xca, 0x09,
	0xdd, 0xca, 0xaa, 0x0f, 0x27, 0xf5, 0xa4, 0x83,
	0x55, 0x6c, 0x9a, 0x39, 0x9b, 0x15, 0x3a, 0x16,
	0x63, 0xdc, 0x5b, 0xf9, 0xac, 0x5b, 0xbc, 0xf7,
	0x9f, 0xbe, 0x0f, 0x8a, 0xa2, 0x3c, 0x31, 0x13,
	0xa3, 0x32, 0x48, 0xca, 0x58, 0x87, 0xf8, 0x7b,
	0xa0, 0xa1, 0x0a, 0x6a, 0x60, 0x96, 0x93, 0x5f,
	0x5d, 0x26, 0x9e, 0x63, 0x1d, 0x09, 0xae, 0x9a,
	0x41, 0xe5, 0xbd, 0x08, 0x47, 0xfe, 0xe5, 0x09,
	0x9b, 0x20, 0xfd, 0x12, 0xe2, 0xe6, 0x40, 0x7f,
	0xba, 0x4a, 0x61, 0x33, 0x66, 0x0d, 0x0e, 0x73,
	0xdb, 0xb0, 0xd5, 0xa2, 0x9a, 0x9a, 0x17, 0x0d,
	0x34, 0x30, 0x85, 0x6a, 0x42, 0x46, 0x9e, 0xff,
	0x34, 0x8f, 0x5f, 0x87, 0x6c, 0x35, 0xe7, 0xa8,
	0x4d, 0x35, 0xeb, 0xc1, 0x41, 0xaa, 0x8a, 0xd2,
	0xda, 0x19, 0xaa, 0x79, 0xa2, 0x5f, 0x35, 0x2c,
	0xa0, 0xfd, 0x25, 0xd3, 0xf7, 0x9d, 0x25, 0x18,
	0x2d, 0xfa, 0xb4, 0xbc, 0xbb, 0x07, 0x34, 0x3c,
	0x8d, 0x81, 0xbd, 0xf4, 0xe9, 0x37, 0xdb, 0x39,
	0xe9, 0xd1, 0x45, 0x5b, 0x20, 0x41, 0x2f, 0x2d,
	0x27, 0x22, 0xdc, 0x92, 0x74, 0x8a, 0x92, 0xd5,
	0x83, 0xfd, 0x09, 0xfb, 0x13, 0x9b, 0xe3, 0x39,
	0x7a, 0x6b, 0x5c, 0xfa, 0xe6, 0x76, 0x9e, 0xe0,
	0xe4, 0xe3, 0xef, 0xad, 0xbc, 0xfd, 0x42, 0x45,
	0x9a, 0xd4, 0x94, 0xd1, 0x7e, 0x8d, 0xa7, 0xd8,
	0x05, 0xd5, 0xd3, 0x62, 0xcf, 0x15, 0xcf, 0x94,
	0x7d, 0x1f, 0x5b, 0x58, 0x20, 0x44, 0x20, 0x90,
	0x71, 0xbe, 0x66, 0xe9, 0x9a, 0xab, 0x74, 0x32,
	0x70, 0x53, 0x1d, 0x69, 0xed, 0x87, 0x66, 0xf4,
	0x09, 0x4f, 0xca, 0x25, 0x30, 0xc2, 0x63, 0x79,
	0x00, 0x3c, 0xb1, 0x9b, 0x39, 0x3f, 0x00, 0xe0,
	0xa8, 0x88, 0xef, 0x7a, 0x51, 0x5b, 0xe7, 0xbd,
	0x49, 0x64, 0xda, 0x41, 0x7b, 0x24, 0xc3, 0x71,
	0x22, 0xfd, 0xd1, 0xd1, 0x20, 0xb3, 0x3f, 0x97,
	0xd3, 0x97, 0xb2, 0xaa, 0x18, 0x1c, 0x9e, 0x03,
	0x77, 0x7b, 0x5b, 0x7e, 0xf9, 0xa3, 0xa0, 0xd6,
	0x20, 0x81, 0x2c, 0x38, 0x8f, 0x9d, 0x25, 0xde,
	0xe9, 0xc8, 0xf5, 0xdd, 0x6a, 0x47, 0x9c, 0x65,
	0x04, 0x5a, 0x56, 0xe6, 0xc2, 0xeb, 0xf2, 0x02,
	0x97, 0xe1, 0xb9, 0xd8, 0xe1, 0x24, 0x76, 0x9f,
	0x23, 0x62, 0x39, 0x03, 0x4b, 0xc8, 0xf7, 0x34,
	0x07, 0x49, 0xd6, 0xe7, 0x4d, 0x9a,
};

const unsigned char sig[70] = {
	0x30, 0x44, 0x02, 0x20, 0x54, 0x92, 0x28, 0x3b,
	0x83, 0x33, 0x47, 0x56, 0x68, 0x79, 0xb2, 0x0c,
	0x84, 0x80, 0xcc, 0x67, 0x27, 0x8b, 0xfa, 0x48,
	0x43, 0x0d, 0x3c, 0xb4, 0x02, 0x36, 0x87, 0x97,
	0x3e, 0xdf, 0x2f, 0x65, 0x02, 0x20, 0x1b, 0x56,
	0x17, 0x06, 0xe2, 0x26, 0x0f, 0x6a, 0xe9, 0xa9,
	0x70, 0x99, 0x62, 0xeb, 0x3a, 0x04, 0x1a, 0xc4,
	0xa7, 0x03, 0x28, 0x56, 0x7c, 0xed, 0x47, 0x08,
	0x68, 0x73, 0x6a, 0xb6, 0x89, 0x0d,
};

const unsigned char pubkey[64] = {
	0x17, 0x5b, 0x27, 0xa6, 0x56, 0xb2, 0x26, 0x0c,
	0x26, 0x0c, 0x55, 0x42, 0x78, 0x17, 0x5d, 0x4c,
	0xf8, 0xa2, 0xfd, 0x1b, 0xb9, 0x54, 0xdf, 0xd5,
	0xeb, 0xbf, 0x22, 0x64, 0xf5, 0x21, 0x9a, 0xc6,
	0x87, 0x5f, 0x90, 0xe6, 0xfd, 0x71, 0x27, 0x9f,
	0xeb, 0xe3, 0x03, 0x44, 0xbc, 0x8d, 0x49, 0xc6,
	0x1c, 0x31, 0x3b, 0x72, 0xae, 0xd4, 0x53, 0xb1,
	0xfe, 0x5d, 0xe1, 0x30, 0xfc, 0x2b, 0x1e, 0xd2,
};

const unsigned char id[64] = {
	0x53, 0xfb, 0xdf, 0xaa, 0xce, 0x63, 0xde, 0xc5,
	0xfe, 0x47, 0xe6, 0x52, 0xeb, 0xf3, 0x5d, 0x53,
	0xa8, 0xbf, 0x9d, 0xd6, 0x09, 0x6b, 0x5e, 0x7f,
	0xe0, 0x0d, 0x51, 0x30, 0x85, 0x6a, 0xda, 0x68,
	0x70, 0x85, 0xb0, 0xdb, 0x08, 0x0b, 0x83, 0x2c,
	0xef, 0x44, 0xe2, 0x36, 0x88, 0xee, 0x76, 0x90,
	0x6e, 0x7b, 0x50, 0x3e, 0x9a, 0xa0, 0xd6, 0x3c,
	0x34, 0xe3, 0x83, 0xe7, 0xd1, 0xbd, 0x9f, 0x25,
};

/*
 * Security Key By Yubico
 * 5.1.X
 * f8a011f3-8c0a-4d15-8006-17111f9edc7d
*/
const unsigned char aaguid[16] = {
	0xf8, 0xa0, 0x11, 0xf3, 0x8c, 0x0a, 0x4d, 0x15,
	0x80, 0x06, 0x17, 0x11, 0x1f, 0x9e, 0xdc, 0x7d,
};

const char rp_id[] = "localhost";
const char rp_name[] = "sweet home localhost";

static void *
dummy_open(const char *path)
{
	(void)path;

	return (FAKE_DEV_HANDLE);
}

static void
dummy_close(void *handle)
{
	assert(handle == FAKE_DEV_HANDLE);
}

static int
dummy_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	(void)handle;
	(void)buf;
	(void)len;
	(void)ms;

	abort();
	/* NOTREACHED */
}

static int
dummy_write(void *handle, const unsigned char *buf, size_t len)
{
	(void)handle;
	(void)buf;
	(void)len;

	abort();
	/* NOTREACHED */
}

static fido_cred_t *
alloc_cred(void)
{
	fido_cred_t *c;

	c = fido_cred_new();
	assert(c != NULL);

	return (c);
}

static void
free_cred(fido_cred_t *c)
{
	fido_cred_free(&c);
	assert(c == NULL);
}

static fido_dev_t *
alloc_dev(void)
{
	fido_dev_t *d;

	d = fido_dev_new();
	assert(d != NULL);

	return (d);
}

static void
free_dev(fido_dev_t *d)
{
	fido_dev_free(&d);
	assert(d == NULL);
}

static void
empty_cred(void)
{
	fido_cred_t *c;
	fido_dev_t *d;
	fido_dev_io_t io_f;

	c = alloc_cred();
	assert(fido_cred_authdata_len(c) == 0);
	assert(fido_cred_authdata_ptr(c) == NULL);
	assert(fido_cred_authdata_raw_len(c) == 0);
	assert(fido_cred_authdata_raw_ptr(c) == NULL);
	assert(fido_cred_clientdata_hash_len(c) == 0);
	assert(fido_cred_clientdata_hash_ptr(c) == NULL);
	assert(fido_cred_flags(c) == 0);
	assert(fido_cred_fmt(c) == NULL);
	assert(fido_cred_id_len(c) == 0);
	assert(fido_cred_id_ptr(c) == NULL);
	assert(fido_cred_prot(c) == 0);
	assert(fido_cred_pubkey_len(c) == 0);
	assert(fido_cred_pubkey_ptr(c) == NULL);
	assert(fido_cred_rp_id(c) == NULL);
	assert(fido_cred_rp_name(c) == NULL);
	assert(fido_cred_sig_len(c) == 0);
	assert(fido_cred_sig_ptr(c) == NULL);
	assert(fido_cred_x5c_len(c) == 0);
	assert(fido_cred_x5c_ptr(c) == NULL);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);

	memset(&io_f, 0, sizeof(io_f));

	io_f.open = dummy_open;
	io_f.close = dummy_close;
	io_f.read = dummy_read;
	io_f.write = dummy_write;

	d = alloc_dev();

	fido_dev_force_u2f(d);
	assert(fido_dev_set_io_functions(d, &io_f) == FIDO_OK);
	assert(fido_dev_make_cred(d, c, NULL) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_make_cred(d, c, "") == FIDO_ERR_UNSUPPORTED_OPTION);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);

	fido_dev_force_fido2(d);
	assert(fido_dev_set_io_functions(d, &io_f) == FIDO_OK);
	assert(fido_dev_make_cred(d, c, NULL) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_make_cred(d, c, "") == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);

	free_cred(c);
	free_dev(d);
}

static void
valid_cred(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_OK);
	assert(fido_cred_prot(c) == 0);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
no_cdh(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
no_rp_id(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
no_rp_name(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, NULL) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_OK);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
no_authdata(void)
{
	fido_cred_t *c;
	unsigned char *unset;

	unset = calloc(1, sizeof(aaguid));
	assert(unset != NULL);

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_pubkey_len(c) == 0);
	assert(fido_cred_pubkey_ptr(c) == NULL);
	assert(fido_cred_id_len(c) == 0);
	assert(fido_cred_id_ptr(c) == NULL);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), unset, sizeof(aaguid)) == 0);
	free_cred(c);
	free(unset);
}

static void
no_x509(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
no_sig(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
no_fmt(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
wrong_options(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_TRUE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_PARAM);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
junk_cdh(void)
{
	fido_cred_t *c;
	unsigned char *junk;

	junk = malloc(sizeof(cdh));
	assert(junk != NULL);
	memcpy(junk, cdh, sizeof(cdh));
	junk[0] = ~junk[0];

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, junk, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_SIG);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
	free(junk);
}

static void
junk_fmt(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "junk") == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	free_cred(c);
}

static void
junk_rp_id(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, "potato", rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_PARAM);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
junk_rp_name(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, "potato") == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_OK);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

static void
junk_authdata(void)
{
	fido_cred_t *c;
	unsigned char *junk;
	unsigned char *unset;

	junk = malloc(sizeof(authdata));
	assert(junk != NULL);
	memcpy(junk, authdata, sizeof(authdata));
	junk[0] = ~junk[0];

	unset = calloc(1, sizeof(aaguid));
	assert(unset != NULL);

	c = alloc_cred();
	assert(fido_cred_set_authdata(c, junk,
	    sizeof(authdata)) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_authdata_len(c) == 0);
	assert(fido_cred_authdata_ptr(c) == NULL);
	assert(fido_cred_authdata_raw_len(c) == 0);
	assert(fido_cred_authdata_raw_ptr(c) == NULL);
	assert(fido_cred_flags(c) == 0);
	assert(fido_cred_fmt(c) == NULL);
	assert(fido_cred_id_len(c) == 0);
	assert(fido_cred_id_ptr(c) == NULL);
	assert(fido_cred_pubkey_len(c) == 0);
	assert(fido_cred_pubkey_ptr(c) == NULL);
	assert(fido_cred_rp_id(c) == NULL);
	assert(fido_cred_rp_name(c) == NULL);
	assert(fido_cred_sig_len(c) == 0);
	assert(fido_cred_sig_ptr(c) == NULL);
	assert(fido_cred_x5c_len(c) == 0);
	assert(fido_cred_x5c_ptr(c) == NULL);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), unset, sizeof(aaguid)) == 0);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	free_cred(c);
	free(junk);
	free(unset);
}

static void
junk_sig(void)
{
	fido_cred_t *c;
	unsigned char *junk;

	junk = malloc(sizeof(sig));
	assert(junk != NULL);
	memcpy(junk, sig, sizeof(sig));
	junk[0] = ~junk[0];

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, junk, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_SIG);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
	free(junk);
}

static void
junk_x509(void)
{
	fido_cred_t *c;
	unsigned char *junk;

	junk = malloc(sizeof(x509));
	assert(junk != NULL);
	memcpy(junk, x509, sizeof(x509));
	junk[0] = ~junk[0];

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, junk, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_SIG);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
	free(junk);
}

/* github issue #6 */
static void
invalid_type(void)
{
	fido_cred_t *c;
	unsigned char *unset;

	unset = calloc(1, sizeof(aaguid));
	assert(unset != NULL);

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_RS256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_pubkey_len(c) == 0);
	assert(fido_cred_pubkey_ptr(c) == NULL);
	assert(fido_cred_id_len(c) == 0);
	assert(fido_cred_id_ptr(c) == NULL);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), unset, sizeof(aaguid)) == 0);
	free_cred(c);
	free(unset);
}

/* cbor_serialize_alloc misuse */
static void
bad_cbor_serialize(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_authdata_len(c) == sizeof(authdata));
	free_cred(c);
}

static void
duplicate_keys(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata_dupkeys,
	    sizeof(authdata_dupkeys)) == FIDO_ERR_INVALID_ARGUMENT);
	free_cred(c);
}

static void
unsorted_keys(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata_unsorted_keys,
	    sizeof(authdata_unsorted_keys)) == FIDO_ERR_INVALID_ARGUMENT);
	free_cred(c);
}

static void
wrong_credprot(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_x509(c, x509, sizeof(x509)) == FIDO_OK);
	assert(fido_cred_set_sig(c, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "packed") == FIDO_OK);
	assert(fido_cred_set_prot(c, FIDO_CRED_PROT_UV_OPTIONAL_WITH_ID) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_PARAM);
	free_cred(c);
}

static void
raw_authdata(void)
{
	fido_cred_t *c;
	cbor_item_t *item;
	struct cbor_load_result cbor_result;
	const unsigned char *ptr;
	unsigned char *cbor;
	size_t len;
	size_t cbor_len;
	size_t alloclen;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert((ptr = fido_cred_authdata_ptr(c)) != NULL);
	assert((len = fido_cred_authdata_len(c)) != 0);
	assert((item = cbor_load(ptr, len, &cbor_result)) != NULL);
	assert(cbor_result.read == len);
	assert(cbor_isa_bytestring(item));
	assert((ptr = fido_cred_authdata_raw_ptr(c)) != NULL);
	assert((len = fido_cred_authdata_raw_len(c)) != 0);
	assert(cbor_bytestring_length(item) == len);
	assert(memcmp(ptr, cbor_bytestring_handle(item), len) == 0);
	assert((len = fido_cred_authdata_len(c)) != 0);
	assert((cbor_len = cbor_serialize_alloc(item, &cbor, &alloclen)) == len);
	assert((ptr = cbor_bytestring_handle(item)) != NULL);
	assert((len = cbor_bytestring_length(item)) != 0);
	assert(fido_cred_set_authdata_raw(c, ptr, len) == FIDO_OK);
	assert((ptr = fido_cred_authdata_ptr(c)) != NULL);
	assert((len = fido_cred_authdata_len(c)) != 0);
	assert(len == cbor_len);
	assert(memcmp(cbor, ptr, len) == 0);
	assert(cbor_len == sizeof(authdata));
	assert(memcmp(cbor, authdata, cbor_len) == 0);
	cbor_decref(&item);
	free(cbor);
	free_cred(c);
}

static void
fmt_none(void)
{
	fido_cred_t *c;

	c = alloc_cred();
	assert(fido_cred_set_type(c, COSE_ES256) == FIDO_OK);
	assert(fido_cred_set_clientdata_hash(c, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_cred_set_rp(c, rp_id, rp_name) == FIDO_OK);
	assert(fido_cred_set_authdata(c, authdata, sizeof(authdata)) == FIDO_OK);
	assert(fido_cred_set_rk(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_uv(c, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_cred_set_fmt(c, "none") == FIDO_OK);
	assert(fido_cred_verify(c) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_cred_prot(c) == 0);
	assert(fido_cred_pubkey_len(c) == sizeof(pubkey));
	assert(memcmp(fido_cred_pubkey_ptr(c), pubkey, sizeof(pubkey)) == 0);
	assert(fido_cred_id_len(c) == sizeof(id));
	assert(memcmp(fido_cred_id_ptr(c), id, sizeof(id)) == 0);
	assert(fido_cred_aaguid_len(c) == sizeof(aaguid));
	assert(memcmp(fido_cred_aaguid_ptr(c), aaguid, sizeof(aaguid)) == 0);
	free_cred(c);
}

int
main(void)
{
	fido_init(0);

	empty_cred();
	valid_cred();
	no_cdh();
	no_rp_id();
	no_rp_name();
	no_authdata();
	no_x509();
	no_sig();
	no_fmt();
	junk_cdh();
	junk_fmt();
	junk_rp_id();
	junk_rp_name();
	junk_authdata();
	junk_x509();
	junk_sig();
	wrong_options();
	invalid_type();
	bad_cbor_serialize();
	duplicate_keys();
	unsorted_keys();
	wrong_credprot();
	raw_authdata();
	fmt_none();

	exit(0);
}
