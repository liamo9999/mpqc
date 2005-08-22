//
// vxb_eval_info.cc
//
// Copyright (C) 2003 Edward Valeev
//
// Author: Edward Valeev <edward.valeev@chemistry.gatech.edu>
// Maintainer: EV
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

#ifdef __GNUG__
#pragma implementation
#endif

#include <stdexcept>
#include <stdlib.h>
#include <util/misc/formio.h>
#include <util/ref/ref.h>
#include <util/misc/string.h>
#include <util/misc/scexception.h>
#include <chemistry/qc/basis/basis.h>
#include <chemistry/qc/basis/petite.h>
#include <chemistry/qc/mbptr12/mbptr12.h>
#include <chemistry/qc/mbptr12/vxb_eval_info.h>
#include <chemistry/qc/mbptr12/moindexspace.h>
#include <chemistry/qc/mbptr12/transform_factory.h>
#include <chemistry/qc/mbptr12/singlerefinfo.h>

using namespace std;
using namespace sc;

inline int max(int a,int b) { return (a > b) ? a : b;}

/*---------------
  R12IntEvalInfo
 ---------------*/
static ClassDesc R12IntEvalInfo_cd(
  typeid(R12IntEvalInfo),"R12IntEvalInfo",5,"virtual public SavableState",
  0, 0, create<R12IntEvalInfo>);

R12IntEvalInfo::R12IntEvalInfo(MBPT2_R12* mbptr12)
{
  // Default values
  memory_ = DEFAULT_SC_MEMORY;
  debug_ = 0;
  dynamic_ = false;
  print_percent_ = 10.0;

  wfn_ = mbptr12;
#if !USE_SINGLEREFINFO
  ref_ = mbptr12->ref();
  integral_ = mbptr12->integral();
  bs_ = mbptr12->basis();
#else
  refinfo_ = new SingleRefInfo(mbptr12->ref(),mbptr12->nfzcore(),mbptr12->nfzvirt());
#endif
  bs_aux_ = mbptr12->aux_basis();
  bs_vir_ = mbptr12->vir_basis();

  matrixkit_ = SCMatrixKit::default_matrixkit();
  mem_ = MemoryGrp::get_default_memorygrp();
  msg_ = MessageGrp::get_default_messagegrp();
  thr_ = ThreadGrp::get_default_threadgrp();

  integral()->set_basis(basis());
  Ref<PetiteList> plist = integral()->petite_list();
  RefSCDimension oso_dim = plist->SO_basisdim();

#if !USE_SINGLEREFINFO
  ndocc_ = 0;
  nsocc_ = 0;
  for (int i=0; i<oso_dim->n(); i++) {
    if (ref_->occupation(i) == 2.0) ndocc_++;
    if (ref_->occupation(i) == 1.0) nsocc_++;
  }
  nfzc_ = mbptr12->nfzcore();
  nfzv_ = mbptr12->nfzvirt();
#endif

  ints_method_ = mbptr12->r12ints_method();
  ints_file_ = mbptr12->r12ints_file();

#if !USE_SINGLEREFINFO
  eigen_();
#endif

  corrfactor_ = mbptr12->corrfactor();
  abs_method_ = mbptr12->abs_method();
  construct_ri_basis_(false);
  construct_orthog_vir_();

  tfactory_ = new MOIntsTransformFactory(integral(),refinfo()->orbs(Alpha));
  tfactory_->set_memory(memory_);
}

R12IntEvalInfo::R12IntEvalInfo(StateIn& si) : SavableState(si)
{
  wfn_ = require_dynamic_cast<Wavefunction*>(SavableState::restore_state(si),
					     "R12IntEvalInfo::R12IntEvalInfo");

#if !USE_SINGLEREFINFO
  ref_ << SavableState::restore_state(si);
  integral_ << SavableState::restore_state(si);
  bs_ << SavableState::restore_state(si);
#endif
  bs_aux_ << SavableState::restore_state(si);
  bs_vir_ << SavableState::restore_state(si);
  bs_ri_ << SavableState::restore_state(si);

  matrixkit_ = SCMatrixKit::default_matrixkit();
  mem_ = MemoryGrp::get_default_memorygrp();
  msg_ = MessageGrp::get_default_messagegrp();
  thr_ = ThreadGrp::get_default_threadgrp();

#if !USE_SINGLEREFINFO
  si.get(ndocc_);
  si.get(nsocc_);
  si.get(nfzc_);
#endif

  int ints_method; si.get(ints_method); ints_method_ = (StoreMethod) ints_method;
  si.getstring(ints_file_);

  double memory; si.get(memory); memory_ = (size_t) memory;
  si.get(debug_);
  int dynamic; si.get(dynamic); dynamic_ = (bool) dynamic;

  if (si.version(::class_desc<R12IntEvalInfo>()) >= 2) {
    si.get(print_percent_);
  }

  if (si.version(::class_desc<R12IntEvalInfo>()) >= 3) {
    int absmethod; si.get(absmethod); abs_method_ = (LinearR12::ABSMethod) absmethod;
  }

  if (si.version(::class_desc<R12IntEvalInfo>()) >= 4) {
#if !USE_SINGLEREFINFO
    obs_space_ << SavableState::restore_state(si);
#endif
    abs_space_ << SavableState::restore_state(si);
    ribs_space_ << SavableState::restore_state(si);
#if !USE_SINGLEREFINFO
    act_occ_space_ << SavableState::restore_state(si);
    occ_space_ << SavableState::restore_state(si);
    occ_space_symblk_ << SavableState::restore_state(si);
#endif
    vir_act_ << SavableState::restore_state(si);
    vir_ << SavableState::restore_state(si);
    vir_sb_ << SavableState::restore_state(si);
    tfactory_ << SavableState::restore_state(si);
  }
  
#if USE_SINGLEREFINFO
  if (si.version(::class_desc<R12IntEvalInfo>()) >= 5) {
    refinfo_ << SavableState::restore_state(si);
  }
#endif

#if !USE_SINGLEREFINFO
  eigen_();
#endif
}

R12IntEvalInfo::~R12IntEvalInfo()
{
  free(ints_file_);
}

void R12IntEvalInfo::save_data_state(StateOut& so)
{
  SavableState::save_state(wfn_,so);
#if !USE_SINGLEREFINFO
  SavableState::save_state(ref_.pointer(),so);
  SavableState::save_state(integral_.pointer(),so);
  SavableState::save_state(bs_.pointer(),so);
#endif
  SavableState::save_state(bs_aux_.pointer(),so);
  SavableState::save_state(bs_vir_.pointer(),so);
  SavableState::save_state(bs_ri_.pointer(),so);

#if !USE_SINGLEREFINFO
  so.put(ndocc_);
  so.put(nsocc_);
  so.put(nfzc_);
#endif

  so.put((int)ints_method_);
  so.putstring(ints_file_);

  so.put((double)memory_);
  so.put(debug_);
  so.put((int)dynamic_);
  so.put(print_percent_);
  so.put((int)abs_method_);
  
#if !USE_SINGLEREFINFO
  SavableState::save_state(obs_space_.pointer(),so);
#endif
  SavableState::save_state(abs_space_.pointer(),so);
  SavableState::save_state(ribs_space_.pointer(),so);
#if !USE_SINGLEREFINFO
  SavableState::save_state(act_occ_space_.pointer(),so);
  SavableState::save_state(occ_space_.pointer(),so);
  SavableState::save_state(occ_space_symblk_.pointer(),so);
#endif
  SavableState::save_state(vir_act_.pointer(),so);
  SavableState::save_state(vir_.pointer(),so);
  SavableState::save_state(vir_sb_.pointer(),so);
  SavableState::save_state(tfactory_.pointer(),so);
  
#if USE_SINGLEREFINFO
  SavableState::save_state(refinfo_.pointer(),so);
#endif
}

#if USE_SINGLEREFINFO
const Ref<SingleRefInfo>&
R12IntEvalInfo::refinfo() const {
  return refinfo_;
}
#endif

char* R12IntEvalInfo::ints_file() const
{
  return strdup(ints_file_);
}

void
R12IntEvalInfo::set_memory(const size_t memory)
{
  if (memory > 0)
    memory_ = memory;
  tfactory_->set_memory(memory_);
}


void
R12IntEvalInfo::set_absmethod(LinearR12::ABSMethod abs_method)
{
  if (abs_method != abs_method_) {
    abs_method = abs_method_;
    construct_ri_basis_(false);
  }
}

#if !USE_SINGLEREFINFO
void R12IntEvalInfo::eigen_()
{
  Ref<Molecule> molecule = bs_->molecule();
  Ref<SCMatrixKit> so_matrixkit = bs_->so_matrixkit();
  Ref<PetiteList> plist = ref_->integral()->petite_list();
  RefSCDimension oso_dim = plist->SO_basisdim();

  if (debug_) ExEnv::out0() << indent << "R12IntEvalInfo: eigen_" << endl;
  if (debug_) ExEnv::out0() << indent << "getting fock matrix" << endl;
  // get the closed shell AO fock matrices -- this is where SCF is computed
  RefSymmSCMatrix fock_c_so = ref_->fock(0);
  ExEnv::out0() << endl;
  
  // transform the AO fock matrices to the MO basis
  RefSymmSCMatrix fock_c_mo1 = so_matrixkit->symmmatrix(oso_dim);
  RefSCMatrix vecs_so_mo1 = ref_->eigenvectors();
  
  fock_c_mo1.assign(0.0);
  fock_c_mo1.accumulate_transform(vecs_so_mo1.t(), fock_c_so);
  fock_c_so = 0;
  
  if (debug_) ExEnv::out0() << indent << "diagonalizing" << endl;
  // diagonalize the fock matrix
  RefDiagSCMatrix vals = fock_c_mo1.eigvals();
  
  // compute the AO to new MO scf vector
  if (debug_) ExEnv::out0() << indent << "AO to MO" << endl;
  RefSCMatrix so_ao = plist->sotoao();
  RefSCMatrix vecs = so_ao.t() * vecs_so_mo1;

  mo_space_ = new MOIndexSpace("p(sym)","symmetry-blocked MOs", vecs, bs_, vals, 0, 0, MOIndexSpace::symmetry);
  obs_space_ = new MOIndexSpace("p","MOs sorted by energy", vecs, bs_, vals, 0, 0);
  occ_space_ = new MOIndexSpace("m","occupied MOs sorted by energy", vecs, bs_, vals, 0, mo_space_->rank() - ndocc());
  occ_space_symblk_ = new MOIndexSpace("m(sym)","occupied MOs symmetry-blocked", vecs, bs_, vals,
                                       0, mo_space_->rank() - ndocc(), MOIndexSpace::symmetry);

  if (nfzc_ == 0)
    act_occ_space_ = occ_space_;
  else
    act_occ_space_ = new MOIndexSpace("i","active occupied MOs sorted by energy", vecs, bs_, vals, nfzc_, mo_space_->rank() - ndocc());

  if (debug_) ExEnv::out0() << indent << "R12IntEvalInfo: eigen_ done" << endl;
}
#endif

void
R12IntEvalInfo::throw_if_spin_polarized() const
{
  if (refinfo()->ref()->spin_polarized())
    throw ProgrammingError("R12IntEvalInfo -- spin-independent space is requested but the reference function is spin-polarized",__FILE__,__LINE__);
}

void
R12IntEvalInfo::SpinSpaces::init(const Ref<SingleRefInfo>& refinfo, const SpinCase1& spincase)
{
  vir_sb_ = refinfo->uocc_sb(spincase);
  vir_ = refinfo->uocc(spincase);
  vir_act_ = refinfo->uocc_act(spincase);
}

void
R12IntEvalInfo::SpinSpaces::init(const Ref<SingleRefInfo>& refinfo,
                                 const SpinCase1& spincase,
                                 const Ref<MOIndexSpace>& vbs)
{
  std::string id, label;
  if (spincase == Alpha)
    id = "E(sym)";
  else
    id = "e(sym)";
  vir_sb_ = R12IntEvalInfo::orthog_comp(refinfo->occ_sb(spincase), vbs, "e(sym)", "VBS", refinfo->ref()->lindep_tol());
  // Design flaw!!! Need to compute Fock matrix right here but can't since Fock is built into R12IntEval
  // Need to move all relevant code outside of MBPT2-F12 code
  vir_ = vir_sb_;
  vir_act_ = vir_;
}

void
R12IntEvalInfo::construct_orthog_vir_()
{
  if (vir_.nonnull())
    return;
  
  const bool spin_polarized = refinfo()->ref()->spin_polarized();

  Ref<GaussianBasisSet> obs = refinfo()->ref()->basis();
  if (obs == bs_vir_) {
    if (!spin_polarized) {
      vir_ = refinfo()->uocc();
      vir_sb_ = refinfo()->uocc_sb();
      if (refinfo()->nfzv() == 0)
        vir_act_ = vir_;
      else
        vir_act_ = refinfo()->uocc_act();
    }
    vir_spaces_[Alpha].init(refinfo(),Alpha);
    vir_spaces_[Beta].init(refinfo(),Beta);
    nlindep_vir_ = 0;
  }
  else {
    if (refinfo()->nfzv() != 0)
      throw std::runtime_error("R12IntEvalInfo::construct_orthog_vir_() -- nfzv_ != 0 is not allowed yet");
    
    // This is a set of orthonormal functions that span VBS
    Ref<MOIndexSpace> vir_space = orthogonalize("e","VBS", bs_vir_, refinfo()->ref()->orthog_method(), refinfo()->ref()->lindep_tol(), nlindep_vir_);
    if (!spin_polarized) {
      // Now project out occupied MOs
      vir_sb_ = orthog_comp(refinfo()->docc_sb(), vir_space, "e(sym)", "VBS", refinfo()->ref()->lindep_tol());
      // Design flaw!!! Need to compute Fock matrix right here but can't since Fock is built into R12IntEval
      // Need to move all relevant code outside of MBPT2-F12 code
      if (refinfo()->nfzv() != 0)
        throw std::runtime_error("R12IntEvalInfo::construct_orthog_vir_() -- nfzv_ != 0 is not allowed yet");
      vir_ = vir_sb_;
      vir_act_ = vir_sb_;
    }
    vir_spaces_[Alpha].init(refinfo(),Alpha,vir_space);
    vir_spaces_[Beta].init(refinfo(),Beta,vir_space);
  }
}


/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
