#!/usr/bin/env python
from sys import stdin
from optparse import OptionParser

parser = OptionParser()
parser.add_option('-f', '--file', help = 'input file')
parser.add_option('-t', '--truncate', help = 'truncate long lines')
parser.add_option('-H', '--header', help = 'print headers')
options, args = parser.parse_args()

MOD = 'exectrace'
procs = {}
roots = {}
INDENT = '  '
fin = stdin
trunc = 0

if options.truncate:
    trunc = int(options.truncate)

if options.file:
    fin = open(options.file, 'r')

class Proc(object):
    def __init__(self, t, pid, ppid, cmd):
        self.pid = pid
        self.ppid = ppid
        self.cmd = cmd
        self.childs = []
        self.t = t
        self.exited = 0

# parse input
for x in fin.readlines():
    if not x.startswith(MOD):
        continue

    x = x[len(MOD) + 1:-1]
    x = x.split(' ', 1)
    cmd = x[0]

    if cmd == 'loaded':
        pass
    elif cmd == 'unloaded':
        pass
    elif cmd == 'fork':
        x = x[1].split(' ', 2)
        t = float(x[0])
        # p is the child of pp
        p, pp = map(int, x[1].split('-'))
        try:
            y = procs[pp]
            #cmd = y.cmd
            cmd = '<fork>'
        except KeyError:
            cmd = '<fork>'
        procs[p] = Proc(t, p, pp, cmd)
    else:
        x = x[1].split(' ', 2)
        t = float(x[0])
        p, pp = map(int, x[1].split('-'))
        
        if cmd == 'execve':
            try:
                y = procs[p]
                y.cmd = x[2]
            except KeyError:
                procs[p] = Proc(t, p, pp, x[2])
        elif cmd == 'exit':
            try:
                y = procs[p]
                y.exited = 1
                y.t = t - y.t
            except KeyError:
                #print '%d (%d) exited. unknown process' % (p, pp)
                #y = procs[pp]
                procs[p] = Proc(0, p, pp, '???')


# resolve parent nodes
for x in procs.values():
    try:
        y = procs[x.ppid]
        y.childs.append(x)
    except KeyError:
        roots[x.pid] = x

# print
def print_procs(p, d):
    if p.exited:
        if p.t < 0:
            t = '-0.0000'
        else:
            t = '%-2.4f' % p.t
    else:
        t = 'noexit'
    o = '%-6d %-6d %7s %s%s' % (p.pid, p.ppid, t, d * INDENT, p.cmd)
    if trunc:
        if len(o) > trunc:
            print o[:trunc - 3] + '...'
        else:
            print o
    else:
        print o
    for i in p.childs:
        print_procs(i, d + 1)

for x in roots.values():
    if options.header:
        print ''
        print '%-6s %-6s %7s %s' % ('pid', 'ppid', 'time', 'command')
    print_procs(x, 0)

