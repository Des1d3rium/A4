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
#include <fstream>
#include <map>
#include "MyDB_INRecord.h"

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
// keyAtt and payloadAtt say where the key and the payload live in the schema;
// a payloadAtt of -1 makes the payload the whole record, as text
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
		if (payloadAtt < 0) {
			ostringstream whole;
			whole << temp;
			result.push_back (make_pair (temp->getAtt (keyAtt)->toInt (), whole.str ()));
		} else
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
inline bool checkRangeAtts (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle,
	int lowKey, int highKey, int keyAtt, int payloadAtt) {

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

inline bool checkRange (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle,
	int lowKey, int highKey, bool keySecond = false) {
	return checkRangeAtts (qunit, tree, oracle, lowKey, highKey, keySecond ? 1 : 0, keySecond ? 0 : 1);
}

// checks the whole tree against the oracle: the full range, a full table
// scan, and the point query [k, k] and the two-key ranges [k - 1, k] and
// [k, k + 1] for every k in [lowKey, highKey].  Every possible split boundary
// in that interval is therefore queried from both sides.  Stops after a few
// failing queries, to keep the output readable
inline void checkAllAtts (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle,
	int lowKey, int highKey, int keyAtt, int payloadAtt) {

	checkRangeAtts (qunit, tree, oracle, INT_MIN, INT_MAX, keyAtt, payloadAtt);
	QUNIT_IS_EQUAL (countScan (tree), (int) oracle.size ());

	int bad = 0;
	for (int k = lowKey; k <= highKey; k++) {
		if (bad > 5)
			break;
		if (!checkRangeAtts (qunit, tree, oracle, k, k, keyAtt, payloadAtt)) bad++;
		if (!checkRangeAtts (qunit, tree, oracle, k - 1, k, keyAtt, payloadAtt)) bad++;
		if (!checkRangeAtts (qunit, tree, oracle, k, k + 1, keyAtt, payloadAtt)) bad++;
	}
}

inline void checkAll (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle,
	int lowKey, int highKey, bool keySecond = false) {
	checkAllAtts (qunit, tree, oracle, lowKey, highKey, keySecond ? 1 : 0, keySecond ? 0 : 1);
}

// the number of records of (key, payload) that fit on one page of pageSize bytes
// (after the page's two-size_t header)
inline int leafCapacity (MyDB_BPlusTreeReaderWriter &tree, size_t pageSize, string payload) {
	MyDB_RecordPtr rec = tree.getEmptyRecord ();
	return (int) ((pageSize - 2 * sizeof (size_t)) / fill (rec, 0, payload)->getBinarySize ());
}

// ---------------------------------------------------------------------
// Structure walk (Stage 3 on).  walkTree () starts at the root location
// stored in the table and reads every page it can reach, using only the
// public interface (directory pages are read with a MyDB_INRecord, as the
// tree itself stores them).  It checks only what every correct tree in this
// code base must satisfy, never how a split chose to lay pages out:
//   * the root is a directory page, and every pointer names a page that exists
//   * no page is reached twice (each child has exactly one parent entry)
//   * every leaf is at the same depth
//   * the keys on each directory page are in non-decreasing order
//   * every record lies inside the key bounds of the entries above it: an
//     entry with key k, preceded by an entry with key j, may hold keys in
//     [j, k] (both ends inclusive, since copies of a key may straddle a split)
// It does not require every page of the file to be reachable.  It returns
// the number of levels (2 for a root over leaves), or -1 if it found a problem
// ---------------------------------------------------------------------

template <typename K> K attKey (MyDB_AttValPtr att);
template <> inline int attKey <int> (MyDB_AttValPtr att) {return att->toInt ();}
template <> inline string attKey <string> (MyDB_AttValPtr att) {return att->toString ();}

template <typename K> MyDB_AttValPtr makeAtt (K key);
template <> inline MyDB_AttValPtr makeAtt <int> (int key) {
	MyDB_IntAttValPtr att = make_shared <MyDB_IntAttVal> ();
	att->set (key);
	return att;
}
template <> inline MyDB_AttValPtr makeAtt <string> (string key) {
	MyDB_StringAttValPtr att = make_shared <MyDB_StringAttVal> ();
	att->set (key);
	return att;
}

template <typename K>
struct TreeWalk {
	MyDB_BPlusTreeReaderWriter *tree;
	int keyAtt;
	set <int> seen;
	int leafDepth = -1;
	int problems = 0;

	void problem (string what) {
		if (problems++ < 5)
			cout << "  walkTree: " << what << endl << flush;
	}

	// low / high bound the keys allowed under this page (when hasLow / hasHigh)
	void walk (int whichPage, int depth, bool hasLow, K low, bool hasHigh, K high) {

		if (whichPage < 0 || whichPage >= tree->getNumPages ()) {
			problem ("pointer to page " + to_string (whichPage) + ", which does not exist");
			return;
		}
		if (seen.count (whichPage)) {
			problem ("page " + to_string (whichPage) + " is reached twice");
			return;
		}
		seen.insert (whichPage);

		MyDB_PageReaderWriter page = (*tree)[whichPage];
		if (page.getType () == MyDB_PageType :: RegularPage) {
			if (leafDepth == -1)
				leafDepth = depth;
			else if (leafDepth != depth)
				problem ("leaf page " + to_string (whichPage) + " is at depth " + to_string (depth) +
					", another at " + to_string (leafDepth));
			MyDB_RecordPtr rec = tree->getEmptyRecord ();
			MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
			while (myIter->advance ()) {
				myIter->getCurrent (rec);
				K key = attKey <K> (rec->getAtt (keyAtt));
				if ((hasLow && key < low) || (hasHigh && high < key))
					problem ("a record on leaf page " + to_string (whichPage) + " is outside its key bounds");
			}
			return;
		}

		// a directory page: copy out its entries before going down
		vector <pair <K, int>> entries;
		MyDB_INRecordPtr entry = make_shared <MyDB_INRecord> (makeAtt <K> (K ()));
		MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
		while (myIter->advance ()) {
			myIter->getCurrent (entry);
			entries.push_back (make_pair (attKey <K> (entry->getKey ()), entry->getPtr ()));
		}
		if (entries.empty ())
			problem ("directory page " + to_string (whichPage) + " is empty");
		for (size_t i = 0; i < entries.size (); i++) {
			if (i > 0 && entries[i].first < entries[i - 1].first)
				problem ("directory page " + to_string (whichPage) + " has keys out of order");
			bool childHasLow = hasLow || i > 0;
			K childLow = (i > 0 && (!hasLow || low < entries[i - 1].first)) ? entries[i - 1].first : low;
			K childHigh = (hasHigh && high < entries[i].first) ? high : entries[i].first;
			walk (entries[i].second, depth + 1, childHasLow, childLow, true, childHigh);
		}
	}
};

// walks the tree (keyed on attribute keyAtt of its records, of type K) and
// checks it as described above.  Returns the number of levels, or -1
template <typename K = int>
inline int walkTree (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, int keyAtt = 0) {
	TreeWalk <K> w;
	w.tree = &tree;
	w.keyAtt = keyAtt;
	int root = tree.getTable ()->getRootLocation ();
	if (root < 0 || root >= tree.getNumPages ())
		w.problem ("the root location " + to_string (root) + " is not a page of the file");
	else if (tree[root].getType () != MyDB_PageType :: DirectoryPage)
		w.problem ("the root (page " + to_string (root) + ") is not a directory page");
	else
		w.walk (root, 0, false, K (), false, K ());
	QUNIT_IS_EQUAL (w.problems, 0);
	return w.problems == 0 ? w.leafDepth + 1 : -1;
}

// ---------------------------------------------------------------------
// A tree keyed on a STRING attribute (Stage 3 on), so that directory
// entries differ in size: (skey string, payload string), keyed on "skey".
// Keys must sort below the string type's maximum ("~~~~~~~~~"), so use
// letters and digits, and must not contain '|'
// ---------------------------------------------------------------------

typedef multiset <pair <string, string>> StrOracle;

inline MyDB_BPlusTreeReaderWriterPtr makeStringTree (MyDB_BufferManagerPtr &myMgr, size_t pageSize, size_t numFrames) {
	unlink ("tiny.bin");
	MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema> ();
	mySchema->appendAtt (make_pair ("skey", make_shared <MyDB_StringAttType> ()));
	mySchema->appendAtt (make_pair ("payload", make_shared <MyDB_StringAttType> ()));
	MyDB_TablePtr myTable = make_shared <MyDB_Table> ("tiny", "tiny.bin", mySchema);
	myMgr = make_shared <MyDB_BufferManager> (pageSize, numFrames, "tempFile");
	return make_shared <MyDB_BPlusTreeReaderWriter> ("skey", myTable, myMgr);
}

inline void appendBothStr (MyDB_BPlusTreeReaderWriter &tree, StrOracle &oracle, string key, string payload) {
	MyDB_RecordPtr rec = tree.getEmptyRecord ();
	rec->fromString (key + "|" + payload + "|");
	tree.append (rec);
	oracle.insert (make_pair (key, payload));
}

inline vector <pair <string, string>> collectRangeStr (MyDB_BPlusTreeReaderWriter &tree, string lowKey,
	string highKey, bool sorted) {
	MyDB_RecordIteratorAltPtr myIter;
	if (sorted)
		myIter = tree.getSortedRangeIteratorAlt (makeAtt <string> (lowKey), makeAtt <string> (highKey));
	else
		myIter = tree.getRangeIteratorAlt (makeAtt <string> (lowKey), makeAtt <string> (highKey));
	vector <pair <string, string>> result;
	MyDB_RecordPtr temp = tree.getEmptyRecord ();
	while (myIter->advance ()) {
		myIter->getCurrent (temp);
		result.push_back (make_pair (temp->getAtt (0)->toString (), temp->getAtt (1)->toString ()));
	}
	return result;
}

// like checkRange, for a string-keyed tree: unsorted first, then sorted
inline bool checkRangeStr (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, StrOracle &oracle,
	string lowKey, string highKey) {

	vector <pair <string, string>> expected;
	for (auto it = oracle.lower_bound (make_pair (lowKey, string ())); it != oracle.end () && it->first <= highKey; ++it)
		expected.push_back (*it);

	vector <pair <string, string>> got = collectRangeStr (tree, lowKey, highKey, false);
	std::sort (got.begin (), got.end ());
	bool ok = (got == expected);

	got = collectRangeStr (tree, lowKey, highKey, true);
	for (size_t i = 1; i < got.size (); i++)
		ok = ok && !(got[i].first < got[i - 1].first);
	std::sort (got.begin (), got.end ());
	ok = (got == expected) && ok;

	if (!ok)
		cout << "  range [" << lowKey << ", " << highKey << "] does not match the oracle ("
			<< expected.size () << " records expected)" << endl << flush;
	QUNIT_IS_TRUE (ok);
	return ok;
}

// like checkAll, for a string-keyed tree: the full range, a full scan, and,
// for every distinct key k in the oracle, [k, k] and [k, next distinct key]
inline void checkAllStr (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, StrOracle &oracle) {
	checkRangeStr (qunit, tree, oracle, "", string (20, '~'));
	QUNIT_IS_EQUAL (countScan (tree), (int) oracle.size ());

	vector <string> keys;
	for (auto &p : oracle)
		if (keys.empty () || keys.back () != p.first)
			keys.push_back (p.first);
	int bad = 0;
	for (size_t i = 0; i < keys.size () && bad <= 5; i++) {
		if (!checkRangeStr (qunit, tree, oracle, keys[i], keys[i])) bad++;
		if (i + 1 < keys.size () && !checkRangeStr (qunit, tree, oracle, keys[i], keys[i + 1])) bad++;
	}
}

// ---------------------------------------------------------------------
// Re-opening (Stage 4 on).  closeAndReopen () does what a later run of the
// program would do: it writes the table's metadata (root location, last
// page) into the catalog file catFile, drops the tree and its buffer
// manager (whose destructor writes every dirty page back to the file), and
// then builds a new catalog object, reads the table back with
// getAllTables (), and opens a new tree over it with a fresh buffer manager.
// No test may hold an iterator or a page of the old tree across the call
// ---------------------------------------------------------------------

inline void closeAndReopen (MyDB_BPlusTreeReaderWriterPtr &tree, MyDB_BufferManagerPtr &myMgr,
	string attName, size_t pageSize, size_t numFrames) {
	string name = tree->getTable ()->getName ();
	{
		MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog> ("catFile");
		tree->getTable ()->putInCatalog (myCatalog);
	}
	tree = nullptr;
	myMgr = nullptr;

	MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog> ("catFile");
	map <string, MyDB_TablePtr> allTables = MyDB_Table :: getAllTables (myCatalog);
	myMgr = make_shared <MyDB_BufferManager> (pageSize, numFrames, "tempFile");
	tree = make_shared <MyDB_BPlusTreeReaderWriter> (attName, allTables[name], myMgr);
}

// the schema of supplier.tbl, as in BPlusQUnit.cc; the key, suppkey, is att 0
inline MyDB_SchemaPtr supplierSchema () {
	MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema> ();
	mySchema->appendAtt (make_pair ("suppkey", make_shared <MyDB_IntAttType> ()));
	mySchema->appendAtt (make_pair ("name", make_shared <MyDB_StringAttType> ()));
	mySchema->appendAtt (make_pair ("address", make_shared <MyDB_StringAttType> ()));
	mySchema->appendAtt (make_pair ("nationkey", make_shared <MyDB_IntAttType> ()));
	mySchema->appendAtt (make_pair ("phone", make_shared <MyDB_StringAttType> ()));
	mySchema->appendAtt (make_pair ("acctbal", make_shared <MyDB_DoubleAttType> ()));
	mySchema->appendAtt (make_pair ("comment", make_shared <MyDB_StringAttType> ()));
	return mySchema;
}

// a brand-new, empty B+-tree on suppkey over a fresh tiny.bin
inline MyDB_BPlusTreeReaderWriterPtr makeSupplierTree (MyDB_BufferManagerPtr &myMgr, size_t pageSize, size_t numFrames) {
	unlink ("tiny.bin");
	MyDB_TablePtr myTable = make_shared <MyDB_Table> ("supplier", "tiny.bin", supplierSchema ());
	myMgr = make_shared <MyDB_BufferManager> (pageSize, numFrames, "tempFile");
	return make_shared <MyDB_BPlusTreeReaderWriter> ("suppkey", myTable, myMgr);
}

// reads a .tbl file into an oracle of (suppkey, whole record as text) pairs,
// parsing each line exactly as loadFromTextFile () does.  The largest binary
// size of any record is returned through maxBytes
inline Oracle supplierOracle (MyDB_BPlusTreeReaderWriter &tree, string fName, size_t &maxBytes) {
	Oracle oracle;
	maxBytes = 0;
	MyDB_RecordPtr rec = tree.getEmptyRecord ();
	ifstream myfile (fName);
	string line;
	while (getline (myfile, line)) {
		rec->fromString (line);
		ostringstream whole;
		whole << rec;
		oracle.insert (make_pair (rec->getAtt (0)->toInt (), whole.str ()));
		maxBytes = max (maxBytes, rec->getBinarySize ());
	}
	return oracle;
}

// appends a supplier record with the given key (and fields made from tag)
// to the tree and to a whole-record oracle
inline void appendSupplier (MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle, int key, string tag) {
	MyDB_RecordPtr rec = tree.getEmptyRecord ();
	rec->fromString (to_string (key) + "|Supplier#" + tag + "|addr " + tag + "|7|12-345-678-9012|10.5|note " + tag + "|");
	tree.append (rec);
	ostringstream whole;
	whole << rec;
	oracle.insert (make_pair (key, whole.str ()));
}

// ---------------------------------------------------------------------
// Leaf bounds (Stage 4 on).  leafBounds () walks an int-keyed tree from the
// root stored in the table and returns every leaf with the key bounds its
// parent entries give it: an entry with key k, preceded by one with key j,
// may hold keys in [j, k], intersected with the bounds of the page above.
// A leaf is "outside" [low, high] if its upper bound is below low or its
// lower bound is above high: then no correct tree needs to read it
// ---------------------------------------------------------------------

struct LeafBound {
	int page;
	bool hasLow, hasHigh;
	int low, high;

	bool outside (int lowKey, int highKey) {
		return (hasHigh && high < lowKey) || (hasLow && low > highKey);
	}
};

inline void leafBounds (MyDB_BPlusTreeReaderWriter &tree, int whichPage, bool hasLow, int low,
	bool hasHigh, int high, vector <LeafBound> &result) {

	MyDB_PageReaderWriter page = tree[whichPage];
	if (page.getType () == MyDB_PageType :: RegularPage) {
		result.push_back (LeafBound {whichPage, hasLow, hasHigh, low, high});
		return;
	}
	vector <pair <int, int>> entries;
	MyDB_INRecordPtr entry = make_shared <MyDB_INRecord> (makeAtt <int> (0));
	MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
	while (myIter->advance ()) {
		myIter->getCurrent (entry);
		entries.push_back (make_pair (entry->getKey ()->toInt (), entry->getPtr ()));
	}
	for (size_t i = 0; i < entries.size (); i++) {
		bool childHasLow = hasLow || i > 0;
		int childLow = (i > 0 && (!hasLow || low < entries[i - 1].first)) ? entries[i - 1].first : low;
		int childHigh = (hasHigh && high < entries[i].first) ? high : entries[i].first;
		leafBounds (tree, entries[i].second, childHasLow, childLow, true, childHigh, result);
	}
}

inline vector <LeafBound> leafBounds (MyDB_BPlusTreeReaderWriter &tree) {
	vector <LeafBound> result;
	leafBounds (tree, tree.getTable ()->getRootLocation (), false, 0, false, 0, result);
	return result;
}

// plants a trap record (key, "TRAP") on every leaf that lies outside
// [low, high] and still has room for it, writing through the public page
// interface.  key must be inside [low, high], so a tree that reads such a
// leaf returns the trap from a query on [low, high].  Returns the number of
// traps planted
inline int plantTraps (MyDB_BPlusTreeReaderWriter &tree, int lowKey, int highKey, int key) {
	int planted = 0;
	MyDB_RecordPtr rec = tree.getEmptyRecord ();
	fill (rec, key, "TRAP");
	for (LeafBound &b : leafBounds (tree))
		if (b.outside (lowKey, highKey) && tree[b.page].append (rec))
			planted++;
	return planted;
}

#endif
