// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2025, The OpenROAD Authors

// Description: Primarily for maintaining the segments.

#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "dpl/Grid.h"
#include "journal.h"
#include "network.h"
#include "rectangle.h"
#include "utility.h"
namespace utl {
class Logger;
}  // namespace utl
namespace dpl {
class PlacementDRC;
}
namespace dpo {

class Architecture;
class DetailedSeg;
class Network;
class RoutingParams;
using dpl::Grid;
using dpl::PlacementDRC;

enum class BlockageType
{
  Placement,
  Routing,
  FixedInstance,
  None
};

struct Blockage
{
  double x_min;
  double x_max;
  int pad_left;
  int pad_right;
  BlockageType type;

  Blockage(double xmin, double xmax, int pad_l, int pad_r, BlockageType t)
      : x_min(xmin), x_max(xmax), pad_left(pad_l), pad_right(pad_r), type(t)
  {
  }
  double getXMin() const { return x_min; }
  double getXMax() const { return x_max; }
  double getPaddedXMin() const { return x_min - pad_left; }
  double getPaddedXMax() const { return x_max + pad_right; }
  bool isFixedInstance() const { return type == BlockageType::FixedInstance; }
  bool isPlacement() const { return type == BlockageType::Placement; }
};

////////////////////////////////////////////////////////////////////////////////
// Classes.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class DetailedMgr
{
 public:
  DetailedMgr(Architecture* arch,
              Network* network,
              RoutingParams* rt,
              Grid* grid,
              PlacementDRC* drc_engine);
  virtual ~DetailedMgr();

  void cleanup();

  Architecture* getArchitecture() const { return arch_; }
  Network* getNetwork() const { return network_; }
  RoutingParams* getRoutingParams() const { return rt_; }

  void setLogger(utl::Logger* logger) { logger_ = logger; }
  utl::Logger* getLogger() const { return logger_; }

  void setSeed(int s);

  void setMaxDisplacement(int x, int y);
  void setDisallowOneSiteGaps(bool disallowOneSiteGaps);
  void getMaxDisplacement(int& x, int& y) const
  {
    x = maxDispX_;
    y = maxDispY_;
  }
  int getMaxDisplacementX() const { return maxDispX_; }
  int getMaxDisplacementY() const { return maxDispY_; }
  bool getDisallowOneSiteGaps() const { return disallowOneSiteGaps_; }
  double measureMaximumDisplacement(u_int64_t& maxX,
                                    u_int64_t& maxY,
                                    int& violatedX,
                                    int& violatedY);

  void internalError(const std::string& msg);

  void setupObstaclesForDrc();

  void findBlockages(bool includeRouteBlockages = true);
  void findRegionIntervals(
      int regId,
      std::vector<std::vector<std::pair<double, double>>>& intervals);

  void findSegments();
  DetailedSeg* findClosestSegment(const Node* nd);
  void findClosestSpanOfSegmentsDfs(
      const Node* ndi,
      DetailedSeg* segPtr,
      double xmin,
      double xmax,
      int bot,
      int top,
      std::vector<DetailedSeg*>& stack,
      std::vector<std::vector<DetailedSeg*>>& candidates);
  bool findClosestSpanOfSegments(Node* nd, std::vector<DetailedSeg*>& segments);
  bool isInsideABlockage(const Node* nd, DbuX position);
  void assignCellsToSegments(const std::vector<Node*>& nodesToConsider);
  int checkOverlapInSegments();
  int checkEdgeSpacingInSegments();
  bool hasEdgeSpacingViolation(const Node* node) const;
  int checkSiteAlignment();
  int checkRowAlignment();
  int checkRegionAssignment();
  void getOneSiteGapViolationsPerSegment(
      std::vector<std::vector<int>>& violating_cells,
      bool fix_violations);
  bool fixOneSiteGapViolations(Node* cell,
                               int one_site_gap,
                               int segment,
                               Node* violatingNode);
  void removeCellFromSegment(const Node* nd, int seg);
  void addCellToSegment(Node* nd, int seg);
  double getCellSpacing(const Node* ndl,
                        const Node* ndr,
                        bool checkPinsOnCells);

  void collectSingleHeightCells();
  void collectMultiHeightCells();
  void collectFixedCells();
  void collectWideCells();

  void restoreOriginalPositions();
  void recordOriginalPositions();

  void resortSegments();
  void resortSegment(DetailedSeg* segPtr);
  void removeAllCellsFromSegments();

  int getNumSegments() const { return static_cast<int>(segments_.size()); }
  DetailedSeg* getSegment(int s) const { return segments_[s]; }
  int getNumSingleHeightRows() const { return numSingleHeightRows_; }
  int getSingleRowHeight() const { return singleRowHeight_; }

  int getNumCellsInSeg(int segId) const { return cellsInSeg_[segId].size(); }
  const std::vector<Node*>& getCellsInSeg(int segId) const
  {
    return cellsInSeg_[segId];
  }
  void addToFrontCellsInSeg(int segId, Node* node)
  {
    cellsInSeg_[segId].insert(cellsInSeg_[segId].begin(), node);
  }
  void addToBackCellsInSeg(int segId, Node* node)
  {
    cellsInSeg_[segId].push_back(node);
  }
  void popFrontCellsInSeg(int segId)
  {
    cellsInSeg_[segId].erase(cellsInSeg_[segId].begin());
  }
  void popBackCellsInSeg(int segId) { cellsInSeg_[segId].pop_back(); }
  void sortCellsInSeg(int segId)
  {
    std::sort(cellsInSeg_[segId].begin(),
              cellsInSeg_[segId].end(),
              DetailedMgr::compareNodesX());
  }
  void sortCellsInSeg(int segId, int start, int end)
  {
    std::sort(cellsInSeg_[segId].begin() + start,
              cellsInSeg_[segId].begin() + end,
              DetailedMgr::compareNodesX());
  }

  int getNumSegsInRow(int rowId) const { return segsInRow_[rowId].size(); }
  const std::vector<DetailedSeg*>& getSegsInRow(int rowId) const
  {
    return segsInRow_[rowId];
  }

  int getNumReverseCellToSegs(int nodeId) const
  {
    return reverseCellToSegs_[nodeId].size();
  }
  const std::vector<DetailedSeg*>& getReverseCellToSegs(int nodeId) const
  {
    return reverseCellToSegs_[nodeId];
  }

  const std::vector<Node*>& getSingleHeightCells() const
  {
    return singleHeightCells_;
  }

  int getNumMultiHeights() const { return multiHeightCells_.size(); }
  const std::vector<Node*>& getMultiHeightCells(int height) const
  {
    return multiHeightCells_[height];
  }
  const std::vector<Node*>& getWideCells() const { return wideCells_; }

  void shuffle(std::vector<Node*>& nodes);
  int getRandom(int limit) const { return (*rng_)() % limit; }

  void getSpaceAroundCell(int seg,
                          int ix,
                          DbuX& space,
                          DbuX& larger,
                          int limit = 3);
  void getSpaceAroundCell(int seg,
                          int ix,
                          DbuX& space_left,
                          DbuX& space_right,
                          DbuX& large_left,
                          DbuX& large_right,
                          int limit = 3);

  void removeSegmentOverlapSingle(int regId = -1);
  void removeSegmentOverlapSingleInner(std::vector<Node*>& nodes,
                                       int l,
                                       int r,
                                       int rowId);

  double getTargetUt() const { return targetUt_; }
  void setTargetUt(double ut) { targetUt_ = ut; }

  // Routines for generating moves and swaps.
  bool tryMove(Node* ndi, DbuX xi, DbuY yi, int si, DbuX xj, DbuY yj, int sj);
  bool trySwap(Node* ndi, DbuX xi, DbuY yi, int si, DbuX xj, DbuY yj, int sj);

  // For accepting or rejecting moves and swaps.
  void acceptMove();
  void rejectMove();

  // For help aligning cells to sites.
  bool alignPos(const Node* ndi, DbuX& xi, DbuX xl, DbuX xr);
  int getMoveLimit() { return moveLimit_; }
  void setMoveLimit(unsigned int newMoveLimit) { moveLimit_ = newMoveLimit; }

  // Journal operations
  const Journal& getJournal() const { return journal; }
  void eraseFromGrid(Node* node);
  void paintInGrid(Node* node);
  void undo(const JournalAction& action, bool positions_only = false);
  void redo(const JournalAction& action, bool positions_only = false);
  struct compareNodesX
  {
    // Needs cell centers.
    bool operator()(Node* p, Node* q) const
    {
      return p->getLeft().v + 0.5 * p->getWidth().v
             < q->getLeft().v + 0.5 * q->getWidth().v;
    }
    bool operator()(Node*& s, double i) const
    {
      return s->getLeft().v + 0.5 * s->getWidth().v < i;
    }
    bool operator()(double i, Node*& s) const
    {
      return i < s->getLeft().v + 0.5 * s->getWidth().v;
    }
  };

 private:
  struct compareBlockages
  {
    bool operator()(const Blockage& i1, const Blockage& i2) const
    {
      if (i1.getPaddedXMin() == i2.getPaddedXMin()) {
        return i1.getPaddedXMax() < i2.getPaddedXMax();
      }
      return i1.getPaddedXMin() < i2.getPaddedXMin();
    }
  };

  struct compareIntervals
  {
    bool operator()(std::pair<double, double> i1,
                    std::pair<double, double> i2) const
    {
      if (i1.first == i2.first) {
        return i1.second < i2.second;
      }
      return i1.first < i2.first;
    }
  };

  struct compareNodesL
  {
    bool operator()(Node* p, Node* q) const
    {
      return p->getLeft() < q->getLeft();
    }
  };

  // Different routines for trying moves and swaps.
  bool verifyMove();
  bool tryMove1(Node* ndi, DbuX xi, DbuY yi, int si, DbuX xj, DbuY yj, int sj);
  bool tryMove2(Node* ndi, DbuX xi, DbuY yi, int si, DbuX xj, DbuY yj, int sj);
  bool tryMove3(Node* ndi, DbuX xi, DbuY yi, int si, DbuX xj, DbuY yj, int sj);

  bool trySwap1(Node* ndi, DbuX xi, DbuY yi, int si, DbuX xj, DbuY yj, int sj);

  // Helper routines for making moves and swaps.
  bool shift(std::vector<Node*>& cells,
             std::vector<DbuX>& targetLeft,
             std::vector<DbuX>& posLeft,
             DbuX leftLimit,
             DbuX rightLimit,
             int segId,
             int rowId);
  bool shiftRightHelper(Node* ndi, DbuX xj, int sj, Node* ndr);
  bool shiftLeftHelper(Node* ndi, DbuX xj, int sj, Node* ndl);
  void getSpaceToLeftAndRight(int seg, int ix, double& left, double& right);

  // For composing list of cells for moves or swaps.
  void clearMoveList();
  bool addToMoveList(Node* ndi,
                     DbuX curLeft,
                     DbuY curBottom,
                     int curSeg,
                     DbuX newLeft,
                     DbuY newBottom,
                     int newSeg);
  bool addToMoveList(Node* ndi,
                     DbuX curLeft,
                     DbuY curBottom,
                     const std::vector<int>& curSegs,
                     DbuX newLeft,
                     DbuY newBottom,
                     const std::vector<int>& newSegs);

  // Standard stuff.
  Architecture* arch_;
  Network* network_;
  RoutingParams* rt_;
  Grid* grid_;
  PlacementDRC* drc_engine_;
  Journal journal;

  // For output.
  utl::Logger* logger_ = nullptr;

  // Info about rows.
  int numSingleHeightRows_;
  int singleRowHeight_;

  // Generic place for utilization.
  double targetUt_;

  // Target displacement limits.
  int maxDispX_;
  int maxDispY_;
  bool disallowOneSiteGaps_{false};
  std::vector<Node*> fixedCells_;  // Fixed; filler, macros, temporary, etc.

  // Blockages and segments.
  std::vector<std::vector<Blockage>> blockages_;
  std::vector<std::vector<Node*>> cellsInSeg_;
  std::vector<std::vector<DetailedSeg*>> segsInRow_;
  std::vector<DetailedSeg*> segments_;
  // size == #nodes
  std::vector<std::vector<DetailedSeg*>> reverseCellToSegs_;

  // For short and pin access stuff...
  std::vector<std::vector<std::vector<Rectangle>>> obstacles_;

  // Random number generator.
  std::unique_ptr<Placer_RNG> rng_;

  // Info about cells:
  // Single height cells.
  std::vector<Node*> singleHeightCells_;
  // Multi height cells by height.
  std::vector<std::vector<Node*>> multiHeightCells_;
  // Wide movable cells.  Can be single or multi height.
  std::vector<Node*> wideCells_;

  // Original cell positions.
  std::vector<DbuY> origBottom_;
  std::vector<DbuX> origLeft_;

  std::vector<Rectangle> boxes_;

  // For generating a move list... (size = moveLimit_)
  int moveLimit_;
};

}  // namespace dpo
