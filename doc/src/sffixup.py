#!/usr/bin/env/python

# This is a hack and totally brittle. Let's not talk about it.

import sys

sfnotes = '''<ul>
<li><a href="http://sourceforge.net/">SourceForge</li>
<ul>
<li><a href="http://sourceforge.net/projects/pyalsaaudio/">Summary page</a>
<li><a href="http://sourceforge.net/project/showfiles.php?group_id=120651">Download</a>
<li><a href="http://sourceforge.net/tracker/?group_id=120651">Bug Tracker</a>
</ul>
</ul>'''

if len(sys.argv) <= 1:
    print "usage: sffixup.py <file>"
    sys.exit(2)

f = open(sys.argv[1], 'r')
lines = f.readlines()
f.close()

try:
    index = lines.index('<a name="CHILD_LINKS"></a>\n')
except ValueError:
    print "Cannot find child links. SourceForge links will not appear on index.html."
    sys.exit(1)

lines.insert(index, sfnotes)

f = open(sys.argv[1], 'w')
for l in lines:
    f.write(l)
f.close()
