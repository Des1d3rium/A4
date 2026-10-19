#ifndef STUDENT_BPLUS_TEST_STAGE3_H
#define STUDENT_BPLUS_TEST_STAGE3_H

// ---------------------------------------------------------------------
// STAGE 3 test suite.  Scope of this stage: internal splits and the root split.  The tree gets deeper.
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
//   * Keep the "start from test N" convention: ./bin/studentUnitTestStage3 3
//     starts at test 3, and ./bin/studentUnitTestStage3 3 only runs test 3
//     and stops.
//
// As in Stage 2, every test checks behaviour only: every record appended
// is also put in an oracle (StudentTestHelpers.h), answers are compared with
// it as whole (key, payload) pairs, no test assumes where a split falls or
// which key a split chooses, and the unsorted iterator always runs before
// the sorted one.  In addition, walkTree () reads the tree's pages through
// the public interface and checks what every correct tree must satisfy
// (one parent per page, leaves all at one depth, keys in order, records
// inside their entries' bounds), and returns the number of levels.
//
// Most tests use 128-byte pages: a leaf then holds at most 5 records of
// (int, 8-character payload), and a directory page at most 8 int entries,
// so a few hundred records already make a tree of four or five levels.
// Tests 12 and 13 use a tree keyed on a string, whose directory entries
// differ in size, so that a split on the count median can overflow a page.
//
// Stage 5: isValid () (StudentValidHelpers.h) is asked wherever walkTree ()
// is, so after every append in Tests 2 and 7.  Tests 14-18 damage a tree
// of three levels through the public page interface, and expect isValid ()
// to say false.
// ---------------------------------------------------------------------

#include "StudentTestHelpers.h"
#include "StudentValidHelpers.h"
#include <cstdio>

// a fixed-width (8 character) payload that is unique for each i
static string pad (int i) {
	char buf[32];
	snprintf (buf, sizeof (buf), "p%07d", i);
	return string (buf);
}

// Tests 4 and 5: keys 0 .. 999, ascending (every split is on the rightmost
// path) or descending (every split is on the leftmost path)
static void deepTree (QUnit::UnitTest &qunit, bool descending) {
	MyDB_BufferManagerPtr myMgr;
	MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
	Oracle oracle;

	for (int i = 0; i < 1000; i++) {
		int key = descending ? 999 - i : i;
		appendBoth (*tree, oracle, key, pad (key));
	}

	// the sorted iterator must give exactly 0 .. 999, each with its payload
	vector <pair <int, string>> res = collectRange (*tree, INT_MIN, INT_MAX, false);
	QUNIT_IS_EQUAL ((int) res.size (), 1000);
	res = collectRange (*tree, INT_MIN, INT_MAX, true);
	bool exact = (res.size () == 1000);
	for (int i = 0; exact && i < 1000; i++)
		exact = (res[i].first == i && res[i].second == pad (i));
	QUNIT_IS_TRUE (exact);

	// at least 200 leaves, at most 8 entries per directory page: at least
	// three directory levels above the leaves
	QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);
	checkValid (qunit, *tree);
	checkAll (qunit, *tree, oracle, -1, 1000);
}

// Tests 14-18: a valid tree of exactly three levels, keys 0, 10, ..., 990
// (each once, scrambled), on 128-byte pages
static MyDB_BPlusTreeReaderWriterPtr threeLevels (QUnit::UnitTest &qunit, MyDB_BufferManagerPtr &myMgr) {
	MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
	Oracle oracle;
	for (int i = 0; i < 100; i++)
		appendBoth (*tree, oracle, 10 * ((i * 37) % 100), pad (i));
	QUNIT_IS_EQUAL (walkTree (qunit, *tree), 3);
	checkValid (qunit, *tree);
	return tree;
}

// a string key of exactly len characters: prefix, a 3-digit number, padding
static string longKey (char prefix, int i, int len) {
	string s = string (1, prefix) + to_string (100 + i);
	return s + string (len - s.size (), 'm');
}

// Tests 12 and 13: on 256-byte pages (240 bytes for entries), a directory
// entry with a 110-character key takes 121 bytes, so no page holds two of
// them, while one with a 4-character key takes 15.  Builds the tree from
// keys, in order, and checks it against the oracle
static void stringTree (QUnit::UnitTest &qunit, vector <string> &keys) {
	MyDB_BufferManagerPtr myMgr;
	MyDB_BPlusTreeReaderWriterPtr tree = makeStringTree (myMgr, 256, 16);
	StrOracle oracle;

	size_t room = 256 - 2 * sizeof (size_t);
	size_t longEntry = make_shared <MyDB_INRecord> (makeAtt <string> (longKey ('c', 0, 110)))->getBinarySize ();
	size_t shortEntry = make_shared <MyDB_INRecord> (makeAtt <string> ("b100"))->getBinarySize ();
	QUNIT_IS_TRUE (2 * longEntry > room);
	QUNIT_IS_TRUE (longEntry + 6 * shortEntry < room);

	for (size_t i = 0; i < keys.size (); i++) {
		appendBothStr (*tree, oracle, keys[i], "x" + to_string (i));
		if (i % 50 == 49 || i + 7 > keys.size ())
			checkRangeStr (qunit, *tree, oracle, "", string (20, '~'));
	}
	QUNIT_IS_TRUE (walkTree <string> (qunit, *tree) >= 4);
	checkValid (qunit, *tree);
	checkAllStr (qunit, *tree, oracle);
	checkRangeStr (qunit, *tree, oracle, "c", "d");
	checkRangeStr (qunit, *tree, oracle, "b150", "c103");
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
		cout << "TEST 2: the first root split..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;

		// a root holds at most 8 entries and a leaf at most 5 records, so 41
		// records cannot fit under a single root: the tree must get a third
		// level by then.  Keep going a little past that
		int levels = walkTree (qunit, *tree), key = 0;
		checkValid (qunit, *tree);
		QUNIT_IS_EQUAL (levels, 2);
		int third = -1;
		for (; key < 80; key++) {
			appendBoth (*tree, oracle, key, pad (key));
			int now = walkTree (qunit, *tree);
			checkValid (qunit, *tree);
			QUNIT_IS_TRUE (now >= levels);
			if (now == 3 && third == -1) {
				third = key;
				cout << "  the root split at append " << key + 1 << endl << flush;
				checkAll (qunit, *tree, oracle, -1, key + 1);
			}
			levels = now;
		}
		QUNIT_IS_TRUE (third != -1 && third < 41);
		QUNIT_IS_TRUE (levels >= 3);
		checkAll (qunit, *tree, oracle, -1, key);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 3:
	{
		cout << "TEST 3: after root splits, a re-opened tree finds the new root..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;

		for (int i = 0; i < 300; i++)
			appendBoth (*tree, oracle, (i * 37) % 300, pad (i));
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);
		checkValid (qunit, *tree);

		// a second tree object over the same table and buffer manager starts
		// from the root location stored in the table, so it sees the new root
		// only if the split stored it there
		MyDB_BPlusTreeReaderWriterPtr again = make_shared <MyDB_BPlusTreeReaderWriter> ("id",
			tree->getTable (), myMgr);
		checkAll (qunit, *again, oracle, -1, 300);

		// and it keeps working (and splitting its root) from there.  The first
		// object is not used again: its idea of the root is now out of date
		for (int i = 0; i < 300; i++)
			appendBoth (*again, oracle, (i * 41) % 300, pad (1000 + i));
		QUNIT_IS_TRUE (walkTree (qunit, *again) >= 4);
		checkValid (qunit, *again);
		checkAll (qunit, *again, oracle, -1, 300);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 4:
	{
		cout << "TEST 4: ascending keys 0 .. 999 build a deep tree..." << endl << flush;
		deepTree (qunit, false);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 5:
	{
		cout << "TEST 5: descending keys 999 .. 0 build a deep tree..." << endl << flush;
		deepTree (qunit, true);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 6:
	{
		cout << "TEST 6: filling a gap splits directory pages in the middle..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;

		// keys 0, 100, ..., 29900, scrambled, so that the tree is several
		// levels deep before the gap is touched
		for (int i = 0; i < 300; i++)
			appendBoth (*tree, oracle, 100 * ((i * 113) % 300), pad (i));
		int levels = walkTree (qunit, *tree);
		checkValid (qunit, *tree);
		QUNIT_IS_TRUE (levels >= 4);
		checkRange (qunit, *tree, oracle, 14000, 16000);

		// pile records into the gap 15001 .. 15099: they all go down one path
		// in the middle of the tree, so the leaves there split, then the
		// directory pages above them (neither the first nor the last)
		for (int j = 0; j < 400; j++) {
			appendBoth (*tree, oracle, 15001 + (j * 31) % 99, pad (10000 + j));
			if (j % 50 == 49) {
				checkRange (qunit, *tree, oracle, 14900, 15200);
				checkRange (qunit, *tree, oracle, 15000, 15100);
				QUNIT_IS_TRUE (walkTree (qunit, *tree) >= levels);
				checkValid (qunit, *tree);
			}
		}

		// keys on both sides of the gap, every key in it, and ranges across it
		checkAll (qunit, *tree, oracle, 14700, 15300);
		int ranges[][2] = {{0, 29900}, {14000, 16000}, {15000, 15000}, {15100, 15100},
			{15050, 15051}, {INT_MIN, 15050}, {15050, INT_MAX}, {100, 29800}};
		for (auto &r : ranges)
			checkRange (qunit, *tree, oracle, r[0], r[1]);
		QUNIT_IS_EQUAL (countScan (*tree), (int) oracle.size ());
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= levels);
		checkValid (qunit, *tree);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 7:
	{
		cout << "TEST 7: one append that splits a leaf, directory pages and the root..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;

		// ascending appends until the tree has five levels.  When an append
		// makes a tree that already has a directory level below the root one
		// level deeper, it split a leaf, then every directory page on its
		// path, then the root: at least three new pages
		int levels = walkTree (qunit, *tree), cascades = 0;
		checkValid (qunit, *tree);
		for (int key = 0; levels < 5 && key < 5000; key++) {
			int pagesBefore = tree->getNumPages ();
			appendBoth (*tree, oracle, key, pad (key));
			int now = walkTree (qunit, *tree);
			checkValid (qunit, *tree);
			QUNIT_IS_TRUE (now >= levels);
			if (now > levels && levels >= 3) {
				cascades++;
				cout << "  append " << key + 1 << " made the tree " << now << " levels deep, "
					<< tree->getNumPages () - pagesBefore << " new pages" << endl << flush;
				QUNIT_IS_TRUE (tree->getNumPages () - pagesBefore >= 3);
				QUNIT_IS_EQUAL ((int) collectRange (*tree, key, key, false).size (), 1);
				QUNIT_IS_EQUAL ((int) collectRange (*tree, key, key, true).size (), 1);
				checkRange (qunit, *tree, oracle, INT_MIN, INT_MAX);
				QUNIT_IS_EQUAL (countScan (*tree), (int) oracle.size ());
			}
			levels = now;
		}
		QUNIT_IS_EQUAL (levels, 5);
		QUNIT_IS_EQUAL (cascades, 2);
		checkAll (qunit, *tree, oracle, -1, (int) oracle.size ());
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 8:
	{
		cout << "TEST 8: seeded random insertions into a deep tree..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;

		// raw mt19937 output is the same on every platform
		mt19937 gen (5303);
		for (int i = 1; i <= 3000; i++) {
			int key = (int) (gen () % 1000);
			appendBoth (*tree, oracle, key, "q" + to_string (gen () % 100000));
			if (i % 250 == 0) {
				checkRange (qunit, *tree, oracle, INT_MIN, INT_MAX);
				QUNIT_IS_EQUAL (countScan (*tree), i);
			}
		}
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);
		checkValid (qunit, *tree);
		checkAll (qunit, *tree, oracle, -1, 1000);

		for (int i = 0; i < 50; i++) {
			int a = (int) (gen () % 1020) - 10;
			int b = (int) (gen () % 1020) - 10;
			checkRange (qunit, *tree, oracle, min (a, b), max (a, b));
		}
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 9:
	{
		cout << "TEST 9: one duplicated key spread over several directory pages..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;

		for (int i = 0; i < 3; i++) {
			appendBoth (*tree, oracle, 4, pad (i));
			appendBoth (*tree, oracle, 6, pad (i));
		}

		// 300 copies of 5 need at least 60 leaves, so they hang under several
		// directory pages, with 5 as a separator on more than one level
		for (int i = 0; i < 300; i++)
			appendBoth (*tree, oracle, 5, pad (100 + i));
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);
		checkValid (qunit, *tree);
		checkAll (qunit, *tree, oracle, 2, 8);
		checkRange (qunit, *tree, oracle, 4, 6);
		QUNIT_IS_EQUAL ((int) collectRange (*tree, 5, 5, false).size (), 300);

		// more 5s and neighbours after the splits
		for (int i = 0; i < 100; i++)
			appendBoth (*tree, oracle, 4 + i % 3, pad (10000 + i));
		QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);
		checkValid (qunit, *tree);
		checkAll (qunit, *tree, oracle, 2, 8);
		QUNIT_IS_EQUAL ((int) collectRange (*tree, 5, 5, true).size (), 333);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 10:
	{
		cout << "TEST 10: queries interleaved with appends that split the root..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 16);
		Oracle oracle;

		// each round adds enough records to deepen the tree or fill it out;
		// the sorted query in checkRange reorders leaves in place each time
		int levels = 2;
		for (int round = 0; round < 8; round++) {
			for (int i = 0; i < 100; i++) {
				int key = (round * 97 + i * 29) % 500;
				appendBoth (*tree, oracle, key, pad (round * 1000 + i));
			}
			checkRange (qunit, *tree, oracle, INT_MIN, INT_MAX);
			checkRange (qunit, *tree, oracle, 100, 199);
			checkRange (qunit, *tree, oracle, 250, 250);
			QUNIT_IS_EQUAL (countScan (*tree), (int) oracle.size ());
			int now = walkTree (qunit, *tree);
			checkValid (qunit, *tree);
			QUNIT_IS_TRUE (now >= levels);
			levels = now;
		}
		QUNIT_IS_TRUE (levels >= 4);
		checkAll (qunit, *tree, oracle, -1, 500);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 11:
	{
		cout << "TEST 11: Stage 2 cases at depth (record sizes, key not first)..." << endl << flush;

		// Part A.  Payloads of 1 to 60 characters on 256-byte pages
		{
			MyDB_BufferManagerPtr myMgr;
			MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 256, 16);
			Oracle oracle;
			mt19937 gen (2027);
			for (int i = 0; i < 1000; i++) {
				int key = (int) (gen () % 400);
				int len = 1 + (int) (gen () % 60);
				appendBoth (*tree, oracle, key, to_string (i) + "_" + string (len, (char) ('a' + i % 26)));
			}
			QUNIT_IS_TRUE (walkTree (qunit, *tree) >= 4);
			checkValid (qunit, *tree);
			checkAll (qunit, *tree, oracle, -1, 400);
		}

		// Part B.  The key is the second attribute
		{
			MyDB_BufferManagerPtr myMgr;
			MyDB_BPlusTreeReaderWriterPtr tree = makeKeySecondTree (myMgr, 128, 16);
			Oracle oracle;
			for (int key = 0; key < 200; key++)
				appendBoth (*tree, oracle, key, pad (key), true);
			for (int key = 199; key >= 0; key--)
				appendBoth (*tree, oracle, key, pad (1000 + key), true);
			for (int i = 0; i < 200; i++)
				appendBoth (*tree, oracle, (i * 7) % 200, pad (2000 + i), true);
			QUNIT_IS_TRUE (walkTree (qunit, *tree, 1) >= 4);
			checkValid (qunit, *tree);
			checkAll (qunit, *tree, oracle, -1, 200, true);
		}
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 12:
	{
		cout << "TEST 12: directory splits that must move left of the count median..." << endl << flush;

		// short keys b100 .. b199 and d100 .. d199 build a three-level tree;
		// then six long keys c100... (between the two groups) all go down the
		// same middle path.  Each one splits a leaf and sends a 121-byte entry
		// up into directory pages full of 15-byte ones, so the half that a
		// count median gives the long entries is too big for a page, and the
		// split has to move to a smaller lower half.  (On our tree this
		// happens below the root, with 6 entries (median 3, used 1) and 4
		// (median 2, used 1), and at the root, with 5 (median 3, used 2).)
		// A tree that splits on the count median alone overflows a page
		vector <string> keys;
		for (int i = 0; i < 100; i++)
			keys.push_back ("b" + to_string (100 + i));
		for (int i = 0; i < 100; i++)
			keys.push_back ("d" + to_string (100 + i));
		for (int i = 0; i < 6; i++)
			keys.push_back (longKey ('c', i, 110));
		stringTree (qunit, keys);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 13:
	{
		cout << "TEST 13: directory splits that must move right of the count median..." << endl << flush;

		// short keys b100 .. b399 ascending build a three-level tree; then six
		// long keys c100... above all of them go down the rightmost path, so
		// the 121-byte entries pile up at the high end of the directory pages
		// and the count median's upper half is too big: the split has to move
		// to a larger lower half.  (On our tree: below the root with 8 entries
		// (median 4, used 6), and at the root with 7 (median 4, used 5).)
		vector <string> keys;
		for (int i = 0; i < 300; i++)
			keys.push_back ("b" + to_string (100 + i));
		for (int i = 0; i < 6; i++)
			keys.push_back (longKey ('c', i, 110));
		stringTree (qunit, keys);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 14:
	{
		cout << "TEST 14: invalid: a cycle back to the root..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = threeLevels (qunit, myMgr);
		QUNIT_IS_TRUE (corruptCycle (*tree));
		expectInvalid (qunit, *tree, "a directory page below the root points to the root");
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 15:
	{
		cout << "TEST 15: invalid: leaves at different depths..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = threeLevels (qunit, myMgr);
		QUNIT_IS_TRUE (corruptUnevenDepth (*tree));
		expectInvalid (qunit, *tree, "a root entry points straight at a leaf two levels down");
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 16:
	{
		cout << "TEST 16: invalid: the root location names a leaf..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = threeLevels (qunit, myMgr);
		QUNIT_IS_TRUE (corruptRootIsLeaf (*tree));
		expectInvalid (qunit, *tree, "the table's root location set to a leaf");
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 17:
	{
		cout << "TEST 17: invalid: an empty directory page..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = threeLevels (qunit, myMgr);
		QUNIT_IS_TRUE (corruptEmptyDirectory (*tree));
		expectInvalid (qunit, *tree, "a directory page below the root emptied");
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 18:
	{
		cout << "TEST 18: invalid: a directory page's last key below its parent's entry..." << endl << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = threeLevels (qunit, myMgr);
		int dropped = 0;
		QUNIT_IS_TRUE (corruptChildLastKey (*tree, dropped));
		QUNIT_IS_EQUAL (dropped, 1);
		expectInvalid (qunit, *tree, "the first directory page's last key lowered, all records in bounds");
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
