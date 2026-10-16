#ifndef STUDENT_BPLUS_TEST_STAGE4_H
#define STUDENT_BPLUS_TEST_STAGE4_H

// ---------------------------------------------------------------------
// STAGE 4 test suite.  Scope of this stage: the rest: discoverPages efficiency, re-opening a tree from the catalog, and the big test in BPlusQUnit.cc.
//
// This file is graded by mutation (see the A4 handout): we compile it
// against a set of deliberately broken B+-trees, and the score depends on
// how many of them it catches.  So that we can do that:
//
//   * Use only the PUBLIC interface of MyDB_BPlusTreeReaderWriter (the
//     constructor, append (), the two range iterators, isValid (), and
//     printTree ()), plus the rest of the public MyDB interface.  We
//     compile this file and everything in Main/BPlusTest/headers against
//     OUR MyDB_BPlusTreeReaderWriter.h and .cc, so anything you add to
//     that header, or to any other file under Main/DatabaseTable, is not
//     there.  Test code goes in this file; helpers go in headers.
//   * Follow the iterator protocol: advance () first, then getCurrent (),
//     and never call advance () again once it has returned false (the page
//     iterator exits the program on that, even on a correct tree).
//   * Every test must be deterministic: no un-seeded randomness, no
//     dependence on the clock, no files other than the .tbl files in Build.
//   * main () must return the number of failed checks (it does, below),
//     and the suite must run in under a minute on Clear.
//   * Keep the "start from test N" convention: ./bin/studentUnitTestStage4 3
//     starts at test 3, and ./bin/studentUnitTestStage4 3 only runs test 3
//     and stops.
//
// The one test below shows the format and nothing more.
// ---------------------------------------------------------------------

#include "StudentTestHelpers.h"

int main (int argc, char *argv[]) {
	int start;
	bool only;
	parseArgs (argc, argv, start, only);

	QUnit::UnitTest qunit (cerr, QUnit::normal);

	switch (start) {

	case 1:
	{
		cout << "TEST 1: append one record and get it back..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		tree->append (fill (rec, 7, "seven"));
		QUNIT_IS_EQUAL (countRange (qunit, *tree, 7, 7, false), 1);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 2:
	{
		// your tests go here, one case per test
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	default:
		break;
	}

	unlink ("catFile");
	unlink ("tiny.bin");

	return qunit.errors ();
}

#endif
