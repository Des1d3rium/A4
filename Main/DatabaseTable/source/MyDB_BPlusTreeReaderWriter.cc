
#ifndef BPLUS_C
#define BPLUS_C

// ---------------------------------------------------------------------
// DESIGN DECISIONS (updated at every stage)
//
// Stage 1: a root and a single leaf.
//
//  1. Storage.  The tree lives in the base class's table file and buffer
//     manager (MyDB_TableReaderWriter); pages are reached with (*this)[i],
//     which also grows the file.  orderingAttType / whichAttIsOrdering hold
//     the ordering attribute.
//  2. New tree.  A table whose getRootLocation () is -1 is new.  For a new
//     tree, page 0 is cleared as an empty leaf (RegularPage) and page 1 is
//     made the root (DirectoryPage), holding one MyDB_INRecord whose key is
//     the maximum value of the key type and whose pointer is 0.
//     rootLocation is set to 1, and also stored in the table with
//     setRootLocation (), so that it reaches the catalog.
//  3. Existing tree.  If getRootLocation () is not -1, the constructor
//     uses that root and does not change any page.
//  4. Directory entries.  An IN record (key k, ptr p) means "page p holds
//     keys <= k".  The last entry of the root, and of every directory page
//     on the rightmost path, has the maximum key.  (From Stage 3, a page
//     split off below it ends with an ordinary key: decision 19.)
//  5. Duplicate keys are allowed; append () never drops or merges a record.
//  6. append () starts at rootLocation and descends: on a directory page it
//     follows the first entry whose key is >= the record's key; on a leaf it
//     appends the record at the end of the page (leaves are not kept sorted,
//     except that a split writes both halves in key order).
//  7. Leaf overflow: Stage 1 printed an error and called exit (1).  From
//     Stage 2 on, the leaf is split (decisions 11-16).
//  8. Range iterators.  Both are one helper: collect, left to right, the
//     leaves whose key range can overlap [low, high] (an entry's child is
//     kept if its key >= low and the previous entry's key <= high; both
//     inclusive, so duplicates on a boundary are never missed), and hand them
//     to MyDB_PageListIteratorSelfSortingAlt, which returns only records with
//     low <= key <= high.  Comparisons all use buildComparator ().
//     getSortedRangeIteratorAlt additionally passes the ascending comparator
//     buildComparator (lhs, rhs) with sortOrNot = true, so each leaf is
//     sorted in place on the key before it is read; since leaves are
//     collected in key order, the whole output is sorted.
//     getRangeIteratorAlt passes sortOrNot = false.  (Stage 4: leaves are
//     no longer read during this search; decision 27.)
//  9. printTree () is recursive and depth-first, indented two spaces per
//     level:
//         Dir page <p>:
//           key=<k> -> page <c>
//             Leaf page <c>:
//               <record>
//     IN records are read with getINRecord (), data records with
//     getEmptyRecord ().
//  10. Tests (StudentQUnitStage1.cc) check behaviour only, never the page
//      layout, and never put more records in the tree than fit on one leaf.
//
// Stage 2: leaf splits.  The root is still one page.
//
//  11. Split procedure.  When the leaf that append () reaches is full, its
//      records and the new one are copied into an in-memory vector (each
//      into its own record object) and sorted on the key with
//      buildComparator ().  The lower part goes to a new leaf, page
//      getNumPages () (the end of the file); the upper part is written back
//      into the original leaf, which is cleared first.
//  12. Split point.  Every position k (records [0, k) below, [k, n) above,
//      both nonempty) is tried; among those where both parts fit on a page
//      (page size minus the two-size_t page header), the one with the
//      smallest difference in bytes between the parts is used (the first
//      such k on a tie).  Records vary in size, so splitting by count can
//      overflow a page.  If no k fits, append () throws runtime_error.
//  13. Separator.  The new directory entry is (largest key of the lower
//      part, new leaf), inserted into the parent just before the original
//      leaf's entry; the parent page is rewritten in order.  The original
//      leaf's entry is unchanged.
//  14. Duplicates.  Copies of one key may end up in both leaves (or many).
//      Appends follow the first entry whose key >= the record's key, so new
//      copies of a separator key go to the left leaf; range queries keep a
//      child whose key >= low and whose predecessor's key <= high (decision
//      8, both inclusive), so every leaf that can hold the key is read.
//  15. Full root: in Stage 2, append () threw runtime_error when the root
//      had no room for the new entry (untested).  Replaced in Stage 3 by
//      directory and root splits (decisions 18-22).
//  16. (Stage 2 only: a recursive append (int, rec) on directory pages.
//      Replaced in Stage 3 by decision 22.)
//  17. Tests (StudentQUnitStage2.cc) compare every answer with an oracle (a
//      multiset of (key, payload) pairs), check whole records (payload with
//      its key), never assume where a split falls or which separator is
//      chosen (every key and pair of neighbouring keys is queried), always
//      run the unsorted iterator before the sorted one, and keep the root
//      from filling.
//
// Stage 3: directory splits and root splits.  The tree gets deeper.
//
//  18. Leaves are unchanged: a record is still appended at the end of its
//      leaf when it fits, and a full leaf splits as in decisions 11-14.  A
//      directory page (the root included) splits when the new entry coming
//      up from below does not fit on it.
//  19. Directory split.  The n entries (the new one already inserted just
//      before the entry that was followed) are split at k: entries [0, k)
//      go to a new page, getNumPages (), and entries [k, n) are written back
//      into the original page, so the parent's entry for it stays valid.
//      The key of entry k - 1, the last of the lower half, moves up: the new
//      parent entry is (that key, new page), inserted just before the
//      original page's entry.  Entry k - 1 keeps its pointer, and its key,
//      in the lower half, so every child is pointed to exactly once.
//  20. Where to split.  k starts at the count median, (n + 1) / 2: the
//      entry at (n - 1) / 2 moves up, so with 4 entries the 2nd moves up,
//      with 5 the 3rd.  If either half would not fit on a page (possible
//      only with keys of varying size, such as strings), the nearest k at
//      which both halves are nonempty and fit is used, trying median - 1,
//      median + 1, median - 2, ... (the smaller lower half first on a tie).
//      If no k fits, append () throws runtime_error.
//  21. Root split.  The root splits like any directory page; then a new
//      root page, getNumPages (), gets two entries: (the key moved up, new
//      lower page) and (maximum key, old root page).  rootLocation, and the
//      table's root location (setRootLocation), are updated right away.
//  22. No partial writes.  append () descends once, remembering each
//      directory page on its path, a copy of its entries, and the entry
//      followed.  It then plans every split, bottom up (leaf split point,
//      each directory page's new entry and split point), without writing a
//      page; only once the whole plan fits does it write the pages, bottom
//      up.  So either throw (decisions 12 and 20) leaves the tree unchanged.
//  23. Tests (StudentQUnitStage3.cc) keep the rules of decision 17, and
//      also walk the tree through the public interface (walkTree in
//      StudentTestHelpers.h), checking only what every correct tree must
//      satisfy: one parent entry per page, leaves all at one depth, keys in
//      order on each directory page, and records inside their entries'
//      bounds.  Most use 128-byte pages, so trees reach four or five levels.
//      Tests 12 and 13 use string keys, so that the count median overflows
//      and the split must move left or right.  The throw when no k fits is
//      checked outside the graded file, since a reference tree need not
//      throw.
//
// Stage 4: re-opening from the catalog, loadFromTextFile (), and range
// queries that read no needless page.
//
//  24. Re-opening.  The tree keeps no state of its own beyond rootLocation,
//      which it always also stores in the table (decisions 2 and 21), so
//      the catalog entry written by putInCatalog () (root location, last
//      page) describes the tree completely.  Whoever re-opens must first
//      write the table to the catalog and drop the old tree and its buffer
//      manager, whose destructor writes every dirty page back to the file;
//      a new tree over the table read back from the catalog then takes the
//      root from it (decision 3) and changes no page.
//  25. loadFromTextFile () (in MyDB_TableReaderWriter, which may not be
//      changed) empties the file by making page 0 the last page and
//      clearing it, without telling the tree; rootLocation then names a
//      page past the end of the file.  append (), both range iterators and
//      printTree () first call checkReset (): if the last page is page 0,
//      rootLocation is past it, and page 0 is an empty leaf, the tree is
//      rebuilt empty, exactly as a new tree (decision 2).  So the load
//      leaves exactly the file's records, none of the old ones, and an
//      empty file (which never calls append ()) leaves an empty, working
//      tree from the next call on.  The root is never page 0, so a real
//      tree never triggers it.  (Stage 5 narrowed the condition, which was
//      "last page below rootLocation, or root not a directory page", so
//      that a damaged root is no longer silently replaced, and took the
//      call out of isValid (): decision 29.)
//  26. Range-query bound (unchanged from decision 8): a child is read only
//      if its key >= low and the previous entry's key <= high.
//  27. Leaves are read once.  discoverPages () reads directory pages only:
//      the children of a page are all on one level, so it reads just the
//      first child it keeps to learn whether they are leaves, and if so
//      adds them all to the list unread; the iterator reads each leaf when
//      it gets to it.  (Before, every leaf was read during the search and
//      again by the iterator, twice from disk once the range outgrew the
//      buffer.)
//  28. Tests (StudentQUnitStage4.cc) keep the rules of decisions 17 and 23.
//      Re-open tests go through the catalog file with a fresh buffer manager
//      (closeAndReopen in StudentTestHelpers.h).  loadFromTextFile () tests
//      use supplier.tbl, with pages that hold at least four of its largest
//      records, and compare every field of every record; the empty-file
//      test writes, and removes, an empty empty.tbl.  Efficiency is checked
//      with trap records: records with keys inside the query range are
//      planted on leaves whose bounds lie entirely outside it, and the
//      records returned are counted, so a tree that reads such a leaf
//      returns a trap.  Page reads themselves are measured outside the
//      graded file, which cannot see them: a page-count test builds against
//      an instrumented copy of the tree and buffer manager, counts page
//      accesses and disk loads by category (root check, directory search,
//      child peek, leaf read by the iterator, append), times each load, and
//      checks the counts against an independent walk of the directory.
//
// Stage 5: isValid ().
//
//  29. Read-only.  isValid () writes nothing: it does not call checkReset (),
//      and it reads a page only after checking that its number is a page of
//      the file ((*this)[i] past the end would grow the file).  Just after
//      loadFromTextFile () of an empty file the root is past the end, so
//      isValid () says false until the next append (), query or printTree ()
//      rebuilds the empty tree (decision 25).
//  30. Reporting.  Every violation prints "B+-tree isValid: page <p>:
//      <reason>" and the walk goes on, so one call reports all it finds;
//      the result is false if anything was printed.
//  31. The root.  The table's root location (what the catalog holds) must
//      equal rootLocation, name a page of the file, and that page must be a
//      directory page; its last key must be the maximum of the key type.
//  32. Structure.  The walk goes depth first from the root.  A page reached
//      twice is reported: as a cycle if it is on the path from the root to
//      itself, otherwise as a child with two parent entries.  Every leaf
//      must be at the same depth.  A page whose type is neither leaf nor
//      directory is reported.
//  33. Directory pages.  Nonempty; keys non-decreasing (duplicates allowed);
//      every key inside the page's bounds; and the last key equal to the
//      key of the parent entry pointing to the page (decision 4 and Stage
//      3's rule), the maximum for the root.  Each pointer must name a page
//      of the file before the child is read.
//  34. Leaves.  Records may be in any order.  Every record's key must lie
//      in the bounds given by the entries above: entry i of a page covers
//      [key of entry i - 1, key of entry i], both ends inclusive since
//      copies of a key may straddle a split, intersected with the page's
//      own bounds.  So a record that a range query could not reach is
//      reported.  An empty leaf is allowed only as the root's only child
//      (the empty tree).
//  35. Capacity.  Every page's records (entries) must fit in the page size
//      minus the page header.  In append (), every write of a split checks
//      that the record or entry fit and throws runtime_error if not; since
//      every split is planned to fit before anything is written (decision
//      22), this never fires on a correct tree.
//  36. Tests.  The Stage 2, 3 and 4 suites call isValid () wherever they
//      call walkTree (), at the end of every test, and after every append
//      in a few.  Invalid trees are made by rewriting pages through the
//      public page interface (StudentValidHelpers.h), finding the page to
//      damage from the root, so no test assumes a page layout: a pointer
//      past the end of the file, directory keys out of order, a child with
//      two parents, a record on the wrong leaf, an empty leaf, the root's
//      last key below the maximum (Stage 2); a cycle, leaves at different
//      depths, a root location naming a leaf, an empty directory page, a
//      directory page's last key below its parent entry's (Stage 3); and a
//      damaged re-opened tree (Stage 4).  A Stage 4 test checks that
//      isValid () changes no page count, root or answer, on a valid tree
//      and (metadata only) on a damaged one.  A page that holds more than
//      it can cannot be made through the public interface, so decision 35's
//      check is not tested.  Stage 2's rule that the root never fills is
//      gone.
// ---------------------------------------------------------------------

#include "MyDB_INRecord.h"
#include "MyDB_BPlusTreeReaderWriter.h"
#include "MyDB_PageReaderWriter.h"
#include "MyDB_PageListIteratorSelfSortingAlt.h"
#include "RecordComparator.h"
#include <algorithm>
#include <stdexcept>

MyDB_BPlusTreeReaderWriter :: MyDB_BPlusTreeReaderWriter (string orderOnAttName, MyDB_TablePtr forMe, 
	MyDB_BufferManagerPtr myBuffer) : MyDB_TableReaderWriter (forMe, myBuffer) {

	// find the ordering attribute
	auto res = forMe->getSchema ()->getAttByName (orderOnAttName);

	// remember information about the ordering attribute
	orderingAttType = res.second;
	whichAttIsOrdering = res.first;

	// and the root location
	rootLocation = getTable ()->getRootLocation ();

	// an existing tree: use its root, and leave every page alone
	if (rootLocation != -1)
		return;

	makeEmptyTree ();
}

void MyDB_BPlusTreeReaderWriter :: makeEmptyTree () {

	// page 0 is an empty leaf...
	MyDB_PageReaderWriter leaf = (*this)[0];
	leaf.clear ();

	// ...and page 1 is the root, with one entry (max key) pointing at the leaf
	MyDB_PageReaderWriter root = (*this)[1];
	root.clear ();
	root.setType (MyDB_PageType :: DirectoryPage);
	MyDB_INRecordPtr entry = getINRecord ();
	entry->setPtr (0);
	root.append (entry);

	rootLocation = 1;
	getTable ()->setRootLocation (rootLocation);
}

void MyDB_BPlusTreeReaderWriter :: checkReset () {

	// loadFromTextFile () empties the file without telling the tree: page 0
	// becomes the last page, and an empty leaf.  The root is never page 0, so
	// that state, with the root past page 0, means the file was emptied:
	// start again from an empty tree.  Anything else is left alone (a damaged
	// root is for isValid () to report, not for this to paper over)
	if (getTable ()->lastPage () != 0 || rootLocation <= 0)
		return;
	MyDB_PageReaderWriter first = (*this)[0];
	if (first.getType () == MyDB_PageType :: RegularPage && !first.getIteratorAlt ()->advance ())
		makeEmptyTree ();
}

MyDB_RecordIteratorAltPtr MyDB_BPlusTreeReaderWriter :: getSortedRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high) {
	return getRangeIteratorAlt (low, high, true);
}

MyDB_RecordIteratorAltPtr MyDB_BPlusTreeReaderWriter :: getRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high) {
	return getRangeIteratorAlt (low, high, false);
}

MyDB_RecordIteratorAltPtr MyDB_BPlusTreeReaderWriter :: getRangeIteratorAlt (MyDB_AttValPtr low,
	MyDB_AttValPtr high, bool sortIt) {

	checkReset ();

	// find the leaves that can hold keys in [low, high]
	vector <MyDB_PageReaderWriter> list;
	discoverPages (rootLocation, list, low, high);

	// records used by the comparators: lhs and rhs for sorting a leaf, myRec
	// for the record being checked against the range
	MyDB_RecordPtr lhs = getEmptyRecord ();
	MyDB_RecordPtr rhs = getEmptyRecord ();
	MyDB_RecordPtr myRec = getEmptyRecord ();

	// wrap the bounds in IN records so that buildComparator () can use them
	MyDB_INRecordPtr lowRec = getINRecord ();
	lowRec->setKey (low);
	MyDB_INRecordPtr highRec = getINRecord ();
	highRec->setKey (high);

	// a record is skipped if myRec < low or high < myRec
	function <bool ()> comparator = buildComparator (lhs, rhs);
	function <bool ()> lowComparator = buildComparator (myRec, lowRec);
	function <bool ()> highComparator = buildComparator (highRec, myRec);

	return make_shared <MyDB_PageListIteratorSelfSortingAlt> (list, lhs, rhs, comparator, myRec,
		lowComparator, highComparator, sortIt);
}

void MyDB_BPlusTreeReaderWriter :: discoverPages (int whichPage, vector <MyDB_PageReaderWriter> &list,
	MyDB_AttValPtr low, MyDB_AttValPtr high) {

	// whichPage is a directory page: entry i covers keys in
	// [key of entry i - 1, key of entry i]
	MyDB_PageReaderWriter page = (*this)[whichPage];
	MyDB_INRecordPtr lowRec = getINRecord ();
	lowRec->setKey (low);
	MyDB_INRecordPtr highRec = getINRecord ();
	highRec->setKey (high);
	MyDB_INRecordPtr entry = getINRecord ();
	MyDB_INRecordPtr prevEntry = getINRecord ();
	function <bool ()> entryBelowLow = buildComparator (entry, lowRec);
	function <bool ()> highBelowPrev = buildComparator (highRec, prevEntry);

	// keep a child if its key >= low and the previous key <= high
	vector <int> children;
	bool hasPrev = false;
	MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
	while (myIter->advance ()) {
		myIter->getCurrent (entry);
		if (!entryBelowLow () && !(hasPrev && highBelowPrev ()))
			children.push_back (entry->getPtr ());
		myIter->getCurrent (prevEntry);
		hasPrev = true;
	}
	if (children.empty ())
		return;

	// the children of one page are all on one level, so only the first is
	// read to see whether they are leaves.  The leaves themselves are read
	// by the iterator; their records are filtered there
	if ((*this)[children[0]].getType () == MyDB_PageType :: RegularPage) {
		for (int child : children)
			list.push_back ((*this)[child]);
		return;
	}
	for (int child : children)
		discoverPages (child, list, low, high);
}

void MyDB_BPlusTreeReaderWriter :: append (MyDB_RecordPtr appendMe) {

	checkReset ();

	// go down from the root to a leaf, remembering, for every directory page
	// on the way, its number, a copy of its entries, and the entry followed
	vector <int> path;
	vector <vector <MyDB_INRecordPtr>> pathEntries;
	vector <size_t> pathIndex;
	int whichPage = rootLocation;
	while (true) {
		MyDB_PageReaderWriter page = (*this)[whichPage];
		if (page.getType () == MyDB_PageType :: RegularPage)
			break;

		vector <MyDB_INRecordPtr> entries;
		readDirectory (page, entries);

		// follow the first entry whose key >= the record's key
		size_t i = 0;
		while (i < entries.size () && buildComparator (entries[i], appendMe) ())
			i++;

		// cannot happen: the last entry of the root has the max key, and every
		// other page's keys are bounded by its parent's entry
		if (i == entries.size ()) {
			cout << "B+-tree: directory page " << whichPage << " has no entry for the key "
				<< getKey (appendMe)->toString () << ".\n";
			exit (1);
		}

		path.push_back (whichPage);
		pathEntries.push_back (entries);
		pathIndex.push_back (i);
		whichPage = entries[i]->getPtr ();
	}

	// a leaf with room: done
	MyDB_PageReaderWriter leaf = (*this)[whichPage];
	if (leaf.append (appendMe))
		return;

	// the leaf is full.  First plan every split, bottom up, without writing a
	// page, so that a throw leaves the tree unchanged.  The leaf's records and
	// the new one, sorted on the key, are split at leafSplit
	size_t capacity = pageCapacity (leaf);
	vector <MyDB_RecordPtr> recs;
	readLeaf (leaf, recs);
	recs.push_back (appendMe);
	std::stable_sort (recs.begin (), recs.end (),
		[this] (MyDB_RecordPtr a, MyDB_RecordPtr b) {return buildComparator (a, b) ();});
	int leafSplit = chooseSplit (recs, capacity);
	if (leafSplit < 0)
		throw runtime_error ("B+-tree: leaf page " + to_string (whichPage) +
			" cannot be split so that both halves fit on a page");

	// carry is the entry that goes up into the next level: its key is the
	// largest key of the lower half; its pointer, the lower half's new page,
	// is set when that page is written.  dirSplit[level] is where the page
	// path[level] splits (0: it takes the new entry without splitting), and
	// top is the highest level that changes
	MyDB_INRecordPtr carry = getINRecord ();
	carry->setKey (getKey (recs[leafSplit - 1]));
	vector <MyDB_INRecordPtr> carries (path.size ());
	vector <int> dirSplit (path.size (), 0);
	int top = (int) path.size () - 1;
	for (int level = (int) path.size () - 1; level >= 0; level--) {

		// the new entry goes just before the entry that was followed
		vector <MyDB_INRecordPtr> &entries = pathEntries[level];
		entries.insert (entries.begin () + pathIndex[level], carry);
		carries[level] = carry;
		top = level;

		size_t used = 0;
		for (MyDB_INRecordPtr e : entries)
			used += e->getBinarySize ();
		if (used <= capacity)
			break;

		int k = chooseDirectorySplit (entries, capacity);
		if (k < 0)
			throw runtime_error ("B+-tree: directory page " + to_string (path[level]) +
				" cannot be split so that both halves fit on a page");
		dirSplit[level] = k;

		// the key of the lower half's last entry moves up; that entry keeps
		// its key and its pointer, in the lower half
		carry = getINRecord ();
		carry->setKey (getKey (entries[k - 1]));
	}
	bool rootSplits = (dirSplit[0] != 0);

	// now write, bottom up.  The leaf's lower half goes to a new page, the
	// upper half back into the leaf
	int lowerPage = getNumPages ();
	MyDB_PageReaderWriter lower = (*this)[lowerPage];
	for (int j = 0; j < leafSplit; j++)
		if (!lower.append (recs[j]))
			throw runtime_error ("B+-tree: a record does not fit on new leaf page " + to_string (lowerPage));
	leaf.clear ();
	for (size_t j = leafSplit; j < recs.size (); j++)
		if (!leaf.append (recs[j]))
			throw runtime_error ("B+-tree: a record does not fit on leaf page " + to_string (whichPage));

	// each directory page on the path takes its new entry; one that splits
	// sends its lower half to a new page and keeps the upper half
	for (int level = (int) path.size () - 1; level >= top; level--) {
		carries[level]->setPtr (lowerPage);
		vector <MyDB_INRecordPtr> &entries = pathEntries[level];
		MyDB_PageReaderWriter page = (*this)[path[level]];
		int k = dirSplit[level];
		if (k == 0) {
			writeDirectory (page, entries, 0, entries.size ());
			break;
		}
		lowerPage = getNumPages ();
		MyDB_PageReaderWriter newPage = (*this)[lowerPage];
		writeDirectory (newPage, entries, 0, k);
		writeDirectory (page, entries, k, entries.size ());
	}

	// the root split: a new root over the two halves
	if (rootSplits) {
		carry->setPtr (lowerPage);
		MyDB_INRecordPtr upper = getINRecord ();
		upper->setPtr (rootLocation);
		vector <MyDB_INRecordPtr> entries {carry, upper};
		int newRoot = getNumPages ();
		MyDB_PageReaderWriter root = (*this)[newRoot];
		writeDirectory (root, entries, 0, entries.size ());
		rootLocation = newRoot;
		getTable ()->setRootLocation (rootLocation);
	}
}

int MyDB_BPlusTreeReaderWriter :: chooseDirectorySplit (vector <MyDB_INRecordPtr> &entries, size_t capacity) {

	// prefix[k] is the size of entries [0, k)
	size_t n = entries.size ();
	vector <size_t> prefix (n + 1, 0);
	for (size_t j = 0; j < n; j++)
		prefix[j + 1] = prefix[j] + entries[j]->getBinarySize ();

	// the median: the entry at (n - 1) / 2 moves up and stays in the lower
	// half, so the lower half is [0, (n + 1) / 2).  Then try the positions
	// nearest it, the smaller lower half first on a tie
	int median = (int) (n + 1) / 2;
	for (int d = 0; d <= (int) n; d++) {
		for (int k : {median - d, median + d}) {
			if (k < 1 || k > (int) n - 1)
				continue;
			if (prefix[k] <= capacity && prefix[n] - prefix[k] <= capacity)
				return k;
			if (d == 0)
				break;
		}
	}
	return -1;
}

void MyDB_BPlusTreeReaderWriter :: writeDirectory (MyDB_PageReaderWriter &page,
	vector <MyDB_INRecordPtr> &entries, size_t from, size_t to) {
	page.clear ();
	page.setType (MyDB_PageType :: DirectoryPage);
	for (size_t j = from; j < to; j++)
		if (!page.append (entries[j]))
			throw runtime_error ("B+-tree: a directory entry does not fit on its page");
}

int MyDB_BPlusTreeReaderWriter :: chooseSplit (vector <MyDB_RecordPtr> &recs, size_t capacity) {

	size_t total = 0;
	for (MyDB_RecordPtr r : recs)
		total += r->getBinarySize ();

	// try every split point; lowerBytes is the size of records [0, k)
	int best = -1;
	size_t bestDiff = 0, lowerBytes = 0;
	for (size_t k = 1; k < recs.size (); k++) {
		lowerBytes += recs[k - 1]->getBinarySize ();
		size_t upperBytes = total - lowerBytes;
		if (lowerBytes > capacity || upperBytes > capacity)
			continue;
		size_t diff = lowerBytes > upperBytes ? lowerBytes - upperBytes : upperBytes - lowerBytes;
		if (best == -1 || diff < bestDiff) {
			best = (int) k;
			bestDiff = diff;
		}
	}
	return best;
}

void MyDB_BPlusTreeReaderWriter :: readLeaf (MyDB_PageReaderWriter &page, vector <MyDB_RecordPtr> &recs) {
	MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
	while (myIter->advance ()) {
		MyDB_RecordPtr rec = getEmptyRecord ();
		myIter->getCurrent (rec);
		recs.push_back (rec);
	}
}

void MyDB_BPlusTreeReaderWriter :: readDirectory (MyDB_PageReaderWriter &page, vector <MyDB_INRecordPtr> &entries) {
	MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
	while (myIter->advance ()) {
		MyDB_INRecordPtr entry = getINRecord ();
		myIter->getCurrent (entry);
		entries.push_back (entry);
	}
}

size_t MyDB_BPlusTreeReaderWriter :: pageCapacity (MyDB_PageReaderWriter &page) {
	// MyDB_PageReaderWriter keeps the page type and the bytes used, one size_t
	// each, at the start of the page
	return page.getPageSize () - 2 * sizeof (size_t);
}

bool MyDB_BPlusTreeReaderWriter :: isValid () {

	// read-only: no checkReset (), and no page is touched before its number
	// is known to be a page of the file ((*this)[i] past the end grows it)
	ValidWalk walk;
	walk.numPages = getNumPages ();
	walk.leafDepth = -1;
	walk.seen.assign (walk.numPages, false);
	walk.onPath.assign (walk.numPages, false);
	walk.ok = true;

	// the root: the tree's idea of it must match the table's (the catalog's),
	// and it must be a directory page of the file
	int root = getTable ()->getRootLocation ();
	if (root != rootLocation)
		validReport (walk, root, "the table's root location is " + to_string (root) +
			", but the tree's is " + to_string (rootLocation));
	if (root < 0 || root >= walk.numPages) {
		validReport (walk, root, "the root is not a page of the file (it has " +
			to_string (walk.numPages) + " pages)");
		return false;
	}
	if ((*this)[root].getType () != MyDB_PageType :: DirectoryPage) {
		validReport (walk, root, "the root is not a directory page");
		return false;
	}

	// everything under the root; its last key must be the maximum
	validWalk (walk, root, 0, nullptr, getINRecord (), false);
	return walk.ok;
}

void MyDB_BPlusTreeReaderWriter :: validReport (ValidWalk &walk, int whichPage, string what) {
	cout << "B+-tree isValid: page " << whichPage << ": " << what << "\n";
	walk.ok = false;
}

void MyDB_BPlusTreeReaderWriter :: validWalk (ValidWalk &walk, int whichPage, int depth,
	MyDB_INRecordPtr low, MyDB_INRecordPtr high, bool emptyLeafOK) {

	// each page is reached once: again on the way down from itself is a
	// cycle, again from elsewhere is a second parent
	if (walk.onPath[whichPage]) {
		validReport (walk, whichPage, "is reached again below itself (a cycle)");
		return;
	}
	if (walk.seen[whichPage]) {
		validReport (walk, whichPage, "has more than one parent entry");
		return;
	}
	walk.seen[whichPage] = true;
	walk.onPath[whichPage] = true;

	MyDB_PageReaderWriter page = (*this)[whichPage];
	size_t capacity = pageCapacity (page);

	if (page.getType () == MyDB_PageType :: RegularPage) {

		// a leaf: at the same depth as every other leaf
		if (walk.leafDepth == -1)
			walk.leafDepth = depth;
		else if (depth != walk.leafDepth)
			validReport (walk, whichPage, "is a leaf at depth " + to_string (depth) +
				", but another leaf is at depth " + to_string (walk.leafDepth));

		// its records, in any order, fit on the page and lie in [low, high]
		MyDB_RecordPtr rec = getEmptyRecord ();
		function <bool ()> belowLow;
		if (low != nullptr)
			belowLow = buildComparator (rec, low);
		function <bool ()> aboveHigh = buildComparator (high, rec);
		size_t used = 0;
		int count = 0, outside = 0;
		MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
		while (myIter->advance ()) {
			myIter->getCurrent (rec);
			used += rec->getBinarySize ();
			count++;
			if ((low != nullptr && belowLow ()) || aboveHigh ())
				outside++;
		}
		if (used > capacity)
			validReport (walk, whichPage, "holds " + to_string (used) + " bytes of records, more than its " +
				to_string (capacity));
		if (outside > 0)
			validReport (walk, whichPage, to_string (outside) + " record(s) lie outside the key range [" +
				(low != nullptr ? getKey (low)->toString () : string ("-inf")) + ", " +
				getKey (high)->toString () + "] of the entry pointing here");
		if (count == 0 && !emptyLeafOK)
			validReport (walk, whichPage, "is an empty leaf, but it is not the only child of the root");

	} else if (page.getType () == MyDB_PageType :: DirectoryPage) {

		// a directory page: nonempty, fits on the page, keys non-decreasing
		// and in [low, high], and the last key equal to high
		vector <MyDB_INRecordPtr> entries;
		readDirectory (page, entries);
		if (entries.empty ()) {
			validReport (walk, whichPage, "is an empty directory page");
			walk.onPath[whichPage] = false;
			return;
		}
		size_t used = 0;
		for (size_t i = 0; i < entries.size (); i++) {
			used += entries[i]->getBinarySize ();
			string at = "entry " + to_string (i) + " (key " + getKey (entries[i])->toString () + ")";
			if (i > 0 && buildComparator (entries[i], entries[i - 1]) ())
				validReport (walk, whichPage, at + " is below the key before it");
			if (low != nullptr && buildComparator (entries[i], low) ())
				validReport (walk, whichPage, at + " is below the lower bound " + getKey (low)->toString ());
			if (buildComparator (high, entries[i]) ())
				validReport (walk, whichPage, at + " is above the upper bound " + getKey (high)->toString ());
		}
		if (used > capacity)
			validReport (walk, whichPage, "holds " + to_string (used) + " bytes of entries, more than its " +
				to_string (capacity));
		if (buildComparator (entries.back (), high) ())
			validReport (walk, whichPage, "its last key, " + getKey (entries.back ())->toString () +
				", is not " + (depth == 0 ? string ("the maximum (this is the root)") :
				"its parent entry's key, " + getKey (high)->toString ()));

		// then each child, whose number is checked before it is read: entry i
		// covers [key of entry i - 1, key of entry i]
		bool onlyChildOfRoot = (depth == 0 && entries.size () == 1);
		for (size_t i = 0; i < entries.size (); i++) {
			int child = entries[i]->getPtr ();
			if (child < 0 || child >= walk.numPages) {
				validReport (walk, whichPage, "entry " + to_string (i) + " points to page " + to_string (child) +
					", which is not a page of the file");
				continue;
			}
			validWalk (walk, child, depth + 1, i > 0 ? entries[i - 1] : low, entries[i], onlyChildOfRoot);
		}

	} else
		validReport (walk, whichPage, "has an unknown page type");

	walk.onPath[whichPage] = false;
}

void MyDB_BPlusTreeReaderWriter :: printTree () {
	checkReset ();
	printTree (rootLocation, 0);
}

void MyDB_BPlusTreeReaderWriter :: printTree (int whichPage, int depth) {

	MyDB_PageReaderWriter page = (*this)[whichPage];
	string indent (2 * depth, ' ');

	// a leaf: print every data record
	if (page.getType () == MyDB_PageType :: RegularPage) {
		cout << indent << "Leaf page " << whichPage << ":\n";
		MyDB_RecordPtr rec = getEmptyRecord ();
		MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
		while (myIter->advance ()) {
			myIter->getCurrent (rec);
			cout << indent << "  " << rec << "\n";
		}
		return;
	}

	// a directory page: print each entry, and the subtree under it
	cout << indent << "Dir page " << whichPage << ":\n";
	MyDB_INRecordPtr entry = getINRecord ();
	MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
	while (myIter->advance ()) {
		myIter->getCurrent (entry);
		cout << indent << "  key=" << entry->getKey ()->toString () << " -> page " << entry->getPtr () << "\n";
		printTree (entry->getPtr (), depth + 2);
	}
}

// ---------------------------------------------------------------------
// The three methods below are provided.  You do not need to change them.
// ---------------------------------------------------------------------

MyDB_INRecordPtr MyDB_BPlusTreeReaderWriter :: getINRecord () {
	return make_shared <MyDB_INRecord> (orderingAttType->createAttMax ());
}

MyDB_AttValPtr MyDB_BPlusTreeReaderWriter :: getKey (MyDB_RecordPtr fromMe) {

	MyDB_AttValPtr source;

	// in this case, got an IN record
	if (fromMe->getSchema () == nullptr) 
		return fromMe->getAtt (0)->getCopy ();

	// in this case, got a data record
	else 
		return fromMe->getAtt (whichAttIsOrdering)->getCopy ();
}

function <bool ()>  MyDB_BPlusTreeReaderWriter :: buildComparator (MyDB_RecordPtr lhs, MyDB_RecordPtr rhs) {

	MyDB_AttValPtr lhAtt, rhAtt;

	// in this case, the LHS is an IN record
	if (lhs->getSchema () == nullptr) {
		lhAtt = lhs->getAtt (0);	

	// here, it is a regular data record
	} else {
		lhAtt = lhs->getAtt (whichAttIsOrdering);
	}

	// in this case, the RHS is an IN record
	if (rhs->getSchema () == nullptr) {
		rhAtt = rhs->getAtt (0);	

	// here, it is a regular data record
	} else {
		rhAtt = rhs->getAtt (whichAttIsOrdering);
	}
	
	// now, build the comparison lambda and return
	if (orderingAttType->promotableToInt ()) {
		return [lhAtt, rhAtt] {return lhAtt->toInt () < rhAtt->toInt ();};
	} else if (orderingAttType->promotableToDouble ()) {
		return [lhAtt, rhAtt] {return lhAtt->toDouble () < rhAtt->toDouble ();};
	} else if (orderingAttType->promotableToString ()) {
		return [lhAtt, rhAtt] {return lhAtt->toString () < rhAtt->toString ();};
	} else {
		cout << "This is bad... cannot do anything with the >.\n";
		exit (1);
	}
}

#endif
