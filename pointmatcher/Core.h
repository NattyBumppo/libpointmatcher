// kate: replace-tabs off; indent-width 4; indent-mode normal
// vim: ts=4:sw=4:noexpandtab
/*

Copyright (c) 2010--2011,
François Pomerleau and Stephane Magnenat, ASL, ETHZ, Switzerland
You can contact the authors at <f.pomerleau@gmail.com> and
<stephane at magnenat dot net>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ETH-ASL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef __POINTMATCHER_CORE_H
#define __POINTMATCHER_CORE_H

#ifndef EIGEN_USE_NEW_STDVECTOR
#define EIGEN_USE_NEW_STDVECTOR
#endif // EIGEN_USE_NEW_STDVECTOR
#include "Eigen/StdVector"
#include "Eigen/Eigen"
#include "Eigen/Geometry"
#include "nabo/nabo.h"
#include <stdexcept>

template<typename T>
T anyabs(const T& v)
{
	if (v < T(0))
		return -v;
	else
		return v;
}

template<typename T>
struct MetricSpaceAligner
{
	typedef T ScalarType;
	typedef typename Eigen::Matrix<T, Eigen::Dynamic, 1> Vector;
	typedef std::vector<Vector, Eigen::aligned_allocator<Vector> > VectorVector;
	typedef typename Eigen::Quaternion<T> Quaternion;
	typedef std::vector<Quaternion, Eigen::aligned_allocator<Quaternion> > QuaternionVector;
	typedef typename Eigen::Matrix<T, 3, 1> Vector3;
	typedef typename Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> Matrix;
	typedef typename Eigen::Matrix<T, 3, 3> Matrix3;
	typedef typename Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> IntMatrix;
	typedef typename Nabo::NearestNeighbourSearch<T> NNS;
	
	typedef Matrix TransformationParameters;
	
	// input types
	struct DataPoints
	{
		typedef Matrix Features;
		typedef Matrix Descriptors;
		
		struct Label
		{
			std::string text;
			size_t span;
			Label(const std::string& text = "", const size_t span = 0):text(text), span(span) {}
		};
		typedef std::vector<Label> Labels;
		
		// Constructor
		DataPoints() {}
		// Constructor
		DataPoints(const Features& features, const Labels& featureLabels):
			features(features),
			featureLabels(featureLabels)
		{}
		// Constructor
		DataPoints(const Features& features, const Labels& featureLabels, const Descriptors& descriptors, const Labels& descriptorLabels):
			features(features),
			featureLabels(featureLabels),
			descriptors(descriptors),
			descriptorLabels(descriptorLabels)
		{}


		// Get descriptor by name
		// Return a matrix containing only the resquested descriptor
		Descriptors getDescriptorByName(const std::string& name) const
		{
			int row(0);
			
			for(unsigned int i = 0; i < descriptorLabels.size(); i++)
			{
				const int span(descriptorLabels[i].span);
				if(descriptorLabels[i].text.compare(name) == 0)
				{
					return descriptors.block(row, 0, 
							span, descriptors.cols());
				}

				row += span;
			}

			return Descriptors();
		}
		
		Features features;
		Labels featureLabels;
		Descriptors descriptors;
		Labels descriptorLabels;
	};
	
	struct ConvergenceError: std::runtime_error
	{
		ConvergenceError(const std::string& reason):runtime_error(reason) {}
	};
	
	// intermediate types
	struct Matches
	{
		typedef Matrix Dists;
		typedef IntMatrix Ids;
		// FIXME: shouldn't we have a matrix of pairs instead?
	
		Matches() {}
		Matches(const Dists& dists, const Ids ids):
			dists(dists),
			ids(ids)
		{}
		
		Dists dists;
		Ids ids;
	};

	typedef Matrix OutlierWeights;
	
	

	// type of processing bricks
	struct Transformation
	{
		virtual ~Transformation() {}
		virtual DataPoints compute(const DataPoints& input, const TransformationParameters& parameters) const = 0;
	};
	struct Transformations: public std::vector<Transformation*>
	{
		void apply(DataPoints& cloud, const TransformationParameters& parameters) const;
	};
	typedef typename Transformations::iterator TransformationsIt;
	typedef typename Transformations::const_iterator TransformationsConstIt;
	
	struct TransformFeatures: public Transformation
	{
		virtual DataPoints compute(const DataPoints& input, const TransformationParameters& parameters) const;
	};
	struct TransformDescriptors: Transformation
	{
		virtual DataPoints compute(const DataPoints& input, const TransformationParameters& parameters) const;
	};
	
	
	// ---------------------------------
	struct DataPointsFilter
	{
		virtual ~DataPointsFilter() {}
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const = 0;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const = 0;
	};
	
	struct DataPointsFilters: public std::vector<DataPointsFilter*>
	{
		void applyPre(DataPoints& cloud, bool iterate) const;
		void applyStep(DataPoints& cloud, bool iterate) const;
	};
	typedef typename DataPointsFilters::iterator DataPointsFiltersIt;
	typedef typename DataPointsFilters::const_iterator DataPointsFiltersConstIt;


	/* Data point operations */

	// Identidy
	struct IdentityDataPointsFilter: public DataPointsFilter
	{
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const {return input;};
	};
	
	// Surface normals
	class SurfaceNormalDataPointsFilter: public DataPointsFilter
	{
		const int knn;
		const double epsilon;
		const bool keepNormals;
		const bool keepDensities;
		const bool keepEigenValues;
		const bool keepEigenVectors;
		const bool keepMatchedIds;
		
	public:
		SurfaceNormalDataPointsFilter(const int knn = 5, 
			const double epsilon = 0,
			const bool keepNormals = true,
			const bool keepDensities = false,
			const bool keepEigenValues = false, 
			const bool keepEigenVectors = false,
			const bool keepMatchedIds = false);
		virtual ~SurfaceNormalDataPointsFilter() {};
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const {return input;};
	};
	
	// Sampling surface normals
	class SamplingSurfaceNormalDataPointsFilter: public DataPointsFilter
	{
		const int k;
		const bool averageExistingDescriptors;
		const bool keepNormals;
		const bool keepDensities;
		const bool keepEigenValues;
		const bool keepEigenVectors;
		
	public:
		SamplingSurfaceNormalDataPointsFilter(const int k = 10,
			const bool averageExistingDescriptors = true,
			const bool keepNormals = true,
			const bool keepDensities = false,
			const bool keepEigenValues = false, 
			const bool keepEigenVectors = false);
		virtual ~SamplingSurfaceNormalDataPointsFilter() {}
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const {return input;};
		
	protected:
		struct BuildData
		{
			typedef std::vector<int> Indices;
			
			Indices indices;
			const Matrix& inputFeatures;
			const Matrix& inputDescriptors;
			Matrix outputFeatures;
			Matrix outputDescriptors;
			int outputInsertionPoint;
			
			BuildData(const Matrix& inputFeatures, const Matrix& inputDescriptors, const int finalDescDim):
				inputFeatures(inputFeatures),
				inputDescriptors(inputDescriptors),
				outputFeatures(inputFeatures.rows(), inputFeatures.cols()),
				outputDescriptors(finalDescDim, inputFeatures.cols()),
				outputInsertionPoint(0)
			{
				const int pointsCount(inputFeatures.cols());
				indices.reserve(pointsCount);
				for (int i = 0; i < pointsCount; ++i)
					indices[i] = i;
			}
		};
		
		struct CompareDim
		{
			const int dim;
			const BuildData& buildData;
			CompareDim(const int dim, const BuildData& buildData):dim(dim),buildData(buildData){}
			bool operator() (const int& p0, const int& p1)
			{
				return  buildData.inputFeatures(dim, p0) < 
						buildData.inputFeatures(dim, p1);
			}
		};
		
	protected:
		void buildNew(BuildData& data, const int first, const int last, const Vector minValues, const Vector maxValues) const;
		void fuseRange(BuildData& data, const int first, const int last) const;
	};

	// Reorientation of normals
	class OrientNormalsDataPointsFilter: public DataPointsFilter
	{
	public:
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const {return input;};
	};

	// Random sampling
	class RandomSamplingDataPointsFilter: public DataPointsFilter
	{
		// Probability to keep points, between 0 and 1
		const double prob;
		const bool enableStepFilter;
		const bool enablePreFilter;
		
	public:
		RandomSamplingDataPointsFilter(const double ratio = 0.5, bool enablePreFilter = true, bool enableStepFilter = false);
		virtual ~RandomSamplingDataPointsFilter() {};
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const;
	private:
		DataPoints randomSample(const DataPoints& input) const;
	};
	
	// Systematic sampling
	class FixstepSamplingDataPointsFilter: public DataPointsFilter
	{
		// Probability to keep points, between 0 and 1
		const int step;
		const bool enableStepFilter;
		const bool enablePreFilter;
		
	public:
		FixstepSamplingDataPointsFilter(const int step = 10, bool enablePreFilter = true, bool enableStepFilter = false);
		virtual ~FixstepSamplingDataPointsFilter() {};
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const;
	private:
		DataPoints fixstepSample(const DataPoints& input) const;
	};

	
	/* Meshing operations */

#ifdef HAVE_CGAL

	// MeshingFilter
	struct MeshingFilter: public DataPointsFilter
	{
		Vector3 computeCentroid(const Matrix3 matrixIn) const;
		Vector3 computeNormal(Matrix3 matrixIn) const;
		
		virtual ~MeshingFilter() {};
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const = 0;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const = 0;
	};

	// ITMLocalMeshingFilter
	class ITMLocalMeshingFilter: public MeshingFilter
	{
	public:
		ITMLocalMeshingFilter();
		virtual ~ITMLocalMeshingFilter() {};
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const {return input;};

	private:
		Matrix cart2Spheric(const Matrix matrixIn) const;
		Matrix delaunay2D(const Matrix matrixIn) const;
		void generateTriMesh(const Matrix matrixFeatures, const Matrix matrixIndices, Matrix& matrixNewFeatures, Matrix& matrixNewDescriptors) const;
	};

	// ITMGlobalMeshingFilter
	// ...

	// MarchingCubeMeshingFilter
	// ...

	// ArtifactsRemovalMeshingFilter
	class ArtifactsRemovalMeshingFilter: public MeshingFilter 
	{
		// Filter thrasholds
		const T thresh1;
		const T thresh2;
		const T thresh3;

	public:
		ArtifactsRemovalMeshingFilter(const T thresh1 = 1.1, const T thresh2 = 10, const T thresh3 = 0.2);
		virtual ~ArtifactsRemovalMeshingFilter() {};
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const {return input;};
 	};

	// SimplifyMeshingFilter
	class SimplifyMeshingFilter: public MeshingFilter
	{
		// Filter thrasholds
		const int edgeCount;

		public:
		SimplifyMeshingFilter(const int edgeCount = 1000);
		virtual ~SimplifyMeshingFilter() {};
		virtual DataPoints preFilter(const DataPoints& input, bool& iterate) const;
		virtual DataPoints stepFilter(const DataPoints& input, bool& iterate) const {return input;};
	};

#endif // HAVE_CGAL


	// ---------------------------------
	struct Matcher
	{
		virtual ~Matcher() {}
		virtual void init(const DataPoints& filteredReading, const DataPoints& filteredReference, bool& iterate) = 0;
		virtual Matches findClosests(const DataPoints& filteredReading, const DataPoints& filteredReference, bool& iterate) = 0;
	};
	
	struct NullMatcher: public Matcher
	{
		virtual void init(const DataPoints& filteredReading, const DataPoints& filteredReference, bool& iterate);
		virtual Matches findClosests(const DataPoints& filteredReading, const DataPoints& filteredReference, bool& iterate);
	};
	
	class KDTreeMatcher: public Matcher
	{
		const int knn;
		const double epsilon;
		NNS* featureNNS;
	
	public:
		KDTreeMatcher(const int knn = 1, const double epsilon = 0);
		virtual ~KDTreeMatcher();
		virtual void init(const DataPoints& filteredReading, const DataPoints& filteredReference, bool& iterate);
		virtual Matches findClosests(const DataPoints& filteredReading, const DataPoints& filteredReference, bool& iterate);
	};
	
	
	// ---------------------------------
	struct FeatureOutlierFilter
	{
		virtual ~FeatureOutlierFilter() {}
		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate) = 0;
	};
	
	struct NullFeatureOutlierFilter: public FeatureOutlierFilter
	{
		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate);
	};
	
	struct MaxDistOutlierFilter: public FeatureOutlierFilter
	{
		const T maxDist;
		
		MaxDistOutlierFilter(const T maxDist);

		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate);
	};
	
	struct MedianDistOutlierFilter: public FeatureOutlierFilter
	{
		const T factor;
		
		MedianDistOutlierFilter(const T factor);
		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate);
	};
	
	struct TrimmedDistOutlierFilter: public FeatureOutlierFilter
	{
		const T ratio;
		
		TrimmedDistOutlierFilter(const T ratio);
		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate);
	};
	
	struct MinDistOutlierFilter: public FeatureOutlierFilter
	{
		const T minDist;
		
		MinDistOutlierFilter(const T minDist): minDist(minDist) {}
		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate);
	};
	
	
	// Vector of transformation checker
	struct FeatureOutlierFilters: public std::vector<FeatureOutlierFilter*>
	{
		OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate);
	};
	typedef typename FeatureOutlierFilters::iterator FeatureOutlierFiltersIt;

	
	// ---------------------------------
	struct DescriptorOutlierFilter
	{
		virtual ~DescriptorOutlierFilter() {}
		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate) = 0;
	};
	
	struct NullDescriptorOutlierFilter: public DescriptorOutlierFilter
	{
		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate);
	};


	// ---------------------------------
	struct ErrorMinimizer
	{
		struct ErrorElements
		{
			DataPoints reading;
			DataPoints reference;
			OutlierWeights weights;
			Matches matches;

			// TODO: put that in ErrorMinimizer.cpp. Tried but didn't succeed
			ErrorElements(const DataPoints& reading, const DataPoints reference, const OutlierWeights weights, const Matches matches):
				reading(reading),
				reference(reference),
				weights(weights),
				matches(matches)
			{
				assert(reading.features.cols() == reference.features.cols());
				assert(reading.features.cols() == weights.cols());
				assert(reading.features.cols() == matches.dists.cols());
				// May have no descriptors... size 0
			}
		};
		
		ErrorMinimizer():pointUsedRatio(-1.),weightedPointUsedRatio(-1.) {}
		virtual ~ErrorMinimizer() {}
		virtual TransformationParameters compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const OutlierWeights& outlierWeights, const Matches& matches, bool& iterate) = 0;
		T getPointUsedRatio() const { return pointUsedRatio; }
		T getWeightedPointUsedRatio() const { return weightedPointUsedRatio; }
		
	protected:
		// helper functions
		Matrix crossProduct(const Matrix& A, const Matrix& B);
		ErrorElements getMatchedPoints(const DataPoints& reading, const DataPoints& reference, const Matches& matches, const OutlierWeights& outlierWeights);
		
	protected:
		T pointUsedRatio;
		T weightedPointUsedRatio;
	};
	
	struct IdentityErrorMinimizer: ErrorMinimizer
	{
		virtual TransformationParameters compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const OutlierWeights& outlierWeights, const Matches& matches, bool& iterate);
	};
	
	// Point-to-point error
	// Based on SVD decomposition
	struct PointToPointErrorMinimizer: ErrorMinimizer
	{
		virtual TransformationParameters compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const OutlierWeights& outlierWeights, const Matches& matches, bool& iterate);
	};
	
	// Point-to-plane error (or point-to-line in 2D)
	struct PointToPlaneErrorMinimizer: public ErrorMinimizer
	{
		virtual TransformationParameters compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const OutlierWeights& outlierWeights, const Matches& matches, bool& iterate);
	};
	
	
	// ---------------------------------
	struct TransformationChecker
	{
		Vector limits;
		Vector values;
		std::vector<std::string> limitNames;
		std::vector<std::string> valueNames;

		virtual ~TransformationChecker() {}
		virtual void init(const TransformationParameters& parameters, bool& iterate) = 0;
		virtual void check(const TransformationParameters& parameters, bool& iterate) = 0;
		
		static Vector matrixToAngles(const TransformationParameters& parameters);
	};

	struct CounterTransformationChecker: public TransformationChecker
	{
		CounterTransformationChecker(const int maxIterationCount = 20);
		
		virtual void init(const TransformationParameters& parameters, bool& iterate);
		virtual void check(const TransformationParameters& parameters, bool& iterate);
	};
	
	class ErrorTransformationChecker: public TransformationChecker
	{
	protected:
		QuaternionVector rotations;
		VectorVector translations;
		const unsigned int tail;

	public:
		ErrorTransformationChecker(const T minDeltaRotErr, const T minDeltaTransErr, const unsigned int tail = 3);
		
		virtual void init(const TransformationParameters& parameters, bool& iterate);
		virtual void check(const TransformationParameters& parameters, bool& iterate);
	};
	
	class BoundTransformationChecker: public TransformationChecker
	{
	protected:
		Quaternion initialRotation;
		Vector initialTranslation;
		
	public:
		BoundTransformationChecker(const T maxRotationNorm, const T maxTranslationNorm);
		virtual void init(const TransformationParameters& parameters, bool& iterate);
		virtual void check(const TransformationParameters& parameters, bool& iterate);
	};
	
	
	// Vector of transformation checker
	struct TransformationCheckers: public std::vector<TransformationChecker*>
	{
		void init(const TransformationParameters& parameters, bool& iterate);
		void check(const TransformationParameters& parameters, bool& iterate);
	};
	typedef typename TransformationCheckers::iterator TransformationCheckersIt;
	typedef typename TransformationCheckers::const_iterator TransformationCheckersConstIt;


	// ---------------------------------
	struct Inspector
	{
		virtual void init() {};
		virtual void dumpFilteredReference(const DataPoints& filteredReference) {}
		virtual void dumpIteration(const size_t iterationCount, const TransformationParameters& parameters, const DataPoints& filteredReference, const DataPoints& reading, const Matches& matches, const OutlierWeights& featureOutlierWeights, const OutlierWeights& descriptorOutlierWeights, const TransformationCheckers& transformationCheckers) {}
		virtual void finish(const size_t iterationCount) {}
		virtual ~Inspector() {}
	};
	
	struct AbstractVTKInspector: public Inspector
	{
	protected:
		virtual std::ostream* openStream(const std::string& role) = 0;
		virtual std::ostream* openStream(const std::string& role, const size_t iterationCount) = 0;
		virtual void closeStream(std::ostream* stream) = 0;
		void dumpDataPoints(const DataPoints& data, std::ostream& stream);
		void dumpMeshNodes(const DataPoints& data, std::ostream& stream);
		void dumpDataLinks(const DataPoints& ref, const DataPoints& reading, 	const Matches& matches, const OutlierWeights& featureOutlierWeights, std::ostream& stream);
		
		std::ostream* streamIter;

	public:
		AbstractVTKInspector();
		virtual void init() {};
		virtual void dumpDataPoints(const DataPoints& cloud, const std::string& name);
		virtual void dumpMeshNodes(const DataPoints& cloud, const std::string& name);
		virtual void dumpIteration(const size_t iterationCount, const TransformationParameters& parameters, const DataPoints& filteredReference, const DataPoints& reading, const Matches& matches, const OutlierWeights& featureOutlierWeights, const OutlierWeights& descriptorOutlierWeights, const TransformationCheckers& transformationCheckers);
		virtual void finish(const size_t iterationCount);
	
	private:
    	void buildGenericAttributeStream(std::ostream& stream, const std::string& attribute, const std::string& nameTag, const DataPoints& cloud, const int forcedDim);

		void buildScalarStream(std::ostream& stream, const std::string& name, const DataPoints& ref, const DataPoints& reading);
    	void buildScalarStream(std::ostream& stream, const std::string& name, const DataPoints& cloud);
		
		void buildNormalStream(std::ostream& stream, const std::string& name, const DataPoints& ref, const DataPoints& reading);
		void buildNormalStream(std::ostream& stream, const std::string& name, const DataPoints& cloud);
		
		void buildVectorStream(std::ostream& stream, const std::string& name, const DataPoints& ref, const DataPoints& reading);
		void buildVectorStream(std::ostream& stream, const std::string& name, const DataPoints& cloud);
		
		void buildTensorStream(std::ostream& stream, const std::string& name, const DataPoints& ref, const DataPoints& reading);
		void buildTensorStream(std::ostream& stream, const std::string& name, const DataPoints& cloud);

		Matrix padWithZeros(const Matrix m, const int expectedRow, const int expectedCols); 
	};

	struct VTKFileInspector: public AbstractVTKInspector
	{

	protected:
		const std::string baseFileName;

		virtual std::ostream* openStream(const std::string& role);
		virtual std::ostream* openStream(const std::string& role, const size_t iterationCount);
		virtual void closeStream(std::ostream* stream);
		
	public:
		VTKFileInspector(const std::string& baseFileName);
		virtual void init();
		virtual void finish(const size_t iterationCount);
	};
	
	
	// strategy
	struct Strategy
	{
		Strategy():
			matcher(0), 
			descriptorOutlierFilter(0),
			errorMinimizer(0),
			inspector(0),
			outlierMixingWeight(0.5)
		{}
		
		~Strategy()
		{
			for (DataPointsFiltersIt it = readingDataPointsFilters.begin(); it != readingDataPointsFilters.end(); ++it)
				delete *it;
			for (DataPointsFiltersIt it = referenceDataPointsFilters.begin(); it != referenceDataPointsFilters.end(); ++it)
				delete *it;
			for (TransformationsIt it = transformations.begin(); it != transformations.end(); ++it)
				delete *it;
			delete matcher;
			for (FeatureOutlierFiltersIt it = featureOutlierFilters.begin(); it != featureOutlierFilters.end(); ++it)
				delete *it;
			delete descriptorOutlierFilter;
			delete errorMinimizer;
			for (TransformationCheckersIt it = transformationCheckers.begin(); it != transformationCheckers.end(); ++it)
				delete *it;
			delete inspector;
		}
		
		DataPointsFilters readingDataPointsFilters;
		DataPointsFilters referenceDataPointsFilters;
		Transformations transformations;
		Matcher* matcher;
		FeatureOutlierFilters featureOutlierFilters;
		DescriptorOutlierFilter* descriptorOutlierFilter;
		ErrorMinimizer* errorMinimizer;
		TransformationCheckers transformationCheckers;
		Inspector* inspector;
		T outlierMixingWeight;
	};
	
	static TransformationParameters icp(
		const TransformationParameters& initialTransformationParameters, 
		DataPoints reading,
		DataPoints reference,
		Strategy& strategy);
}; // MetricSpaceAligner

template<typename T>
void swapDataPoints(typename MetricSpaceAligner<T>::DataPoints& a, typename MetricSpaceAligner<T>::DataPoints& b)
{
	a.features.swap(b.features);
	swap(a.featureLabels, b.featureLabels);
	a.descriptors.swap(b.descriptors);
	swap(a.descriptorLabels, b.descriptorLabels);
}

#endif // __POINTMATCHER_CORE_H
