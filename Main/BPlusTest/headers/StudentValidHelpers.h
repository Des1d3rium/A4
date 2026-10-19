#ifndef STUDENT_VALID_HELPERS_H
#define STUDENT_VALID_HELPERS_H

// ---------------------------------------------------------------------
// isValid () helpers (Stage 5), for the Stage 2, 3 and 4 suites.
//
// checkValid () asks a tree whether it is valid and expects true.
// expectInvalid () expects false, with the tree's messages hidden, since
// a damaged tree may print a lot.
//
// The corrupt* helpers damage an int-keyed tree on purpose, through the
// public page interface only (tree[i], clear (), setType (), append ()),
// as plantTraps () does.  They find the page to damage by walking down
// from the root location stored in the table, so they assume nothing
// about where a split puts pages.  Each returns false if the tree is too
// small for that corruption.  Once a tree is damaged, call only isValid ()
// on it (and getNumPages () / getTable ()): a query over a cycle, say, may
// never end.
// ---------------------------------------------------------------------

#include "StudentTestHelpers.h"

// expects tree.isValid () to be true
inline void checkValid (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree) {
	QUNIT_IS_TRUE (tree.isValid ());
}

// expects tree.isValid () to be false, hiding what it prints
inline void expectInvalid (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &tree, string what) {
	cout << "  corrupted: " << what << endl << flush;
	ostringstream sink;
	streambuf *saved = cout.rdbuf (sink.rdbuf ());
	bool valid = tree.isValid ();
	cout.rdbuf (saved);
	QUNIT_IS_FALSE (valid);
}

// the root location stored in the table
inline int rootOf (MyDB_BPlusTreeReaderWriter &tree) {
	return tree.getTable ()->getRootLocation ();
}

// the (key, pointer) entries of directory page whichPage, in order
inline vector <pair <int, int>> dirEntries (MyDB_BPlusTreeReaderWriter &tree, int whichPage) {
	vector <pair <int, int>> entries;
	MyDB_INRecordPtr entry = make_shared <MyDB_INRecord> (makeAtt <int> (0));
	MyDB_RecordIteratorAltPtr myIter = tree[whichPage].getIteratorAlt ();
	while (myIter->advance ()) {
		myIter->getCurrent (entry);
		entries.push_back (make_pair (entry->getKey ()->toInt (), entry->getPtr ()));
	}
	return entries;
}

// rewrites page whichPage as a directory page holding exactly entries
inline void writeDir (MyDB_BPlusTreeReaderWriter &tree, int whichPage, vector <pair <int, int>> &entries) {
	MyDB_PageReaderWriter page = tree[whichPage];
	page.clear ();
	page.setType (MyDB_PageType :: DirectoryPage);
	for (auto &e : entries) {
		MyDB_INRecordPtr entry = make_shared <MyDB_INRecord> (makeAtt <int> (e.first));
		entry->setPtr (e.second);
		page.append (entry);
	}
}

inline bool isLeafPage (MyDB_BPlusTreeReaderWriter &tree, int whichPage) {
	return tree[whichPage].getType () == MyDB_PageType :: RegularPage;
}

// the keys of the records on leaf whichPage
inline vector <int> leafKeys (MyDB_BPlusTreeReaderWriter &tree, int whichPage) {
	vector <int> keys;
	MyDB_RecordPtr rec = tree.getEmptyRecord ();
	MyDB_RecordIteratorAltPtr myIter = tree[whichPage].getIteratorAlt ();
	while (myIter->advance ()) {
		myIter->getCurrent (rec);
		keys.push_back (rec->getAtt (0)->toInt ());
	}
	return keys;
}

// the leftmost leaf under page whichPage
inline int leftmostLeaf (MyDB_BPlusTreeReaderWriter &tree, int whichPage) {
	while (!isLeafPage (tree, whichPage))
		whichPage = dirEntries (tree, whichPage)[0].second;
	return whichPage;
}

// the index of the first root entry that points to a directory page, or -1
inline int dirChildOfRoot (MyDB_BPlusTreeReaderWriter &tree) {
	vector <pair <int, int>> root = dirEntries (tree, rootOf (tree));
	for (size_t i = 0; i < root.size (); i++)
		if (!isLeafPage (tree, root[i].second))
			return (int) i;
	return -1;
}

// a root entry points to a page past the end of the file
inline bool corruptBadPointer (MyDB_BPlusTreeReaderWriter &tree) {
	vector <pair <int, int>> root = dirEntries (tree, rootOf (tree));
	if (root.size () < 2)
		return false;
	root[0].second = tree.getNumPages () + 5;
	writeDir (tree, rootOf (tree), root);
	return true;
}

// the first two root entries swap places, so the keys are out of order
inline bool corruptUnsortedRoot (MyDB_BPlusTreeReaderWriter &tree) {
	vector <pair <int, int>> root = dirEntries (tree, rootOf (tree));
	if (root.size () < 3 || root[0].first == root[1].first)
		return false;
	swap (root[0], root[1]);
	writeDir (tree, rootOf (tree), root);
	return true;
}

// the second root entry points to the first one's child as well
inline bool corruptTwoParents (MyDB_BPlusTreeReaderWriter &tree) {
	vector <pair <int, int>> root = dirEntries (tree, rootOf (tree));
	if (root.size () < 2)
		return false;
	root[1].second = root[0].second;
	writeDir (tree, rootOf (tree), root);
	return true;
}

// a record with a key above its leaf's upper bound is written straight onto
// that leaf (a leaf under a root entry other than the last).  Its key is
// returned through key
inline bool corruptWrongLeaf (MyDB_BPlusTreeReaderWriter &tree, int &key) {
	vector <pair <int, int>> root = dirEntries (tree, rootOf (tree));
	key = 1000000000;
	MyDB_RecordPtr rec = tree.getEmptyRecord ();
	fill (rec, key, "MISPLACED");
	for (size_t i = 0; i + 1 < root.size (); i++)
		if (isLeafPage (tree, root[i].second) && tree[root[i].second].append (rec))
			return true;
	return false;
}

// one leaf (not the root's only child) is emptied
inline bool corruptEmptyLeaf (MyDB_BPlusTreeReaderWriter &tree) {
	vector <pair <int, int>> root = dirEntries (tree, rootOf (tree));
	if (root.size () < 2)
		return false;
	int leaf = leftmostLeaf (tree, root[0].second);
	tree[leaf].clear ();
	return true;
}

// the root's last key is lowered from the maximum to the largest key in the
// tree, so that every record is still inside its entry's bounds
inline bool corruptRootNotMax (MyDB_BPlusTreeReaderWriter &tree, int largestKey) {
	vector <pair <int, int>> root = dirEntries (tree, rootOf (tree));
	if (root.empty () || root.back ().first != INT_MAX)
		return false;
	root.back ().first = largestKey;
	writeDir (tree, rootOf (tree), root);
	return true;
}

// a directory page below the root gets an entry pointing back to the root
inline bool corruptCycle (MyDB_BPlusTreeReaderWriter &tree) {
	int i = dirChildOfRoot (tree);
	if (i < 0)
		return false;
	int dir = dirEntries (tree, rootOf (tree))[i].second;
	vector <pair <int, int>> entries = dirEntries (tree, dir);
	entries[0].second = rootOf (tree);
	writeDir (tree, dir, entries);
	return true;
}

// a root entry that points to a directory page is pointed instead at the
// leftmost leaf under it, so that leaf is nearer the root than the others.
// The leaf's keys stay inside the entry's bounds
inline bool corruptUnevenDepth (MyDB_BPlusTreeReaderWriter &tree) {
	int i = dirChildOfRoot (tree);
	if (i < 0)
		return false;
	vector <pair <int, int>> root = dirEntries (tree, rootOf (tree));
	root[i].second = leftmostLeaf (tree, root[i].second);
	writeDir (tree, rootOf (tree), root);
	return true;
}

// the table's root location is set to a leaf
inline bool corruptRootIsLeaf (MyDB_BPlusTreeReaderWriter &tree) {
	tree.getTable ()->setRootLocation (leftmostLeaf (tree, rootOf (tree)));
	return true;
}

// a directory page below the root is emptied (it stays a directory page)
inline bool corruptEmptyDirectory (MyDB_BPlusTreeReaderWriter &tree) {
	int i = dirChildOfRoot (tree);
	if (i < 0)
		return false;
	int dir = dirEntries (tree, rootOf (tree))[i].second;
	MyDB_PageReaderWriter page = tree[dir];
	page.clear ();
	page.setType (MyDB_PageType :: DirectoryPage);
	return true;
}

// in a tree of exactly three levels with distinct keys: under the first root
// entry (key k), the directory page's last key is lowered below k.  The
// records with key k are first moved off the leaf below it, so that every
// record stays inside its entry's bounds; only the rule "a directory page's
// last key equals its parent entry's key" is broken.  The records moved are
// simply dropped, and their number returned through dropped
inline bool corruptChildLastKey (MyDB_BPlusTreeReaderWriter &tree, int &dropped) {
	vector <pair <int, int>> root = dirEntries (tree, rootOf (tree));
	if (root.size () < 2 || isLeafPage (tree, root[0].second))
		return false;
	int k = root[0].first, dir = root[0].second;
	vector <pair <int, int>> entries = dirEntries (tree, dir);
	if (entries.back ().first != k || !isLeafPage (tree, entries.back ().second))
		return false;

	// rewrite the last leaf without its records of key k
	int leaf = entries.back ().second;
	vector <MyDB_RecordPtr> keep;
	MyDB_RecordIteratorAltPtr myIter = tree[leaf].getIteratorAlt ();
	int newLast = INT_MIN;
	dropped = 0;
	while (myIter->advance ()) {
		MyDB_RecordPtr rec = tree.getEmptyRecord ();
		myIter->getCurrent (rec);
		int key = rec->getAtt (0)->toInt ();
		if (key == k)
			dropped++;
		else {
			keep.push_back (rec);
			newLast = max (newLast, key);
		}
	}
	if (keep.empty ())
		return false;
	int prev = entries.size () > 1 ? entries[entries.size () - 2].first : INT_MIN;
	newLast = max (newLast, prev);
	if (newLast >= k)
		return false;
	tree[leaf].clear ();
	for (MyDB_RecordPtr rec : keep)
		tree[leaf].append (rec);

	entries.back ().first = newLast;
	writeDir (tree, dir, entries);
	return true;
}

#endif
