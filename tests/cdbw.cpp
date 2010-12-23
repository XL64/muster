//////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory  
// LLNL-CODE-433662
// All rights reserved.  
//
// This file is part of Muster. For details, see http://github.com/tgamblin/muster. 
// Please also read the LICENSE file for further information.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//////////////////////////////////////////////////////////////////////////////////////////////////

///
/// @file cdbw.cpp
/// @author Juan Gonzalez juan.gonzalez@bsc.es
/// 
#include "cdbw.h"

#include <cmath>
#include <deque>
using namespace std;

namespace cluster {

  /*****************************************************************************
   * dbscan_clusters
   ****************************************************************************/

  dbscan_cluster::dbscan_cluster(size_t& cluster_id, std::vector<point>& points)
    : points_(points), cluster_id_(cluster_id) { }

  dbscan_cluster::~dbscan_cluster() { }

  void dbscan_cluster::add_point(size_t point_id) {
    cluster_points_.push_back(point_id);
  }

  ///
  /// Compute the centroid and the standard deviation for each cluster data
  ///
  void dbscan_cluster::compute_data() {
    if (cluster_points_.size() == 0) {
      return;
    }

    /* Compute centroid */
    centroid_ = points_[cluster_points_[0]];

    if (cluster_points_.size() > 1) {
      for (size_t i = 1; i < cluster_points_.size(); i++) {
        centroid_ += points_[cluster_points_[i]];
      }

      centroid_ /= cluster_points_.size();
    }

    /* Stdev */
    double sum_squares = 0.0;
    for (size_t i = 0; i < cluster_points_.size(); i++) {
      double val = centroid_.distance(points_[cluster_points_[i]]);
      sum_squares += val * val;
    }

    stdev_ = sqrt(sum_squares/(cluster_points_.size()-1));
  }

  ///
  /// Choose the representatives of each cluster using the 'fartherst-first' method
  ///
  void dbscan_cluster::choose_representatives(size_t r) {
    double max_distance = std::numeric_limits<double>::min();

    deque<bool> point_used (cluster_points_.size(), false);

    if (r >= cluster_points_.size()) {
      representatives_ = cluster_points_;
      return;
    }
    
    point& last_representative = centroid_;
    
    representatives_ = vector<size_t>(r, 0);
  
    size_t representative_id = 0;
    size_t representative_position = 0;
    for (size_t i = 0; i < r; ++i) {
      max_distance = std::numeric_limits<double>::min();
      representative_id = 0;
      
      for (size_t j = 0; j < cluster_points_.size(); ++j) {
        if (!point_used[j]) {
          double current_distance = last_representative.distance(points_[cluster_points_[j]]);
          
          if (current_distance > max_distance) {
            max_distance            = current_distance;
            representative_id       = cluster_points_[j];
            representative_position = j;
          }
        }
      }

      representatives_[i]                 = representative_id;
      point_used[representative_position] = true;
      last_representative                 = points_[representative_id];
    }
  }
  
  ///
  /// Returns the closest representative of the given point
  ///
  size_t dbscan_cluster::closest_representative(point& p) {
    double min_distance = p.distance(points_[representatives_[0]]);
    size_t result       = representatives_[0];
    
    for (size_t i = 1; i < representatives_.size(); i++) {
      double current_distance = p.distance(points_[representatives_[i]]);
      
      if (current_distance < min_distance) {
        min_distance = current_distance;
        result       = representatives_[i];
      }
    }

    return result;
  }

  ///
  /// Returns a vector of the representative points shrunken through the centroid with an 's' factor
  ///
  vector<point> dbscan_cluster::shrunk_representatives(double s) {
    vector<point> result;
    
    for (size_t i = 0; i < representatives_.size(); ++i) {
      point shrunken_point = points_[representatives_[i]];

      shrunken_point.x = shrunken_point.x + s*(centroid_.x - shrunken_point.x);
      shrunken_point.y = shrunken_point.y + s*(centroid_.y - shrunken_point.y);

      result.push_back(shrunken_point);
    }
    return result;
  }

  dbscan_cluster::dbscan_cluster(const dbscan_cluster& other) 
    : points_(other.points_),
      cluster_id_(other.cluster_id_),
      cluster_points_(other.cluster_points_),
      centroid_(other.centroid_),
      representatives_(other.representatives_),
      stdev_(other.stdev_)
  { }

  void dbscan_cluster::operator=(const dbscan_cluster& other) {
    points_          = other.points_;
    cluster_id_      = other.cluster_id_;
    cluster_points_  = other.cluster_points_;
    centroid_        = other.centroid_;
    representatives_ = other.representatives_;
    stdev_           = other.stdev_;
  }

  /*****************************************************************************
   * CDbw
   ****************************************************************************/


  CDbw::CDbw(partition& p, std::vector<point>& points) 
    : p_(p), points_(points), cdbw_(0), separation_(0), compactness_(0), cohesion_(0) {
    create_clusters();
  }

  CDbw::~CDbw() { }

  ///
  /// Compute the CDbw criterion
  ///
  double CDbw::compute(size_t r) {
    // omit the noise cluster from the cluster count.
    size_t num_clusters = p_.num_clusters();

    // Criterion is not defined when number of clusters is 1
    if (num_clusters == 1) {
      if (std::numeric_limits<double>::has_quiet_NaN) {
        return std::numeric_limits<double>::quiet_NaN();

      } else if (std::numeric_limits<double>::has_infinity) {
        return std::numeric_limits<double>::infinity();
          
      } else {
        return std::numeric_limits<double>::max();
      }
    }
    
    r_ = r;
    RCRs_ = matrix< vector< pair<size_t, size_t> > >(num_clusters, num_clusters);

    for (size_t i = 0; i < num_clusters; i++) {
      clusters_[i].choose_representatives(r_);
    }

    compute_rcrs();
  
    separation_  = compute_separation();
    compactness_ = compactness_and_intra_density_changes();
    cohesion_    = compute_cohesion(compactness_);
    cdbw_        = cohesion_*separation_*compactness_;

    return cdbw_;
  }


  double CDbw::cdbw() const { 
    return cdbw_; 
  }

  double CDbw::separation() const { 
    return separation_;
  }

  double CDbw::compactness() const {
    return compactness_;
  }

  double CDbw::cohesion() const {
    return cohesion_;
  }


  ///
  /// Create the 'dbscan_cluster' objects using, so as to manipulate the cluster data easily
  ///
  void CDbw::create_clusters() {
    size_t num_clusters = p_.num_clusters();
    for (size_t i=0; i < num_clusters; i++) {
      clusters_.push_back(dbscan_cluster(i, points_));
    }

    ann_data_points_ = annAllocPts(p_.size(), 2);
  
    for (size_t i=0; i < p_.size(); i++) {
      if (p_.cluster_ids[i] != partition::UNCLASSIFIED) {
        clusters_[p_.cluster_ids[i]].add_point(i);
      }
      
      ANNpoint current_point = annAllocPt(2);
      
      current_point[0] = points_[i].x;
      current_point[1] = points_[i].y;
      
      ann_data_points_[i] = current_point;
    }
    
    for (size_t i=0; i < clusters_.size(); i++) {
      clusters_[i].compute_data();
    }
  
    kd_tree_ = new ANNkd_tree(ann_data_points_, p_.size(), 2);
  }

  ///
  /// Compute RCRs for each pair of clusters
  ///
  void CDbw::compute_rcrs() {
    for (medoid_id i = 0; i < (medoid_id)clusters_.size(); ++i) {
      for (medoid_id j = 0; j < (medoid_id)clusters_.size(); ++j) {
        if (i != j)
          RCRs_(i,j) = compute_rcrs_i_j(i,j);
      }
    }
  }

  ///
  /// Compute the RCRs for a single pair of clusters
  ///
  vector<pair<size_t, size_t> > CDbw::compute_rcrs_i_j(medoid_id c_i, medoid_id c_j) {
    vector<pair<size_t, size_t> > result;

    vector<size_t>& reps_i = clusters_[c_i].representatives();
    vector<size_t>& reps_j = clusters_[c_j].representatives();

    vector<pair<size_t, size_t> > rcs_i_j;
    vector<pair<size_t, size_t> > rcs_j_i;

    for (size_t v = 0; v < reps_i.size(); ++v) {
      rcs_i_j.push_back(make_pair(reps_i[v],
                                  clusters_[c_j].closest_representative(points_[reps_i[v]])));
    }

    for (size_t v = 0; v < reps_j.size(); ++v) {
      rcs_j_i.push_back(make_pair(reps_j[v],
                                  clusters_[c_i].closest_representative(points_[reps_j[v]])));
    }

    // Now we have the rc's for both clusters, next, we have to compute the rcr's
    for (size_t vi = 0; vi < reps_i.size(); ++vi) {
      for (size_t vj = 0; vj < reps_j.size(); ++vj) {
        if (rcs_i_j[vi].first  == rcs_j_i[vj].second &&
            rcs_i_j[vi].second == rcs_j_i[vj].first) {
          result.push_back(rcs_i_j[vi]); // RCR pair found!
          continue;
        }
      }
    }

    return result;
  }

  ///
  /// Compute the separation of the current partition C
  ///
  double CDbw::compute_separation() {
    double min_distance_btw_clusters_i_j;
    double sum_min_distance_btw_clusters = 0.0;
    double inter_cluster_density_val = inter_cluster_density();

    for (medoid_id i = 0; i < (medoid_id)clusters_.size(); ++i) {
      min_distance_btw_clusters_i_j = std::numeric_limits<double>::max();
    
      for (medoid_id j = 0; j < (medoid_id)clusters_.size(); ++j) {
        if (i != j) {
          double distance_btw_clusters = distance_between_clusters(i, j);
        
          if (distance_btw_clusters < min_distance_btw_clusters_i_j) {
            min_distance_btw_clusters_i_j = distance_btw_clusters;
          }
        }
      }

      sum_min_distance_btw_clusters += min_distance_btw_clusters_i_j;
    }
  
    return (sum_min_distance_btw_clusters / clusters_.size())/(1 + inter_cluster_density_val);
  }

  ///
  /// Compute the inter-cluster density of the current partition C
  ///
  double CDbw::inter_cluster_density() {
    double max_density_btw_clusters_i_j;
    double sum_max_density_btw_clusters = 0.0;

    for (medoid_id i = 0; i < (medoid_id)clusters_.size(); ++i) {
      max_density_btw_clusters_i_j = std::numeric_limits<double>::min();
      
      for (medoid_id j = 0; j < (medoid_id)clusters_.size(); ++j) {
        if (i != j) {
          double density_btw_clusters = density_between_clusters(i,j);

          if (density_btw_clusters > max_density_btw_clusters_i_j) {
            max_density_btw_clusters_i_j = density_btw_clusters;
          }
        }
      }

      sum_max_density_btw_clusters += max_density_btw_clusters_i_j;
    }

    return (sum_max_density_btw_clusters / clusters_.size());
  }

  ///
  /// Compute the density between a pair of clusters c_i and c_j
  ///
  double CDbw::density_between_clusters(medoid_id c_i, medoid_id c_j) {
    vector<pair<size_t, size_t> >& RCR_i_j = RCRs_(c_i, c_j);

    double sum_densities = 0.0;

    double c_i_stdev = clusters_[c_i].stdev();
    double c_j_stdev = clusters_[c_j].stdev();
    double avg_stdev = sqrt((c_i_stdev * c_i_stdev + c_j_stdev * c_j_stdev) / 2);

    for (size_t p = 0; p < RCR_i_j.size(); ++p) {
      point  u;
      double distance_vi_vj = points_[RCR_i_j[p].first].distance(points_[RCR_i_j[p].second]);
      double cardinality_u;
    
      u  = points_[RCR_i_j[p].first];
      u += points_[RCR_i_j[p].second];
      u /= 2.0;
    
      cardinality_u = cardinality(u, avg_stdev, c_i, c_j);

      sum_densities += ((distance_vi_vj/(2*avg_stdev))*cardinality_u);
    }

    return (sum_densities/RCR_i_j.size());
  }

  ///
  /// Compute the cardinality of set defined by u as center point and radix, where the points
  /// belong to clusters c_i and c_j
  ///
  double CDbw::cardinality(point u, double radix, medoid_id c_i, medoid_id c_j) {
    size_t number_of_points = 0;
    // Perform the radix search (across all points), using libANN
    vector<size_t> neighbourhood_u = range_query(u, radix);
  
    // Count just those points that belong to clusters c_i and c_j
    for (size_t x = 0; x < neighbourhood_u.size(); ++x) {
      medoid_id cid = p_.cluster_ids[neighbourhood_u[x]];
      if (cid == c_i || cid == c_j) {
        ++number_of_points;
      }
    }

    return (number_of_points / (clusters_[c_i].size() + clusters_[c_j].size()));
  }

  ///
  /// Computes the distance between clusters c_i and c_j
  ///
  double CDbw::distance_between_clusters(medoid_id c_i, medoid_id c_j) {
    double sum_distances = 0.0;
    vector<pair<size_t, size_t> >& RCR_i_j = RCRs_(c_i, c_j);

    for (size_t p=0; p < RCR_i_j.size(); ++p) {
      sum_distances += points_[RCR_i_j[p].first].distance(points_[RCR_i_j[p].second]);
    }
  
    return (sum_distances/RCR_i_j.size());
  }

  ///
  /// Computes the compactness of the partition and also the intra-cluster density changes, so
  /// as to save intra-cluster density computations
  ///
  double CDbw::compactness_and_intra_density_changes() {
    double         sum_intra_densities = 0.0;
    vector<double> intra_densities_values;
  
    for (double s = 0.1; s <= 0.8; s += 0.1) {
      double current_intra_density = intra_cluster_density(s);
      sum_intra_densities += current_intra_density;
    
      intra_densities_values.push_back(current_intra_density);
    }

    intra_cluster_density_change_ = 0.0;
    for (size_t i = 1; i < intra_densities_values.size(); ++i) {
      intra_cluster_density_change_ += fabs(intra_densities_values[i] - intra_densities_values[i-1]);
    }
    intra_cluster_density_change_ /= 7;

    return (sum_intra_densities/8); // 8 -> number of different 's'
  }

  double CDbw::intra_cluster_density(double s) {
    double avg_stdev = 0.0;
    for (size_t i = 0; i < clusters_.size(); ++i) {
      avg_stdev += clusters_[i].stdev() * clusters_[i].stdev();
    }
    avg_stdev = sqrt(avg_stdev/clusters_.size());

    return density(s)/(clusters_.size()*avg_stdev);
  }

  double CDbw::density(double s) {
    double sum_cardinalities = 0.0;

    for (size_t i = 0; i < clusters_.size(); ++i) {
      vector<point> shrunk_rep = clusters_[i].shrunk_representatives(s);

      for (size_t j = 0; j < shrunk_rep.size(); ++j) {
        sum_cardinalities += cardinality(shrunk_rep[j], clusters_[i].stdev(), i);
      }
    }

    return sum_cardinalities / r_;
  }

  double CDbw::cardinality(point u, double radix, medoid_id c_i) {
    // Perform the radix search (across all points), using libANN
    vector<size_t> neighbourhood_u = range_query(u, radix);

    // Count just those points that belong to cluster c_i
    size_t number_of_points = 0;
    for (size_t x = 0; x < neighbourhood_u.size(); ++x) {
      medoid_id cid = p_.cluster_ids[neighbourhood_u[x]];
      if (cid == c_i) {
        ++number_of_points;
      }
    }

    return number_of_points * 1.0/clusters_[c_i].size();
  }

  /* Cohesion */
  double CDbw::compute_cohesion(double compact) {
    return compact / (1 + intra_cluster_density_change_);
  }

  ///
  /// Performs a range query in the KD Tree, centered on point u, with the radix indicated
  ///
  vector<size_t> CDbw::range_query(point u, double radix) {
    ANNpoint       ann_query_point = annAllocPt(2);
    ANNidxArray    ann_results;
    size_t         results_size;
    vector<size_t> result;

    ann_query_point[0] = u.x;
    ann_query_point[1] = u.y;

    double radix2 = radix * radix;
    results_size = kd_tree_->annkFRSearch(ann_query_point, radix2, 0);
  
    ann_results = new ANNidx[results_size];
    kd_tree_->annkFRSearch(ann_query_point, radix2, results_size, ann_results);

    for (size_t i = 0; i < results_size; ++i) {
      result.push_back(ann_results[i]);
    }

    return result;
  }

} // namespace cluster
