#ifndef STUDENT_TEST_HELPERS_H
#define STUDENT_TEST_HELPERS_H

// ---------------------------------------------------------------------
// Helpers shared by the four StudentQUnitStageN.cc files: a tiny table
// with an int key and a string payload, over a B+-tree with whatever
// page size you ask for, so that a few dozen records force splits.
// Add to this file as you need to.
// ---------------------------------------------------------------------

#include "MyDB_AttType.h"
#include "MyDB_BufferManager.h"
#include "MyDB_Catalog.h"
#include "MyDB_Page.h"
#include "MyDB_PageReaderWriter.h"
#include "MyDB_Record.h"
#include "MyDB_Table.h"
#include "MyDB_TableReaderWriter.h"
#include "MyDB_BPlusTreeReaderWriter.h"
#include "MyDB_Schema.h"
#include "QUnit.h"
#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>
#include <climits>
#include <sstream>
#include <set>
#include <random>

#define FALLTHROUGH_INTENDED do {} while (0)

// the schema for the tiny table: (id int, payload string)
inline MyDB_SchemaPtr tinySchema () {
	MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema> ();
	mySchema->appendAtt (make_pair ("id", make_shared <MyDB_IntAttType> ()));
	mySchema->appendAtt (make_pair ("payload", make_shared <MyDB_StringAttType> ()));
	return mySchema;
}

// makes a brand-new, empty B+-tree on "id" over a fresh file, with the given
// page size and number of buffer frames.  The buffer manager is returned
// through myMgr; keep it alive for as long as you use the tree
inline MyDB_BPlusTreeReaderWriterPtr makeTree (MyDB_BufferManagerPtr &myMgr, size_t pageSize, size_t numFrames) {
	unlink ("tiny.bin");
	MyDB_TablePtr myTable = make_shared <MyDB_Table> ("tiny", "tiny.bin", tinySchema ());
	myMgr = make_shared <MyDB_BufferManager> (pageSize, numFrames, "tempFile");
	return make_shared <MyDB_BPlusTreeReaderWriter> ("id", myTable, myMgr);
}

// fills rec with (key, payload) and returns it
inline MyDB_RecordPtr fill (MyDB_RecordPtr rec, int key, string payload) {
	rec->fromString (to_string (key) + "|" + payload + "|");
	return rec;
}

// runs a range query [low, high] over the tree, with the sorted or the
// unsorted iterator, and returns the number of records that came back.
// Any record whose key is outside [low, high] counts as a failure, and
// so does any pair of out-of-order records when sorted is true
inline int countRange (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, int lowKey, int highKey, bool sorted) {

	MyDB_IntAttValPtr low = make_shared <MyDB_IntAttVal> ();
	low->set (lowKey);
	MyDB_IntAttValPtr high = make_shared <MyDB_IntAttVal> ();
	high->set (highKey);

	MyDB_RecordIteratorAltPtr myIter;
	if (sorted)
		myIter = tree.getSortedRangeIteratorAlt (low, high);
	else
		myIter = tree.getRangeIteratorAlt (low, high);

	MyDB_RecordPtr temp = tree.getEmptyRecord ();
	int counter = 0, lastKey = lowKey, outOfOrder = 0, outOfRange = 0;
	while (myIter->advance ()) {
		myIter->getCurrent (temp);
		int key = temp->getAtt (0)->toInt ();
		if (key < lowKey || key > highKey)
			outOfRange++;
		if (sorted && key < lastKey)
			outOfOrder++;
		lastKey = key;
		counter++;
	}
	QUNIT_IS_EQUAL (outOfRange, 0);
	QUNIT_IS_EQUAL (outOfOrder, 0);
	return counter;
}

// checks that the range [low, high] returns exactly expected records with BOTH
// iterators (countRange also checks range membership and, for sorted, order)
inline void checkBoth (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, int lowKey, int highKey, int expected) {
	QUNIT_IS_EQUAL (countRange (qunit, tree, lowKey, highKey, false), expected);
	QUNIT_IS_EQUAL (countRange (qunit, tree, lowKey, highKey, true), expected);
}

// runs a range query [low, high] with the sorted or the unsorted iterator, and
// returns every (key, payload) pair that came back, in the order it came back.
// keyAtt and payloadAtt say where the key and the payload live in the schema
inline vector <pair <int, string>> collectRange (MyDB_BPlusTreeReaderWriter &tree, int lowKey, int highKey,
	bool sorted, int keyAtt = 0, int payloadAtt = 1) {

	MyDB_IntAttValPtr low = make_shared <MyDB_IntAttVal> ();
	low->set (lowKey);
	MyDB_IntAttValPtr high = make_shared <MyDB_IntAttVal> ();
	high->set (highKey);

	MyDB_RecordIteratorAltPtr myIter;
	if (sorted)
		myIter = tree.getSortedRangeIteratorAlt (low, high);
	else
		myIter = tree.getRangeIteratorAlt (low, high);

	vector <pair <int, string>> result;
	MyDB_RecordPtr temp = tree.getEmptyRecord ();
	while (myIter->advance ()) {
		myIter->getCurrent (temp);
		result.push_back (make_pair (temp->getAtt (keyAtt)->toInt (), temp->getAtt (payloadAtt)->toString ()));
	}
	return result;
}

// counts the records seen by a full scan of the table with getIteratorAlt ()
inline int countScan (MyDB_BPlusTreeReaderWriter &tree) {
	MyDB_RecordIteratorAltPtr myIter = tree.getIteratorAlt ();
	MyDB_RecordPtr temp = tree.getEmptyRecord ();
	int counter = 0;
	while (myIter->advance ()) {
		myIter->getCurrent (temp);
		counter++;
	}
	return counter;
}

// returns true if the keys in v are in ascending order
inline bool keysAscending (vector <pair <int, string>> &v) {
	for (size_t i = 1; i < v.size (); i++)
		if (v[i].first < v[i - 1].first)
			return false;
	return true;
}

// returns just the keys in v, sorted
inline vector <int> sortedKeys (vector <pair <int, string>> &v) {
	vector <int> keys;
	for (auto &p : v)
		keys.push_back (p.first);
	std::sort (keys.begin (), keys.end ());
	return keys;
}

// the schema for a table whose key is NOT the first column: (payload string, id int)
inline MyDB_SchemaPtr keySecondSchema () {
	MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema> ();
	mySchema->appendAtt (make_pair ("payload", make_shared <MyDB_StringAttType> ()));
	mySchema->appendAtt (make_pair ("id", make_shared <MyDB_IntAttType> ()));
	return mySchema;
}

// like makeTree, but over keySecondSchema (), still keyed on "id"
inline MyDB_BPlusTreeReaderWriterPtr makeKeySecondTree (MyDB_BufferManagerPtr &myMgr, size_t pageSize, size_t numFrames) {
	unlink ("tiny.bin");
	MyDB_TablePtr myTable = make_shared <MyDB_Table> ("tiny", "tiny.bin", keySecondSchema ());
	myMgr = make_shared <MyDB_BufferManager> (pageSize, numFrames, "tempFile");
	return make_shared <MyDB_BPlusTreeReaderWriter> ("id", myTable, myMgr);
}

// parses the command line: ./bin/studentUnitTestStageN [start [only]]
inline void parseArgs (int argc, char *argv[], int &start, bool &only) {
	start = 1;
	only = false;
	if (argc > 1 && argv[1][0] >= '0' && argv[1][0] <= '9')
		start = atoi (argv[1]);
	if (argc > 2 && string (argv[2]) == "only")
		only = true;
	cout << "start from test " << start << (only ? " (only)" : "") << endl << flush;
}

// ---------------------------------------------------------------------
// Oracle helpers (Stage 2 on).  An Oracle is an independent model of the
// tree: the multiset of every (key, payload) pair appended.  Checks compare
// what the tree returns against it, record by record, so a record that is
// lost, duplicated, or comes back with a damaged or different payload is
// caught, and nothing depends on how a correct tree lays out its pages.
// ---------------------------------------------------------------------

typedef multiset <pair <int, string>> Oracle;

// appends (key, payload) to the tree and to the oracle.  keySecond says the
// tree uses keySecondSchema () rather than tinySchema ()
inline void appendBoth (MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle, int key, string payload,
	bool keySecond = false) {
	MyDB_RecordPtr rec = tree.getEmptyRecord ();
	if (keySecond)
		rec->fromString (payload + "|" + to_string (key) + "|");
	else
		fill (rec, key, payload);
	tree.append (rec);
	oracle.insert (make_pair (key, payload));
}

// the pairs in the oracle with low <= key <= high, sorted
inline vector <pair <int, string>> oracleRange (Oracle &oracle, int lowKey, int highKey) {
	vector <pair <int, string>> result;
	for (auto it = oracle.lower_bound (make_pair (lowKey, string ())); it != oracle.end () && it->first <= highKey; ++it)
		result.push_back (*it);
	return result;
}

// checks the range [low, high] against the oracle: first with the unsorted
// iterator (before any sorted query can reorder the leaves), then with the
// sorted one, which must also return the keys in ascending order.  Both must
// return exactly the oracle's (key, payload) pairs.  Returns true if all is well
inline bool checkRange (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle,
	int lowKey, int highKey, bool keySecond = false) {

	int keyAtt = keySecond ? 1 : 0;
	int payloadAtt = keySecond ? 0 : 1;

	vector <pair <int, string>> expected = oracleRange (oracle, lowKey, highKey);

	vector <pair <int, string>> got = collectRange (tree, lowKey, highKey, false, keyAtt, payloadAtt);
	std::sort (got.begin (), got.end ());
	bool ok = (got == expected);

	got = collectRange (tree, lowKey, highKey, true, keyAtt, payloadAtt);
	ok = keysAscending (got) && ok;
	std::sort (got.begin (), got.end ());
	ok = (got == expected) && ok;

	if (!ok)
		cout << "  range [" << lowKey << ", " << highKey << "] does not match the oracle ("
			<< expected.size () << " records expected)" << endl << flush;
	QUNIT_IS_TRUE (ok);
	return ok;
}

// checks the whole tree against the oracle: the full range, a full table
// scan, and the point query [k, k] and the two-key ranges [k - 1, k] and
// [k, k + 1] for every k in [lowKey, highKey].  Every possible split boundary
// in that interval is therefore queried from both sides.  Stops after a few
// failing queries, to keep the output readable
inline void checkAll (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle,
	int lowKey, int highKey, bool keySecond = false) {

	checkRange (qunit, tree, oracle, INT_MIN, INT_MAX, keySecond);
	QUNIT_IS_EQUAL (countScan (tree), (int) oracle.size ());

	int bad = 0;
	for (int k = lowKey; k <= highKey; k++) {
		if (bad > 5)
			break;
		if (!checkRange (qunit, tree, oracle, k, k, keySecond)) bad++;
		if (!checkRange (qunit, tree, oracle, k - 1, k, keySecond)) bad++;
		if (!checkRange (qunit, tree, oracle, k, k + 1, keySecond)) bad++;
	}
}

// the number of records of (key, payload) that fit on one page of pageSize bytes
// (after the page's two-size_t header)
inline int leafCapacity (MyDB_BPlusTreeReaderWriter &tree, size_t pageSize, string payload) {
	MyDB_RecordPtr rec = tree.getEmptyRecord ();
	return (int) ((pageSize - 2 * sizeof (size_t)) / fill (rec, 0, payload)->getBinarySize ());
}

#endif
