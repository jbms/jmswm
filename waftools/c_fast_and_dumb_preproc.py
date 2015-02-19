from waflib.Tools import c_preproc

import re

parser = c_preproc.c_parser

no_recurse_patterns = ['^boost/', '^chaos/']

no_recurse_regex = ''

def update_no_recurse_regex():
    global no_recurse_regex
    no_recurse_regex = re.compile('|'.join(map(lambda x: '(?:' + x + ')', no_recurse_patterns)))

def add_no_recurse_pattern(p):
    no_recurse_patterns.append(p)
    update_no_recurse_regex()

update_no_recurse_regex()

class fast_and_dumb_parser(parser):
    def tryfind(self, y):
        if no_recurse_regex.match(y):
            self.names.append(y)
        else:
            return super(fast_and_dumb_parser,self).tryfind(y)

c_preproc.c_parser = fast_and_dumb_parser
