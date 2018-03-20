#ifndef PIN_CACHE_H
#define PIN_CACHE_H


#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>

int aggression;

using namespace std;

/* ===================================================================== */

// Convert a number to string
template <typename T>
string NumberToString (T Number)
{
   ostringstream ss ;
   ss << Number;
   return ss.str();
}

/* ===================================================================== */

// Implements the true LRU functionality for the d-cache
class LRU {
public:
  LRU(int n): _lru(n, 0), _ways(n) {}
  void putWayInMRU(const int way);
  const bool checkLRU(const int) const;
  void insertInMRU(const int way);
  void setLRU(const int way , const int invalids);
  const bool checkLRU(const bool) const;
  const int getLRU() const {return _lru.at(_ways - 1);}
  void invalidateWay(const int);
private:
  vector<int> _lru;
  int _ways;
};

/* ===================================================================== */

// The block in the LRU gets evicted and the new block gets moved to the MRU
void LRU::insertInMRU(const int way)
{
  for (int i = _ways - 1; i > 0; i--)
    _lru.at(i) = _lru.at(i - 1);
  _lru.at(0) = way;
}

/* ===================================================================== */

// Put a block in MRU after a probe
void LRU::putWayInMRU(const int way)
{
  int pos = 0;
  for (int i = 0; i < _ways; i++) {
    if (_lru.at(i) == way ) {
      pos =  i;
      break;
    }
  }
  for (int i = pos; i > 0; i--)
    _lru.at(i) = _lru.at(i - 1);
  _lru.at(0) = way;
}

/* ===================================================================== */

// Put a block at the LRU position
void LRU::setLRU(const int way, const int invalids)
{
  if (invalids > 0) _lru.at(_ways  - invalids) = way;
  else _lru.at(_ways  - 1) = way;
}

/* ===================================================================== */

// Check the LRU functionality for correctness issues
const bool LRU::checkLRU(const bool allValid) const
{
  if (allValid){
    vector<bool> waysExist(_ways, false);
    for (int i = 0; i < _ways; i++) {
      for (int j = 0; j < _ways; j++) {
        if ( _lru.at(j) == i) waysExist.at(i) = true;
      }
    }
    for (int i = 0; i < _ways; i++) {
      if (waysExist.at(i) == false) {
        cout << "after" << endl;
        for (int i = 0; i < _ways; i++) cout << "way: " << _lru.at(i) << endl;
        return false;
      }
    }
  }
  return true;
}

/* ===================================================================== */

// Move a way to LRU after it has been invalidated
void LRU::invalidateWay(const int way)
{
  int pos = 0;
  for (int i = 0; i < _ways; i++) {
    if (_lru.at(i) == way ) {
      pos =  i;
      break;
    }
  }
  for (int i = pos; i < _ways - 1; i++)
    _lru.at(i) = _lru.at(i + 1);
  _lru.at(_ways - 1) = 0;
}

/* ===================================================================== */

// The class that implements the functionality of an n-way associative cache
class Cache {
public:
    Cache(const int sets, const int ways, const int blockSize);
    void fillLine(const UINT64, const UINT64 = 0);
    void prefetchFillLine(const UINT64);
    const bool probeTag(const UINT64 addr);
    const bool exists(const UINT64 addr);
    void store(const UINT64 addr);
    const long getPrefHits() const {return _prefHits;}
    const long getSuccessfulPrefs() const {return _successfulPrefs;}
    void invalidateAddr(const UINT64 addr);
    void print() const;
private:
    const int getSetInvalids(const int set) const;
    void demandFillPrefStatsManaging(const int set, const int way);
    const UINT64 getSet(const UINT64 addr) { return (addr / _blockSize ) % _lineNo;};
    const UINT64 getTag(const UINT64 addr) { return addr / (_blockSize * _lineNo);};
    void LRUcheck(const int, const bool) const;
    vector<vector<UINT64> > _tagStore;
    vector<vector<bool> > _validBits;
    vector<vector<bool> > _dirtyBits;
    vector<vector<bool> > _prefetched;
    vector<vector<bool> > _successfulPrefetch;
    vector<LRU*> _lru;
    UINT64 _lineNo;
    UINT64 _blockSize;
    int _ways;
    long _prefHits;
    long _successfulPrefs;
};

/* ===================================================================== */

// Default Constructor of cache
Cache::Cache(const int sets, const int ways, const int blockSize): _tagStore(sets, vector<UINT64>(ways, 0)), _validBits(sets, vector<bool>(ways, false)), _dirtyBits(sets, vector<bool>(ways, false)),
                _prefetched(sets, vector<bool>(ways, false)), _successfulPrefetch(sets, vector<bool>(ways, false)), _lineNo(sets), _blockSize(blockSize), _ways(ways), _prefHits(0),
                _successfulPrefs(0)
{
  for (uint i = 0; i < _lineNo; i++) {
    LRU* lru = new LRU(_ways);
    _lru.push_back(lru);
  }
}

/* ===================================================================== */

// Read the Tags of a set to find a block after a demand access; triggers LRU changes
const bool Cache::probeTag(const UINT64 addr)
{
  bool hit = false;
  int set = getSet(addr);
  bool allValid = getSetInvalids(set) == 0;
  for (int i = 0; i < _ways; i++) {
    if (  _validBits.at(set).at(i) && (_tagStore.at(set).at(i) == getTag(addr))) {
      hit = true;
      if (_prefetched.at(set).at(i)) {
        _prefHits++;
        _successfulPrefetch.at(set).at(i) = true;
      }
      _lru.at(set)->putWayInMRU(i);
      LRUcheck(set, allValid);
      break;
    }
  }
  return hit;
}

/* ===================================================================== */

// Fill a block after a demand; put it in MRU
void Cache::fillLine(const UINT64 addr, const UINT64 data)
{
  int set = getSet(addr);
  for (int i = 0; i < _ways; i++) {
    if (_validBits.at(set).at(i) ==  false) {
      _validBits.at(set).at(i) =  true;
      _tagStore.at(set).at(i) = getTag(addr);
      demandFillPrefStatsManaging(set, i);
      _lru.at(set)->insertInMRU(i);
      bool allValid = getSetInvalids(set) == 0;
      LRUcheck(set, allValid);
      return;
    }
  }
  // if there is no empty way in the set find the LRU
  int way = _lru.at(set)->getLRU();
  _validBits.at(set).at(way) =  true;
  _tagStore.at(set).at(way) = getTag(addr);
  demandFillPrefStatsManaging(set, way);
  _lru.at(set)->insertInMRU(way);
  bool allValid = getSetInvalids(set) == 0;
  LRUcheck(set, allValid);
}

/* ===================================================================== */

// Fill a block after a prefetch; put the block into the LRU
void Cache::prefetchFillLine(const UINT64 addr)
{
  int set = getSet(addr);
  int invalids = getSetInvalids(set);
  bool allValid =  invalids == 0;
  for (int i = 0; i < _ways; i++) {
    if (_validBits.at(set).at(i) ==  false) {
      _tagStore.at(set).at(i) = getTag(addr);
      _lru.at(set)->setLRU(i, invalids);
      _validBits.at(set).at(i) =  true;
      _prefetched.at(set).at(i) =  true;
      LRUcheck(set, allValid);
      return;
    }
  }
  // if there is no empty way in the set find the LRU
  int way = _lru.at(set)->getLRU();
  LRUcheck(set, allValid);
  _validBits.at(set).at(way) =  true;
  _tagStore.at(set).at(way) = getTag(addr);
  _prefetched.at(set).at(way) =  true;
  // leave the prefetched block at the LRU position
}

/* ===================================================================== */

// Get the number of invalid ways in a set
const int Cache::getSetInvalids(const int set) const
{
  int counter = 0;
  for (int i = 0; i < _ways; i++) {
    if (!_validBits.at(set).at(i)) counter++;
  }
  return counter;
}

/* ===================================================================== */

// Check if a tag exists in the cache without triggerring LRU changes
const bool Cache::exists(const UINT64 addr)
{
  for (int i = 0; i < _ways; i++) {
    if (_validBits.at(getSet(addr)).at(i) && (_tagStore.at(getSet(addr)).at(i) == getTag(addr)))
      return true;
  }
  return false;
}

/* ===================================================================== */

// Manage the prefetching stats when filling because of demand
void Cache::demandFillPrefStatsManaging(const int set, const int way)
{
  if (_prefetched.at(set).at(way)) {
    if (_successfulPrefetch.at(set).at(way)) _successfulPrefs++;
  }
  _prefetched.at(set).at(way) = false;
  _successfulPrefetch.at(set).at(way) = false;
}

/* ===================================================================== */

// Invalidate a block from the cache
void Cache::invalidateAddr(const UINT64 addr)
{
  int set = getSet(addr);
  for (int i = 0; i < _ways; i++) {
    if (_validBits.at(set).at(i) && (_tagStore.at(set).at(i) == getTag(addr))) {
      _lru.at(set)->invalidateWay(i);
      _validBits.at(set).at(i) = false;
      bool allValid = getSetInvalids(set) == 0;
      LRUcheck(set, allValid);
      return;
    }
  }
}

/* ===================================================================== */

// Check the LRU and exit if it is Wrong
void Cache::LRUcheck(const int set, const bool allValid) const
{
  if (!_lru.at(set)->checkLRU(allValid)) {
    cout << "lru check failled" << endl;
    exit(0);
  }
}

#endif

/* ===================================================================== */
/* eof */
/* ===================================================================== */
