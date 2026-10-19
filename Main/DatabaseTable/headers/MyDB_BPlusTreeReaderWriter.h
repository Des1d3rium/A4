
#ifndef BPLUS_H
#define BPLUS_H

#include <memory>
#include <functional>
#include "MyDB_BufferManager.h"
#include "MyDB_Record.h"
#include "MyDB_INRecord.h"
#include "MyDB_RecordIteratorAlt.h"
#include "MyDB_Table.h"
#include "MyDB_TableReaderWriter.h"

// create a smart pointer for the catalog
using namespace std;
class MyDB_PageReaderWriter;
class MyDB_BPlusTreeReaderWriter;
typedef shared_ptr <MyDB_BPlusTreeReaderWriter> MyDB_BPlusTreeReaderWriterPtr;

class MyDB_BPlusTreeReaderWriter : public MyDB_TableReaderWriter {

public:

	// create a BTree TableReaderWriter.  nameOfAttToOrderOn is the name of the
	// attribute (in the table's schema) that the tree is keyed on
	MyDB_BPlusTreeReaderWriter (string nameOfAttToOrderOn, MyDB_TablePtr forMe, MyDB_BufferManagerPtr myBuffer);

	// gets an instance of an alternate iterator over the table... this is an
	// iterator that has the alternate getCurrent ()/advance () interface.
	// Returns all records with a key value in the range [low, high], inclusive,
	// in no particular order
	MyDB_RecordIteratorAltPtr getRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high);

	// exactly like the above, except that the records come back sorted on the key
	MyDB_RecordIteratorAltPtr getSortedRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high);

	// insert a record into the B+-Tree.  Overrides MyDB_TableReaderWriter :: append (),
	// so loadFromTextFile () goes through this method, one record at a time
	void append (MyDB_RecordPtr appendMe);

	// print the contents of the tree to the screen.  For debugging
	void printTree ();

	// walks the whole tree and returns true if and only if it is a legal B+-tree,
	// printing a message for every violation found.  What "legal" means is for you
	// to decide (and for your Stage 5 prompt to state); see the A4 handout
	bool isValid ();

private:

	// ---------------------------------------------------------------------
	// Three helpers are provided (see the bottom of the .cc file).  Add
	// whatever other private methods you need below them.
	// ---------------------------------------------------------------------

	// constructs and returns an empty internal node record for this particular tree.
	// Its key is set to the largest possible value of the key type, and its pointer
	// is not set
	MyDB_INRecordPtr getINRecord ();

	// gets the search key from a record: the key attribute for a data record, the
	// key for an internal node record
	MyDB_AttValPtr getKey (MyDB_RecordPtr fromMe);

	// constructs and returns a comparator for the two records given... both must either
	// be IN records for this particular tree, or they must be data records for this tree,
	// or a combination.  The resulting lambda returns true if and only if the first record
	// has a key value less than the second record.  NOTE: the lambda captures the two
	// records' key attributes when it is built, so build it once and then load new contents
	// into the records with getCurrent (); if you replace a record's key with setKey (),
	// build the comparator again
	function <bool ()> buildComparator (MyDB_RecordPtr lhs, MyDB_RecordPtr rhs);

	// given records sorted on the key, returns k such that records [0, k) and
	// [k, n) each fit in capacity bytes with the smallest difference in bytes
	// between the two halves, or -1 if no such k exists (both halves nonempty)
	int chooseSplit (vector <MyDB_RecordPtr> &recs, size_t capacity);

	// given the entries of an overflowing directory page, in order, returns k
	// such that entries [0, k) and [k, n) each fit in capacity bytes: k is the
	// count median (n + 1) / 2 (the entry at (n - 1) / 2 moves up) if both
	// halves fit there, otherwise the nearest k that fits (the smaller k on a
	// tie), or -1 if none does (both halves nonempty)
	int chooseDirectorySplit (vector <MyDB_INRecordPtr> &entries, size_t capacity);

	// clears page, makes it a directory page, and writes entries [from, to) to
	// it; throws runtime_error if any entry does not fit
	void writeDirectory (MyDB_PageReaderWriter &page, vector <MyDB_INRecordPtr> &entries, size_t from, size_t to);

	// copies every data record on a leaf / every entry on a directory page
	// into its own record object
	void readLeaf (MyDB_PageReaderWriter &page, vector <MyDB_RecordPtr> &recs);
	void readDirectory (MyDB_PageReaderWriter &page, vector <MyDB_INRecordPtr> &entries);

	// the number of bytes a page can hold for records (page size minus header)
	size_t pageCapacity (MyDB_PageReaderWriter &page);

	// makes this an empty tree: page 0 an empty leaf, page 1 a root with one
	// entry (max key, page 0); sets rootLocation and the table's root location
	void makeEmptyTree ();

	// if loadFromTextFile () has emptied the file since the tree last looked
	// (the last page is page 0, the root is past it, and page 0 is an empty
	// leaf), starts again from an empty tree.  Called first by append (), the
	// range iterators and printTree (); not by isValid (), which never writes
	void checkReset ();

	// adds, in key order, every leaf page under directory page whichPage
	// whose key range can overlap [low, high] to the list, reading only the
	// first such leaf under each directory page (to learn it is a leaf)
	void discoverPages (int whichPage, vector <MyDB_PageReaderWriter> &list,
		MyDB_AttValPtr low, MyDB_AttValPtr high);

	// the shared body of the two range iterators; sortIt says whether each
	// leaf is sorted on the key before its records are returned
	MyDB_RecordIteratorAltPtr getRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high, bool sortIt);

	// prints the subtree rooted at page whichPage, indented by depth
	void printTree (int whichPage, int depth);

	// what isValid () knows as it walks the tree: the number of pages in the
	// file, the depth of the first leaf found, the pages reached so far and
	// the pages on the path from the root to the current page, and whether
	// any violation has been found
	struct ValidWalk {
		int numPages;
		int leafDepth;
		vector <bool> seen;
		vector <bool> onPath;
		bool ok;
	};

	// prints one violation, "B+-tree isValid: page <whichPage>: <what>", and
	// marks the walk as failed
	void validReport (ValidWalk &walk, int whichPage, string what);

	// checks the subtree under page whichPage (which must be a page of the
	// file) at the given depth: every key in it must lie in [low, high] (no
	// lower bound if low is nullptr), and a directory page's last key must
	// equal high.  emptyLeafOK says whether whichPage may be an empty leaf
	// (only the root's only child may be).  Reads pages; never writes
	void validWalk (ValidWalk &walk, int whichPage, int depth, MyDB_INRecordPtr low,
		MyDB_INRecordPtr high, bool emptyLeafOK);

	// the location (page number) of the root in the tree
	int rootLocation;

	// the type of the attribute that we are ordering on
	MyDB_AttTypePtr orderingAttType;

	// the number of the attribute that we are ordering on, in the data records
	int whichAttIsOrdering;

};

#endif
