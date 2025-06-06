// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2025, The OpenROAD Authors

#include "detailed_global.h"

#include <algorithm>
#include <boost/tokenizer.hpp>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "detailed_hpwl.h"
#include "detailed_manager.h"
#include "dpl/Objects.h"
#include "rectangle.h"
#include "utl/Logger.h"

namespace dpo {

using utl::DPO;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
DetailedGlobalSwap::DetailedGlobalSwap(Architecture* arch,
                                       Network* network,
                                       RoutingParams* rt)
    : DetailedGenerator("global swap"),
      mgr_(nullptr),
      arch_(arch),
      network_(network),
      rt_(rt),
      skipNetsLargerThanThis_(100),
      traversal_(0),
      attempts_(0),
      moves_(0),
      swaps_(0)
{
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
DetailedGlobalSwap::DetailedGlobalSwap()
    : DetailedGlobalSwap(nullptr, nullptr, nullptr)
{
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void DetailedGlobalSwap::run(DetailedMgr* mgrPtr, const std::string& command)
{
  // A temporary interface to allow for a string which we will decode to create
  // the arguments.
  boost::char_separator<char> separators(" \r\t\n;");
  boost::tokenizer<boost::char_separator<char>> tokens(command, separators);
  std::vector<std::string> args;
  for (const auto& token : tokens) {
    args.push_back(token);
  }
  run(mgrPtr, args);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void DetailedGlobalSwap::run(DetailedMgr* mgrPtr,
                             std::vector<std::string>& args)
{
  // Given the arguments, figure out which routine to run to do the reordering.

  mgr_ = mgrPtr;
  arch_ = mgr_->getArchitecture();
  network_ = mgr_->getNetwork();
  rt_ = mgr_->getRoutingParams();

  int passes = 1;
  double tol = 0.01;
  for (size_t i = 1; i < args.size(); i++) {
    if (args[i] == "-p" && i + 1 < args.size()) {
      passes = std::atoi(args[++i].c_str());
    } else if (args[i] == "-t" && i + 1 < args.size()) {
      tol = std::atof(args[++i].c_str());
    }
  }
  passes = std::max(passes, 1);
  tol = std::max(tol, 0.01);

  double last_hpwl, curr_hpwl, init_hpwl, hpwl_x, hpwl_y;

  curr_hpwl = Utility::hpwl(network_, hpwl_x, hpwl_y);
  init_hpwl = curr_hpwl;
  for (int p = 1; p <= passes; p++) {
    last_hpwl = curr_hpwl;

    // XXX: Actually, global swapping is nothing more than random
    // greedy improvement in which the move generating is done
    // using this object to generate a target which is the optimal
    // region for each candidate cell.
    globalSwap();

    curr_hpwl = Utility::hpwl(network_, hpwl_x, hpwl_y);

    mgr_->getLogger()->info(
        DPO, 306, "Pass {:3d} of global swaps; hpwl is {:.6e}.", p, curr_hpwl);

    if (std::fabs(curr_hpwl - last_hpwl) / last_hpwl <= tol) {
      break;
    }
  }
  double curr_imp = (((init_hpwl - curr_hpwl) / init_hpwl) * 100.);
  mgr_->getLogger()->info(DPO,
                          307,
                          "End of global swaps; objective is {:.6e}, "
                          "improvement is {:.2f} percent.",
                          curr_hpwl,
                          curr_imp);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void DetailedGlobalSwap::globalSwap()
{
  // Nothing for than random greedy improvement with only a hpwl objective
  // and done such that every candidate cell is considered once!!!

  traversal_ = 0;
  edgeMask_.resize(network_->getNumEdges());
  std::fill(edgeMask_.begin(), edgeMask_.end(), 0);

  mgr_->resortSegments();

  // Get candidate cells.
  std::vector<Node*> candidates = mgr_->getSingleHeightCells();
  mgr_->shuffle(candidates);

  // Wirelength objective.
  DetailedHPWL hpwlObj(network_);
  hpwlObj.init(mgr_, nullptr);  // Ignore orientation.

  double currHpwl = hpwlObj.curr();
  double nextHpwl = 0.;
  // Consider each candidate cell once.
  for (auto ndi : candidates) {
    if (!generate(ndi)) {
      continue;
    }

    double delta = hpwlObj.delta(mgr_->getJournal());

    nextHpwl = currHpwl - delta;  // -delta is +ve is less.

    if (nextHpwl <= currHpwl) {
      mgr_->acceptMove();
      currHpwl = nextHpwl;
    } else {
      mgr_->rejectMove();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool DetailedGlobalSwap::getRange(Node* nd, Rectangle& nodeBbox)
{
  // Determines the median location for a node.

  Edge* ed;
  unsigned mid;

  Pin* pin;
  unsigned t = 0;

  double xmin = arch_->getMinX();
  double xmax = arch_->getMaxX();
  double ymin = arch_->getMinY();
  double ymax = arch_->getMaxY();

  xpts_.clear();
  ypts_.clear();
  for (int n = 0; n < nd->getNumPins(); n++) {
    pin = nd->getPins()[n];

    ed = pin->getEdge();

    nodeBbox.reset();

    int numPins = ed->getNumPins();
    if (numPins <= 1) {
      continue;
    }
    if (numPins > skipNetsLargerThanThis_) {
      continue;
    }
    if (!calculateEdgeBB(ed, nd, nodeBbox)) {
      continue;
    }

    // We've computed an interval for the pin.  We need to alter it to work for
    // the cell center. Also, we need to avoid going off the edge of the chip.
    nodeBbox.set_xmin(
        std::min(std::max(xmin, nodeBbox.xmin() - pin->getOffsetX().v), xmax));
    nodeBbox.set_xmax(
        std::max(std::min(xmax, nodeBbox.xmax() - pin->getOffsetX().v), xmin));
    nodeBbox.set_ymin(
        std::min(std::max(ymin, nodeBbox.ymin() - pin->getOffsetY().v), ymax));
    nodeBbox.set_ymax(
        std::max(std::min(ymax, nodeBbox.ymax() - pin->getOffsetY().v), ymin));

    // Record the location and pin offset used to generate this point.

    xpts_.push_back(nodeBbox.xmin());
    xpts_.push_back(nodeBbox.xmax());

    ypts_.push_back(nodeBbox.ymin());
    ypts_.push_back(nodeBbox.ymax());

    ++t;
    ++t;
  }

  // If, for some weird reason, we didn't find anything connected, then
  // return false to indicate that there's nowhere to move the cell.
  if (t <= 1) {
    return false;
  }

  // Get the median values.
  mid = t >> 1;

  std::sort(xpts_.begin(), xpts_.end());
  std::sort(ypts_.begin(), ypts_.end());

  nodeBbox.set_xmin(xpts_[mid - 1]);
  nodeBbox.set_xmax(xpts_[mid]);

  nodeBbox.set_ymin(ypts_[mid - 1]);
  nodeBbox.set_ymax(ypts_[mid]);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool DetailedGlobalSwap::calculateEdgeBB(Edge* ed, Node* nd, Rectangle& bbox)
{
  // Computes the bounding box of an edge.  Node 'nd' is the node to SKIP.
  double curX, curY;

  bbox.reset();

  int count = 0;
  for (Pin* pin : ed->getPins()) {
    auto other = pin->getNode();
    if (other == nd) {
      continue;
    }
    curX = other->getLeft().v + 0.5 * other->getWidth().v + pin->getOffsetX().v;
    curY = other->getBottom().v + 0.5 * other->getHeight().v
           + pin->getOffsetY().v;

    bbox.set_xmin(std::min(curX, bbox.xmin()));
    bbox.set_xmax(std::max(curX, bbox.xmax()));
    bbox.set_ymin(std::min(curY, bbox.ymin()));
    bbox.set_ymax(std::max(curY, bbox.ymax()));

    ++count;
  }

  return (count == 0) ? false : true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
double DetailedGlobalSwap::delta(Node* ndi, double new_x, double new_y)
{
  // Compute change in wire length for moving node to new position.

  double old_wl = 0.;
  double new_wl = 0.;
  double x, y;
  Rectangle old_box, new_box;

  ++traversal_;
  for (int pi = 0; pi < ndi->getPins().size(); pi++) {
    Pin* pini = ndi->getPins()[pi];

    Edge* edi = pini->getEdge();

    int npins = edi->getNumPins();
    if (npins <= 1 || npins >= skipNetsLargerThanThis_) {
      continue;
    }
    if (edgeMask_[edi->getId()] == traversal_) {
      continue;
    }
    edgeMask_[edi->getId()] = traversal_;

    old_box.reset();
    new_box.reset();

    for (Pin* pinj : edi->getPins()) {
      auto ndj = pinj->getNode();

      x = ndj->getLeft().v + 0.5 * ndj->getWidth().v + pinj->getOffsetX().v;
      y = ndj->getBottom().v + 0.5 * ndj->getHeight().v + pinj->getOffsetY().v;

      old_box.addPt(x, y);

      if (ndj == ndi) {
        x = new_x + pinj->getOffsetX().v;
        y = new_y + pinj->getOffsetY().v;
      }

      new_box.addPt(x, y);
    }
    old_wl += old_box.getWidth() + old_box.getHeight();
    new_wl += new_box.getWidth() + new_box.getHeight();
  }
  return old_wl - new_wl;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
double DetailedGlobalSwap::delta(Node* ndi, Node* ndj)
{
  // Compute change in wire length for swapping the two nodes.

  double old_wl = 0.;
  double new_wl = 0.;
  double x, y;
  Rectangle old_box, new_box;
  Node* nodes[2];
  nodes[0] = ndi;
  nodes[1] = ndj;

  ++traversal_;
  for (int c = 0; c <= 1; c++) {
    Node* ndi = nodes[c];
    for (Pin* pini : ndi->getPins()) {
      Edge* edi = pini->getEdge();

      int npins = edi->getNumPins();
      if (npins <= 1 || npins >= skipNetsLargerThanThis_) {
        continue;
      }
      if (edgeMask_[edi->getId()] == traversal_) {
        continue;
      }
      edgeMask_[edi->getId()] = traversal_;

      old_box.reset();
      new_box.reset();

      for (Pin* pinj : edi->getPins()) {
        auto ndj = pinj->getNode();

        x = ndj->getLeft().v + 0.5 * ndj->getWidth().v + pinj->getOffsetX().v;
        y = ndj->getBottom().v + 0.5 * ndj->getHeight().v
            + pinj->getOffsetY().v;

        old_box.addPt(x, y);

        if (ndj == nodes[0]) {
          ndj = nodes[1];
        } else if (ndj == nodes[1]) {
          ndj = nodes[0];
        }

        x = ndj->getLeft().v + 0.5 * ndj->getWidth().v + pinj->getOffsetX().v;
        y = ndj->getBottom().v + 0.5 * ndj->getHeight().v
            + pinj->getOffsetY().v;

        new_box.addPt(x, y);
      }

      old_wl += old_box.getWidth() + old_box.getHeight();
      new_wl += new_box.getWidth() + new_box.getHeight();
    }
  }
  return old_wl - new_wl;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool DetailedGlobalSwap::generate(Node* ndi)
{
  double yi = ndi->getBottom().v + 0.5 * ndi->getHeight().v;
  double xi = ndi->getLeft().v + 0.5 * ndi->getWidth().v;

  // Determine optimal region.
  Rectangle_d bbox;
  if (!getRange(ndi, bbox)) {
    // Failed to find an optimal region.
    return false;
  }
  if (xi >= bbox.xmin() && xi <= bbox.xmax() && yi >= bbox.ymin()
      && yi <= bbox.ymax()) {
    // If cell inside box, do nothing.
    return false;
  }

  // Observe displacement limit.  I suppose there are options.
  // If we cannot move into the optimal region, we could try
  // to move closer to it.  Or, we could just reject if we cannot
  // get into the optimal region.
  int dispX, dispY;
  mgr_->getMaxDisplacement(dispX, dispY);
  Rectangle_d lbox(ndi->getLeft().v - dispX,
                   ndi->getBottom().v - dispY,
                   ndi->getLeft().v + dispX,
                   ndi->getBottom().v + dispY);
  if (lbox.xmax() <= bbox.xmin()) {
    bbox.set_xmin(ndi->getLeft().v);
    bbox.set_xmax(lbox.xmax());
  } else if (lbox.xmin() >= bbox.xmax()) {
    bbox.set_xmin(lbox.xmin());
    bbox.set_xmax(ndi->getLeft().v);
  } else {
    bbox.set_xmin(std::max(bbox.xmin(), lbox.xmin()));
    bbox.set_xmax(std::min(bbox.xmax(), lbox.xmax()));
  }
  if (lbox.ymax() <= bbox.ymin()) {
    bbox.set_ymin(ndi->getBottom().v);
    bbox.set_ymax(lbox.ymax());
  } else if (lbox.ymin() >= bbox.ymax()) {
    bbox.set_ymin(lbox.ymin());
    bbox.set_ymax(ndi->getBottom().v);
  } else {
    bbox.set_ymin(std::max(bbox.ymin(), lbox.ymin()));
    bbox.set_ymax(std::min(bbox.ymax(), lbox.ymax()));
  }

  if (mgr_->getNumReverseCellToSegs(ndi->getId()) != 1) {
    return false;
  }
  int si = mgr_->getReverseCellToSegs(ndi->getId())[0]->getSegId();

  // Position target so center of cell at center of box.
  DbuX xj{(int) std::floor(0.5 * (bbox.xmin() + bbox.xmax())
                           - 0.5 * ndi->getWidth().v)};
  DbuY yj{(int) std::floor(0.5 * (bbox.ymin() + bbox.ymax())
                           - 0.5 * ndi->getHeight().v)};

  // Row and segment for the destination.
  int rj = arch_->find_closest_row(yj);
  yj = DbuY{arch_->getRow(rj)->getBottom()};  // Row alignment.
  int sj = -1;
  for (int s = 0; s < mgr_->getNumSegsInRow(rj); s++) {
    DetailedSeg* segPtr = mgr_->getSegsInRow(rj)[s];
    if (xj >= segPtr->getMinX() && xj <= segPtr->getMaxX()) {
      sj = segPtr->getSegId();
      break;
    }
  }
  if (sj == -1) {
    return false;
  }
  if (ndi->getGroupId() != mgr_->getSegment(sj)->getRegId()) {
    return false;
  }

  if (mgr_->tryMove(ndi, ndi->getLeft(), ndi->getBottom(), si, xj, yj, sj)) {
    ++moves_;
    return true;
  }
  if (mgr_->trySwap(ndi, ndi->getLeft(), ndi->getBottom(), si, xj, yj, sj)) {
    ++swaps_;
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void DetailedGlobalSwap::init(DetailedMgr* mgr)
{
  mgr_ = mgr;
  arch_ = mgr->getArchitecture();
  network_ = mgr->getNetwork();
  rt_ = mgr->getRoutingParams();

  traversal_ = 0;
  edgeMask_.resize(network_->getNumEdges());
  std::fill(edgeMask_.begin(), edgeMask_.end(), 0);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
bool DetailedGlobalSwap::generate(DetailedMgr* mgr,
                                  std::vector<Node*>& candidates)
{
  ++attempts_;

  mgr_ = mgr;
  arch_ = mgr->getArchitecture();
  network_ = mgr->getNetwork();
  rt_ = mgr->getRoutingParams();

  Node* ndi = candidates[mgr_->getRandom(candidates.size())];

  return generate(ndi);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void DetailedGlobalSwap::stats()
{
  mgr_->getLogger()->info(
      DPO,
      334,
      "Generator {:s}, "
      "Cumulative attempts {:d}, swaps {:d}, moves {:5d} since last reset.",
      getName().c_str(),
      attempts_,
      swaps_,
      moves_);
}

}  // namespace dpo
