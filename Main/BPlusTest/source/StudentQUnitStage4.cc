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
// As in Stages 2 and 3, every test checks behaviour only: every record
// appended is also put in an oracle, answers are compared with it as whole
// records, no test assumes where a split falls, the unsorted iterator always
// runs before the sorted one, and walkTree () checks the structure through
// the public interface.
//
// Tests 2-6 close a tree and re-open it from the catalog with a fresh buffer
// manager (closeAndReopen in StudentTestHelpers.h).  Tests 7-11 use
// loadFromTextFile () on supplier.tbl (10,000 records, suppkey 1 .. 10000,
// each once); their oracle holds each record as text, so every field is
// compared.  Test 10 writes, and removes, an empty file, empty.tbl.  Tests
// 12-14 check that a range query reads no leaf it does not need: they plant
// "trap" records, with keys inside the query range, on leaves whose key
// bounds lie entirely outside it, and count what comes back; a tree that
// reads such a leaf returns a trap.
//
// Stage 5: isValid () (StudentValidHelpers.h) is asked wherever walkTree ()
// is: after every re-open and every load (after a query, for the empty
// file), and before any traps are planted, since a trap is a record outside
// its leaf's bounds.  Test 15 checks that isValid () changes nothing, on a
// valid tree and on a damaged one; Test 16 damages a re-opened tree.
// ---------------------------------------------------------------------

#include "StudentTestHelpers.h"
#include "StudentValidHelpers.h"
#include <cstdio>
#include <fstream>

// a fixed-width (8 character) payload that is unique for each i
static string pad (int i) {
	char buf[32];
	snprintf (buf, sizeof (buf), "p%07d", i);
	return string (buf);
}

// appends keys (i * 7919) % n, i = 0 .. n - 1 (each of 0 .. n - 1 once, in
// a scrambled order), plus a second copy of every multiple of 10
static void scrambled (MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle, int n) {
	for (int i = 0; i < n; i++) {
		int key = (int) (((long) i * 7919) % n);
		appendBoth (tree, oracle, key, pad (i));
		if (key % 10 == 0)
			appendBoth (tree, oracle, key, pad (i + n));
	}
}

// the ranges whose sorted answers Tests 3 and 4 compare across a re-open
static const vector <pair <int, int>> someRanges = {
	{INT_MIN, INT_MAX}, {0, 0}, {10, 10}, {37, 412}, {-5, 3}, {598, 700}, {599, 599}, {250, 260}};

// Tests 3 and 4: a deep tree with numFrames buffer frames, closed and
// re-opened.  Sorted queries run before the close: they sort leaves in place,
// so those pages are dirty, and with few frames many are written back when
// evicted rather than when the buffer manager goes away
static void reopenDeep (QUnit::UnitTest &qunit, size_t numFrames) {
	MyDB_BufferManagerPtr myMgr;
	MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, numFrames);
	Oracle oracle;
	scrambled (*tree, oracle, 600);

	int levels = walkTree (qunit, *tree);
	checkValid (qunit, *tree);
	QUNIT_IS_TRUE (levels >= 4);
	int pages = tree->getNumPages ();
	int root = tree->getTable ()->getRootLocation ();

	vector <vector <pair <int, string>>> before;
	for (auto &r : someRanges) {
		collectRange (*tree, r.first, r.second, false);
		before.push_back (collectRange (*tree, r.first, r.second, true));
	}

	closeAndReopen (tree, myMgr, "id", 128, numFrames);

	QUNIT_IS_EQUAL (tree->getNumPages (), pages);
	QUNIT_IS_EQUAL (tree->getTable ()->getRootLocation (), root);
	QUNIT_IS_EQUAL (walkTree (qunit, *tree), levels);
	checkValid (qunit, *tree);
	checkAll (qunit, *tree, oracle, -1, 600);

	// the sorted answers have the same keys in the same order, and the same
	// records, as before the close
	for (size_t i = 0; i < someRanges.size (); i++) {
		vector <pair <int, string>> after = collectRange (*tree, someRanges[i].first, someRanges[i].second, true);
		QUNIT_IS_TRUE (sortedKeys (after) == sortedKeys (before[i]));
		bool sameOrder = after.size () == before[i].size ();
		for (size_t j = 0; sameOrder && j < after.size (); j++)
			sameOrder = (after[j].first == before[i][j].first);
		QUNIT_IS_TRUE (sameOrder);
		std::sort (after.begin (), after.end ());
		std::sort (before[i].begin (), before[i].end ());
		QUNIT_IS_TRUE (after == before[i]);
	}
}

// Tests 7-11: the oracle for supplier.tbl, and a check of a supplier tree
// against an oracle, with every field compared: the full range, a full
// scan, a few wide ranges, and the point and two-key ranges at every key in
// three windows of [lowKey, highKey] (its two ends and its middle; checking
// every key of 10,000 in each test would take too long)
static Oracle fileOracle (MyDB_BPlusTreeReaderWriter &tree) {
	size_t maxBytes;
	return supplierOracle (tree, "supplier.tbl", maxBytes);
}

static void checkSupplier (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle,
	int lowKey, int highKey) {
	int mid = lowKey + (highKey - lowKey) / 2;
	checkAllAtts (qunit, tree, oracle, lowKey, min (highKey, lowKey + 400), 0, -1);
	checkAllAtts (qunit, tree, oracle, max (lowKey, mid - 200), min (highKey, mid + 200), 0, -1);
	checkAllAtts (qunit, tree, oracle, max (lowKey, highKey - 400), highKey, 0, -1);
	checkRangeAtts (qunit, tree, oracle, 1, 10000, 0, -1);
	checkRangeAtts (qunit, tree, oracle, 2500, 7499, 0, -1);
	checkRangeAtts (qunit, tree, oracle, 9990, 20000, 0, -1);
}

// the page size for the supplier trees: room (after the page header) for
// at least four of the largest record in supplier.tbl
static const size_t supplierPage = 1024;

// Tests 12 and 13: plants traps for [low, high] in tree and checks that
// both iterators return exactly the oracle's records for that range, and
// none of the traps.  Returns the number of traps planted
static int trapQuery (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle,
	int lowKey, int highKey, int trapKey) {
	int planted = plantTraps (tree, lowKey, highKey, trapKey);
	vector <pair <int, string>> expected = oracleRange (oracle, lowKey, highKey);
	QUNIT_IS_EQUAL ((int) collectRange (tree, lowKey, highKey, false).size (), (int) expected.size ());
	QUNIT_IS_EQUAL ((int) collectRange (tree, lowKey, highKey, true).size (), (int) expected.size ());
	checkRange (qunit, tree, oracle, lowKey, highKey);
	return planted;
}

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
		checkValid (qunit, *tree);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 2:
	{
		cout << "TEST 2: re-open a tree of one leaf..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);
		Oracle oracle;
		for (int i = 0; i < 12; i++)
			appendBoth (*tree, oracle, (i * 5) % 12, pad (i));
		appendBoth (*tree, oracle, 3, "again");
		int root = tree->getTable ()->getRootLocation ();

		closeAndReopen (tree, myMgr, "id", 1024, 16);

		QUNIT_IS_EQUAL (tree->getTable ()->getRootLocation (), root);
		QUNIT_IS_EQUAL (walkTree (qunit, *tree), 2);
		checkValid (qunit, *tree);
		checkAll (qunit, *tree, oracle, -1, 13);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 3:
	{
		cout << "TEST 3: re-open a deep tree..." << endl << flush;
		reopenDeep (qunit, 16);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 4:
	{
		cout << "TEST 4: re-open a deep tree built with only 4 buffer frames..." << endl << flush;
		reopenDeep (qunit, 4);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 5:
	{
		cout << "TEST 5: append after re-opening, three round trips..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;
		scrambled (*tree, oracle, 200);

		for (int round = 1; round <= 3; round++) {
			closeAndReopen (tree, myMgr, "id", 128, 16);
			checkRange (qunit, *tree, oracle, INT_MIN, INT_MAX);

			// new keys on both ends and in the middle, and more copies of
			// old keys, enough to split leaves, directory pages and the root
			int levels = walkTree (qunit, *tree);
			checkValid (qunit, *tree);
			for (int i = 0; i < 300; i++) {
				int key = (i % 3 == 0) ? -round * 1000 - i : (i % 3 == 1) ? round * 1000 + i : (i * 13) % 200;
				appendBoth (*tree, oracle, key, pad (round * 10000 + i));
			}
			QUNIT_IS_TRUE (walkTree (qunit, *tree) >= levels);
			checkValid (qunit, *tree);
			checkAll (qunit, *tree, oracle, -1, 200);
			checkRange (qunit, *tree, oracle, -round * 1000 - 300, -round * 1000);
			checkRange (qunit, *tree, oracle, round * 1000, round * 1000 + 300);
		}
		closeAndReopen (tree, myMgr, "id", 128, 16);
		walkTree (qunit, *tree);
		checkValid (qunit, *tree);
		checkRange (qunit, *tree, oracle, INT_MIN, INT_MAX);
		checkAll (qunit, *tree, oracle, -1, 200);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 6:
	{
		cout << "TEST 6: re-open an empty tree..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;

		closeAndReopen (tree, myMgr, "id", 128, 16);

		checkBoth (qunit, *tree, INT_MIN, INT_MAX, 0);
		checkBoth (qunit, *tree, 0, 0, 0);
		QUNIT_IS_EQUAL (countScan (*tree), 0);
		QUNIT_IS_EQUAL (walkTree (qunit, *tree), 2);
		checkValid (qunit, *tree);

		scrambled (*tree, oracle, 150);
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 3);
		checkValid (qunit, *tree);
		checkAll (qunit, *tree, oracle, -1, 150);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 7:
	{
		cout << "TEST 7: loadFromTextFile into a new tree..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeSupplierTree (myMgr, supplierPage, 16);
		size_t maxBytes;
		Oracle oracle = supplierOracle (*tree, "supplier.tbl", maxBytes);
		QUNIT_IS_EQUAL ((int) oracle.size (), 10000);

		// the page holds at least four of the largest record, so every leaf
		// split has room for both halves
		QUNIT_IS_TRUE (4 * maxBytes <= supplierPage - 2 * sizeof (size_t));

		tree->loadFromTextFile ("supplier.tbl");
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 3);
		checkValid (qunit, *tree);
		checkSupplier (qunit, *tree, oracle, 0, 10001);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 8:
	{
		cout << "TEST 8: loadFromTextFile replaces what the tree held..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeSupplierTree (myMgr, supplierPage, 16);
		Oracle old;
		for (int i = 0; i < 1500; i++) {
			int key = (i % 2 == 0) ? 20000 + i : -i;
			appendSupplier (*tree, old, key, "old" + to_string (i));
		}
		for (int i = 0; i < 40; i++)
			appendSupplier (*tree, old, 1 + (i * 37) % 10000, "dup" + to_string (i));
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 3);
		checkValid (qunit, *tree);

		tree->loadFromTextFile ("supplier.tbl");

		Oracle oracle = fileOracle (*tree);
		walkTree (qunit, *tree);
		checkValid (qunit, *tree);
		checkSupplier (qunit, *tree, oracle, -1, 10001);
		checkBoth (qunit, *tree, -1000, 0, 0);
		checkBoth (qunit, *tree, 10001, INT_MAX, 0);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 9:
	{
		cout << "TEST 9: loadFromTextFile twice..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeSupplierTree (myMgr, supplierPage, 16);
		tree->loadFromTextFile ("supplier.tbl");
		tree->loadFromTextFile ("supplier.tbl");

		Oracle oracle = fileOracle (*tree);
		walkTree (qunit, *tree);
		checkValid (qunit, *tree);
		checkSupplier (qunit, *tree, oracle, 0, 10001);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 10:
	{
		cout << "TEST 10: loadFromTextFile of an empty file..." << endl << flush;
		{
			ofstream empty ("empty.tbl");
		}
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeSupplierTree (myMgr, supplierPage, 16);
		tree->loadFromTextFile ("supplier.tbl");
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 3);
		checkValid (qunit, *tree);

		tree->loadFromTextFile ("empty.tbl");

		checkBoth (qunit, *tree, INT_MIN, INT_MAX, 0);
		checkBoth (qunit, *tree, 1, 1, 0);
		checkBoth (qunit, *tree, 5000, 6000, 0);
		QUNIT_IS_EQUAL (countScan (*tree), 0);

		// printTree () must cope with the emptied tree (its output is not checked)
		{
			ostringstream sink;
			streambuf *saved = cout.rdbuf (sink.rdbuf ());
			tree->printTree ();
			cout.rdbuf (saved);
		}
		QUNIT_IS_EQUAL (walkTree (qunit, *tree), 2);
		checkValid (qunit, *tree);

		// appends then build a deep tree again from nothing
		Oracle oracle;
		for (int i = 0; i < 1500; i++)
			appendSupplier (*tree, oracle, (i * 7) % 1500, "new" + to_string (i));
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 3);
		checkValid (qunit, *tree);
		checkSupplier (qunit, *tree, oracle, -1, 1500);
		unlink ("empty.tbl");
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 11:
	{
		cout << "TEST 11: loadFromTextFile, then re-open..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeSupplierTree (myMgr, supplierPage, 16);
		tree->loadFromTextFile ("supplier.tbl");
		Oracle oracle = fileOracle (*tree);

		closeAndReopen (tree, myMgr, "suppkey", supplierPage, 16);
		int levels = walkTree (qunit, *tree);
		checkValid (qunit, *tree);
		QUNIT_IS_TRUE (levels >= 3);
		checkSupplier (qunit, *tree, oracle, 0, 10001);

		for (int i = 0; i < 300; i++) {
			int key = (i % 2 == 0) ? 10000 + i : 1 + (i * 31) % 10000;
			appendSupplier (*tree, oracle, key, "more" + to_string (i));
		}
		walkTree (qunit, *tree);
		checkValid (qunit, *tree);
		checkSupplier (qunit, *tree, oracle, 0, 10301);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 12:
	{
		cout << "TEST 12: range queries read no leaf outside the range..." << endl << flush;

		// each range gets its own tree, so that no trap lies inside a later range
		struct Range {int low, high, trap;};
		vector <Range> ranges = {{300, 300, 300}, {120, 180, 150}, {INT_MIN, 40, 0}, {560, INT_MAX, 599},
			{-50, 5, -7}, {250, 251, 251}};
		for (Range &r : ranges) {
			MyDB_BufferManagerPtr myMgr;
			MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
			Oracle oracle;
			scrambled (*tree, oracle, 600);
			QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);
			checkValid (qunit, *tree);
			int planted = trapQuery (qunit, *tree, oracle, r.low, r.high, r.trap);
			QUNIT_IS_TRUE (planted >= 10);
		}
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 13:
	{
		cout << "TEST 13: no needless leaf, and no needed leaf skipped, with duplicates..." << endl << flush;

		// 60 copies of key 500 straddle many leaves; a query on 500 must read
		// every leaf that can hold it, and none that cannot
		struct Range {int low, high, trap;};
		vector <Range> ranges = {{500, 500, 500}, {499, 501, 500}, {500, 520, 510}, {480, 500, 490}};
		for (Range &r : ranges) {
			MyDB_BufferManagerPtr myMgr;
			MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
			Oracle oracle;
			for (int i = 0; i < 1000; i++) {
				int key = (int) (((long) i * 7919) % 1000);
				appendBoth (*tree, oracle, key, pad (i));
				if (i % 15 == 0)
					appendBoth (*tree, oracle, 500, pad (5000 + i));
			}
			QUNIT_IS_EQUAL ((int) oracleRange (oracle, 500, 500).size (), 68);
			QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);
			checkValid (qunit, *tree);
			int planted = trapQuery (qunit, *tree, oracle, r.low, r.high, r.trap);
			QUNIT_IS_TRUE (planted >= 10);
		}
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 14:
	{
		cout << "TEST 14: still no needless leaf after re-opening..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;
		scrambled (*tree, oracle, 600);
		int planted = plantTraps (*tree, 200, 230, 215);
		QUNIT_IS_TRUE (planted >= 10);

		closeAndReopen (tree, myMgr, "id", 128, 16);

		vector <pair <int, string>> expected = oracleRange (oracle, 200, 230);
		QUNIT_IS_EQUAL ((int) collectRange (*tree, 200, 230, false).size (), (int) expected.size ());
		QUNIT_IS_EQUAL ((int) collectRange (*tree, 200, 230, true).size (), (int) expected.size ());
		checkRange (qunit, *tree, oracle, 200, 230);
		checkRange (qunit, *tree, oracle, 215, 215);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 15:
	{
		cout << "TEST 15: isValid () changes nothing..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;
		scrambled (*tree, oracle, 600);
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);

		// a valid tree: the same pages, root, and records before and after
		int pages = tree->getNumPages ();
		int root = rootOf (*tree);
		vector <vector <pair <int, string>>> before;
		for (auto &r : someRanges)
			before.push_back (collectRange (*tree, r.first, r.second, false));
		checkValid (qunit, *tree);
		checkValid (qunit, *tree);
		QUNIT_IS_EQUAL (tree->getNumPages (), pages);
		QUNIT_IS_EQUAL (rootOf (*tree), root);
		for (size_t i = 0; i < someRanges.size (); i++)
			QUNIT_IS_TRUE (collectRange (*tree, someRanges[i].first, someRanges[i].second, false) == before[i]);
		checkAll (qunit, *tree, oracle, -1, 600);

		// a damaged tree (a pointer past the end of the file): isValid () must
		// not grow the file or move the root.  No query is run on it
		QUNIT_IS_TRUE (corruptBadPointer (*tree));
		pages = tree->getNumPages ();
		expectInvalid (qunit, *tree, "a root entry points past the end of the file");
		expectInvalid (qunit, *tree, "the same, asked again");
		QUNIT_IS_EQUAL (tree->getNumPages (), pages);
		QUNIT_IS_EQUAL (rootOf (*tree), root);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 16:
	{
		cout << "TEST 16: invalid: a re-opened tree, damaged..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;
		scrambled (*tree, oracle, 600);
		closeAndReopen (tree, myMgr, "id", 128, 16);
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);
		checkValid (qunit, *tree);
		QUNIT_IS_TRUE (corruptTwoParents (*tree));
		expectInvalid (qunit, *tree, "two root entries point to the same page");
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
