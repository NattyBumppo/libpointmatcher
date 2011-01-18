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

#include "pointmatcher/PointMatcher.h"
#include <cassert>
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		cerr << "Error in command line, usage " << argv[0] << " reference.csv reading.csv" << endl;
		return 1;
	}
	
	typedef MetricSpaceAlignerD MSA;
	MSA::Strategy p;
	
	p.transformations.push_back(new MSA::TransformFeatures());
	p.transformations.push_back(new MSA::TransformDescriptors());
	
	p.readingDataPointsFilters.push_back(new MSA::RandomSamplingDataPointsFilter(0.5, true, false));
	
	p.referenceDataPointsFilters.push_back(new MSA::RandomSamplingDataPointsFilter(0.5, true, false));
	//p.referenceDataPointsFilters.push_back(new MSA::SurfaceNormalDataPointsFilter(10, 0, true, true, true, true, true));
	
	p.matcher = new MSA::KDTreeMatcher();
	
	p.featureOutlierFilters.push_back(new MSA::MaxDistOutlierFilter(0.05));
	//p.featureOutlierFilters.push_back(new MSA::MedianDistOutlierFilter(3));
	//p.featureOutlierFilters.push_back(new MSA::TrimmedDistOutlierFilter(0.85));
	
	p.descriptorOutlierFilter = new MSA::NullDescriptorOutlierFilter();

	p.errorMinimizer = new MSA::PointToPointErrorMinimizer();
	//p.errorMinimizer = new MSA::PointToPlaneErrorMinimizer();
	
	p.transformationCheckers.push_back(new MSA::CounterTransformationChecker(60));
	p.transformationCheckers.push_back(new MSA::ErrorTransformationChecker(0.001, 0.001, 3));
	
	p.inspector = new MSA::VTKFileInspector("test");
	//p.inspector = new MSA::Inspector;
	
	p.outlierMixingWeight = 1;
	
	typedef MSA::TransformationParameters TP;
	typedef MSA::DataPoints DP;
	
	const DP ref(loadCSV<MSA::ScalarType>(argv[1]));
	DP data(loadCSV<MSA::ScalarType>(argv[2]));
	TP t(TP::Identity(data.features.rows(), data.features.rows()));
	
	for (int i = 0; i < data.features.cols(); ++i)
	{
		data.features.block(0, i, 2, 1) = Eigen::Rotation2D<double>(0.2) * data.features.block(0, i, 2, 1);
		data.features(0, i) += 0.2;
		data.features(1, i) -= 0.1;
	}
	
	TP res = MSA::icp(t, data, ref, p);

	return 0;
}