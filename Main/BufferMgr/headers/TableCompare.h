

/****************************************************
** COPYRIGHT 2016, Chris Jermaine, Rice University **
**                                                 **
** The MyDB Database System, COMP 530              **
** Note that this file contains SOLUTION CODE for  **
** A1.  You should not be looking at this file     **
** unless you have completed A1!                   **
****************************************************/

#ifndef TABLE_COMP_H
#define TABLE_COMP_H

#include "MyDB_Table.h"

// so that pages can be put into a map
struct TableCompare {

public:

	bool operator() (const MyDB_TablePtr lhs, const MyDB_TablePtr rhs) const {

		// deal with the null case
		if (lhs == nullptr && rhs != nullptr) {
			return true;
		} else if (rhs == nullptr) {
			return false;
		}

		// otherwise, compare the storage locations.  A table is identified by
		// where it lives on disk, not by its name: two MyDB_Table objects with
		// the same storage location are the same table, and their pages must
		// be the same pages (and must share one file descriptor)
		return lhs->getStorageLoc () < rhs->getStorageLoc ();
	}
};

#endif

