// ---------------------------------------------------------------------
// Stage 4 page-count test.  NOT a graded file: it is built only against the
// instrumented copy of the code base made by patch.py (see run.sh), in which
// the buffer manager counts and times every page it reads from disk (a
// "load"), and the tree announces every page it touches (an "access"), by
// category (PageCounters.h).
//
// Built twice by run.sh:
//   count_new   the Stage 4 tree
//   count_old   the same tree with the Stage 3 search (-DOLD_DISCOVER),
//               which reads every child it keeps to learn its page type
//
// Every expectation is worked out independently, before the measured run,
// by expectPages (), which walks the directory pages with the rule from the
// design decisions (a child is needed iff its key >= low and the previous
// key <= high).  Measured runs start from a cold buffer: the tree is closed
// and re-opened from the catalog with a fresh buffer manager first.
//
//   Test 1  deep int tree, buffer bigger than the tree: per query, exact
//           accesses and exact loads in every category
//   Test 2  supplier.tbl, buffer (8 frames) far smaller than the range:
//           exact accesses; loads bounded (and, for the old search, the
//           double read of every leaf shown)
//   Test 3  300 point queries on the supplier tree: exact accesses per
//           query, and average accesses, loads and cost per load
//   Test 4  3000 appends: accesses and loads per append
// ---------------------------------------------------------------------

#include "StudentTestHelpers.h"
#include "PageCounters.h"
#include <cstdio>
#include <iomanip>

#ifdef OLD_DISCOVER
static const bool oldSearch = true;
static const char *mode = "OLD (Stage 3 search: every kept child read for its type)";
#else
static const bool oldSearch = false;
static const char *mode = "NEW (Stage 4 search: only the first kept child read for its type)";
#endif

static string pad (int i) {
	char buf[32];
	snprintf (buf, sizeof (buf), "p%07d", i);
	return string (buf);
}

// keys 0 .. n - 1 in a scrambled order, plus a second copy of each multiple of 10
static void scrambled (MyDB_BPlusTreeReaderWriter &tree, Oracle &oracle, int n) {
	for (int i = 0; i < n; i++) {
		int key = (int) (((long) i * 7919) % n);
		appendBoth (tree, oracle, key, pad (i));
		if (key % 10 == 0)
			appendBoth (tree, oracle, key, pad (i + n));
	}
}

// what a query on [low, high] must touch, worked out from the directory pages
struct Expected {
	int dirs = 0;		// directory pages searched (the root included)
	int leaves = 0;		// leaves the iterator must read
	int peeks = 0;		// searched pages that keep at least one child
	int leafPeeks = 0;	// ... of which the children are leaves
	int kept = 0;		// children kept, over all searched pages
};

static void expectPages (MyDB_BPlusTreeReaderWriter &tree, int whichPage, int low, int high, Expected &e) {
	e.dirs++;
	vector <int> children;
	bool hasPrev = false;
	int prev = 0;
	MyDB_INRecordPtr entry = make_shared <MyDB_INRecord> (makeAtt <int> (0));
	MyDB_RecordIteratorAltPtr myIter = tree[whichPage].getIteratorAlt ();
	while (myIter->advance ()) {
		myIter->getCurrent (entry);
		int key = entry->getKey ()->toInt ();
		if (key >= low && !(hasPrev && prev > high))
			children.push_back (entry->getPtr ());
		prev = key;
		hasPrev = true;
	}
	if (children.empty ())
		return;
	e.peeks++;
	e.kept += (int) children.size ();
	if (tree[children[0]].getType () == MyDB_PageType :: RegularPage) {
		e.leafPeeks++;
		e.leaves += (int) children.size ();
		return;
	}
	for (int child : children)
		expectPages (tree, child, low, high, e);
}

static Expected expectPages (MyDB_BPlusTreeReaderWriter &tree, int low, int high) {
	Expected e;
	expectPages (tree, tree.getTable ()->getRootLocation (), low, high, e);
	return e;
}

// runs one query to the end and returns the counters for it alone
static PageCounters measure (MyDB_BPlusTreeReaderWriter &tree, int low, int high, bool sorted, int &records) {
	counters ().reset ();
	MyDB_RecordIteratorAltPtr myIter = sorted ? tree.getSortedRangeIteratorAlt (makeAtt <int> (low), makeAtt <int> (high))
		: tree.getRangeIteratorAlt (makeAtt <int> (low), makeAtt <int> (high));
	MyDB_RecordPtr temp = tree.getEmptyRecord ();
	records = 0;
	while (myIter->advance ()) {
		myIter->getCurrent (temp);
		records++;
	}
	PageCounters c = counters ();
	counters ().current = CAT_OTHER;
	return c;
}

static void add (PageCounters &sum, PageCounters &c) {
	for (int i = 0; i < CAT_NUM; i++) {
		sum.accesses[i] += c.accesses[i];
		sum.loads[i] += c.loads[i];
		sum.loadNanos[i] += c.loadNanos[i];
	}
	sum.tempLoads += c.tempLoads;
	sum.writes += c.writes;
}

static string range (int low, int high) {
	auto s = [] (int v) {return v == INT_MIN ? string ("-inf") : v == INT_MAX ? string ("+inf") : to_string (v);};
	return "[" + s (low) + ", " + s (high) + "]";
}

// prints the counters by category; per divides them (an average over that many operations)
static void report (string title, PageCounters &c, int per = 1) {
	cout << "  " << title << (per > 1 ? "  (average of " + to_string (per) + ")" : string ()) << "\n";
	cout << "    " << left << setw (26) << "category" << right << setw (10) << "accesses" << setw (10) << "loads"
		<< setw (14) << "us per load" << "\n";
	for (int i = 0; i < CAT_NUM; i++) {
		if (c.accesses[i] == 0 && c.loads[i] == 0)
			continue;
		cout << "    " << left << setw (26) << catName[i] << right << fixed << setprecision (per > 1 ? 2 : 0)
			<< setw (10) << (double) c.accesses[i] / per << setw (10) << (double) c.loads[i] / per
			<< setprecision (2) << setw (14) << (c.loads[i] ? c.loadNanos[i] / c.loads[i] / 1000 : 0.0) << "\n";
	}
	cout << "    " << left << setw (26) << "TOTAL" << right << setprecision (per > 1 ? 2 : 0)
		<< setw (10) << (double) c.totalAccesses () / per << setw (10) << (double) c.totalLoads () / per
		<< setprecision (2) << setw (14) << (c.totalLoads () ? c.totalNanos () / c.totalLoads () / 1000 : 0.0) << "\n";
	cout << "    temp-page loads " << setprecision (per > 1 ? 2 : 0) << (double) c.tempLoads / per
		<< ", page writes " << (double) c.writes / per << "\n" << flush;
}

// the accesses every query must make, exactly
static void checkAccesses (QUnit::UnitTest &qunit, PageCounters &c, Expected &e) {
	QUNIT_IS_EQUAL (c.accesses[CAT_RESET], 1);
	QUNIT_IS_EQUAL (c.accesses[CAT_DIR_SEARCH], e.dirs);
	QUNIT_IS_EQUAL (c.accesses[CAT_CHILD_PEEK], oldSearch ? e.kept : e.peeks);
	QUNIT_IS_EQUAL (c.accesses[CAT_LEAF_ITER], e.leaves);
	QUNIT_IS_EQUAL (c.accesses[CAT_APPEND], 0);
	QUNIT_IS_EQUAL (c.accesses[CAT_OTHER], 0);
}

int main (int argc, char *argv[]) {
	int start;
	bool only;
	parseArgs (argc, argv, start, only);
	QUnit::UnitTest qunit (cerr, QUnit::normal);
	cout << "MODE: " << mode << "\n" << flush;

	switch (start) {

	case 1:
	{
		cout << "\nTEST 1: cold queries on a deep int tree; the buffer holds the whole tree" << endl;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeTree (myMgr, 128, 512);
		Oracle oracle;
		scrambled (*tree, oracle, 600);
		int levels = walkTree (qunit, *tree);
		cout << "  " << levels << " levels, " << tree->getNumPages () << " pages" << endl;

		vector <pair <int, int>> ranges = {{300, 300}, {120, 180}, {INT_MIN, 40}, {560, INT_MAX},
			{-50, 5}, {INT_MIN, INT_MAX}, {10000, 20000}};
		for (auto &r : ranges) {
			Expected e = expectPages (*tree, r.first, r.second);
			closeAndReopen (tree, myMgr, "id", 128, 512);

			int records;
			PageCounters c = measure (*tree, r.first, r.second, false, records);
			QUNIT_IS_EQUAL (records, (int) oracleRange (oracle, r.first, r.second).size ());
			checkAccesses (qunit, c, e);

			// cold, and nothing is evicted: every needed page is loaded exactly
			// once, no other page is loaded, and each load is charged to the
			// first category that touches the page
			QUNIT_IS_EQUAL (c.totalLoads (), e.dirs + e.leaves);
			QUNIT_IS_EQUAL (c.loads[CAT_RESET], 1);
			QUNIT_IS_EQUAL (c.tempLoads, 0);
			if (oldSearch) {
				QUNIT_IS_EQUAL (c.loads[CAT_CHILD_PEEK], e.kept);
				QUNIT_IS_EQUAL (c.loads[CAT_LEAF_ITER], 0);
			} else {
				QUNIT_IS_EQUAL (c.loads[CAT_CHILD_PEEK], e.peeks);
				QUNIT_IS_EQUAL (c.loads[CAT_DIR_SEARCH], e.dirs - 1 - (e.peeks - e.leafPeeks));
				QUNIT_IS_EQUAL (c.loads[CAT_LEAF_ITER], e.leaves - e.leafPeeks);
			}
			report ("unsorted " + range (r.first, r.second) + ", cold: needs " + to_string (e.dirs) +
				" directory pages and " + to_string (e.leaves) + " leaves", c);

			// the same query, sorted, now warm: the same accesses, no loads
			PageCounters w = measure (*tree, r.first, r.second, true, records);
			QUNIT_IS_EQUAL (records, (int) oracleRange (oracle, r.first, r.second).size ());
			checkAccesses (qunit, w, e);
			QUNIT_IS_EQUAL (w.totalLoads (), 0);
		}
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 2:
	{
		cout << "\nTEST 2: cold wide queries on supplier.tbl; the buffer (8 frames) is far smaller than the range" << endl;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeSupplierTree (myMgr, 1024, 16);
		tree->loadFromTextFile ("supplier.tbl");
		int levels = walkTree (qunit, *tree);
		cout << "  " << levels << " levels, " << tree->getNumPages () << " pages" << endl;

		const int frames = 8;
		vector <pair <int, int>> ranges = {{1, 10000}, {2000, 6000}, {5000, 5100}};
		for (auto &r : ranges) {
			Expected e = expectPages (*tree, r.first, r.second);
			closeAndReopen (tree, myMgr, "suppkey", 1024, frames);

			int records;
			PageCounters c = measure (*tree, r.first, r.second, false, records);
			QUNIT_IS_EQUAL (records, r.second - r.first + 1);
			checkAccesses (qunit, c, e);

			// every needed page is loaded at least once.  The new search loads
			// a leaf twice only if it peeked at it and it was evicted before
			// the iterator got to it; the old search reads every leaf during
			// the search, so once the range outgrows the buffer the iterator
			// has to load almost every leaf again
			QUNIT_IS_TRUE (c.totalLoads () >= e.dirs + e.leaves);
			if (oldSearch) {
				QUNIT_IS_EQUAL (c.loads[CAT_CHILD_PEEK], e.kept);
				if (e.leaves > 2 * frames)
					QUNIT_IS_TRUE (c.loads[CAT_LEAF_ITER] >= e.leaves - frames);
			} else {
				QUNIT_IS_TRUE (c.totalLoads () <= e.dirs + e.leaves + e.leafPeeks);
				QUNIT_IS_TRUE (c.loads[CAT_LEAF_ITER] >= e.leaves - e.leafPeeks);
			}
			report ("unsorted " + range (r.first, r.second) + ", cold: needs " + to_string (e.dirs) +
				" directory pages and " + to_string (e.leaves) + " leaves", c);
			cout << "    leaf loads per needed leaf: " << setprecision (2)
				<< (double) (c.loads[CAT_LEAF_ITER] + (oldSearch ? c.loads[CAT_CHILD_PEEK] - (e.kept - e.leaves)
					: e.leafPeeks)) / e.leaves << "\n" << flush;
		}
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 3:
	{
		cout << "\nTEST 3: 300 point queries on supplier.tbl, 8 frames, alternating unsorted / sorted" << endl;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeSupplierTree (myMgr, 1024, 16);
		tree->loadFromTextFile ("supplier.tbl");

		vector <int> keys;
		vector <Expected> expected;
		for (int i = 0; i < 300; i++) {
			keys.push_back (1 + (int) (((long) i * 7919) % 10000));
			expected.push_back (expectPages (*tree, keys.back (), keys.back ()));
		}
		closeAndReopen (tree, myMgr, "suppkey", 1024, 8);

		PageCounters sum;
		sum.reset ();
		int wrong = 0;
		for (int i = 0; i < 300; i++) {
			int records;
			PageCounters c = measure (*tree, keys[i], keys[i], i % 2 == 1, records);
			wrong += (records != 1);
			checkAccesses (qunit, c, expected[i]);
			QUNIT_IS_TRUE (c.totalLoads () <= c.totalAccesses ());
			add (sum, c);
		}
		QUNIT_IS_EQUAL (wrong, 0);
		report ("point query", sum, 300);
	}
	if (only) break;
	FALLTHROUGH_INTENDED;

	case 4:
	{
		cout << "\nTEST 4: 3000 appends to a new supplier tree, 16 frames" << endl;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr tree = makeSupplierTree (myMgr, 1024, 16);
		Oracle oracle;

		counters ().reset ();
		for (int i = 0; i < 3000; i++)
			appendSupplier (*tree, oracle, (int) (((long) i * 7919) % 3000), "a" + to_string (i));
		PageCounters c = counters ();

		// one root check per append, and at least the root and a leaf below it
		QUNIT_IS_EQUAL (c.accesses[CAT_RESET], 3000);
		QUNIT_IS_TRUE (c.accesses[CAT_APPEND] >= 2 * 3000);
		QUNIT_IS_EQUAL (c.accesses[CAT_DIR_SEARCH] + c.accesses[CAT_CHILD_PEEK] + c.accesses[CAT_LEAF_ITER], 0);
		QUNIT_IS_TRUE (c.totalLoads () <= c.totalAccesses ());
		report ("append", c, 3000);
		cout << "  " << walkTree (qunit, *tree) << " levels, " << tree->getNumPages () << " pages at the end" << endl;
		checkRangeAtts (qunit, *tree, oracle, INT_MIN, INT_MAX, 0, -1);
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
