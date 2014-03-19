//
// compute_k.cc
//
// Copyright (C) 2014 David Hollman
//
// Author: David Hollman
// Maintainer: DSH
// Created: Feb 13, 2014
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

#include <numeric>

#include <chemistry/qc/basis/petite.h>
#include <util/misc/xmlwriter.h>

#include "cadfclhf.h"

using namespace sc;
using std::cout;
using std::endl;



double CADFCLHF::get_distance_factor(
    const ShellData& ish, const ShellData& jsh, const ShellData& Xsh
) const
{
  double dist_factor;
  if(linK_use_distance_){
    const double R = get_R(ish, jsh, Xsh);
    const double l_X = double(dfbs_->shell(Xsh).am(0));
    double r_expon = l_X + 1.0;
    if(ish.center == jsh.center) {
      if(ish.center == Xsh.center) {
        r_expon = 0.0;
      }
      else {
        const double l_i = double(gbs_->shell(ish).am(0));
        const double l_j = double(gbs_->shell(jsh).am(0));
        r_expon += abs(l_i-l_j);
      }
    }
    dist_factor = 1.0 / (pow(R, pow(r_expon, distance_damping_factor_)));            //latex `\label{sc:link:dist_damp}`
    // If the distance factor actually makes the bound larger, then ignore it.
    dist_factor = std::min(1.0, dist_factor);
  }
  else {
    dist_factor = 1.0;
  }
  return dist_factor;
};


double CADFCLHF::get_R(
    const ShellData& ish,
    const ShellData& jsh,
    const ShellData& Xsh
) const
{
  double rv = (pair_centers_.at({(int)ish, (int)jsh}) - centers_[Xsh.center]).norm();
  if(use_extents_) {
    const double ext_a = pair_extents_.at({(int)ish, (int)jsh});
    const double ext_b = df_extents_[(int)Xsh];
    if(subtract_extents_) {
      rv -= ext_a + ext_b;
    }
    else if(ext_a + ext_b >= rv){
      rv = 1.0; // Don't do distance screening
    }
  }
  if(rv < 1.0) {
    rv = 1.0;
  }
  return rv;
};

typedef std::pair<int, int> IntPair;

RefSCMatrix
CADFCLHF::compute_K()
{
  /*=======================================================================================*/
  /* Setup                                                 		                        {{{1 */ #if 1 // begin fold
  //----------------------------------------//
  // Convenience variables
  Timer timer("compute K");
  const int me = scf_grp_->me();
  const int n_node = scf_grp_->n();
  const Ref<GaussianBasisSet>& obs = gbs_;
  const int nbf = obs->nbasis();
  const int dfnbf = dfbs_->nbasis();

  //----------------------------------------//
  // Get the density in an Eigen::Map form
  double *D_ptr = allocate<double>(nbf*nbf);
  D_.convert(D_ptr);
  typedef Eigen::Map<Eigen::VectorXd> VectorMap;
  typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> RowMatrix;
  typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor> ColMatrix;
  //typedef Eigen::Map<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> MatrixMap;
  typedef Eigen::Map<ColMatrix> MatrixMap;
  // Matrix and vector wrappers, for convenience
  VectorMap d(D_ptr, nbf*nbf);
  MatrixMap D(D_ptr, nbf, nbf);
  // Match density scaling in old code:
  D *= 0.5;
  //----------------------------------------//
  Eigen::MatrixXd Kt(nbf, nbf);
  Kt = Eigen::MatrixXd::Zero(nbf, nbf);
  //----------------------------------------//
  // Compute all of the integrals outside of
  //   the thread loop.  This will need to be
  //   rethought in HUGE cases, but for now
  //   it is not a limiting factor
  timer.enter("compute (X|Y)");
  auto g2_full_ptr = ints_to_eigen_threaded(
      ShellBlockData<>(dfbs_),
      ShellBlockData<>(dfbs_),
      eris_2c_, coulomb_oper_type_
  );
  const auto& g2 = *g2_full_ptr;
  timer.exit();
  //----------------------------------------//
  //----------------------------------------//
  /*****************************************************************************************/ #endif //1}}}
  /*=======================================================================================*/
  /* Make the CADF-LinK lists                                                         {{{1 */ #if 1 //latex `\label{sc:link}`

  std::vector<std::tuple<int, int, int>> L_3_keys;

  // Now make the linK lists if we're doing linK
  if(do_linK_){
    timer.enter("LinK lists");
    // First clear all of the lists
    L_D.clear();
    L_DC.clear();
    L_3.clear();
    //============================================================================//
    Eigen::MatrixXd D_frob(obs->nshell(), obs->nshell());
    do_threaded(nthread_, [&](int ithr){
      for(int lsh_index = ithr; lsh_index < obs->nshell(); lsh_index += nthread_) {           //latex `\label{sc:link:ld}`
        ShellData lsh(lsh_index, obs, dfbs_);
        for(auto&& jsh : shell_range(obs)) {
          double dnorm = D.block(lsh.bfoff, jsh.bfoff, lsh.nbf, jsh.nbf).norm();
          D_frob(lsh, jsh) = dnorm;
          L_D[lsh].insert(jsh, dnorm);
        }
        L_D[lsh].sort();
      }
    });
    // TODO Distribute over MPI processes as well
    //----------------------------------------//                                             //latex `\label{sc:link:setupend}`
    // Form L_DC

    timer.enter("build L_DC");
    for(auto&& jsh : shell_range(obs)) {                                                       //latex `\label{sc:link:ldc}`

      const auto& Drho = D_frob.row(jsh);

      for(auto&& Xsh : shell_range(dfbs_)) {
        auto& Cmaxes_X = Cmaxes_[Xsh];
        // TODO Optimize this to not be N^3

        double max_val = 0.0;

        if(use_norms_sigma_) {
          for(auto&& lsh : shell_range(obs)) {
            //const double this_val = Drho(lsh) * Cmaxes_X[lsh].value;
            //max_val += this_val * this_val;
            max_val += Drho(lsh) * Cmaxes_X[lsh].value;
          } // end loop over lsh
          //max_val = sqrt(max_val);
        }
        else {
          for(auto&& lsh : shell_range(obs)) {
            const double this_val = Drho(lsh) * Cmaxes_X[lsh].value;
            if(this_val > max_val) max_val = this_val;
          } // end loop over lsh
        }
        L_DC[jsh].insert(Xsh,                                                                //latex `\label{sc:link:ldcstore}`
            max_val * schwarz_df_[Xsh]
        );
      } // end loop over Xsh

      L_DC[jsh].sort();

    } // end loop over jsh

    //----------------------------------------//                                             //latex `\label{sc:link:ldc:end}`
    // Form L_3
    timer.change("build L_3");
    double epsilon, epsilon_dist;                                                            //latex `\label{sc:link:l3}`

    if(density_reset_){
      epsilon = full_screening_thresh_;
      epsilon_dist = distance_screening_thresh_;
    }
    else{
      epsilon = pow(full_screening_thresh_, full_screening_expon_);                          //latex `\label{sc:link:expon}`
      epsilon_dist = pow(distance_screening_thresh_, full_screening_expon_);
    }

    if(all_to_all_L_3_) {

      /*-----------------------------------------------------*/
      /* Old parallel L_3                               {{{2 */ #if 2 // begin fold

      do_threaded(nthread_, [&](int ithr){

        int thr_offset = me*nthread_ + ithr;
        int increment = n_node*nthread_;

        auto jsh_iter = shell_range(gbs_, dfbs_).begin();
        const auto& jsh_end = shell_range(gbs_, dfbs_).end();
        auto Xsh_iter = L_DC[*jsh_iter].begin();
        bool Xsh_done = false;
        const auto& advance_iters_rho_X = [this, &Xsh_done, &jsh_end](
            int amount,
            decltype(jsh_iter)& jsh_it,
            decltype(Xsh_iter)& Xsh_it
        ) -> bool {
          int incr_remain = amount;
          const auto& curr_end = L_DC[*jsh_it].end();
          int nremain = std::distance(Xsh_it, curr_end);
          // This might be inefficient if the shell iterators are not random access?
          if(Xsh_done) {
            if(nremain > amount) {
              // Skip to the last one we would have done
              nremain = nremain % amount;
            }
            // Otherwise, we're moving on already anyway
            Xsh_done = false;
          }
          bool jsh_changed = false;
          while(nremain <= incr_remain) {
            ++jsh_it;
            jsh_changed = true;
            if(jsh_it == jsh_end) break;
            incr_remain -= nremain;
            nremain = L_DC[*jsh_it].size();
          }
          if(jsh_it != jsh_end) {
            if(jsh_changed) Xsh_it = L_DC[*jsh_it].begin();
            std::advance(Xsh_it, incr_remain);
            return true;
          }
          else {
            return false;
          }
        };

        int advance_size = thr_offset;

        while(advance_iters_rho_X(advance_size, jsh_iter, Xsh_iter)) {
          advance_size = increment;
          const auto& jsh = *jsh_iter;
          const auto& Xsh = *Xsh_iter;
          auto& L_sch_jsh = L_schwarz[jsh];

          const double pf = Xsh.value;                                                       //latex `\label{sc:link:pf}`
          const double eps_prime = epsilon / pf;
          const double eps_prime_dist = epsilon_dist / pf;
          const double Xsh_schwarz = schwarz_df_[Xsh];
          bool jsh_added = false;
          for(auto ish : L_sch_jsh) {

            double dist_factor = get_distance_factor(ish, jsh, Xsh);
            assert(ish.value == schwarz_frob_(ish, jsh));

            if(ish.value > eps_prime) {
              jsh_added = true;
              if(!linK_use_distance_ or ish.value * dist_factor > eps_prime_dist) {
                L_3[{ish, Xsh}].insert(jsh,
                    ish.value * dist_factor * Xsh_schwarz
                );

                if(print_screening_stats_) {
                  ++iter_stats_->K_3c_needed;
                  iter_stats_->K_3c_needed_fxn += ish.nbf * jsh.nbf * Xsh.nbf;
                }

              }
              else if(print_screening_stats_ and linK_use_distance_) {
                ++iter_stats_->K_3c_dist_screened;
                iter_stats_->K_3c_dist_screened_fxn += ish.nbf * jsh.nbf * Xsh.nbf;
              }

            }
            else {
              break;
            }

          } // end loop over ish
          if(not jsh_added){
            Xsh_done = true;
          }

        }

      });

      /*-----------------------------------------------------*/
      /* Distribute L_3 (this is a mess)                {{{3 */ #if 3 // begin fold
      timer.change("distribute L_3");

      if(n_node > 1) {
        timer.enter("01 - build my L_3");
        // TODO use std::array here instead of tuple
        std::vector<std::tuple<int, int, int, int>> my_L_3_keys;
        for(auto&& L_3_list : L_3) {
          my_L_3_keys.emplace_back(
            me,
            L_3_list.first.first,
            L_3_list.first.second,
            L_3_list.second.size()
          );
        }

        // Get the number of L3 lists per node
        timer.change("02 - get n_l3_per_node");
        int n_l3_per_node[n_node];
        int ones[n_node];
        std::fill_n(ones, n_node, 1*sizeof(int));
        int L3size = L_3.size();
        scf_grp_->raw_collect(&L3size, (const int*)&ones, &n_l3_per_node);
        int total_n_l3 = 0;
        for(int i = 0; i < n_node; total_n_l3 += n_l3_per_node[i++]);

        // Now get all of the (node, ish, Xsh, size) lists
        timer.change("03 - distribute my_L3");
        int l3_idxs_sizes[total_n_l3*4];
        for(int i = 0; i < n_node; ++i) {
          n_l3_per_node[i] = 4*sizeof(int)*n_l3_per_node[i];
        }
        scf_grp_->raw_collect(my_L_3_keys.data(), (const int*)&n_l3_per_node, &l3_idxs_sizes);

        // Extract the size of each list and sum
        timer.change("04 - extract L3_node_sizes");
        std::map<std::pair<int, int>, int> L3_total_sizes;
        std::map<std::pair<int, int>, std::vector<int>> L3_node_counts;
        std::vector<int> full_sizes(n_node);
        int full_data_count = 0;
        for(int i = 0; i < total_n_l3; ++i) {
          int node_src, ish, Xsh, njsh;
          // This is disgusting
          std::tie(node_src, ish, Xsh, njsh) = *reinterpret_cast<std::tuple<int, int, int, int>*>(&(l3_idxs_sizes[0]) + 4*i);
          L3_total_sizes[{ish, Xsh}] += njsh;
          if(L3_node_counts.find({ish, Xsh}) == L3_node_counts.end()) {
            L3_node_counts.emplace(
              std::piecewise_construct,
              std::forward_as_tuple(ish, Xsh),
              std::forward_as_tuple(n_node)
            );
            assert((L3_node_counts[{ish, Xsh}].size() == n_node));
          }
          out_assert(node_src, <=, n_node);
          L3_node_counts[{ish, Xsh}][node_src] = njsh;
          full_sizes[node_src] += njsh * sizeof(ShellIndexWithValue);
          full_data_count += njsh;
        }

        for(auto&& pair : L3_total_sizes) {
          L_3_keys.emplace_back(
              pair.first.first,
              pair.first.second,
              pair.second
          );
        }

        timer.change("05 - sort L3_keys");
        std::sort(L_3_keys.begin(), L_3_keys.end(),
            [](const std::tuple<int, int, int>& a, const std::tuple<int, int, int>& b) {
              int isha, Xsha, sizea, ishb, Xshb, sizeb;
              std::tie(isha, Xsha, sizea) = a;
              std::tie(ishb, Xshb, sizeb) = b;
              if(sizea > sizeb) return true;
              else if(sizea == sizeb) {
                if(isha < ishb) return true;
                else if(isha == ishb) return Xsha < Xshb;
                else return false;
              }
              else return false;
            }
        );

        timer.change("06 - make L3 data containers");
        std::vector<ShellIndexWithValue> my_data(full_sizes[me]/sizeof(ShellIndexWithValue));
        int offset = 0;
        for(auto&& L_3_key : L_3_keys) {
          int ish, Xsh, size;
          Xsh = 0;
          std::tie(ish, Xsh, size) = L_3_key;
          auto& my_L3_part = L_3[{ish, Xsh}];
          const auto& unsrt = my_L3_part.unsorted_indices();
          if(unsrt.size() > 0) {
            std::copy(unsrt.begin(), unsrt.end(), my_data.begin()+offset);
          }
          offset += unsrt.size();
          my_L3_part.clear();
        }
        std::vector<ShellIndexWithValue> full_data(full_data_count);

        timer.change("07 - distribute each L_3");
        scf_grp_->raw_collect(
            &(my_data[0]),
            (const int*)full_sizes.data(),
            &(full_data[0])
        );
        std::vector<int> offsets;
        int offsum = 0;
        for(auto sz : full_sizes) {
          offsets.push_back(offsum/sizeof(ShellIndexWithValue));
          offsum += sz;
        }

        timer.change("08 - unpack full L_3");
        // TODO Thread this, if possible
        std::vector<int> offsets_within(n_node);
        for(auto&& L_3_key : L_3_keys) {
          int ish, Xsh, size;
          std::tie(ish, Xsh, size) = L_3_key;
          auto& my_L3_part = L_3[{ish, Xsh}];
          const auto& ndcnts = L3_node_counts[{ish, Xsh}];
          std::vector<ShellIndexWithValue> iX_list(size);
          int full_off = 0;

          for(int inode = 0; inode < n_node; ++inode) {
            const int icount = ndcnts[inode];
            if(icount > 0) {
              std::copy(
                  &(full_data[offsets[inode] + offsets_within[inode]]),
                  &(full_data[offsets[inode] + offsets_within[inode]]) + icount,
                  &(iX_list[full_off])
              );
              full_off += icount;
              offsets_within[inode] += icount;
            }
          }

          assert(full_off == size);
          my_L3_part.acquire_and_sort(&(iX_list[0]), size);
          my_L3_part.set_basis(gbs_, dfbs_);
        }

        timer.exit();
      }
      else {
        for(auto&& pair : L_3) {
          L_3_keys.emplace_back(
              pair.first.first,
              pair.first.second,
              pair.second.size()
          );
        }
      }

      /********************************************************/ #endif //3}}}
      /*-----------------------------------------------------*/

      /********************************************************/ #endif //2}}}
      /*-----------------------------------------------------*/

    } // end if all_to_all_L_3_  (old LinK lists parallelism)
    else {

      // Loop over all jsh
      const auto& const_loc_pairs = local_pairs_linK_;
      const auto& loc_pairs_end = const_loc_pairs.end();
      do_threaded(nthread_, [&](int ithr) {
        for(auto&& jsh : thread_over_range(shell_range(gbs_, dfbs_), ithr, nthread_)) {
          auto& L_sch_jsh = L_schwarz[jsh];
          for(auto&& Xsh : L_DC[jsh]) {

            const double pf = Xsh.value;
            const double eps_prime = epsilon / pf;
            const double eps_prime_dist = epsilon_dist / pf;
            const double Xsh_schwarz = schwarz_df_[Xsh];
            bool jsh_added = false;
            bool ish_found = false;
            for(auto&& ish : L_sch_jsh) {

              // TODO this can be improved by precomputing the (jsh, Xsh) pairs with non-empty local ish by making a map from local Xsh -> local ish list at the same time local_pairs_linK_ is created
              if(n_node == 1 or const_loc_pairs.find({(int)ish, (int)Xsh}) != const_loc_pairs.end()) {

                ish_found = true;

                double dist_factor = get_distance_factor(ish, jsh, Xsh);
                assert(ish.value == schwarz_frob_(ish, jsh));

                if(ish.value > eps_prime) {
                  jsh_added = true;
                  if(!linK_use_distance_ or ish.value * dist_factor > eps_prime_dist) {
                    L_3[{ish, Xsh}].insert(jsh,
                        ish.value * dist_factor * Xsh_schwarz
                    );

                    if(print_screening_stats_) {
                      ++iter_stats_->K_3c_needed;
                      iter_stats_->K_3c_needed_fxn += ish.nbf * jsh.nbf * Xsh.nbf;
                    }

                  }
                  else if(print_screening_stats_ and linK_use_distance_) {
                    ++iter_stats_->K_3c_dist_screened;
                    iter_stats_->K_3c_dist_screened_fxn += ish.nbf * jsh.nbf * Xsh.nbf;
                  }

                }
                else if(linK_break_early_) {
                  break;
                }

              }

            } // end loop over ish
            if(linK_break_early_ and ish_found and not jsh_added){
              break;
            }

          }

        }
      });

      for(auto&& pair : L_3) {
        L_3_keys.emplace_back(
            pair.first.first,
            pair.first.second,
            pair.second.size()
        );
      }

    }

    do_threaded(nthread_, [&](int ithr){
      auto L_3_iter = L_3.begin();
      const auto& L_3_end = L_3.end();
      L_3_iter.advance(ithr);
      while(L_3_iter != L_3_end) {
        if(not linK_block_rho_) L_3_iter->second.set_sort_by_value(false);
        L_3_iter->second.sort();
        L_3_iter.advance(nthread_);
      }
    });                                                                                      //latex `\label{sc:link:l3:end}`

    timer.exit();

    timer.exit("LinK lists");

  } // end if do_linK_

  /*****************************************************************************************/ #endif //1}}} //latex `\label{sc:link:end}`
  /*=======================================================================================*/
  /* Loop over local shell pairs for three body contributions                         {{{1 */ #if 1 //latex `\label{sc:k3b:begin}`

  {

    Timer timer("three body contributions");
    boost::mutex tmp_mutex;
    std::mutex L_3_mutex;
    auto L_3_key_iter = L_3_keys.begin();
    if(all_to_all_L_3_) L_3_key_iter += me;                                                         //latex `\label{sc:k3b:iterset}`
    boost::thread_group compute_threads;
    // reset the iteration over local pairs
    local_pairs_spot_ = 0;
    // Loop over number of threads
    MultiThreadTimer mt_timer("threaded part", nthread_);
    auto get_ish_Xblk_3 = [&](ShellData& ish, ShellBlockData<>& Xblk) -> bool {                                                //latex `\label{sc:k3b:getiX}`
      if(do_linK_) {
        std::lock_guard<std::mutex> lg(L_3_mutex);
        if(L_3_key_iter == L_3_keys.end()) {
          return false;
        }
        else {

          int ishidx, Xshidx, list_size;
          std::tie(ishidx, Xshidx, list_size) = *L_3_key_iter;
          ish = ShellData(ishidx, gbs_, dfbs_);
          Xblk = ShellBlockData<>(ShellData(Xshidx, dfbs_, gbs_));
          if(all_to_all_L_3_) {
            L_3_key_iter += std::min(
                L_3_keys.end() - L_3_key_iter,
                std::iterator_traits<decltype(L_3_key_iter)>::difference_type(n_node)
            );
          }
          else {
            ++L_3_key_iter;
          }
          return true;
        }
      }
      else {
        int spot = local_pairs_spot_++;
        if(spot < local_pairs_k_.size()){
          auto& sig_pair = local_pairs_k_[spot];
          ish = ShellData(sig_pair.first, gbs_, dfbs_);
          Xblk = sig_pair.second;
          return true;
        }
        else{
          return false;
        }
      }
    };                                                                                   //latex `\label{sc:k3b:getiXend}`

    for(int ithr = 0; ithr < nthread_; ++ithr) {
      // ...and create each thread that computes pairs
      compute_threads.create_thread([&,ithr](){                                              //latex `\label{sc:k3b:thrpatend}`

        Eigen::MatrixXd Kt_part(nbf, nbf);
        Kt_part = Eigen::MatrixXd::Zero(nbf, nbf);
        //----------------------------------------//
        ShellData ish;
        ShellBlockData<> Xblk;
        //----------------------------------------//                                         //latex `\label{sc:k3b:thrvars}`
        //----------------------------------------//
        while(get_ish_Xblk_3(ish, Xblk)) {                                                            //latex `\label{sc:k3b:while}`
          /*-----------------------------------------------------*/
          /* Compute B intermediate                         {{{2 */ #if 2 // begin fold      //latex `\label{sc:k3b:b}`
          mt_timer.enter("compute B", ithr);
          auto ints_timer = mt_timer.get_subtimer("compute ints", ithr);
          auto k2_part_timer = mt_timer.get_subtimer("k2 part", ithr);
          auto contract_timer = mt_timer.get_subtimer("contract", ithr);

          ColMatrix B_ish(ish.nbf * Xblk.nbf, nbf);
          B_ish = ColMatrix::Zero(ish.nbf * Xblk.nbf, nbf);

          if(do_linK_){

            // TODO figure out how to take advantage of L_3 sorting
            assert(Xblk.nshell == 1);
            for(const auto&& Xsh : shell_range(Xblk)) {                                              //latex `\label{sc:k3b:Xshloop}`

              int block_offset = 0;

              Eigen::MatrixXd D_ordered;                                                   //latex `\label{sc:k3b:reD}`

              if(linK_block_rho_) {

                mt_timer.enter("rearrange D", ithr);
                D_ordered.resize(nbf, nbf);
                for(auto jblk : shell_block_range(L_3[{ish, Xsh}], NoRestrictions)){

                  for(auto jsh : shell_range(jblk)) {
                    D_ordered.middleRows(block_offset, jsh.nbf) = D.middleRows(jsh.bfoff, jsh.nbf);
                    block_offset += jsh.nbf;
                  }

                }                                                                            //latex `\label{sc:k3b:reD:end}`
                mt_timer.exit(ithr);

              }

              int restrictions = linK_block_rho_ ? NoRestrictions : Contiguous;
              block_offset = 0;

              for(const auto&& jblk : shell_block_range(L_3[{ish, Xsh}], restrictions)){             //latex `\label{sc:k3b:noblk:loop}`
                TimerHolder subtimer(ints_timer);

                auto g3_ptr = ints_to_eigen(
                    jblk, ish, Xsh,
                    eris_3c_[ithr], coulomb_oper_type_
                );
                auto& g3_in = *g3_ptr;

                //----------------------------------------//
                // Two-body part

                // TODO This breaks integral caching (if I ever use it again)

                subtimer.change(k2_part_timer);

                int subblock_offset = 0;
                for(const auto&& jsblk : shell_block_range(jblk, Contiguous|SameCenter)) {
                  int inner_size = ish.atom_dfnbf;
                  if(ish.center != jsblk.center) {
                    inner_size += jsblk.atom_dfnbf;
                  }

                  const int tot_cols = coefs_blocked_[jsblk.center].cols();
                  const int col_offset = coef_block_offsets_[jsblk.center][ish.center]
                      + ish.bfoff_in_atom*inner_size;
                  double* data_start = coefs_blocked_[jsblk.center].data() +
                      jsblk.bfoff_in_atom * tot_cols + col_offset;

                  StridedRowMap Ctmp(data_start, jsblk.nbf, ish.nbf*inner_size,
                      Eigen::OuterStride<>(tot_cols)
                  );

                  RowMatrix C(Ctmp.nestByValue());
                  C.resize(jsblk.nbf*ish.nbf, inner_size);

                  g3_in.middleRows(subblock_offset*ish.nbf, jsblk.nbf*ish.nbf) -= 0.5
                      * C.rightCols(ish.atom_dfnbf) * g2.block(
                          ish.atom_dfbfoff, Xsh.bfoff,
                          ish.atom_dfnbf, Xsh.nbf
                  );
                  if(ish.center != jsblk.center) {
                    g3_in.middleRows(subblock_offset*ish.nbf, jsblk.nbf*ish.nbf) -= 0.5
                        * C.leftCols(jsblk.atom_dfnbf) * g2.block(
                            jsblk.atom_dfbfoff, Xsh.bfoff,
                            jsblk.atom_dfnbf, Xsh.nbf
                    );
                  }

                  subblock_offset += jsblk.nbf;

                }


                //----------------------------------------//

                // Now view the integrals as a jblk.nbf x (ish.nbf*Xsh.nbf) matrix, which makes
                //   the contraction more convenient.  Doesn't require any movement of data

                Eigen::Map<ThreeCenterIntContainer> g3(g3_in.data(), jblk.nbf, ish.nbf*Xsh.nbf);

                //----------------------------------------//

                if(print_screening_stats_ > 2) {
                  mt_timer.enter("count underestimated ints", ithr);

                  double epsilon;
                  if(density_reset_){ epsilon = full_screening_thresh_; }
                  else{ epsilon = pow(full_screening_thresh_, full_screening_expon_); }

                  int offset_in_block = 0;
                  for(const auto&& jsh : shell_range(jblk)) {
                    const double g3_norm = g3.middleRows(offset_in_block, jsh.nbf).norm();
                    offset_in_block += jsh.nbf;
                    const int nfxn = jsh.nbf*ish.nbf*Xsh.nbf;
                    if(L_3[{ish, Xsh}].value_for_index(jsh) < g3_norm) {
                      ++iter_stats_->K_3c_underestimated;
                      iter_stats_->K_3c_underestimated_fxn += jsh.nbf*ish.nbf*Xsh.nbf;
                    }
                    if(g3_norm * L_DC[jsh].value_for_index(Xsh) > epsilon) {
                      ++iter_stats_->K_3c_perfect;
                      iter_stats_->K_3c_perfect_fxn += nfxn;
                    }
                    if(xml_screening_data_ and iter_stats_->is_first) {
                      iter_stats_->int_screening_values.mine(ithr).push_back(
                          L_3[{ish, Xsh}].value_for_index(jsh)
                      );
                      iter_stats_->int_actual_values.mine(ithr).push_back(g3_norm);
                      iter_stats_->int_distance_factors.mine(ithr).push_back(
                          get_distance_factor(ish, jsh, Xsh)
                      );
                      iter_stats_->int_distances.mine(ithr).push_back(
                          get_R(ish, jsh, Xsh)
                      );
                      iter_stats_->int_indices.mine(ithr).push_back(
                          std::make_tuple(ish, jsh, Xsh)
                      );
                      iter_stats_->int_ams.mine(ithr).push_back(
                          std::make_tuple(ish.am, jsh.am, Xsh.am)
                      );
                    }
                  }
                  mt_timer.exit(ithr);
                }

                //----------------------------------------//

                subtimer.change(contract_timer);

                #if !CADF_USE_BLAS
                // Eigen version
                if(linK_block_rho_) {
                  B_ish += 2.0 * g3.transpose() * D_ordered.middleRows(block_offset, jblk.nbf);
                }
                else {
                  B_ish += 2.0 * g3.transpose() * D.middleRows(jblk.bfoff, jblk.nbf);
                }
                #else
                // BLAS version
                const char notrans = 'n', trans = 't';
                const blasint M = ish.nbf*Xblk.nbf;
                const blasint K = jblk.nbf;
                const double alpha = 2.0, one = 1.0;
                double* D_data;
                if(linK_block_rho_) {
                  D_data = D_ordered.data() + block_offset;
                }
                else {
                  D_data = D.data() + jblk.bfoff;
                }
                F77_DGEMM(&notrans, &notrans,
                    &M, &nbf, &K,
                    &alpha, g3.data(), &M,
                    D_data, &nbf,
                    &one, B_ish.data(), &M
                );
                #endif

                block_offset += jblk.nbf;

              } // end loop over jsh

            } // end loop over Xsh

          } // end if do_linK_
          else {                                                                             //latex `\label{sc:k3b:nolink}`

            for(auto&& jblk : iter_significant_partners_blocked(ish, Contiguous)){
              TimerHolder subtimer(ints_timer);

              auto g3_ptr = ints_to_eigen(
                  jblk, ish, Xblk,
                  eris_3c_[ithr], coulomb_oper_type_
              );
              auto& g3_in = *g3_ptr;

              //----------------------------------------//
              // Two-body part

              // TODO This breaks integral caching (if I ever use it again)

              subtimer.change(k2_part_timer);

              int subblock_offset = 0;
              for(const auto&& jsblk : shell_block_range(jblk, SameCenter)) {
                int inner_size = ish.atom_dfnbf;
                if(ish.center != jsblk.center) {
                  inner_size += jsblk.atom_dfnbf;
                }

                const int tot_cols = coefs_blocked_[jsblk.center].cols();
                const int col_offset = coef_block_offsets_[jsblk.center][ish.center]
                    + ish.bfoff_in_atom*inner_size;
                double* data_start = coefs_blocked_[jsblk.center].data() +
                    jsblk.bfoff_in_atom * tot_cols + col_offset;

                StridedRowMap Ctmp(data_start, jsblk.nbf, ish.nbf*inner_size,
                    Eigen::OuterStride<>(tot_cols)
                );

                RowMatrix C(Ctmp.nestByValue());
                C.resize(jsblk.nbf*ish.nbf, inner_size);

                g3_in.middleRows(subblock_offset*ish.nbf, jsblk.nbf*ish.nbf) -= 0.5
                    * C.rightCols(ish.atom_dfnbf) * g2.block(
                        ish.atom_dfbfoff, Xblk.bfoff,
                        ish.atom_dfnbf, Xblk.nbf
                );
                if(ish.center != jsblk.center) {
                  g3_in.middleRows(subblock_offset*ish.nbf, jsblk.nbf*ish.nbf) -= 0.5
                      * C.leftCols(jsblk.atom_dfnbf) * g2.block(
                          jsblk.atom_dfbfoff, Xblk.bfoff,
                          jsblk.atom_dfnbf, Xblk.nbf
                  );
                }

                subblock_offset += jsblk.nbf;

              } // end loop over jsblk

              //----------------------------------------//

              // Now view the integrals as a jblk.nbf x (ish.nbf*Xsh.nbf) matrix, which makes
              //   the contraction more convenient.  Doesn't require any movement of data

              Eigen::Map<ThreeCenterIntContainer> g3(g3_in.data(), jblk.nbf, ish.nbf*Xblk.nbf);

              //----------------------------------------//

              subtimer.change(contract_timer);

              #if !CADF_USE_BLAS
              // Eigen version
              B_ish += 2.0 * g3.transpose() * D.middleRows(jblk.bfoff, jblk.nbf);
              #else
              // BLAS version
              const char notrans = 'n', trans = 't';
              const blasint M = ish.nbf*Xblk.nbf;
              const blasint K = jblk.nbf;
              const double alpha = 2.0, one = 1.0;
              F77_DGEMM(&notrans, &notrans,
                  &M, &nbf, &K,
                  &alpha, g3.data(), &M,
                  D.data() + jblk.bfoff, &nbf,
                  &one, B_ish.data(), &M
              );
              #endif

            } // end loop over jsh

          } // end else (do_linK_ == false)                                                  //latex `\label{sc:k3b:bend}`
          /*******************************************************/ #endif //2}}}
          /*-----------------------------------------------------*/
          /* Compute K contributions                        {{{2 */ #if 2 // begin fold      //latex `\label{sc:k3b:kcontrib}`
          mt_timer.change("K contributions", ithr);
          const int obs_atom_bfoff = obs->shell_to_function(obs->shell_on_center(Xblk.center, 0));
          const int obs_atom_nbf = obs->nbasis_on_center(Xblk.center);
          for(auto&& X : function_range(Xblk)) {
            const auto& C_X = coefs_transpose_[X];
            for(auto&& mu : function_range(ish)) {

              // B_mus[mu.bfoff_in_shell] is (nbf x Ysh.nbf)
              // C_Y is (Y.{obs_}atom_nbf x nbf)
              // result should be (Y.{obs_}atom_nbf x 1)

              Kt_part.row(mu).segment(obs_atom_bfoff, obs_atom_nbf).transpose() +=
                  C_X * B_ish.row(mu.bfoff_in_shell*Xblk.nbf + X.bfoff_in_block).transpose();

              Kt_part.row(mu).transpose() += C_X.transpose()
                  * B_ish.row(mu.bfoff_in_shell*Xblk.nbf + X.bfoff_in_block).segment(obs_atom_bfoff, obs_atom_nbf).transpose();

              Kt_part.row(mu).segment(obs_atom_bfoff, obs_atom_nbf).transpose() -=
                  C_X.middleCols(obs_atom_bfoff, obs_atom_nbf).transpose()
                  * B_ish.row(mu.bfoff_in_shell*Xblk.nbf + X.bfoff_in_block).segment(obs_atom_bfoff, obs_atom_nbf).transpose();

              //----------------------------------------//
            }
          }
          mt_timer.exit(ithr);
          /*******************************************************/ #endif //2}}}            //latex `\label{sc:k3b:kcontrib:end}`
          /*-----------------------------------------------------*/
        } // end while get ish Xblk pair
        //============================================================================//
        //----------------------------------------//
        // Sum Kt parts within node
        boost::lock_guard<boost::mutex> lg(tmp_mutex);
        Kt += Kt_part;
      }); // end create_thread
    } // end enumeration of threads
    compute_threads.join_all();
    mt_timer.exit();
    timer.insert(mt_timer);
    if(print_iteration_timings_) mt_timer.print(ExEnv::out0(), 12, 45);
  } // compute_threads is destroyed here
  /*****************************************************************************************/ #endif //1}}} //latex `\label{sc:k3b:end}`
  /*=======================================================================================*/
  /* Global sum K                                         		                        {{{1 */ #if 1 //latex `\label{sc:kglobalsum}`
  //----------------------------------------//
  scf_grp_->sum(Kt.data(), nbf*nbf);
  //----------------------------------------//
  // Symmetrize K
  Eigen::MatrixXd K(nbf, nbf);
  K = Kt + Kt.transpose();
  //----------------------------------------//
  /*****************************************************************************************/ #endif //1}}} //latex `\label{sc:kglobalsumend}`
  /*=======================================================================================*/
  /* Transfer K to a RefSCMatrix                           		                        {{{1 */ #if 1 // begin fold
  Ref<Integral> localints = integral()->clone();
  localints->set_basis(obs);
  Ref<PetiteList> pl = localints->petite_list();
  RefSCDimension obsdim = pl->AO_basisdim();
  RefSCMatrix result(
      obsdim,
      obsdim,
      obs->so_matrixkit()
  );
  result.assign(K.data());
  /*****************************************************************************************/ #endif //1}}}
  /*=======================================================================================*/
  /* Clean up                                             		                        {{{1 */ #if 1 // begin fold
  //----------------------------------------//
  deallocate(D_ptr);
  /*****************************************************************************************/ #endif //1}}}
  /*=======================================================================================*/
  return result;
}

//////////////////////////////////////////////////////////////////////////////////


