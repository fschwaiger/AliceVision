// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#pragma once

#include "aliceVision/sfm/sfm_data_io.hpp"
#include "aliceVision/sfm/pipelines/sfm_engine.hpp"
#include "aliceVision/feature/FeaturesPerView.hpp"
#include "aliceVision/sfm/pipelines/sfm_matches_provider.hpp"
#include "aliceVision/track/Track.hpp"

#include "dependencies/htmlDoc/htmlDoc.hpp"
#include "dependencies/histogram/histogram.hpp"

#if ALICEVISION_IS_DEFINED(ALICEVISION_HAVE_BOOST)
  #include <boost/property_tree/ptree.hpp>
  #include <boost/property_tree/json_parser.hpp>
  namespace pt = boost::property_tree;
#endif

namespace aliceVision {
namespace sfm {

/// Sequential SfM Pipeline Reconstruction Engine.
class SequentialSfMReconstructionEngine : public ReconstructionEngine
{
public:
  SequentialSfMReconstructionEngine(
    const SfM_Data & sfm_data,
    const std::string & soutDirectory,
    const std::string & loggingFile = "");

  ~SequentialSfMReconstructionEngine();

  void setFeatures(feature::FeaturesPerView * featuresPerView)
  {
    _featuresPerView = featuresPerView;
  }

  void setMatches(matching::PairwiseMatches * pairwiseMatches)
  {
    _pairwiseMatches = pairwiseMatches;
  }

  void RobustResectionOfImages(
    const std::set<size_t>& viewIds,
    std::set<size_t>& set_reconstructedViewId,
    std::set<size_t>& set_rejectedViewId);

  virtual bool Process();

  void setInitialPair(const Pair & initialPair)
  {
    _userInitialImagePair = initialPair;
  }

  /// Initialize tracks
  bool InitLandmarkTracks();

  /// Select a candidate initial pair
  bool ChooseInitialPair(Pair & initialPairIndex) const;

  /// Compute the initial 3D seed (First camera t=0; R=Id, second estimated by 5 point algorithm)
  bool MakeInitialPair3D(const Pair & initialPair);

  /// Automatic initial pair selection (based on a 'baseline' computation score)
  bool getBestInitialImagePairs(std::vector<Pair>& out_bestImagePairs) const;

  /**
   * Set the default lens distortion type to use if it is declared unknown
   * in the intrinsics camera parameters by the previous steps.
   *
   * It can be declared unknown if the type cannot be deduced from the metadata.
   */
  void SetUnknownCameraType(const camera::EINTRINSIC camType)
  {
    _camType = camType;
  }
  
  /**
   * @brief Extension of the file format to store intermediate reconstruction files.
   */
  void setSfmdataInterFileExtension(const std::string& interFileExtension)
  {
    _sfmdataInterFileExtension = interFileExtension;
  }

  void setAllowUserInteraction(bool v)
  {
    _userInteraction = v;
  }

  void setMinInputTrackLength(int minInputTrackLength)
  {
    _minInputTrackLength = minInputTrackLength;
  }

  void setMinTrackLength(int minTrackLength)
  {
    _minTrackLength = minTrackLength;
  }

protected:


private:
  /// Image score contains <ImageId, NbPutativeCommonPoint, score, isIntrinsicsReconstructed>
  typedef std::tuple<IndexT, std::size_t, std::size_t, bool> ViewConnectionScore;

  /// Return MSE (Mean Square Error) and a histogram of residual values.
  double ComputeResidualsHistogram(Histogram<double> * histo) const;

  /// Return MSE (Mean Square Error) and a histogram of tracks size.
  double ComputeTracksLengthsHistogram(Histogram<double> * histo) const;

  /**
   * @brief Compute a score of the view for a subset of features. This is
   *        used for the next best view choice.
   *
   * The score is based on a pyramid which allows to compute a weighting
   * strategy to promote a good repartition in the image (instead of relying
   * only on the number of features).
   * Inspired by [Schonberger 2016]:
   * "Structure-from-Motion Revisited", Johannes L. Schonberger, Jan-Michael Frahm
   * 
   * http://people.inf.ethz.ch/jschoenb/papers/schoenberger2016sfm.pdf
   * We don't use the same weighting strategy. The weighting choice
   * is not justified in the paper.
   *
   * @param[in] viewId: the ID of the view
   * @param[in] trackIds: set of track IDs contained in viewId
   * @return the computed score
   */
  std::size_t computeImageScore(std::size_t viewId, const std::vector<std::size_t>& trackIds) const;

  /**
   * @brief Return all the images containing matches with already reconstructed 3D points.
   * The images are sorted by a score based on the number of features id shared with
   * the reconstruction and the repartition of these points in the image.
   *
   * @param[out] out_connectedViews: output list of view IDs connected with the 3D reconstruction.
   * @param[in] remainingViewIds: input list of remaining view IDs in which we will search for connected views.
   * @return False if there is no view connected.
   */
  bool FindConnectedViews(
    std::vector<ViewConnectionScore>& out_connectedViews,
    const std::set<size_t>& remainingViewIds) const;

  /**
   * @brief Estimate the best images on which we can compute the resectioning safely.
   * The images are sorted by a score based on the number of features id shared with
   * the reconstruction and the repartition of these points in the image.
   *
   * @param[out] out_selectedViewIds: output list of view IDs we can use for resectioning.
   * @param[in] remainingViewIds: input list of remaining view IDs in which we will search for the best ones for resectioning.
   * @return False if there is no possible resection.
   */
  bool FindNextImagesGroupForResection(
    std::vector<size_t>& out_selectedViewIds,
    const std::set<size_t>& remainingViewIds) const;

  /**
   * @brief Add a single Image to the scene and triangulate new possible tracks.
   * @param imageIndex
   * @return false if resection failed
   */
  bool Resection(const size_t imageIndex);

  /**
   * @brief  Triangulate new possible 2D tracks
   * List tracks that share content with this view and add observations and new 3D track if required.
   * @param previousReconstructedViews
   * @param newReconstructedViews
   */
  void triangulate(SfM_Data& scene, const std::set<IndexT>& previousReconstructedViews, const std::set<IndexT>& newReconstructedViews);

  /**
   * @brief Bundle adjustment to refine Structure; Motion and Intrinsics
   * @param fixedIntrinsics
   */
  bool BundleAdjustment(bool fixedIntrinsics);

  /// Discard track with too large residual error
  bool badTrackRejector(double dPrecision, size_t count = 0);

  #if ALICEVISION_IS_DEFINED(ALICEVISION_HAVE_BOOST)
  /// Export statistics in a JSON file
  void exportStatistics(double time_sfm);
  #endif

  //----
  //-- Data
  //----

  // HTML logger
  std::shared_ptr<htmlDocument::htmlDocumentStream> _htmlDocStream;
  std::string _sLoggingFile;

  // Extension of the file format to store intermediate reconstruction files.
  std::string _sfmdataInterFileExtension = ".ply";
  ESfM_Data _sfmdataInterFilter = ESfM_Data(EXTRINSICS | INTRINSICS | STRUCTURE | OBSERVATIONS | CONTROL_POINTS);

  // Parameter
  bool _userInteraction = true;
  Pair _userInitialImagePair;
  camera::EINTRINSIC _camType; // The camera type for the unknown cameras
  int _minInputTrackLength = 2;
  int _minTrackLength = 2;
  int _minPointsPerPose = 30;
  
  //-- Data provider
  feature::FeaturesPerView  * _featuresPerView;
  matching::PairwiseMatches  * _pairwiseMatches;

  // Pyramid scoring
  const int _pyramidBase = 2;
  const int _pyramidDepth = 5;
  /// internal cache of precomputed values for the weighting of the pyramid levels
  std::vector<int> _pyramidWeights;
  int _pyramidThreshold;

  #if ALICEVISION_IS_DEFINED(ALICEVISION_HAVE_BOOST)
    // Property tree for json stats export
    pt::ptree _tree;
  #endif

  // Temporary data
  /// Putative landmark tracks (visibility per potential 3D point)
  track::TracksMap _map_tracks;
  /// Putative tracks per view
  track::TracksPerView _map_tracksPerView;
  /// Precomputed pyramid index for each trackId of each viewId.
  track::TracksPyramidPerView _map_featsPyramidPerView;
  /// Per camera confidence (A contrario estimated threshold error)
  Hash_Map<IndexT, double> _map_ACThreshold;

  /// Remaining camera index that can be used for resection
  std::set<size_t> _set_remainingViewId;
};

} // namespace sfm
} // namespace aliceVision

