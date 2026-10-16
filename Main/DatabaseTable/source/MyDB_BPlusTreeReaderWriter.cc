
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
//     keys <= k".  The last entry of a directory page has the maximum key.
//  5. Duplicate keys are allowed; append () never drops or merges a record.
//  6. append () starts at rootLocation and descends: on a directory page it
//     follows the first entry whose key is >= the record's key; on a leaf it
//     appends the record at the end of the page (leaves are not kept sorted).
//  7. Leaf overflow (Stage 1 only, no splits): if the leaf has no room,
//     append () prints an error and calls exit (1).  Stage 2 replaces this
//     with a split.
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
//     getRangeIteratorAlt passes sortOrNot = false.
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
// ---------------------------------------------------------------------

#include "MyDB_INRecord.h"
#include "MyDB_BPlusTreeReaderWriter.h"
#include "MyDB_PageReaderWriter.h"
#include "MyDB_PageListIteratorSelfSortingAlt.h"
#include "RecordComparator.h"
#include <algorithm>

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

	// a new tree: page 0 is an empty leaf...
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

MyDB_RecordIteratorAltPtr MyDB_BPlusTreeReaderWriter :: getSortedRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high) {
	return getRangeIteratorAlt (low, high, true);
}

MyDB_RecordIteratorAltPtr MyDB_BPlusTreeReaderWriter :: getRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high) {
	return getRangeIteratorAlt (low, high, false);
}

MyDB_RecordIteratorAltPtr MyDB_BPlusTreeReaderWriter :: getRangeIteratorAlt (MyDB_AttValPtr low,
	MyDB_AttValPtr high, bool sortIt) {

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

	MyDB_PageReaderWriter page = (*this)[whichPage];

	// a leaf: its records are filtered later, by the iterator
	if (page.getType () == MyDB_PageType :: RegularPage) {
		list.push_back (page);
		return;
	}

	// a directory page: entry i covers keys in [key of entry i - 1, key of entry i]
	MyDB_INRecordPtr lowRec = getINRecord ();
	lowRec->setKey (low);
	MyDB_INRecordPtr highRec = getINRecord ();
	highRec->setKey (high);
	MyDB_INRecordPtr entry = getINRecord ();
	MyDB_INRecordPtr prevEntry = getINRecord ();
	function <bool ()> entryBelowLow = buildComparator (entry, lowRec);
	function <bool ()> highBelowPrev = buildComparator (highRec, prevEntry);

	bool hasPrev = false;
	MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
	while (myIter->advance ()) {
		myIter->getCurrent (entry);

		// keep the child if its key >= low and the previous key <= high
		if (!entryBelowLow () && !(hasPrev && highBelowPrev ()))
			discoverPages (entry->getPtr (), list, low, high);

		myIter->getCurrent (prevEntry);
		hasPrev = true;
	}
}

void MyDB_BPlusTreeReaderWriter :: append (MyDB_RecordPtr appendMe) {
	append (rootLocation, appendMe);
}

void MyDB_BPlusTreeReaderWriter :: append (int whichPage, MyDB_RecordPtr appendMe) {

	MyDB_PageReaderWriter page = (*this)[whichPage];

	// a leaf: put the record at the end.  There are no splits in Stage 1
	if (page.getType () == MyDB_PageType :: RegularPage) {
		if (!page.append (appendMe)) {
			cout << "B+-tree: leaf page " << whichPage << " is full, and splits are not implemented.\n";
			exit (1);
		}
		return;
	}

	// a directory page: descend into the first child whose key >= the record's key
	MyDB_INRecordPtr entry = getINRecord ();
	function <bool ()> entryBelowRec = buildComparator (entry, appendMe);
	MyDB_RecordIteratorAltPtr myIter = page.getIteratorAlt ();
	while (myIter->advance ()) {
		myIter->getCurrent (entry);
		if (!entryBelowRec ()) {
			append (entry->getPtr (), appendMe);
			return;
		}
	}

	// cannot happen: the last entry of every directory page has the max key
	cout << "B+-tree: directory page " << whichPage << " has no entry for the key "
		<< getKey (appendMe)->toString () << ".\n";
	exit (1);
}

bool MyDB_BPlusTreeReaderWriter :: isValid () {
	return false;
}

void MyDB_BPlusTreeReaderWriter :: printTree () {
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
