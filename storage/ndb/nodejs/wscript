import os
import string

srcdir = 'Adapter/impl'
blddir = 'Adapter/impl/build'
VERSION = '0.35'

def set_options(opt):
  opt.tool_options('compiler_cxx')
  opt.add_option('--mysql', action='store')

def configure(conf):
  import Options
  
  if(Options.options.mysql):
    mysql_path = Options.options.mysql
  else:
    infile = open('./config.waf', 'r')
    mysql_path = infile.read()
 
  mysql_path = string.rstrip(mysql_path)     
  my_lib = mysql_path + "/lib/"
  my_inc = mysql_path + "/include/"

  if os.path.isdir(my_lib + "/mysql"):
    my_lib = my_lib + "/mysql"
 
  if os.path.isdir(my_inc + "mysql/storage"):
    my_inc = my_inc + "mysql/"
  
  ndb_inc = my_inc + "/storage/ndb"
 
  conf.env.my_lib = my_lib
  conf.env.my_inc = my_inc
  conf.env.ndb_inc = ndb_inc
    
  conf.check_tool('compiler_cxx')
  conf.check_tool('node_addon')

  conf.recurse("Adapter/impl/")

def build(ctx):
  ctx.recurse("Adapter/impl/")

