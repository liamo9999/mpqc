//
// linkage.h
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@ca.sandia.gov>
// Maintainer: LPS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifndef _chemistry_molecule_linkage_h
#define _chemistry_molecule_linkage_h

#ifndef __PIC__

#include <chemistry/molecule/coor.h>
#include <chemistry/molecule/taylor.h>
#include <chemistry/molecule/molfreq.h>
#include <chemistry/molecule/molrender.h>
#include <chemistry/molecule/molshape.h>

#include <util/render/linkage.h>
#include <math/scmat/linkage.h>
#include <math/optimize/linkage.h>

const ClassDesc &molecule_force_link_a_ = RedundMolecularCoor::class_desc_;
const ClassDesc &molecule_force_link_b_ = CartMolecularCoor::class_desc_;
const ClassDesc &molecule_force_link_c_ = SymmMolecularCoor::class_desc_;
const ClassDesc &molecule_force_link_d_ = TaylorMolecularEnergy::class_desc_;
const ClassDesc &molecule_force_link_e_ = MolecularFrequencies::class_desc_;
const ClassDesc &molecule_force_link_f_ = RenderedStickMolecule::class_desc_;
const ClassDesc &molecule_force_link_g_ = RenderedBallMolecule::class_desc_;
const ClassDesc &molecule_force_link_h_ =RenderedMolecularSurface::class_desc_;
const ClassDesc &molecule_force_link_i_ = VDWShape::class_desc_;
const ClassDesc &molecule_force_link_j_ = ConnollyShape::class_desc_;
const ClassDesc &molecule_force_link_k_ = ConnollyShape2::class_desc_;

#endif /* __PIC__ */

#endif
