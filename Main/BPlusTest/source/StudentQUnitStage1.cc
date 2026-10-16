#ifndef STUDENT_BPLUS_TEST_STAGE1_H
#define STUDENT_BPLUS_TEST_STAGE1_H

// ---------------------------------------------------------------------
// STAGE 1 test suite.  Scope of this stage: a root and a single leaf: the constructor, append (), both range iterators, printTree ().  No splits.
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
//   * Keep the "start from test N" convention: ./bin/studentUnitTestStage1 3
//     starts at test 3, and ./bin/studentUnitTestStage1 3 only runs test 3
//     and stops.
//
// All tests check behaviour only (no assumptions about which page is the
// root or the leaf), so that they hold for any correct B+-tree.  No test
// ever puts more records into the tree than fit on a single leaf page.
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
		cout << "TEST 2: a new tree is initialized..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		// the constructor must record a root location in the table
		QUNIT_IS_NOT_EQUAL (tree->getTable ()->getRootLocation (), -1);
		QUNIT_IS_TRUE (tree->getTable ()->getRootLocation () >= 0);
		QUNIT_IS_TRUE (tree->getTable ()->getRootLocation () < tree->getNumPages ());

		// and must have built at least a directory page and a leaf page
		QUNIT_IS_TRUE (tree->getNumPages () >= 2);
		int numDir = 0, numLeaf = 0;
		for (int i = 0; i < tree->getNumPages (); i++) {
			if ((*tree)[i].getType () == MyDB_PageType :: DirectoryPage)
				numDir++;
			else
				numLeaf++;
		}
		QUNIT_IS_TRUE (numDir >= 1);
		QUNIT_IS_TRUE (numLeaf >= 1);

		// the root itself is a directory page
		QUNIT_IS_TRUE ((*tree)[tree->getTable ()->getRootLocation ()].getType () == MyDB_PageType :: DirectoryPage);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 3:
	{
		cout << "TEST 3: an empty tree returns nothing..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		checkBoth (qunit, *tree, INT_MIN, INT_MAX, 0);
		checkBoth (qunit, *tree, 0, 0, 0);
		checkBoth (qunit, *tree, -5, 5, 0);
		QUNIT_IS_EQUAL (countScan (*tree), 0);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 4:
	{
		cout << "TEST 4: a single record, in detail..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		tree->append (fill (rec, 7, "seven"));

		// every range that contains 7 returns exactly that record
		int ranges[3][2] = {{7, 7}, {0, 100}, {INT_MIN, INT_MAX}};
		for (auto &r : ranges) {
			for (int sorted = 0; sorted < 2; sorted++) {
				vector <pair <int, string>> res = collectRange (*tree, r[0], r[1], sorted);
				QUNIT_IS_EQUAL (res.size (), 1);
				if (res.size () == 1) {
					QUNIT_IS_EQUAL (res[0].first, 7);
					QUNIT_IS_EQUAL (res[0].second, "seven");
				}
			}
		}

		// and every range that does not contain 7 returns nothing
		checkBoth (qunit, *tree, 6, 6, 0);
		checkBoth (qunit, *tree, 8, 8, 0);
		checkBoth (qunit, *tree, 8, 100, 0);
		checkBoth (qunit, *tree, -100, 6, 0);
		QUNIT_IS_EQUAL (countScan (*tree), 1);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 5:
	{
		cout << "TEST 5: ranges are closed intervals..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		for (int i = 10; i <= 20; i++)
			tree->append (fill (rec, i, "v" + to_string (i)));

		checkBoth (qunit, *tree, 10, 20, 11);
		checkBoth (qunit, *tree, 10, 10, 1);
		checkBoth (qunit, *tree, 20, 20, 1);
		checkBoth (qunit, *tree, 11, 19, 9);
		checkBoth (qunit, *tree, 5, 12, 3);
		checkBoth (qunit, *tree, 18, 25, 3);
		checkBoth (qunit, *tree, 9, 9, 0);
		checkBoth (qunit, *tree, 21, 21, 0);
		checkBoth (qunit, *tree, 0, 9, 0);
		checkBoth (qunit, *tree, 21, 99, 0);

		// an inverted range is empty
		checkBoth (qunit, *tree, 15, 12, 0);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 6:
	{
		cout << "TEST 6: sorted output from scrambled input..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		// 0 .. 12 in a scrambled order, then -1 .. -3
		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		for (int i = 0; i < 13; i++)
			tree->append (fill (rec, (i * 7) % 13, "k"));
		for (int i = -1; i >= -3; i--)
			tree->append (fill (rec, i, "neg"));

		vector <int> expected;
		for (int i = -3; i <= 12; i++)
			expected.push_back (i);

		// the sorted iterator gives exactly the ascending sequence
		vector <pair <int, string>> res = collectRange (*tree, INT_MIN, INT_MAX, true);
		vector <int> keys;
		for (auto &p : res)
			keys.push_back (p.first);
		QUNIT_IS_TRUE (keys == expected);

		// the unsorted one gives the same set, in any order
		res = collectRange (*tree, INT_MIN, INT_MAX, false);
		QUNIT_IS_TRUE (sortedKeys (res) == expected);

		// a sub-range, which only includes some of the negative keys
		res = collectRange (*tree, -2, 9, true);
		QUNIT_IS_EQUAL (res.size (), 12);
		QUNIT_IS_TRUE (keysAscending (res));
		checkBoth (qunit, *tree, -2, 9, 12);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 7:
	{
		cout << "TEST 7: duplicate keys are all kept..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		for (int i = 0; i < 3; i++)
			tree->append (fill (rec, 6, "six" + to_string (i)));
		for (int i = 0; i < 10; i++)
			tree->append (fill (rec, 5, "five" + to_string (i)));
		for (int i = 0; i < 3; i++)
			tree->append (fill (rec, 4, "four" + to_string (i)));

		checkBoth (qunit, *tree, 5, 5, 10);
		checkBoth (qunit, *tree, 4, 6, 16);
		checkBoth (qunit, *tree, 4, 4, 3);
		checkBoth (qunit, *tree, 6, 6, 3);

		// all ten distinct payloads for key 5 come back, with both iterators
		for (int sorted = 0; sorted < 2; sorted++) {
			vector <pair <int, string>> res = collectRange (*tree, 5, 5, sorted);
			vector <string> payloads;
			for (auto &p : res)
				payloads.push_back (p.second);
			std::sort (payloads.begin (), payloads.end ());
			vector <string> expected;
			for (int i = 0; i < 10; i++)
				expected.push_back ("five" + to_string (i));
			QUNIT_IS_TRUE (payloads == expected);
		}
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 8:
	{
		cout << "TEST 8: fill the leaf exactly..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		// every record is the same size (fixed-width int key, 8-char payload),
		// so work out how many fit on one page after its two-size_t header
		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		size_t recSize = fill (rec, 0, "abcdefgh")->getBinarySize ();
		int n = (int) ((1024 - 2 * sizeof (size_t)) / recSize);
		cout << "  " << n << " records of " << recSize << " bytes" << endl << flush;

		// keys 0 .. n - 1, in the order 0, n-1, 1, n-2, ...
		for (int i = 0; i < n; i++) {
			int key = (i % 2 == 0) ? i / 2 : n - 1 - i / 2;
			tree->append (fill (rec, key, "abcdefgh"));
		}

		vector <int> expected;
		for (int i = 0; i < n; i++)
			expected.push_back (i);

		vector <pair <int, string>> res = collectRange (*tree, INT_MIN, INT_MAX, true);
		QUNIT_IS_EQUAL ((int) res.size (), n);
		QUNIT_IS_TRUE (keysAscending (res));
		QUNIT_IS_TRUE (sortedKeys (res) == expected);

		res = collectRange (*tree, INT_MIN, INT_MAX, false);
		QUNIT_IS_TRUE (sortedKeys (res) == expected);

		checkBoth (qunit, *tree, 0, n - 1, n);
		checkBoth (qunit, *tree, 1, n - 2, n - 2);
		QUNIT_IS_EQUAL (countScan (*tree), n);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 9:
	{
		cout << "TEST 9: queries interleaved with appends..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		tree->append (fill (rec, 20, "a"));
		tree->append (fill (rec, 10, "b"));
		tree->append (fill (rec, 30, "c"));

		// a sorted query, which may reorganize the leaf...
		checkBoth (qunit, *tree, 0, 100, 3);

		// ...must not stop later appends from working
		tree->append (fill (rec, 5, "d"));
		tree->append (fill (rec, 25, "e"));
		tree->append (fill (rec, 15, "f"));

		vector <int> expected = {5, 10, 15, 20, 25, 30};
		vector <pair <int, string>> res = collectRange (*tree, 0, 100, true);
		QUNIT_IS_TRUE (sortedKeys (res) == expected);
		QUNIT_IS_TRUE (keysAscending (res));
		res = collectRange (*tree, 0, 100, false);
		QUNIT_IS_TRUE (sortedKeys (res) == expected);

		// the payloads still go with the right keys
		res = collectRange (*tree, 15, 15, true);
		QUNIT_IS_EQUAL (res.size (), 1);
		if (res.size () == 1)
			QUNIT_IS_EQUAL (res[0].second, "f");

		tree->append (fill (rec, 1, "g"));
		checkBoth (qunit, *tree, 0, 100, 7);
		checkBoth (qunit, *tree, 1, 5, 2);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 10:
	{
		cout << "TEST 10: printTree () prints, and changes nothing..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		for (int i = 0; i < 8; i++)
			tree->append (fill (rec, (i * 3) % 8, "p" + to_string (i)));

		vector <pair <int, string>> before = collectRange (*tree, INT_MIN, INT_MAX, false);

		// capture what printTree () writes to cout
		stringstream captured;
		streambuf *old = cout.rdbuf (captured.rdbuf ());
		tree->printTree ();
		cout.rdbuf (old);
		QUNIT_IS_TRUE (captured.str ().size () > 0);

		vector <pair <int, string>> after = collectRange (*tree, INT_MIN, INT_MAX, false);
		std::sort (before.begin (), before.end ());
		std::sort (after.begin (), after.end ());
		QUNIT_IS_TRUE (before == after);
		QUNIT_IS_EQUAL (after.size (), 8);
		checkBoth (qunit, *tree, 0, 7, 8);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 11:
	{
		cout << "TEST 11: the constructor does not overwrite an existing root..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 1024, 16);

		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		for (int i = 1; i <= 5; i++)
			tree->append (fill (rec, i, "r" + to_string (i)));
		int rootBefore = tree->getTable ()->getRootLocation ();

		// a second tree over the same table and the same buffer manager
		MyDB_BPlusTreeReaderWriterPtr again = make_shared <MyDB_BPlusTreeReaderWriter> ("id", tree->getTable (), myMgr);
		QUNIT_IS_EQUAL (again->getTable ()->getRootLocation (), rootBefore);
		checkBoth (qunit, *again, 1, 5, 5);
		checkBoth (qunit, *again, 3, 3, 1);
		QUNIT_IS_EQUAL (countScan (*again), 5);

		// and it can still be appended to
		MyDB_RecordPtr rec2 = again->getEmptyRecord ();
		again->append (fill (rec2, 6, "r6"));
		checkBoth (qunit, *again, 1, 6, 6);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 12:
	{
		cout << "TEST 12: the key is not the first attribute..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeKeySecondTree (myMgr, 1024, 16);

		// schema is (payload, id).  The payloads sort in the opposite order to
		// the keys, and the keys include 2, 9, 10, 100 so that comparing them as
		// strings, or comparing the wrong attribute, gives a different answer
		int keys[] = {10, 2, 100, 9, 10, 55, 2, 30};
		MyDB_RecordPtr rec = tree->getEmptyRecord ();
		for (int k : keys) {
			rec->fromString ("p" + to_string (1000 - k) + "|" + to_string (k) + "|");
			tree->append (rec);
		}

		for (int sorted = 0; sorted < 2; sorted++) {
			vector <pair <int, string>> res = collectRange (*tree, INT_MIN, INT_MAX, sorted, 1, 0);
			vector <int> expected = {2, 2, 9, 10, 10, 30, 55, 100};
			QUNIT_IS_TRUE (sortedKeys (res) == expected);
			if (sorted)
				QUNIT_IS_TRUE (keysAscending (res));

			// each payload still belongs to its key
			for (auto &p : res)
				QUNIT_IS_EQUAL (p.second, "p" + to_string (1000 - p.first));

			res = collectRange (*tree, 9, 30, sorted, 1, 0);
			expected = {9, 10, 10, 30};
			QUNIT_IS_TRUE (sortedKeys (res) == expected);

			res = collectRange (*tree, 2, 2, sorted, 1, 0);
			QUNIT_IS_EQUAL (res.size (), 2);

			res = collectRange (*tree, 11, 29, sorted, 1, 0);
			QUNIT_IS_EQUAL (res.size (), 0);

			res = collectRange (*tree, 56, 100, sorted, 1, 0);
			QUNIT_IS_EQUAL (res.size (), 1);
		}
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
