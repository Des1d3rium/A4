# The stage prompts

Each stage of A4 starts with ONE opening prompt to an AI tool.  Use one
continuing conversation for the whole assignment if you can, so that the
tool remembers the decisions it has already asked you about; the preambles
below mark where each stage begins.  If you start a new conversation for a
stage, its opening prompt must carry forward every decision already made.
The opening prompt begins with the preamble for that stage, copied exactly
from below, followed by your own specification: what the stage has to do,
the design decisions you have made, the cases you are worried about, and a
request that the tool propose its test list before writing anything so
that you can approve or reject each test (see the handout, Sections 2 and
3).  Ask for the design decisions to be recorded in a comment block at the
top of MyDB_BPlusTreeReaderWriter.cc, updated at every stage.  Follow-up
prompts are fine and expected.  Every conversation is exported into AILog/
(one subdirectory per conversation, named for the stages it covers).

----------------------------------------------------------------------

## Stage 1: A root and a single leaf: the constructor, append (), both range iterators, printTree ().  No splits.

This is Stage 1 of COMP 530 (Database System Implementation) Assignment 4
at Rice University. This assignment is being completed for academic purposes
by a student, with the help of an AI tool. The AI tool must restrict any code
or documentation it develops to the tasks directly requested in the remainder
of this prompt, and must complete those tasks using only general knowledge of
database systems and C++, the assignment handout, and the distributed code
base: no web search, and no code repositories. Where the handout leaves a
design decision open and this prompt does not settle it, the tool must ask the
student rather than choose. Any other questions or clarifications needed to
complete the task must be directed back to the student who wrote this prompt.
In Stage 1, development is restricted to the following existing files:
  - Main/DatabaseTable/source/MyDB_BPlusTreeReaderWriter.cc
  - the private section of Main/DatabaseTable/headers/MyDB_BPlusTreeReaderWriter.h
  - Main/BPlusTest/headers/StudentTestHelpers.h
  - Main/BPlusTest/source/StudentQUnitStage1.cc

New header and source files may be created as necessary under
Main/DatabaseTable/headers, Main/DatabaseTable/source and
Main/BPlusTest/headers (the build picks them up automatically). No other
existing file may be changed, and test code goes only in the stage's
StudentQUnitStage1.cc file.

Implement the constructor: Reuse the existing base-class storage for the table and buffer manager. Use the existing ‘orderingAttType’ and ‘whichAttIsOrdering’ members to store information about the ordering attribute. For a new tree, Initiate two pages at the beginning: Page 0 as empty leaf page with type ‘RegularPage’ for later data update; Page 1 as directory page with type ‘DirectoryPage’; Store a ‘MyDB_INRecord’ in root with maximum value, and child pointer to 0; update ‘rootLocation’. If the root is already existed, one should not overwrite it. Duplicate keys are allowed. When 'rootLocation == -1', we claims it is a new tree. 

Implement the append():  According to the root location of this tree, find down through the directory, construct an node with records, append the node onto tree. 

Implement both range iterator: Since they are mostly same, reuse as many code snippet as you could. Add all satisfied pages (low <= currentKey && currentKey <= high) into recordIterator. Use ‘buildComparator’ in your comparision. For ‘getSortedRangeIteratorAlt’, add one more ascending comparison for inter-pages sort. Wrap and pass all needed things into recordIterator. To avoid 'low' and 'high' being modify after call of iterator, use a copy inside iterator. 

Implement printTree(): Implement an recursive helper method doing all follows: For all directory page, call ‘getInRecord’ for upper bound and child page number, and print them. For all regularPage, call ‘getEmptyRecord()’ for data records, and print them.  

Testing: Write tests that correctly handle following circumstances in ‘StudentQUnitStage1.cc’ before actually writing the code: 

- Initialization of new tree
- Empty tree
- Single Record
- Correct implementation of closed interval
- No drop of duplicate keys
- Fill tree
- Correct function of printTree() 

----------------------------------------------------------------------

## Stage 2: Leaf splits.  The root is still one page, but it points to more and more leaves.

This is Stage 2 of COMP 530 (Database System Implementation) Assignment 4
at Rice University. This assignment is being completed for academic purposes
by a student, with the help of an AI tool. The AI tool must restrict any code
or documentation it develops to the tasks directly requested in the remainder
of this prompt, and must complete those tasks using only general knowledge of
database systems and C++, the assignment handout, and the distributed code
base: no web search, and no code repositories. Where the handout leaves a
design decision open and this prompt does not settle it, the tool must ask the
student rather than choose. Any other questions or clarifications needed to
complete the task must be directed back to the student who wrote this prompt.
In Stage 2, development is restricted to the following existing files:
  - Main/DatabaseTable/source/MyDB_BPlusTreeReaderWriter.cc
  - the private section of Main/DatabaseTable/headers/MyDB_BPlusTreeReaderWriter.h
  - Main/BPlusTest/headers/StudentTestHelpers.h
  - Main/BPlusTest/source/StudentQUnitStage2.cc

New header and source files may be created as necessary under
Main/DatabaseTable/headers, Main/DatabaseTable/source and
Main/BPlusTest/headers (the build picks them up automatically). No other
existing file may be changed, and test code goes only in the stage's
StudentQUnitStage2.cc file.

When appending nodes and no split are required, search down into regularPage and place it at the end of leaf. We sort record only at split or sortIterator.

When the leaves are filled and be appended, follow this procedure: copy all records and the appended one into temporary space; sort them with comparator; split them into two half with the median key, make sure both pages accommodate both half without overflow; arrange a new leaf and write lower half into it; larger half goes into original leaf. Make sure when duplicate key are split into two leaves, both leaves need to be accessed. Pass the root node with the (maximum key of left half, new left page number), and insert it in sorted order. Place the entry exactly before the original leaf's directory entry. If append() failure, records and tree structure should remain unchanged.  

Testing: Write tests that correctly handle following circumstances in ‘StudentQUnitStage2.cc’ before actually writing the code. Keep the test scale small enough that the root does not overflow in stage 2.  
- split of right leaf and left leaf
- after split, all records remains intact
- random insertion that might cause split
- correct up-copy of key into directory root
- duplicate key among two/several leaves
- query across multiple leaves
- insertion into root when root is full should raise an exception
- tests in stage 1 should still function properly, except that Stage 2 split after full page


----------------------------------------------------------------------

## Stage 3: Internal splits and the root split.  The tree gets deeper.

This is Stage 3 of COMP 530 (Database System Implementation) Assignment 4
at Rice University. This assignment is being completed for academic purposes
by a student, with the help of an AI tool. The AI tool must restrict any code
or documentation it develops to the tasks directly requested in the remainder
of this prompt, and must complete those tasks using only general knowledge of
database systems and C++, the assignment handout, and the distributed code
base: no web search, and no code repositories. Where the handout leaves a
design decision open and this prompt does not settle it, the tool must ask the
student rather than choose. Any other questions or clarifications needed to
complete the task must be directed back to the student who wrote this prompt.
In Stage 3, development is restricted to the following existing files:
  - Main/DatabaseTable/source/MyDB_BPlusTreeReaderWriter.cc
  - the private section of Main/DatabaseTable/headers/MyDB_BPlusTreeReaderWriter.h
  - Main/BPlusTest/headers/StudentTestHelpers.h
  - Main/BPlusTest/source/StudentQUnitStage3.cc

New header and source files may be created as necessary under
Main/DatabaseTable/headers, Main/DatabaseTable/source and
Main/BPlusTest/headers (the build picks them up automatically). No other
existing file may be changed, and test code goes only in the stage's
StudentQUnitStage3.cc file.

As in Stage 2, we still append data record into end of leaf if possible. We split when the directoryPage, or root in the first time, is overflow with new directory entry. Original directoryPage (could be root) are split into two halves, we passed the parent with (median key, left Page number). When even number of PE exists, the rightmost PE of left half are copied. i.e., when there are 4 nodes, the second PE are copied. If the root is split, update the rootLocation immediately after. Make sure all children are pointed once and only once after split. the last (key, ptr) pair inside internal page is (upperBound, childPage). If the child is internal page, the key of its last entry is upperBound. Root always has MAX as its last key of entry. 

Testing: Write tests that correctly handle following circumstances in ‘StudentQUnitStage3.cc’ before actually writing the code. 
- new root, rootLocation and all data are accessible after root split
- correct insertion into directionPage
- Multiple split among layers after onc insertion
- test cases in stage 2 still function, involving multiple keys, range query, etc. 
----------------------------------------------------------------------

## Stage 4: The rest: re-opening a tree from the catalog with a fresh buffer manager, avoiding needless page reads in the range iterators, and passing my tests in BPlusQUnit.cc.

This is Stage 4 of COMP 530 (Database System Implementation) Assignment 4
at Rice University. This assignment is being completed for academic purposes
by a student, with the help of an AI tool. The AI tool must restrict any code
or documentation it develops to the tasks directly requested in the remainder
of this prompt, and must complete those tasks using only general knowledge of
database systems and C++, the assignment handout, and the distributed code
base: no web search, and no code repositories. Where the handout leaves a
design decision open and this prompt does not settle it, the tool must ask the
student rather than choose. Any other questions or clarifications needed to
complete the task must be directed back to the student who wrote this prompt.
In Stage 4, development is restricted to the following existing files:
  - Main/DatabaseTable/source/MyDB_BPlusTreeReaderWriter.cc
  - the private section of Main/DatabaseTable/headers/MyDB_BPlusTreeReaderWriter.h
  - Main/BPlusTest/headers/StudentTestHelpers.h
  - Main/BPlusTest/source/StudentQUnitStage4.cc

New header and source files may be created as necessary under
Main/DatabaseTable/headers, Main/DatabaseTable/source and
Main/BPlusTest/headers (the build picks them up automatically). No other
existing file may be changed, and test code goes only in the stage's
StudentQUnitStage4.cc file.

When re-opening a tree from catalog, ensure table, buffer manager, all threads and objects from current process of tree are released. page data and metadata are saved before we read from the catalog. Dirty pages are written back into disk. Loading metadata from catalog, and loads pages from disk. 
Reopening from catalog preserve all existing records and tree structure. 'loadFromTextFile', on the other hand, replaces with exactly same records from file without records from previous content. Do intend to modify the behavior of 'loadFromTextFile', as it reset the lastPage to Page 0 and clear it. However, rootLocation is not changed, leading potential access exceed maximum index of pages. Do it before the first append(), as empty file would not call append() at all. 
To ensure range-query efficiency, access a child only if its upper bound is >= low and preceding upper bound is <= high.  


Testing: Write tests that correctly handle following circumstances in ‘StudentQUnitStage4.cc’ before actually writing the code.  
- verify key and payload are rebuild correctly from disks. Current tree are saved correctly. 
- range query, sort, insertions perform exactly same as before. 
- Using counter to ensure range-query efficiency. 
- pass all provided tests and previous tests, excepting 'isValid' and exception due to stage switch.   
----------------------------------------------------------------------

## Stage 5: isValid ().  Then add isValid () calls to the Stage 2, 3 and 4 suites, and fix whatever they turn up.

This is Stage 5 of COMP 530 (Database System Implementation) Assignment 4
at Rice University. This assignment is being completed for academic purposes
by a student, with the help of an AI tool. The AI tool must restrict any code
or documentation it develops to the tasks directly requested in the remainder
of this prompt, and must complete those tasks using only general knowledge of
database systems and C++, the assignment handout, and the distributed code
base: no web search, and no code repositories. Where the handout leaves a
design decision open and this prompt does not settle it, the tool must ask the
student rather than choose. Any other questions or clarifications needed to
complete the task must be directed back to the student who wrote this prompt.
In Stage 5, development is restricted to the following existing files:
  - Main/DatabaseTable/source/MyDB_BPlusTreeReaderWriter.cc
  - the private section of Main/DatabaseTable/headers/MyDB_BPlusTreeReaderWriter.h
  - Main/BPlusTest/source/StudentQUnitStage2.cc
  - Main/BPlusTest/source/StudentQUnitStage3.cc
  - Main/BPlusTest/source/StudentQUnitStage4.cc

New header and source files may be created as necessary under
Main/DatabaseTable/headers, Main/DatabaseTable/source and
Main/BPlusTest/headers (the build picks them up automatically). No other
existing file may be changed, and test code goes only in the
StudentQUnitStage2.cc, StudentQUnitStage3.cc and StudentQUnitStage4.cc files.

isValid implement following things:(1)read-only check, return false, and print reason and page number when there is a failure. (2)access after ensure page number is valid. (3) the root is the top of tree and match to metadata. (4)All leaf nodes should have same depth. No cycle appears in tree, no child could be pointed by two parents. (5) key in directoryPage allow duplicates and non-decreasing from left to right. (6) directory entry pointing to any leaf node is updated after split. (7) keys in leaf node are allowed to not be sorted. (8) root and only root is allowed pointing to an empty leaf node, specifying a empty tree. (9)verify whether the directoryPage/RegularPage is capable of before any write, including normal write and split write. Return false is any of these fails. 

Testing: Write tests in providing files that correctly handle following circumstances before actually writing the code. If something goes wrong, prompt me with exact problem and candidate solution.
- valid tree: testing existing functionality. return true after split, sorting iterator, range query, rebuild, overwrite, duplicate keys insertion, etc.
- invalid tree: invalid children pointer, unsorted entry in directoryPage, cycle, children with multiple pointers, range query fails to find children, children are placed in wrong leaf, tree with wrong structure.    
- remove tests made by stages restriction.
----------------------------------------------------------------------
