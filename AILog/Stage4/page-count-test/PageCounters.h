#ifndef PAGE_COUNTERS_H
#define PAGE_COUNTERS_H

// ---------------------------------------------------------------------
// Instrumentation for the Stage 4 page-count test.  ONLY in the patched
// copies of the code base under scratchpad/count/src; never in Main/.
//
// The tree sets current before it touches a page, and counts the page
// access (a logical read of a page's bytes) in that category.  The buffer
// manager counts every page it actually reads from disk (a load) in the
// category that is current at the time, and times each load, so the
// average cost of a load can be reported.  Loads of anonymous (temp) pages
// are counted apart, as are writes back to disk.
// ---------------------------------------------------------------------

#include <chrono>
#include <cstring>

enum PageCat {
	CAT_RESET,		// checkReset (): the root's page type
	CAT_DIR_SEARCH,		// discoverPages (): a directory page searched
	CAT_CHILD_PEEK,		// discoverPages (): reading a child only for its type
	CAT_LEAF_ITER,		// the range iterator reading a leaf's records
	CAT_APPEND,		// append (): everything it touches
	CAT_OTHER,		// anything else (tests, walks, printTree ...)
	CAT_NUM
};

static const char *catName[CAT_NUM] = {
	"root check (checkReset)", "directory search", "child peek (type only)",
	"leaf read by iterator", "append", "other"};

struct PageCounters {
	long accesses[CAT_NUM];
	long loads[CAT_NUM];
	double loadNanos[CAT_NUM];
	long tempLoads;
	long writes;
	PageCat current;

	void reset () {
		memset (accesses, 0, sizeof (accesses));
		memset (loads, 0, sizeof (loads));
		memset (loadNanos, 0, sizeof (loadNanos));
		tempLoads = writes = 0;
		current = CAT_OTHER;
	}

	long totalAccesses () {long t = 0; for (long a : accesses) t += a; return t;}
	long totalLoads () {long t = 0; for (long a : loads) t += a; return t;}
	double totalNanos () {double t = 0; for (double a : loadNanos) t += a; return t;}

	// the tree announces a page access in category c
	void access (PageCat c) {current = c; accesses[c]++;}
};

inline PageCounters &counters () {
	static PageCounters c = [] {PageCounters p; p.reset (); return p;} ();
	return c;
}

// times one disk read in the buffer manager
struct LoadTimer {
	bool temp;
	std::chrono::steady_clock::time_point start;
	LoadTimer (bool tempIn) : temp (tempIn), start (std::chrono::steady_clock::now ()) {}
	~LoadTimer () {
		PageCounters &c = counters ();
		if (temp) {
			c.tempLoads++;
			return;
		}
		c.loads[c.current]++;
		c.loadNanos[c.current] += std::chrono::duration <double, std::nano>
			(std::chrono::steady_clock::now () - start).count ();
	}
};

#endif
