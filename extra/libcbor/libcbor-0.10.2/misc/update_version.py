import sys, re
from datetime import date
import logging

logging.basicConfig(level=logging.INFO)

# Update version label in all configuration files
# Usage: python3 misc/update_version.py X.Y.Z

# When testing, reset local state using:
# git checkout -- CHANGELOG.md Doxyfile CMakeLists.txt doc/source/conf.py examples/bazel/third_party/libcbor/cbor/configuration.h

version = sys.argv[1]
release_date = date.today().strftime('%Y-%m-%d')
major, minor, patch = version.split('.')


def replace(file_path, pattern, replacement):
    logging.info(f'Updating {file_path}')
    original = open(file_path).read()
    updated = re.sub(pattern, replacement, original)
    assert updated != original
    with open(file_path, 'w') as f:
        f.write(updated)

# Update changelog
SEP = '---------------------'
NEXT = f'Next\n{SEP}'
changelog_header = f'{NEXT}\n\n{version} ({release_date})\n{SEP}'
replace('CHANGELOG.md', NEXT, changelog_header)

# Update Doxyfile
DOXY_VERSION = 'PROJECT_NUMBER         = '
replace('Doxyfile', DOXY_VERSION + '.*', DOXY_VERSION + version)

# Update CMakeLists.txt
replace('CMakeLists.txt',
        '''SET\\(CBOR_VERSION_MAJOR "\d+"\\)
SET\\(CBOR_VERSION_MINOR "\d+"\\)
SET\\(CBOR_VERSION_PATCH "\d+"\\)''',
        f'''SET(CBOR_VERSION_MAJOR "{major}")
SET(CBOR_VERSION_MINOR "{minor}")
SET(CBOR_VERSION_PATCH "{patch}")''')

# Update Basel build example
replace('examples/bazel/third_party/libcbor/cbor/configuration.h',
        '''#define CBOR_MAJOR_VERSION \d+
#define CBOR_MINOR_VERSION \d+
#define CBOR_PATCH_VERSION \d+''',
        f'''#define CBOR_MAJOR_VERSION {major}
#define CBOR_MINOR_VERSION {minor}
#define CBOR_PATCH_VERSION {patch}''')

# Update Sphinx
replace('doc/source/conf.py',
        """version = '.*'
release = '.*'""",
        f"""version = '{major}.{minor}'
release = '{major}.{minor}.{patch}'""")
