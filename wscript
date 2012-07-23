import os

srcdir = 'Adapter/impl'
blddir = 'Adapter/impl/build'
VERSION = '0.35'

def set_options(opt):
  opt.tool_options('compiler_cxx')
  opt.add_option('--mysql', action='store')

def configure(conf):
  import Options
  mysqlinclude = Options.options.mysql + "include/"
 
  if os.path.isfile(mysqlinclude + "mysql/mysql.h"):
    mysqlinclude = mysqlinclude + "mysql/"
  
  ndbinclude = mysqlinclude + "/storage/ndb"
  ndblib = Options.options.mysql + 'lib'
  if os.path.isdir(ndblib + '/mysql'):
    ndblib = ndblib + "/mysql"
  
  conf.check_tool('compiler_cxx')
  conf.check_tool('node_addon')

  conf.env.append_unique('CXXFLAGS', ["-I" + "../common/include"])
  conf.env.append_unique('CXXFLAGS', ["-I" + mysqlinclude])
  conf.env.append_unique('CXXFLAGS', ["-I" + ndbinclude])
  conf.env.append_unique('CXXFLAGS', ["-I" + ndbinclude + "/ndbapi"])
  conf.env.append_unique('LINKFLAGS', ['-L' + ndblib])
  conf.env.append_unique('LINKFLAGS', ['-lndbclient'])

  conf.env.rpath = ndblib


def build(ctx):
  ctx.recurse("Adapter/impl/ndb")
  ctx.recurse("Adapter/impl/common")
