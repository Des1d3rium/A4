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

Implement the constructor: Reuse the existing base-class storage for the table and buffer manager. Use the existing ‘orderingAttType’ and ‘whichAttIsOrdering’ members to store information about the ordering attribute. For a new tree, Initiate two pages at the beginning: Page 0 as empty leaf page with type ‘RegularPage’ for later data update; Page 1 as directory page with type ‘DirectoryPage’; Store a ‘MyDB_INRecord’ in root with maximum value, and child pointer to 0; update ‘rootLocation’. If the root is already existed, one should not overwrite it. Duplicate keys are allowed. 

Implement the append():  According to the root location of this tree, find down through the directory, construct an node, append the node onto tree. 

Implement both range iterator: Since they are mostly same, reuse as many code snippet as you could. Add all satisfied pages (low <= currentKey && currentKey <= high) into recordIterator. Use ‘buildComparator’ in your comparision. For ‘getSortedRangeIteratorAlt’, add one more ascending comparison for inter-pages sort. Wrap and pass all needed things into recordIterator. 

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

When the leaves are filled and be appended, follow this procedure: copy all records and the appended one into temporary space; sort them with comparator; split them into two half with the median key, make sure both pages accommodate both half without overflow; arrange a new leaf and write lower half into it; larger half goes into original leaf. Make sure when duplicate key are split into two leaves, both leaves need to be accessed. Pass the root node with the maximum key of lower half, and insert it in sorted order. Place the entry exactly before the original leaf's directory entry.

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

<your specification for Stage 3 goes here>

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

<your specification for Stage 4 goes here>

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

<your specification for Stage 5 goes here>

----------------------------------------------------------------------
