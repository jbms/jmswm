"""
Only main.cpp is added to the program, then by looking at the include files,
a <file.h> file is found. If a corresponding <file.cpp> exists,
then a new c++ task is created to compile that file and to add it to the
program (modify the link task).

The idea is to change the method runnable_status of the task. A more correct but less obvious
approach would be the creation of a specific c++ subclass, and using another
extension mapping function (@extension).
"""

header_suffix_map = {'.hpp': ['.cpp', '.cc'], '.h': ['.c', '.cpp', '.cc']}

import collections
class task_cache_entry(collections.namedtuple('task_cache_entry',
                                              ['task', 'parent_entries', 'child_entries', 'link_tasks',
                                               'inferred_uselib'])):
    def __hash__(self): return id(self)

    def get_all_related_entries(self, attr):
        e = self
        closed_entries = set()
        open_entries = [e]
        while open_entries:
            e = open_entries.pop()
            if e in closed_entries: continue
            closed_entries.add(e)
            open_entries.extend(getattr(e, attr))
        return closed_entries

def propagate_uselib_vars(task, uselib):
    # copied from waflib.Tools.ccroot.propagate_uselib_vars
    generator = task.generator
    env = task.env
    _vars = generator.get_uselib_vars()

    for x in uselib:
        for v in _vars:
            env.append_unique(v, env[v + '_' + x])

    # copied from waflib.Tools.ccroot.apply_incpaths
    lst = generator.to_incnodes(generator.to_list(getattr(generator, 'includes', [])) + env['INCLUDES'])
    task.includes_nodes = lst
    env['INCPATHS'] = [x.abspath() for x in lst]

from waflib.Task import ASK_LATER
from waflib.Tools.cxx import cxx, cxxprogram
def cxx_runnable_status(self):
    ret = super(cxx, self).runnable_status()

    self.more_tasks = []

    # use a cache to avoid creating the same tasks
    # for example, truc.cpp might be compiled twice

    try:
        shared = self.generator.bld.shared_tasks
    except AttributeError:
        shared = self.generator.bld.shared_tasks = {}

    if ret != ASK_LATER:
        try:
            cur_entry = shared[self.inputs[0]]
        except:
            cur_entry = shared[self.inputs[0]] = task_cache_entry(task = self,
                                                                  parent_entries = set(),
                                                                  child_entries = [],
                                                                  link_tasks = set(),
                                                                  inferred_uselib = set())
            link = getattr(self.generator, 'link_task', None)
            if link:
                cur_entry.link_tasks.add(link)
                
        all_parents = cur_entry.get_all_related_entries('parent_entries')
        link_tasks = set(); link_tasks.update(*map(task_cache_entry.link_tasks.fget, all_parents))
        object_tasks = set()

        # Infer library dependencies
        try:
            libmap = self.env.cxx_autodep_header_uselib_map
        except AttributeError:
            libmap = None
            
        uselib = cur_entry.inferred_uselib
        uselib_non_recursive = set()
        if libmap:
            raw_deps = self.generator.bld.raw_deps[self.uid()]
            for dep in raw_deps:
                parts = dep.split('/')
                partial = ''
                for part in parts:
                    if len(partial) > 0: partial += '/'
                    partial += part
                    if partial in libmap:
                        uselib.update(libmap[partial])
                        uselib_non_recursive.update(libmap[partial])

        for x in self.generator.bld.node_deps[self.uid()]:
            suffix = x.suffix()
            node = None
            if suffix in header_suffix_map:
                for source_suffix in header_suffix_map[suffix]:
                    node = x.parent.get_src().find_resource(x.name.replace(suffix, source_suffix))
                    if node: break
            if node:
                if node in shared:
                    entry = shared[node]
                else:
                    #print 'Generating task: ' + repr(node)
                    entry = shared[node] = task_cache_entry(task = self.generator.cxx_hook(node),
                                                            parent_entries = set(),
                                                            child_entries = [],
                                                            link_tasks = set(),
                                                            inferred_uselib = set())
                    self.more_tasks.append(entry.task)

                entry.parent_entries.add(cur_entry)
                cur_entry.child_entries.append(entry)

                for child in entry.get_all_related_entries('child_entries'):
                    object_tasks.add(child.task)
                    uselib.update(child.inferred_uselib)

        # add object_tasks as inputs to all link_tasks
        for link in link_tasks:
            for obj in object_tasks:
                if obj.outputs[0] not in link.inputs:
                    link.inputs.append(obj.outputs[0])
                    link.set_run_after(obj)
            link.inputs.sort(key = lambda x: x.abspath())
            link.env.append_unique('cxx_autodep_uselib', uselib)
            link.env.cxx_autodep_uselib.sort()

        # possibly modify flags
        if uselib:
            propagate_uselib_vars(self, uselib)
            delattr(self, 'cache_sig')
            return super(cxx, self).runnable_status()

    return ret

def cxxprogram_runnable_status(self):
    ret = super(cxxprogram, self).runnable_status()

    if ret != ASK_LATER:
        try:
            uselib = self.env.cxx_autodep_uselib
        except AttributeError:
            uselib = None
        if uselib:
            propagate_uselib_vars(self, uselib)
            delattr(self, 'cache_sig')
            return super(cxxprogram, self).runnable_status()
    return ret

cxx.runnable_status = cxx_runnable_status
cxxprogram.runnable_status = cxxprogram_runnable_status
