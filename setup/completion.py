import os
import re
import readline

class Completer(object):

  def __init__(self):
    readline.set_completer_delims(' \t\n;')
    readline.parse_and_bind("tab: complete")

  def ls(self, dir):
    list = []
    for name in os.listdir(dir):
      path = os.path.join(dir, name)
      if os.path.isdir(path):
        name += os.sep
      list.append(name)
    return list

  def complete_path(self, path=None):
    if not path:
      return self.ls('.')
    dirname, rest = os.path.split(path)
    tmp = dirname if dirname else '.'
    result = [os.path.join(dirname, p)
              for p in self.ls(tmp) if p.startswith(rest)]
    # more than one match, or single match which does not exist (typo)
    if len(result) > 1 or not os.path.exists(path):
      return result
    # resolved to a single directory, so return list of its files
    if os.path.isdir(path):
      return [os.path.join(path, p) for p in self.ls(path)]
    # exact match
    return [path + ' ']

  def complete(self, text, state):
    "readline completion entry point."
    return self.complete_path(text)[state]