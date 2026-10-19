# Makes the instrumented copy of the code base: copies Main/ (the parts the
# tree needs) into src/, then patches the copy so that the buffer manager
# counts and times every disk read, and the tree announces, by category,
# every page it touches.  Main/ itself is never changed.
#   usage: python -I patch.py <Main dir> <out dir>
import re, shutil, sys, os

main, out = sys.argv[1], sys.argv[2]
here = os.path.dirname(os.path.abspath(__file__))
if os.path.exists(out):
    shutil.rmtree(out)
for d in ['Catalog', 'BufferMgr', 'Record', 'DatabaseTable', 'Qunit', 'BPlusTest/headers']:
    shutil.copytree(os.path.join(main, d), os.path.join(out, d))
shutil.copy(os.path.join(here, 'PageCounters.h'), os.path.join(out, 'DatabaseTable/headers'))

def patch(rel, fn):
    p = os.path.join(out, rel)
    s = open(p, encoding='utf-8').read()
    t = fn(s)
    assert t != s, rel
    open(p, 'w', encoding='utf-8', newline='').write(t)

def once(s, old, new):
    assert s.count(old) == 1, old
    return s.replace(old, new)

def body(s, header):
    """start and end of the function whose definition starts with header"""
    i = s.index(header)
    j = s.index('\n}\n', i)
    return i, j

# --- buffer manager: count and time loads, count writes -------------------
def bm(s):
    s = once(s, '#include "MyDB_BufferManager.h"', '#include "MyDB_BufferManager.h"\n#include "PageCounters.h"')
    for who in ['updateMe', 'returnVal']:
        s = once(s, f'read (fds[{who}->myTable], {who}->bytes, pageSize);',
                 f'{{LoadTimer lt ({who}->myTable == nullptr); read (fds[{who}->myTable], {who}->bytes, pageSize);}}')
    s = s.replace('\twrite (fds[', '\tcounters ().writes++; write (fds[')
    return s
patch('BufferMgr/source/MyDB_BufferManager.cc', bm)

# --- the tree --------------------------------------------------------------
def tree(s):
    s = once(s, '#include "RecordComparator.h"', '#include "RecordComparator.h"\n#include "PageCounters.h"')

    # checkReset: the root check
    i, j = body(s, 'void MyDB_BPlusTreeReaderWriter :: checkReset ()')
    s = s[:i] + s[i:j].replace('(*this)[rootLocation].getType ()',
        '(counters ().access (CAT_RESET), (*this)[rootLocation]).getType ()') + s[j:]

    # discoverPages: the directory page searched, and the peek at a child
    i, j = body(s, 'void MyDB_BPlusTreeReaderWriter :: discoverPages')
    b = s[i:j]
    b = once(b, 'MyDB_PageReaderWriter page = (*this)[whichPage];',
             'MyDB_PageReaderWriter page = (counters ().access (CAT_DIR_SEARCH), (*this)[whichPage]);')
    peek = 'if ((*this)[children[0]].getType () == MyDB_PageType :: RegularPage) {'
    b = once(b, peek,
        '#ifdef OLD_DISCOVER\n'
        '\t// the Stage 3 search: every child kept is read for its type\n'
        '\tfor (int child : children) {\n'
        '\t\tcounters ().access (CAT_CHILD_PEEK);\n'
        '\t\tif ((*this)[child].getType () == MyDB_PageType :: RegularPage)\n'
        '\t\t\tlist.push_back ((*this)[child]);\n'
        '\t\telse\n'
        '\t\t\tdiscoverPages (child, list, low, high);\n'
        '\t}\n'
        '\treturn;\n'
        '#endif\n'
        '\tcounters ().access (CAT_CHILD_PEEK);\n\t' + peek)
    s = s[:i] + b + s[j:]

    # append: every page it touches, after the root check
    i, j = body(s, 'void MyDB_BPlusTreeReaderWriter :: append (MyDB_RecordPtr appendMe)')
    b = s[i:j]
    k = b.index('checkReset ();') + len('checkReset ();')
    b = b[:k] + re.sub(r'\(\*this\)\[((?:[^\[\]]|\[[^\]]*\])+)\]', r'(counters ().access (CAT_APPEND), (*this)[\1])', b[k:])
    b = b.replace('\treturn;', '\t{counters ().current = CAT_OTHER; return;}')
    s = s[:i] + b + '\n\tcounters ().current = CAT_OTHER;' + s[j:]
    return s
patch('DatabaseTable/source/MyDB_BPlusTreeReaderWriter.cc', tree)

# --- the range iterator: every leaf it reads -------------------------------
def it(s):
    s = once(s, '#include <vector>', '#include <vector>\n#include "PageCounters.h"')
    s = once(s, '\t\t\t\tcurPage++;\n', '\t\t\t\tcurPage++;\n\t\t\t\tcounters ().access (CAT_LEAF_ITER);\n')
    s = once(s, '\t\tif (forUs.size () == 0)\n\t\t\treturn;\n',
             '\t\tif (forUs.size () == 0)\n\t\t\treturn;\n\t\tcounters ().access (CAT_LEAF_ITER);\n')
    return s
patch('DatabaseTable/headers/MyDB_PageListIteratorSelfSortingAlt.h', it)
print('patched copy in', out)
