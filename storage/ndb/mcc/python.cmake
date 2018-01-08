# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE(mcc_utils)

PY_INSTALL(FILES python.exe python27.dll msvcr100.dll 
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python")

SET(PY_DLL_SRC _bsddb.pyd _ctypes.pyd 
               _ctypes_test.pyd 
			   _elementtree.pyd 
			   _hashlib.pyd 
			   _msi.pyd 
			   _multiprocessing.pyd _socket.pyd _sqlite3.pyd _ssl.pyd _testcapi.pyd 
			   bz2.pyd pyexpat.pyd select.pyd unicodedata.pyd winsound.pyd sqlite3.dll)


PY_INSTALL(FILES ${PY_DLL_SRC} 
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/DLLs" 
	DESTINATION "${MCC_INSTALL_SUBDIR}/Python/DLLs")

SET(python_lib 
__future__.py
__phello__.foo.py
_abcoll.py
_LWPCookieJar.py
_MozillaCookieJar.py
_pyio.py
_strptime.py
_threading_local.py
_weakrefset.py
abc.py
aifc.py
antigravity.py
anydbm.py
argparse.py
ast.py
asynchat.py
asyncore.py
atexit.py
audiodev.py
base64.py
BaseHTTPServer.py
Bastion.py
bdb.py
binhex.py
bisect.py
calendar.py
cgi.py
CGIHTTPServer.py
cgitb.py
chunk.py
cmd.py
code.py
codecs.py
codeop.py
collections.py
colorsys.py
commands.py
compileall.py
ConfigParser.py
contextlib.py
Cookie.py
cookielib.py
copy.py
copy_reg.py
cProfile.py
csv.py
dbhash.py
decimal.py
difflib.py
dircache.py
dis.py
doctest.py
DocXMLRPCServer.py
dumbdbm.py
dummy_thread.py
dummy_threading.py
filecmp.py
fileinput.py
fnmatch.py
formatter.py
fpformat.py
fractions.py
ftplib.py
functools.py
genericpath.py
getopt.py
getpass.py
gettext.py
glob.py
gzip.py
hashlib.py
heapq.py
hmac.py
htmlentitydefs.py
htmllib.py
HTMLParser.py
httplib.py
ihooks.py
imaplib.py
imghdr.py
imputil.py
inspect.py
io.py
keyword.py
linecache.py
locale.py
macpath.py
macurl2path.py
mailbox.py
mailcap.py
markupbase.py
md5.py
mhlib.py
mimetools.py
mimetypes.py
MimeWriter.py
mimify.py
modulefinder.py
multifile.py
mutex.py
netrc.py
new.py
nntplib.py
ntpath.py
nturl2path.py
numbers.py
opcode.py
optparse.py
os.py
os2emxpath.py
pdb.py
pickle.py
pickletools.py
pipes.py
pkgutil.py
platform.py
plistlib.py
popen2.py
poplib.py
posixfile.py
posixpath.py
pprint.py
profile.py
pstats.py
pty.py
py_compile.py
pydoc.py
Queue.py
quopri.py
random.py
re.py
repr.py
rexec.py
rfc822.py
rlcompleter.py
robotparser.py
runpy.py
sched.py
sets.py
sgmllib.py
sha.py
shelve.py
shlex.py
shutil.py
SimpleHTTPServer.py
SimpleXMLRPCServer.py
site.py
smtpd.py
smtplib.py
sndhdr.py
socket.py
SocketServer.py
sre.py
sre_compile.py
sre_constants.py
sre_parse.py
ssl.py
stat.py
statvfs.py
string.py
StringIO.py
stringold.py
stringprep.py
struct.py
subprocess.py
sunau.py
sunaudio.py
symbol.py
symtable.py
sysconfig.py
tabnanny.py
tarfile.py
telnetlib.py
tempfile.py
textwrap.py
this.py
threading.py
timeit.py
toaiff.py
token.py
tokenize.py
trace.py
traceback.py
tty.py
types.py
urllib.py
urllib2.py
urlparse.py
user.py
UserDict.py
UserList.py
UserString.py
uu.py
uuid.py
warnings.py
wave.py
weakref.py
webbrowser.py
whichdb.py
xdrlib.py
xmllib.py
xmlrpclib.py
zipfile.py
pyclbr.py)

PY_INSTALL(FILES ${python_lib}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib")

SET(python_site_packages_crypto_protocol
__init__.py
AllOrNothing.py
Chaffing.py)

PY_INSTALL(FILES ${python_site_packages_crypto_protocol}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/Protocol"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/Protocol")

SET(python_site_packages_paramiko 
__init__.py 
agent.py
auth_handler.py
ber.py
buffered_pipe.py
channel.py
client.py
common.py
compress.py
config.py
dsskey.py
file.py
hostkeys.py
kex_gex.py
kex_group1.py
logging22.py
message.py
packet.py
pipe.py
pkey.py
primes.py
resource.py
rng.py
rng_posix.py
rng_win32.py
rsakey.py
server.py
sftp.py
sftp_attr.py
sftp_client.py
sftp_file.py
sftp_handle.py
sftp_server.py
sftp_si.py
ssh_exception.py
transport.py
util.py
win_pageant.py)

PY_INSTALL(FILES ${python_site_packages_paramiko}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/paramiko"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/paramiko")


SET(python_lib_ctypes
__init__.py
_endian.py
util.py
wintypes.py)

PY_INSTALL(FILES ${python_lib_ctypes}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/ctypes"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/ctypes")

SET(python_lib_ctypes_macholib
__init__.py
dyld.py
dylib.py
framework.py)

PY_INSTALL(FILES ${python_lib_ctypes_macholib}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/ctypes/macholib"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/ctypes/macholib")

SET(python_lib_encodings
__init__.py
aliases.py
ascii.py
base64_codec.py
big5.py
big5hkscs.py
bz2_codec.py
charmap.py
cp037.py
cp1006.py
cp1026.py
cp1140.py
cp1250.py
cp1251.py
cp1252.py
cp1253.py
cp1254.py
cp1255.py
cp1256.py
cp1257.py
cp1258.py
cp424.py
cp437.py
cp500.py
cp720.py
cp737.py
cp775.py
cp850.py
cp852.py
cp855.py
cp856.py
cp857.py
cp858.py
cp860.py
cp861.py
cp862.py
cp863.py
cp864.py
cp865.py
cp866.py
cp869.py
cp874.py
cp875.py
cp932.py
cp949.py
cp950.py
euc_jis_2004.py
euc_jisx0213.py
euc_jp.py
euc_kr.py
gb18030.py
gb2312.py
gbk.py
hex_codec.py
hp_roman8.py
hz.py
idna.py
iso2022_jp.py
iso2022_jp_1.py
iso2022_jp_2.py
iso2022_jp_2004.py
iso2022_jp_3.py
iso2022_jp_ext.py
iso2022_kr.py
iso8859_1.py
iso8859_10.py
iso8859_11.py
iso8859_13.py
iso8859_14.py
iso8859_15.py
iso8859_16.py
iso8859_2.py
iso8859_3.py
iso8859_4.py
iso8859_5.py
iso8859_6.py
iso8859_7.py
iso8859_8.py
iso8859_9.py
johab.py
koi8_r.py
koi8_u.py
latin_1.py
mac_arabic.py
mac_centeuro.py
mac_croatian.py
mac_cyrillic.py
mac_farsi.py
mac_greek.py
mac_iceland.py
mac_latin2.py
mac_roman.py
mac_romanian.py
mac_turkish.py
mbcs.py
palmos.py
ptcp154.py
punycode.py
quopri_codec.py
raw_unicode_escape.py
rot_13.py
shift_jis.py
shift_jis_2004.py
shift_jisx0213.py
string_escape.py
tis_620.py
undefined.py
unicode_escape.py
unicode_internal.py
utf_16.py
utf_16_be.py
utf_16_le.py
utf_32.py
utf_32_be.py
utf_32_le.py
utf_7.py
utf_8.py
utf_8_sig.py
uu_codec.py
zlib_codec.py)

PY_INSTALL(FILES ${python_lib_encodings}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/encodings"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/encodings")


SET(python_lib_logging
__init__.py
config.py
handlers.py)

PY_INSTALL(FILES ${python_lib_logging}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/logging"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/logging")

SET(python_lib_multiprocessing
__init__.py
connection.py
forking.py
heap.py
managers.py
pool.py
process.py
queues.py
reduction.py
sharedctypes.py
synchronize.py
util.py)

PY_INSTALL(FILES ${python_lib_multiprocessing}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/multiprocessing"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/multiprocessing")

SET(python_lib_multiprocessing_dummy
__init__.py
connection.py)

PY_INSTALL(FILES ${python_lib_multiprocessing_dummy}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/multiprocessing/dummy"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/multiprocessing/dummy")

SET(python_libs
_bsddb.lib
_ctypes.lib
_ctypes_test.lib
_elementtree.lib
_hashlib.lib
_msi.lib
_multiprocessing.lib
_socket.lib
_sqlite3.lib
_ssl.lib
_testcapi.lib
_tkinter.lib
bz2.lib
pyexpat.lib
python27.lib
select.lib
sqlite3.lib
unicodedata.lib
winsound.lib)

PY_INSTALL(FILES ${python_libs}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/libs"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/libs")

SET(python_site_packages_crypto
__init__.py
pct_warnings.py)

PY_INSTALL(FILES ${python_site_packages_crypto}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/")

SET(python_site_packages_crypto_cipher
__init__.py
AES.pyd
AES_d.pyd
ARC2.pyd
ARC2_d.pyd
ARC4.pyd
ARC4_d.pyd
Blowfish.pyd
Blowfish_d.pyd
CAST.pyd
CAST_d.pyd
DES.pyd
DES3.pyd
DES3_d.pyd
DES_d.pyd
XOR.pyd
XOR_d.pyd)

PY_INSTALL(FILES ${python_site_packages_crypto_cipher}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/Cipher"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/Cipher/")

SET(python_site_packages_crypto_hash
__init__.py
HMAC.py
MD2.pyd
MD2_d.pyd
MD4.pyd
MD4_d.pyd
MD5.py
RIPEMD.py
RIPEMD160.pyd
RIPEMD160_d.pyd
SHA.py
SHA256.pyd
SHA256_d.pyd)

PY_INSTALL(FILES ${python_site_packages_crypto_hash}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/Hash"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/Hash/")

SET(python_site_packages_crypto_selftest_hash
__init__.py
common.py
test_HMAC.py
test_MD2.py
test_MD4.py
test_MD5.py
test_RIPEMD.py
test_SHA.py
test_SHA256.py)

PY_INSTALL(FILES ${python_site_packages_crypto_selftest_hash}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/SelfTest/Hash"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/SelfTest/Hash/")

SET(python_site_packages_crypto_selftest_protocol
__init__.py
test_chaffing.py
test_rfc1751.py)

PY_INSTALL(FILES ${python_site_packages_crypto_selftest_protocol}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/SelfTest/Protocol"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/SelfTest/Protocol/")

SET(python_site_packages_crypto_selftest_publickey
__init__.py
test_DSA.py
test_importKey.py
test_RSA.py)

PY_INSTALL(FILES ${python_site_packages_crypto_selftest_publickey}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/SelfTest/PublicKey"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/SelfTest/PublicKey/")

SET(python_site_packages_crypto_selftest_random
__init__.py
test_random.py
test_rpoolcompat.py)

PY_INSTALL(FILES ${python_site_packages_crypto_selftest_random}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/SelfTest/Random"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/SelfTest/Random/")

SET(python_site_packages_crypto_selftest_random_fortuna
__init__.py
test_FortunaAccumulator.py
test_FortunaGenerator.py
test_SHAd256.py)

PY_INSTALL(FILES ${python_site_packages_crypto_selftest_random_fortuna}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/SelfTest/Random/Fortuna"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/SelfTest/Random/Fortuna/")

SET(python_site_packages_crypto_selftest_random_osrng
__init__.py
test_fallback.py
test_generic.py
test_nt.py
test_posix.py
test_winrandom.py)

PY_INSTALL(FILES ${python_site_packages_crypto_selftest_random_osrng}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/SelfTest/Random/OSRNG"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/SelfTest/Random/OSRNG/")

SET(python_site_packages_crypto_selftest_util
__init__.py
test_asn1.py
test_Counter.py
test_number.py
test_winrandom.py)

PY_INSTALL(FILES ${python_site_packages_crypto_selftest_util}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/SelfTest/Util"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/SelfTest/Util/")

SET(python_site_packages_crypto_util
__init__.py
_counter.pyd
_counter_d.pyd
_number_new.py
asn1.py
Counter.py
number.py
python_compat.py
randpool.py
RFC1751.py
strxor.pyd
strxor_d.pyd
winrandom.py)

PY_INSTALL(FILES ${python_site_packages_crypto_util}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/Util"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/Util/")

SET(python_site_packages_crypto_publickey
__init__.py
_DSA.py
_RSA.py
_slowmath.py
DSA.py
ElGamal.py
pubkey.py
qNEW.py
RSA.py)

PY_INSTALL(FILES ${python_site_packages_crypto_publickey}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/PublicKey"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/PublicKey/")

SET(python_site_packages_crypto_random
__init__.py
_UserFriendlyRNG.py
random.py)

PY_INSTALL(FILES ${python_site_packages_crypto_random}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/Random"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/Random/")

SET(python_site_packages_crypto_random_fortuna
__init__.py
FortunaAccumulator.py
FortunaGenerator.py
SHAd256.py)

PY_INSTALL(FILES ${python_site_packages_crypto_random_fortuna}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/Random/Fortuna"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/Random/Fortuna/")

SET(python_site_packages_crypto_random_osrng
__init__.py
fallback.py
nt.py
posix.py
rng_base.py
winrandom.pyd
winrandom.pyd.manifest
winrandom_d.pyd)

PY_INSTALL(FILES ${python_site_packages_crypto_random_osrng}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/Random/OSRNG"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/Random/OSRNG/")

SET(python_site_packages_crypto_selftest
__init__.py
st_common.py)

PY_INSTALL(FILES ${python_site_packages_crypto_selftest}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/SelfTest"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/SelfTest/")

SET(python_site_packages_crypto_selftest_cipher
__init__.py
common.py
test_AES.py
test_ARC2.py
test_ARC4.py
test_Blowfish.py
test_CAST.py
test_DES.py
test_DES3.py
test_XOR.py)

PY_INSTALL(FILES ${python_site_packages_crypto_selftest_cipher}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/site-packages/Crypto/SelfTest/Cipher"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/site-packages/Crypto/SelfTest/Cipher/")
	
SET(python_lib_json __init__.py  decoder.py  encoder.py  scanner.py  tool.py)

PY_INSTALL(FILES ${python_lib_json}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/json"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/json")

SET(python_lib_xml __init__.py)

PY_INSTALL(FILES ${python_lib_xml}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/xml"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/xml")

SET(python_lib_xml_sax __init__.py  _exceptions.py  expatreader.py  handler.py  saxutils.py  xmlreader.py)

PY_INSTALL(FILES ${python_lib_xml_sax}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/xml/sax"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/xml/sax")

SET(python_lib_unittest 
  __init__.py
  __main__.py
  case.py
  loader.py
  main.py
  result.py
  runner.py
  signals.py
  suite.py
  util.py)

PY_INSTALL(FILES ${python_lib_unittest}
	SRC_DIR "${MCC_PYTHON_TO_BUNDLE}/Lib/unittest"
    DESTINATION "${MCC_INSTALL_SUBDIR}/Python/Lib/unittest")
  
