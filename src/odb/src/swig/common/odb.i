// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2019-2025, The OpenROAD Authors

%{
#define SWIG_FILE_WITH_INIT
#include "geom.h"
#include "db.h"
#include "dbShape.h"
#include "dbViaParams.h"
#include "dbWireCodec.h"
#include "dbBlockCallBackObj.h"
#include "dbIterator.h"
#include "dbTransform.h"
#include "dbWireGraph.h"
#include "dbMap.h"
#include "dbSet.h"
#include "dbTypes.h"
#include "geom.h"
#include "wOrder.h"
#include "util.h"

using namespace odb;
%}

%include <stl.i>
%include <typemaps.i>
%include <std_string.i>
%include <std_vector.i>
%include <std_pair.i>

%typemap(in) (uint) = (int);
%typemap(out) (uint) = (int);
%typemap(out) (uint64) = (long);
%typemap(out) (int64_t) = (long);
%apply int* OUTPUT {int* x, int* y, int& ext};

%ignore odb::dbTechLayerAntennaRule::pwl_pair;
%ignore odb::dbTechLayerAntennaRule::getDiffPAR() const;
%ignore odb::dbTechLayerAntennaRule::getDiffCAR() const;
%ignore odb::dbTechLayerAntennaRule::getDiffPSR() const;
%ignore odb::dbTechLayerAntennaRule::getDiffCSR() const;
%ignore odb::dbTechLayerAntennaRule::getAreaDiffReduce() const;

// Swig can't handle non-assignable types
%ignore odb::Point::get(Orientation2D orient) const;
%ignore odb::Rect::low(Orientation2D orient) const;
%ignore odb::Rect::high(Orientation2D orient) const;
%ignore odb::Rect::get(Orientation2D orient, Direction1D dir) const;
%ignore odb::Rect::set(Orientation2D orient, Direction1D dir, int value);
%ignore odb::Point::set(Orientation2D orient, int value);
%ignore odb::Rect::bloat(int margin, Orientation2D orient) const;

%ignore odb::dbGDSStructure::operator[];

%include "dbenums.i"
%include "parserenums.i"
%include "dbtypes.i"
%include "dbtypes_common.i"

%include "odb/geom.h"
%include "polygon.i"
%include "odb/db.h"

%include "dbhelpers.i"  

%rename(getPoint_ext) odb::dbWireDecoder::getPoint(int& x, int& y, int& ext) const;

%include "odb/dbViaParams.h"
%include "odb/dbWireCodec.h"
%include "odb/dbBlockCallBackObj.h"
%include "odb/dbIterator.h"
%include "odb/dbTransform.h"
%include "odb/dbWireGraph.h"
%include "odb/dbSet.h"
%include "odb/wOrder.h"

std::string generateMacroPlacementString(odb::dbBlock* block);

