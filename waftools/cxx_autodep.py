# To avoid unnecessary rebuilds, make sure to pass idx = 0 to TaskGen constructor, i.e. bld(..., idx = 0)


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

import collections, time
class task_cache_entry(object):
    def __init__(self, task):
        self.task = task

        self.parent_entries = set()
        self.child_entries = []
        self.link_tasks = set()
        self.inferred_uselib = set()
        self.handled = False

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

    #print 'Got called: ret = %d, self = %s' % (ret, repr(self).strip())
    if ret == ASK_LATER:
        return ret

    #start_time = time.time()

    #print 'path = %r id = %r' % (self.generator.path, id(self.generator.path))
    #print 'idx = %r' % (self.generator.bld.idx)

    try:
        shared = self.generator.bld.autodep_shared_tasks
    except AttributeError:
        shared = self.generator.bld.autodep_shared_tasks = {}

    try:
        dep_to_node_cache = self.generator.bld.autodep_dep_to_node_cache
    except AttributeError:
        dep_to_node_cache = self.generator.bld.autodep_dep_to_node_cache = {}


    #print ' Processing: %s' % repr(self).strip()
    cur_entry = shared.get(self.inputs[0])
    if cur_entry is not None:
        if cur_entry.handled:
            #print '    Handled!'
            return ret
    else:
        cur_entry = shared[self.inputs[0]] = task_cache_entry(task = self)
        link = getattr(self.generator, 'link_task', None)
        if link:
            cur_entry.link_tasks.add(link)

    if not hasattr(self, 'more_tasks'):
        self.more_tasks = []

    cur_entry.handled = True

    all_parents = cur_entry.get_all_related_entries('parent_entries')
    link_tasks = set(); link_tasks.update(*map(lambda x: x.link_tasks, all_parents))
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
        #print 'raw_deps = %r' % raw_deps
        for dep in raw_deps:
            parts = dep.split('/')
            partial = ''
            for part in parts:
                if len(partial) > 0: partial += '/'
                partial += part
                if partial in libmap:
                    uselib_non_recursive.update(libmap[partial])

    uselib.update(uselib_non_recursive)
    #print '%s: after uselib %.4f' % (self.inputs[0], time.time() - start_time)

    nodes_to_check = []
    hits = 0
    misses = 0
    for x in self.generator.bld.node_deps[self.uid()]:
        node = dep_to_node_cache.get(x,False)
        if node is False:
            misses += 1
            suffix = x.suffix()
            if suffix in header_suffix_map:
                for source_suffix in header_suffix_map[suffix]:
                    node = x.parent.get_src().find_resource(x.name.replace(suffix, source_suffix))
                    if node:
                        nodes_to_check.append(node)
                        break
            dep_to_node_cache[x] = node
        else:
            hits += 1
            if node is not None:
                nodes_to_check.append(node)

    #print '%s: hits = %d, misses = %d, after getting nodes %.4f' % (self.inputs[0], hits, misses, time.time() - start_time)

    for node in nodes_to_check:
        if node in shared:
            entry = shared[node]
        else:
            #print 'Generating task: ' + repr(node)
            sub_task = self.generator.cxx_hook(node)
            entry = shared[node] = task_cache_entry(task = sub_task)
            self.more_tasks.append(entry.task)

        entry.parent_entries.add(cur_entry)
        cur_entry.child_entries.append(entry)

        for child in entry.get_all_related_entries('child_entries'):
            object_tasks.add(child.task)
            uselib.update(child.inferred_uselib)

    #print '%s: after deps %.4f' % (self.inputs[0], time.time() - start_time)
    # add object_tasks as inputs to all link_tasks
    for link in link_tasks:
        for obj in object_tasks:
            if obj.outputs[0] not in link.inputs:
                link.inputs.append(obj.outputs[0])
                link.set_run_after(obj)
        link.inputs.sort(key = lambda x: x.abspath())
        link.env.append_unique('cxx_autodep_uselib', uselib)
        link.env.cxx_autodep_uselib.sort()
    #print '%s: after link stuff %.4f' % (self.inputs[0], time.time() - start_time)

    # possibly modify flags
    if len(uselib_non_recursive) > 0:
        propagate_uselib_vars(self, uselib_non_recursive)
        #delattr(self, 'cache_sig')
        #self.reuse_scan_result = True
        #ret = super(cxx, self).runnable_status()
        #self.reuse_scan_result = False

        # WARNING:  signature does in fact change
        #    however, recomputing it wastes some time and it doesn't seem to hurt anything not to

    #print '%s: after recalc %.4f' % (self.inputs[0], time.time() - start_time)

    for sub_task in self.more_tasks:
        # Invoke runnable_status() now so that additional dependencies are calculated
        sub_task.runnable_status()
    #print '%s: after children %.4f' % (self.inputs[0], time.time() - start_time)

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

import waflib.Runner

existing_add_task = waflib.Runner.Parallel.add_task

#existing_refill_task_list = waflib.Runner.Parallel.refill_task_list
# def refill_task_list(self):
#     print 'refill_task_list called: count = %d, deadlock = %r, processed = %d' % (self.count, getattr(self, 'deadlock', None), self.processed)
#     for x in self.outstanding:
#         print '   outstanding: %s' % (repr(x).strip())

#     for x in self.frozen:
#         print '   frozen: status = %d, %s' % (x.runnable_status(), repr(x).strip())
#         for y in x.run_after:
#             if not y.hasrun:
#                 print '     still waiting on: %s' % (repr(y).strip())
#     existing_refill_task_list(self)
#waflib.Runner.Parallel.refill_task_list = refill_task_list


def more_parallel_add_task(self, tsk):
    existing_add_task(self, tsk)

    if hasattr(tsk, 'more_tasks'):
        self.add_more_tasks(tsk)
        tsk.more_tasks = []
        


waflib.Runner.Parallel.add_task = more_parallel_add_task



# existing_scan = cxx.scan
# def my_scan(self):
#     if getattr(self, 'reuse_scan_result', False) and hasattr(self, 'last_scan_result'):
#         return self.last_scan_result

#     start_time = time.time()
#     ret = existing_scan(self)
#     self.last_scan_result = ret
#     print 'scanned: %.4f  %s' % (time.time() - start_time, self.inputs[0])
#     return ret
# cxx.scan = my_scan

