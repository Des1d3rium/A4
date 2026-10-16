#ifndef BPLUS_TEST_H
#define BPLUS_TEST_H

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

#define FALLTHROUGH_INTENDED do {} while (0)

// loads supplierBig.tbl (320,000 records, 10,000 distinct suppkey values,
// 32 records per value) into a B+-tree on suppkey, and puts the table in
// the catalog.  Every test starts from a tree built this way
void initialize () {
	cout << "start initialization..." << flush;

	// create a catalog
	MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog> ("catFile");

	// now make a schema
	MyDB_SchemaPtr mySchema = make_shared <MyDB_Schema> ();
	mySchema->appendAtt (make_pair ("suppkey", make_shared <MyDB_IntAttType> ()));
	mySchema->appendAtt (make_pair ("name", make_shared <MyDB_StringAttType> ()));
	mySchema->appendAtt (make_pair ("address", make_shared <MyDB_StringAttType> ()));
	mySchema->appendAtt (make_pair ("nationkey", make_shared <MyDB_IntAttType> ()));
	mySchema->appendAtt (make_pair ("phone", make_shared <MyDB_StringAttType> ()));
	mySchema->appendAtt (make_pair ("acctbal", make_shared <MyDB_DoubleAttType> ()));
	mySchema->appendAtt (make_pair ("comment", make_shared <MyDB_StringAttType> ()));

	// use the schema to create a table, and a B+-tree over it
	MyDB_TablePtr myTable = make_shared <MyDB_Table> ("supplier", "supplier.bin", mySchema);
	MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager> (131072, 128, "tempFile");
	MyDB_BPlusTreeReaderWriter supplierTable ("suppkey", myTable, myMgr);

	// load it from a text file; this goes through append (), one record at a time
	supplierTable.loadFromTextFile ("supplierBig.tbl");

	// put the supplier table into the catalog, so that later tests can re-open it
	myTable->putInCatalog (myCatalog);

	cout << "finish initialization..." << flush;
}

// re-opens the supplier table from the catalog, with a fresh buffer manager
MyDB_BPlusTreeReaderWriterPtr reopen (MyDB_BufferManagerPtr &myMgr) {
	MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog> ("catFile");
	map <string, MyDB_TablePtr> allTables = MyDB_Table :: getAllTables (myCatalog);
	myMgr = make_shared <MyDB_BufferManager> (131072, 128, "tempFile");
	return make_shared <MyDB_BPlusTreeReaderWriter> ("suppkey", allTables["supplier"], myMgr);
}

// runs the i-th query from a fixed sequence of pseudo-random [low, high]
// ranges over suppkey, using the sorted or unsorted iterator, and checks
// that exactly 32 * (high - low + 1) records come back.  When sorted is
// true, also checks that they come back in order
void checkQuery (QUnit::UnitTest &qunit, MyDB_BPlusTreeReaderWriter &supplierTable, int i, bool pointQuery, bool sorted) {

	srand48 (i);
	int lowBound = lrand48 () % 10000;
	int highBound = pointQuery ? lowBound : lrand48 () % 10000;
	if (lowBound > highBound) {
		int temp = lowBound;
		lowBound = highBound;
		highBound = temp;
	}

	MyDB_IntAttValPtr low = make_shared <MyDB_IntAttVal> ();
	low->set (lowBound);
	MyDB_IntAttValPtr high = make_shared <MyDB_IntAttVal> ();
	high->set (highBound);

	MyDB_RecordIteratorAltPtr myIter;
	if (sorted)
		myIter = supplierTable.getSortedRangeIteratorAlt (low, high);
	else
		myIter = supplierTable.getRangeIteratorAlt (low, high);

	MyDB_RecordPtr temp = supplierTable.getEmptyRecord ();
	int counter = 0, lastKey = -1, outOfOrder = 0, outOfRange = 0;
	while (myIter->advance ()) {
		myIter->getCurrent (temp);
		int key = temp->getAtt (0)->toInt ();
		if (key < lowBound || key > highBound)
			outOfRange++;
		if (sorted && key < lastKey)
			outOfOrder++;
		lastKey = key;
		counter++;
	}

	QUNIT_IS_EQUAL (counter, 32 * (highBound - lowBound + 1));
	QUNIT_IS_EQUAL (outOfRange, 0);
	QUNIT_IS_EQUAL (outOfOrder, 0);
}

int main (int argc, char *argv[]) {
	int start = 1;
	if (argc > 1 && argv[1][0] >= '0' && argv[1][0] <= '9') {
		start = argv[1][0] - '0';
	}
	cout << "start from test " << start << endl << flush;

	QUnit::UnitTest qunit (cerr, QUnit::normal);

	// tests 2 and 3 use the tree that test 1 builds, so build it if we are
	// skipping test 1
	if (start > 1) {
		unlink ("catFile");
		initialize ();
		cout << endl;
	}

	switch (start) {
	case 1:
	{
		// build the tree, check that every record is in it, and check that
		// the tree is a legal B+-tree
		cout << "TEST 1..." << flush;
		unlink ("catFile");
		initialize ();

		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr supplierTable = reopen (myMgr);

		cout << "scan the tree..." << flush;
		MyDB_RecordPtr temp = supplierTable->getEmptyRecord ();
		MyDB_RecordIteratorAltPtr myIter = supplierTable->getIteratorAlt ();
		int counter = 0;
		while (myIter->advance ()) {
			myIter->getCurrent (temp);
			counter++;
		}
		cout << counter << " records..." << flush;
		QUNIT_IS_EQUAL (counter, 320000);

		cout << "check the tree..." << endl << flush;
		QUNIT_IS_TRUE (supplierTable->isValid ());
	}
	FALLTHROUGH_INTENDED;

	case 2:
	{
		// 100 point queries, alternating between the two iterators
		cout << "TEST 2..." << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr supplierTable = reopen (myMgr);

		cout << "100 point queries..." << endl << flush;
		for (int i = 0; i < 100; i++)
			checkQuery (qunit, *supplierTable, i, true, i % 2 == 1);
	}
	FALLTHROUGH_INTENDED;

	case 3:
	{
		// 100 range queries, alternating between the two iterators
		cout << "TEST 3..." << flush;
		MyDB_BufferManagerPtr myMgr;
		MyDB_BPlusTreeReaderWriterPtr supplierTable = reopen (myMgr);

		cout << "100 range queries..." << endl << flush;
		for (int i = 0; i < 100; i++)
			checkQuery (qunit, *supplierTable, i, false, i % 2 == 1);
	}
	FALLTHROUGH_INTENDED;

	default:
		break;
	}

	unlink ("catFile");
	unlink ("supplier.bin");

	return qunit.errors ();
}

#endif
