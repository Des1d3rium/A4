
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

	// appends appendMe into the subtree rooted at page whichPage: on a directory
	// page, descends into the first child whose key is >= the record's key; on a
	// leaf, appends the record to the page (Stage 1: exits if the leaf is full)
	void append (int whichPage, MyDB_RecordPtr appendMe);

	// adds, in key order, every leaf page under page whichPage whose key range
	// can overlap [low, high] to the list
	void discoverPages (int whichPage, vector <MyDB_PageReaderWriter> &list,
		MyDB_AttValPtr low, MyDB_AttValPtr high);

	// the shared body of the two range iterators; sortIt says whether each
	// leaf is sorted on the key before its records are returned
	MyDB_RecordIteratorAltPtr getRangeIteratorAlt (MyDB_AttValPtr low, MyDB_AttValPtr high, bool sortIt);

	// prints the subtree rooted at page whichPage, indented by depth
	void printTree (int whichPage, int depth);

	// the location (page number) of the root in the tree
	int rootLocation;

	// the type of the attribute that we are ordering on
	MyDB_AttTypePtr orderingAttType;

	// the number of the attribute that we are ordering on, in the data records
	int whichAttIsOrdering;

};

#endif
