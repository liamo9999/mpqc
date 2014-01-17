//
// cadf_iters_impl.h
//
// Copyright (C) 2013 David Hollman
//
// Author: David Hollman
// Maintainer: DSH
// Created: Dec 18, 2013
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

#ifndef _chemistry_qc_scf_cadf_iters_impl_h
#define _chemistry_qc_scf_cadf_iters_impl_h

#include <util/misc/scexception.h>

#define out_assert(a, op, b) assert(a op b || ((std::cout << "Failed assertion output: " << #a << " ( = " << a << ") " << #op << " " << #b <<  " ( = " << b << ")" << std::endl), false))

namespace sc {

template<typename Range>
ShellBlockData<Range>::ShellBlockData(
    const ShellBlockSkeleton<Range>& sk
) : ShellBlockData<Range>(
      sk.shell_range,
      sk.nshell, sk.nbf,
      sk.restrictions
    )
{ }

//============================================================================//
// ShellData

inline ShellData
shell_data(
    GaussianBasisSet* basis,
    int ish, GaussianBasisSet* dfbasis
)
{
  return ShellData(ish, basis, dfbasis);
}

inline const BasisFunctionData
function_data(
    const Ref<GaussianBasisSet>& basis,
    int ish, const Ref<GaussianBasisSet>& dfbasis
)
{
  return BasisFunctionData(ish, basis, dfbasis);
}

//============================================================================//

template<typename Range>
inline void
ShellBlockData<Range>::init()
{
  bfoff = first_shell.bfoff;
  last_function = last_shell.last_function;
  if(restrictions & SameCenter) {
    center = first_shell.center;
    atom_bfoff = first_shell.atom_bfoff;
    atom_shoff = first_shell.atom_shoff;
    atom_nsh = first_shell.atom_nsh;
    atom_nbf = first_shell.atom_nbf;
    bfoff_in_atom = first_shell.bfoff_in_atom;
    shoff_in_atom = first_shell.shoff_in_atom;
    atom_last_function = last_shell.atom_last_function;
    atom_last_shell = last_shell.atom_last_function;
    if(dfbasis != 0){
      atom_dfshoff = first_shell.atom_dfshoff;
      atom_dfbfoff = first_shell.atom_dfbfoff;
      atom_dfnbf = first_shell.atom_dfnbf;
      atom_dfnsh = first_shell.atom_dfnsh;
      atom_df_last_function = last_shell.atom_df_last_function;
      atom_df_last_shell = last_shell.atom_df_last_shell;
    }
  }
}

template<typename ShellIterator>
inline void
shell_block_iterator<ShellIterator>::init()
{
  typedef range_of<ShellData, ShellIterator> ShellRange;
  //----------------------------------------//
  if(all_shells.begin() == all_shells.end()){
    const auto& begin = all_shells.begin();
    const auto& end = all_shells.end();
    current_skeleton = ShellBlockSkeleton<ShellRange>(
        shell_range(*(all_shells.begin()), *(all_shells.end())), 0, 0, restrictions
    );
    return;
  }
  //----------------------------------------//
  auto first_shell = *(all_shells.begin());
  int first_center = first_shell.center;
  auto first_am = basis->shell(first_shell).am();
  int block_nbf = 0;
  int block_nshell = 0;
  for(auto ish : all_shells){
    if(
        // Same center condition
        ((restrictions & SameCenter) and ish.center != first_center)
        or
        // Same angular momentum condition
        ((restrictions & SameAngularMomentum) and
            basis->shell(ish).am() != first_am) or
        // Maximum block size overflow condition
        (target_size != NoMaximumBlockSize and block_nbf >= target_size)
    ){
      // Store the current block
      break;
    }
    else{
      ++block_nshell;
      block_nbf += ish.nbf;
    }
  }
  //----------------------------------------//
  current_skeleton = ShellBlockSkeleton<ShellRange>(
      shell_range(first_shell, first_shell.iterator + block_nshell),
      block_nshell, block_nbf, restrictions
  );
  //----------------------------------------//
}


//============================================================================//

template<typename Iterator>
inline auto
shell_range(
    const ShellBlockData<Iterator>& block
) -> const decltype(block.shell_range)&
{
  return block.shell_range;
}

template<typename ShellRange>
inline range_of<BasisFunctionData>
function_range(
    const ShellBlockData<ShellRange>& block
)
{
  throw FeatureNotImplemented("function iteration over arbitrary shell block ranges", __FILE__, __LINE__);
}

template<>
inline range_of<BasisFunctionData>
function_range(
    const ShellBlockData<range_of<ShellData>>& block
)
{
  return function_range(block.basis, block.dfbasis, block.bfoff, block.last_function, block.bfoff);
}

//============================================================================//

} // end namespace sc


#endif /* _chemistry_qc_scf_cadf_iters_impl_h */
