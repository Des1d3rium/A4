#ifndef SORT_TEST_H
#define SORT_TEST_H

#include "MyDB_AttType.h"
#include "MyDB_BufferManager.h"
#include "MyDB_Catalog.h"
#include "MyDB_Page.h"
#include "MyDB_PageReaderWriter.h"
#include "MyDB_Record.h"
#include "MyDB_Table.h"
#include "MyDB_TableReaderWriter.h"
#include "MyDB_Schema.h"
#include "QUnit.h"
#include "Sorting.h"
#include <iostream>
#include <unistd.h>

#define FALLTHROUGH_INTENDED do {} while (0)

// loads supplierBig.tbl (320,000 records) into supplier.bin and puts the
// table in the catalog.  Every test starts from this, so each one can be
// run on its own
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

	// use the schema to create a table
	MyDB_TablePtr myTable = make_shared <MyDB_Table> ("supplier", "supplier.bin", mySchema);
	MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager> (131072, 128, "tempFile");
	MyDB_TableReaderWriter supplierTable (myTable, myMgr);

	// load it from a text file
	supplierTable.loadFromTextFile ("supplierBig.tbl");

	// put the supplier table into the catalog
	myTable->putInCatalog (myCatalog);

	cout << "finish initialization..." << flush;
}

int main (int argc, char *argv[]) {
	int start = 1;
	if (argc > 1 && argv[1][0] >= '0' && argv[1][0] <= '9') {
		start = argv[1][0] - '0';
	}
	cout << "start from test " << start << endl << flush;

	QUnit::UnitTest qunit (cerr, QUnit::normal);

	switch (start) {
	case 1:
	{
		// load the big file, then scan it two ways: with the table's iterator,
		// and with the iterator over a list of pages
		cout << "TEST 1..." << flush;
		unlink ("catFile");
		initialize ();

		// load up the supplier table from the catalog
		MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog> ("catFile");
		map <string, MyDB_TablePtr> allTables = MyDB_Table :: getAllTables (myCatalog);
		MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager> (131072, 128, "tempFile");
		MyDB_TableReaderWriter supplierTable (allTables["supplier"], myMgr);

		// loop through all of the records in the file
		cout << "scan the table..." << flush;
		MyDB_RecordPtr temp = supplierTable.getEmptyRecord ();
		MyDB_RecordIteratorAltPtr myIter = supplierTable.getIteratorAlt ();
		int counter = 0;
		while (myIter->advance ()) {
			myIter->getCurrent (temp);
			counter++;
		}

		// there should be 320,000 records
		cout << counter << " records..." << flush;
		QUNIT_IS_EQUAL (counter, 320000);

		// now do the same thing with an iterator over a list of pages
		cout << "scan the list of pages..." << flush;
		vector <MyDB_PageReaderWriter> allPages;
		for (int i = 0; i < supplierTable.getNumPages (); i++) {
			allPages.push_back (supplierTable[i]);
		}
		myIter = getIteratorAlt (allPages);
		counter = 0;
		while (myIter->advance ()) {
			myIter->getCurrent (temp);
			counter++;
		}
		cout << counter << " records..." << endl << flush;
		QUNIT_IS_EQUAL (counter, 320000);
	}
	FALLTHROUGH_INTENDED;

	case 2:
	{
		// sort the contents of one page, and check that the result is in
		// order and has the same number of records
		cout << "TEST 2..." << flush;
		unlink ("catFile");
		initialize ();

		MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog> ("catFile");
		map <string, MyDB_TablePtr> allTables = MyDB_Table :: getAllTables (myCatalog);
		MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager> (131072, 128, "tempFile");
		MyDB_TableReaderWriter supplierTable (allTables["supplier"], myMgr);

		// count the records on the 37th page
		cout << "count page 36..." << flush;
		MyDB_RecordPtr temp = supplierTable.getEmptyRecord ();
		MyDB_RecordIteratorAltPtr myIter = supplierTable[36].getIteratorAlt ();
		int before = 0;
		while (myIter->advance ()) {
			myIter->getCurrent (temp);
			before++;
		}

		// sort the contents of the page by acctbal
		cout << "sort page 36..." << flush;
		MyDB_RecordPtr temp2 = supplierTable.getEmptyRecord ();
		function <bool ()> myComp = buildRecordComparator (temp, temp2, "[acctbal]");
		MyDB_PageReaderWriterPtr sorted = supplierTable[36].sort (myComp, temp, temp2);

		// now iterate over the sorted page: every record must be >= the one
		// before it, and the count must match
		cout << "check page 36..." << flush;
		myIter = sorted->getIteratorAlt ();
		int after = 0;
		bool inOrder = true;
		if (myIter->advance ()) {
			myIter->getCurrent (temp2);	// temp2 is the previous record
			after++;
			while (myIter->advance ()) {
				myIter->getCurrent (temp);	// temp is the current record
				after++;
				if (myComp ())				// current < previous?
					inOrder = false;
				myIter->getCurrent (temp2);
			}
		}
		cout << before << " records before, " << after << " after..." << endl << flush;
		QUNIT_IS_EQUAL (before, after);
		QUNIT_IS_TRUE (inOrder);
	}
	FALLTHROUGH_INTENDED;

	case 3:
	{
		// TPMMS sort of the whole file, then count the output
		cout << "TEST 3..." << flush;
		unlink ("catFile");
		initialize ();

		MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog> ("catFile");
		map <string, MyDB_TablePtr> allTables = MyDB_Table :: getAllTables (myCatalog);
		MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager> (131072, 128, "tempFile");
		MyDB_TableReaderWriter supplierTable (allTables["supplier"], myMgr);

		// use the schema to create the output table
		MyDB_TablePtr outTable = make_shared <MyDB_Table> ("supplierSorted",
			"supplierSorted.bin", allTables["supplier"]->getSchema ());
		MyDB_TableReaderWriter outputTable (outTable, myMgr);

		// get two empty records and a comparator
		MyDB_RecordPtr rec1 = supplierTable.getEmptyRecord ();
		MyDB_RecordPtr rec2 = supplierTable.getEmptyRecord ();
		function <bool ()> myComp = buildRecordComparator (rec1, rec2, "[acctbal]");

		// and sort
		cout << "sort..." << flush;
		sort (64, supplierTable, outputTable, myComp, rec1, rec2);

		// there should be 320,000 records in the output
		cout << "count the output..." << flush;
		MyDB_RecordIteratorAltPtr myIter = outputTable.getIteratorAlt ();
		int counter = 0;
		while (myIter->advance ()) {
			myIter->getCurrent (rec1);
			counter++;
		}
		cout << counter << " records..." << endl << flush;
		QUNIT_IS_EQUAL (counter, 320000);

		// put the sorted table into the catalog for the next test
		outTable->putInCatalog (myCatalog);
	}
	FALLTHROUGH_INTENDED;

	case 4:
	{
		// TPMMS sort of the whole file, then compare the output record by
		// record with a file that was sorted independently.  Records with
		// equal sort keys may come out in either order, so only the sort key
		// is compared
		cout << "TEST 4..." << flush;
		unlink ("catFile");
		initialize ();

		MyDB_CatalogPtr myCatalog = make_shared <MyDB_Catalog> ("catFile");
		map <string, MyDB_TablePtr> allTables = MyDB_Table :: getAllTables (myCatalog);
		MyDB_BufferManagerPtr myMgr = make_shared <MyDB_BufferManager> (131072, 128, "tempFile");
		MyDB_TableReaderWriter supplierTable (allTables["supplier"], myMgr);

		MyDB_TablePtr outTable = make_shared <MyDB_Table> ("supplierSorted",
			"supplierSorted.bin", allTables["supplier"]->getSchema ());
		MyDB_TableReaderWriter sortedTable (outTable, myMgr);

		MyDB_RecordPtr rec1 = supplierTable.getEmptyRecord ();
		MyDB_RecordPtr rec2 = supplierTable.getEmptyRecord ();
		function <bool ()> myComp = buildRecordComparator (rec1, rec2, "[acctbal]");

		cout << "sort..." << flush;
		sort (64, supplierTable, sortedTable, myComp, rec1, rec2);

		// load up the independently sorted text file
		cout << "load the reference..." << flush;
		MyDB_TablePtr refTable = make_shared <MyDB_Table> ("supplierRef",
			"supplierRef.bin", allTables["supplier"]->getSchema ());
		MyDB_TableReaderWriter otherSortedTable (refTable, myMgr);
		otherSortedTable.loadFromTextFile ("supplierBigSorted.tbl");

		// walk the two files in step
		cout << "compare..." << flush;
		MyDB_RecordIteratorAltPtr myIterOne = sortedTable.getIteratorAlt ();
		MyDB_RecordIteratorAltPtr myIterTwo = otherSortedTable.getIteratorAlt ();
		int matches = 0, total = 0;
		while (myIterOne->advance ()) {
			if (!myIterTwo->advance ())
				break;
			total++;

			// get the two records; they match if neither is less than the other
			myIterOne->getCurrent (rec1);
			myIterTwo->getCurrent (rec2);
			if (!myComp ()) {
				myIterOne->getCurrent (rec2);
				myIterTwo->getCurrent (rec1);
				if (!myComp ())
					matches++;
			}
		}
		cout << matches << " of " << total << " match..." << endl << flush;
		QUNIT_IS_EQUAL (matches, 320000);
	}
	FALLTHROUGH_INTENDED;

	default:
		break;
	}

	unlink ("catFile");
	unlink ("supplier.bin");
	unlink ("supplierSorted.bin");
	unlink ("supplierRef.bin");
}

#endif
