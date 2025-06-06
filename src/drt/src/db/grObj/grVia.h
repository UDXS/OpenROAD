// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2019-2025, The OpenROAD Authors

#pragma once

#include <memory>

#include "db/grObj/grRef.h"
#include "db/tech/frViaDef.h"

namespace drt {

class grVia : public grRef
{
 public:
  // constructor
  grVia() = default;
  grVia(const grVia& in)
      : grRef(in),
        origin(in.origin),
        viaDef(in.viaDef),
        child(in.child),
        parent(in.parent),
        owner(in.owner)
  {
  }

  // getters
  const frViaDef* getViaDef() const { return viaDef; }

  // setters
  void setViaDef(const frViaDef* in) { viaDef = in; }

  // others
  frBlockObjectEnum typeId() const override { return grcVia; }

  /* from grRef
   * getOrient
   * setOrient
   * getOrigin
   * setOrigin
   * getTransform
   * setTransform
   */

  dbOrientType getOrient() const override { return dbOrientType(); }
  void setOrient(const dbOrientType& in) override { ; }
  Point getOrigin() const override { return origin; }
  void setOrigin(const Point& in) override { origin = in; }

  dbTransform getTransform() const override { return dbTransform(origin); }
  void setTransform(const dbTransform& in) override { ; }

  /* from gfrPinFig
   * hasPin
   * getPin
   * addToPin
   * removeFromPin
   */
  bool hasPin() const override
  {
    return (owner) && (owner->typeId() == grcPin);
  }
  grPin* getPin() const override { return reinterpret_cast<grPin*>(owner); }
  void addToPin(grPin* in) override
  {
    owner = reinterpret_cast<frBlockObject*>(in);
  }
  void removeFromPin() override { owner = nullptr; }

  /* from grConnFig
   * hasNet
   * getNet
   * hasGrNet
   * getGrNet
   * getChild
   * getParent
   * getGrChild
   * getGrParent
   * addToNet
   * removeFromNet
   * setChild
   * setParent
   */
  // if obj hasNet, then it is global GR net
  // if obj hasGrNet, then it is GR worker subnet
  bool hasNet() const override
  {
    return (owner) && (owner->typeId() == frcNet);
  }
  bool hasGrNet() const override
  {
    return (owner) && (owner->typeId() == grcNet);
  }
  frNet* getNet() const override { return reinterpret_cast<frNet*>(owner); }
  grNet* getGrNet() const override { return reinterpret_cast<grNet*>(owner); }
  frNode* getChild() const override { return reinterpret_cast<frNode*>(child); }
  frNode* getParent() const override
  {
    return reinterpret_cast<frNode*>(parent);
  }
  grNode* getGrChild() const override
  {
    return reinterpret_cast<grNode*>(child);
  }
  grNode* getGrParent() const override
  {
    return reinterpret_cast<grNode*>(parent);
  }
  void addToNet(frBlockObject* in) override { owner = in; }
  void removeFromNet() override { owner = nullptr; }
  void setChild(frBlockObject* in) override { child = in; }
  void setParent(frBlockObject* in) override { parent = in; }

  /* from grFig
   * getBBox
   * move
   * overlaps
   */

  Rect getBBox() const override { return Rect(origin, origin); }

  void setIter(frListIter<std::unique_ptr<grVia>>& in) { iter = in; }
  frListIter<std::unique_ptr<grVia>> getIter() const { return iter; }

 protected:
  Point origin;
  const frViaDef* viaDef{nullptr};
  frBlockObject* child{nullptr};
  frBlockObject* parent{nullptr};
  frBlockObject* owner{nullptr};
  frListIter<std::unique_ptr<grVia>> iter;
};
}  // namespace drt
