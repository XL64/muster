#ifndef BAYESIAN_INFORMATION_CRITERION_H
#define BAYESIAN_INFORMATION_CRITERION_H
///
/// @file bic.h
/// @brief Template function implementations of the Bayesian Information Criterion.
///
/// The Bayesian Information Criterion (BIC) is a criterion for model selection
/// that balances a maximum likelihood estimator and a parameter count.  This
/// implementation is designed for clustering algorithms, in particular K-means and
/// K-medoids clustering algorithms where we expect clusters with  spherical gaussian
/// distributions.  
///
/// Here, we want to test whether a clustering's centroids or medoids are good predictors
/// of the points in a data set, so these are our parameters, and we try to find the best
/// clustering without too many clusters.  For more on this technique and the approach
/// we've based this implementation on, see this paper:
/// @par
/// Pelleg, Dan and Moore, Andrew.  <a href="http://www.cs.cmu.edu/~dpelleg/download/xmeans.pdf">
/// <b>X-Means: Extending K-Means with Efficient Estimation of the Number of Clusters</b></a>.
/// <i>Proceedings of the Seventeenth International Conference on Machine Learning</i>,
/// pp 727-734, Morgan Kaufman, San Francisco, CA.
/// 

#include <stdint.h>
#include <numeric>
#include <iostream>
#include <cmath>
#include <vector>
#include "dissimilarity.h"
#include "partition.h"

namespace cluster {
  
  ///
  /// Directly computes the BIC from a partition object based on the cluster centroids
  /// and the number of clusters.
  /// 
  /// @param[in] p         A partition object describing the clustering to be evaluated.
  /// @param[in] distance  A distance function callable on two \em indices from the partition p.
  /// @param[in] M         Dimensionality parameter -- degrees of freedom in the input dataset.
  ///
  /// @return A valid BIC score based on the input parameters.  Higher numbers indicate better fit.
  /// 
  template <typename D>
  double bic(const partition& p, D distance, size_t M) {
    double R = p.size();
    size_t k = p.num_clusters();

    // calculate variance.
    double s2 = total_squared_dissimilarity(p, distance) / (R - k);
    double s  = sqrt(s2);
    double sM = pow(s, (double)M);

    // compute sizes of the clusters in the partition.
    size_t sizes[k];
    for (size_t i=0; i < k; i++) {
      sizes[i] = p.size(i);
    }

    // compute BIC from point probabilities
    double root2pi = sqrt(2 * M_PI);
    double lD = 0;
    for (size_t i=0; i < p.size(); i++) {
      double d  = distance(i, p.medoid_ids[p.cluster_ids[i]]);
      double Ri = sizes[p.cluster_ids[i]];
      lD += 
        + log(1.0 / (root2pi * sM))
        - (1 / (2 * s2)) * d * d
        + log(Ri / R);
    }

    const size_t pj = (k-1) + M*k + 1;   // free parameter count
    return lD - pj/2 * log((double)R);
  }


  ///
  /// This version of the BIC assumes some precomputed information.  This is useful for
  /// parallel implementations, where it is more efficient to compute some global sums as a 
  /// reduction rather than aggregating a full partition to one process.  Here,
  /// we assume that the sizes of the distributed clusters as well as the total squared intra-cluster
  /// dissimilarity (between each object and its medoid) is known.
  /// 
  ///    @param[in] k               Number of clusters in the clustering.  Same as k from k-means or k-medoids.
  ///    @param[in] cluster_sizes   Start of range of k sizes.  
  ///                               <code>*cluster_sizes .. *(cluster_sizes + k)</code> should be the
  ///                               sizes of clusters 1 to k
  ///    @param[in] sum2_dissim     Sum of squared dissimilarities of each object w.r.t. its nearest medoid.
  ///    @param[in] dimensionality  Dimensionality of clustered data.  e.g., 2 for 2-dimensional points.
  ///
  template <typename SizeIterator, typename DissimIterator>
  double bic(size_t k, SizeIterator cluster_sizes, DissimIterator sum2_dissim, size_t dimensionality) {
    // figure out total size of data set and put it in R.
    const double R      = std::accumulate(cluster_sizes, cluster_sizes + k, 0);
    const double M      = dimensionality;    // Shorthand for model dimensionality
    const double logR   = log(R);
    const double log2pi = log(2 * M_PI);
    const double pj     = (k-1) + M*k + 1;   // free parameter count
    const double s2     = std::accumulate(sum2_dissim, sum2_dissim + k, 0.0) / (R - k);

    // apply criterion formula from xmeans paper.
    double criterion = 0;
    for (size_t i=0; i < k; i++) {
      const double Rn = *(cluster_sizes + i);
      criterion += 
        - (Rn * log2pi) / 2.0
        - (Rn * M * log(s2)) / 2.0 
        - (Rn - 1) / 2.0 
        + Rn * log(Rn) 
        - Rn * logR;
    }
    criterion -= (pj/2.0 * logR);
    
    return criterion;
  }

};

#endif // BAYESIAN_INFORMATION_CRITERION_H
