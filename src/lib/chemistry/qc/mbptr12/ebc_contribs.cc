//
// ebc_contribs.cc
//
// Copyright (C) 2004 Edward Valeev
//
// Author: Edward Valeev <evaleev@vt.edu>
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

#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <mpqc_config.h>
#include <util/misc/formio.h>
#include <util/misc/regtime.h>
#include <util/class/class.h>
#include <util/state/state.h>
#include <util/state/state_text.h>
#include <util/state/state_bin.h>
#include <math/scmat/matrix.h>
#include <math/scmat/local.h>
#include <chemistry/molecule/molecule.h>
#include <chemistry/qc/basis/integral.h>
#include <math/distarray4/distarray4.h>
#include <chemistry/qc/mbptr12/r12wfnworld.h>
#include <math/mmisc/pairiter.h>
#include <chemistry/qc/mbptr12/r12int_eval.h>
#include <chemistry/qc/mbptr12/r12_amps.h>
#include <chemistry/qc/mbptr12/compute_tbint_tensor.h>
#include <math/scmat/util.h>
#include <chemistry/qc/mbptr12/creator.h>
#include <chemistry/qc/mbptr12/container.h>
#include <chemistry/qc/lcao/utils.h>
#include <chemistry/qc/lcao/utils.impl.h>
#include <util/misc/print.h>

using namespace std;
using namespace sc;

#define TEST_T2 0
#define TEST_A 0
// if set to 1 then use f+k rather than f to compute A
#define A_DIRECT_EXCLUDE_K 0

void R12IntEval::compute_A() {
  Ref<SCMatrixKit> local_matrix_kit = new LocalSCMatrixKit;
  for (int s = 0; s < nspincases2(); s++) {
    const SpinCase2 spincase2 = static_cast<SpinCase2> (s);
    const SpinCase1 spin1 = case1(spincase2);
    const SpinCase1 spin2 = case2(spincase2);

    Ref<OrbitalSpace> vir1_act = vir_act(spin1);
    Ref<OrbitalSpace> vir2_act = vir_act(spin2);
    Ref<OrbitalSpace> fvir1_act = F_a_A(spin1);
    Ref<OrbitalSpace> fvir2_act = F_a_A(spin2);
    const Ref<OrbitalSpace>& GG1space = GGspace(spin1);
    const Ref<OrbitalSpace>& GG2space = GGspace(spin2);

    const Ref<RefWavefunction> refinfo = r12world()->refwfn();

    A_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_vv_[s]);
    A_[s].assign(0.0);

    compute_A_direct_(A_[s], GG1space, vir1_act, GG2space, vir2_act, fvir1_act,
                      fvir2_act, spincase2 != AlphaBeta);
  }
  if (!spin_polarized()) A_[BetaBeta] = A_[AlphaAlpha];
}


void
R12IntEval::compute_A_direct_(RefSCMatrix& A,
                              const Ref<OrbitalSpace>& space1,
                              const Ref<OrbitalSpace>& space2,
                              const Ref<OrbitalSpace>& space3,
                              const Ref<OrbitalSpace>& space4,
                              const Ref<OrbitalSpace>& fspace2,
                              const Ref<OrbitalSpace>& fspace4,
                              bool antisymmetrize)
{
  // make sure that I have electrons for both spins
  if (space1->rank() == 0 || space3->rank() == 0)
    return;

  // are particles 1 and 2 equivalent?
  const bool part1_equiv_part2 = (space1==space3 && space2 == space4);

  Timer tim_A_direct("A intermediate (direct)");
  std::ostringstream oss;
  oss << "<" << space1->id() << " " << space3->id() << "|A|"
      << space2->id() << " " << space4->id() << ">";
  const std::string label = oss.str();
  ExEnv::out0() << endl << indent
                << "Entered \"direct\" A intermediate (" << label << ") evaluator" << endl
                << incindent;
  //
  // ij|A|kl = ij|f12|kl_f, symmetrized if part1_equiv_part2
  //
  std::vector<std::string> tform4f_keys; // get 1 3 |F12| 2 4_f
  {
    R12TwoBodyIntKeyCreator tform_creator(moints_runtime4(),
      space1,
      space2,
      space3,
      fspace4,
      corrfactor(),
      true);
      fill_container(tform_creator,tform4f_keys);
  }
  compute_F12_(A,space1,space2,space3,fspace4,antisymmetrize,tform4f_keys);
  if (part1_equiv_part2) {
    // no need to symmetrize if computing antisymmetric matrix -- compute_tbint_tensor takes care of that
    if (!antisymmetrize)
      symmetrize<false>(A,A,space1,space2);
    A.scale(2.0);
  }
  else {
    std::vector<std::string> tform2f_keys;
    {
      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),
        space1,
        fspace2,
        space3,
        space4,
        corrfactor(),
        true);
        fill_container(tformkey_creator,tform2f_keys);
    }
    compute_F12_(A,space1,fspace2,space3,space4,antisymmetrize,tform2f_keys);
  }

  ExEnv::out0() << decindent << indent << "Exited \"direct\" A intermediate (" << label << ") evaluator" << endl;
}

void
R12IntEval::AT2_contrib_to_V_()
{
  if (evaluated_)
    return;

    for(unsigned int s=0; s<nspincases2(); s++) {
      SpinCase2 spin = static_cast<SpinCase2>(s);

      RefSCMatrix V = A_[s] * amps()->T2(spin).t();

      std::string label = prepend_spincase(spin,"AT2 contribution to V");
      if (debug_ >= DefaultPrintThresholds::O4) {
        V.print(label.c_str());
      }
      if (debug_ >= DefaultPrintThresholds::mostN0)
        print_scmat_norms(V,label.c_str());
      if (r12world()->world()->msg()->me() == 0)
        V_[s].accumulate(V);
    }

  globally_sum_intermeds_();
}

void
R12IntEval::AF12_contrib_to_B_()
{
  if (evaluated_)
    return;
  if (r12world()->world()->msg()->me() == 0) {
    for(unsigned int s=0; s<nspincases2(); s++) {
      SpinCase2 spin = static_cast<SpinCase2>(s);

      RefSCMatrix AF = A_[s] * amps()->Fvv(spin).t();

      // B^{EBC} implies summation over all ab, not just unique ones, hence a factor of 2
      const double spin_pfac = 2.0;
      // minus 1/2 for Symmetrize AND -1/2 factor in B^{EBC} expression: B^{EBC} = -0.5 A . F12^t
      const double scale = -0.25 * spin_pfac;
      RefSCMatrix B = B_[s].clone();  B.assign(0.0);
      AF.scale(scale); B.accumulate(AF);
      RefSCMatrix AFt = AF.t();
      B.accumulate(AFt);

      const std::string label = prepend_spincase(spin,"B^{EBC} contribution");
      if (debug_ >= DefaultPrintThresholds::O4) {
        B.print(label.c_str());
      }
      B_[s].accumulate(B);
      if (debug_ >= DefaultPrintThresholds::mostN0) {
        print_scmat_norms(B,label.c_str());
      }
    }
  }
  globally_sum_intermeds_();
}

////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
