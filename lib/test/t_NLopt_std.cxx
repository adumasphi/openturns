//                                               -*- C++ -*-
/**
 *  @brief The test file of class NLopt for standard methods
 *
 *  Copyright 2005-2018 Airbus-EDF-IMACS-Phimeca
 *
 *  This library is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "openturns/OT.hxx"
#include "openturns/OTtestcode.hxx"

using namespace OT;
using namespace OT::Test;

inline String printPoint(const Point & point, const UnsignedInteger digits)
{
  OSS oss;
  oss << "[";
  Scalar eps = pow(0.1, 1.0 * digits);
  for (UnsignedInteger i = 0; i < point.getDimension(); i++)
  {
    oss << std::fixed << std::setprecision(digits) << (i == 0 ? "" : ",") << Bulk<double>((std::abs(point[i]) < eps) ? std::abs(point[i]) : point[i]);
  }
  oss << "]";
  return oss;
}

int main(int, char *[])
{
  TESTPREAMBLE;
  OStream fullprint(std::cout);

  try
  {
    // bounds
    Description inVars;
    inVars.add("x1");
    inVars.add("x2");
    inVars.add("x3");
    inVars.add("x4");
    Description formula(1, "x1+2*x2-3*x3+4*x4");

    SymbolicFunction linear(inVars, formula);

    UnsignedInteger dim = linear.getInputDimension();
    Point startingPoint(dim);

    Interval bounds(Point(dim, -3.0), Point(dim, 5.0));

    Description algoNames(NLopt::GetAlgorithmNames());
    for (UnsignedInteger i = 0; i < algoNames.getSize(); ++i)
    {
      // STOGO might not be enabled
      // NEWUOA nan/-nan
      // COBYLA crashes on squeeze
      // ESCH not same results with 2.4.1
      // AUGLAG_EQ raises a roundoff-limited on i386
      if ((algoNames[i] == "GD_STOGO") || (algoNames[i] == "GD_STOGO_RAND")
          || (algoNames[i] == "LN_NEWUOA")
          || (algoNames[i] == "LN_COBYLA")
          || (algoNames[i] == "GN_ESCH")
          || (algoNames[i] == "AUGLAG_EQ"))
      {
        fullprint << "-- Skipped: algo=" << algoNames[i] << std::endl;
        continue;
      }

      NLopt algo(algoNames[i]);
      for (UnsignedInteger minimization = 0; minimization < 2; ++minimization)
        for (UnsignedInteger inequality = 0; inequality < 2; ++inequality)
          for (UnsignedInteger equality = 0; equality < 2; ++equality)
          {
            OptimizationProblem problem(linear, SymbolicFunction(), SymbolicFunction(), bounds);
            problem.setMinimization(minimization == 0);
            if (inequality == 0)
              // x3 <= x1
              problem.setInequalityConstraint(SymbolicFunction(inVars, Description(1, "x1-x3")));
            if (equality == 0)
              // x4 = 2
              problem.setEqualityConstraint(SymbolicFunction(inVars, Description(1, "x4-2")));
            try
            {
              NLopt::SetSeed(0);
              algo.setProblem(problem);
              algo.setMaximumEvaluationNumber(5000);
              //algo.setInitialStep(Point(dim, 0.1));
              NLopt localAlgo("LD_MMA");
              algo.setLocalSolver(localAlgo);
              algo.setStartingPoint(startingPoint);
              fullprint << "algo=" << algo << std::endl;
              algo.run();
              OptimizationResult result(algo.getResult());
              fullprint << "x^=" << printPoint(result.getOptimalPoint(), 3) << std::endl;
            }
            catch (...)
            {
              fullprint << "-- Not supported: algo=" << algoNames[i] << " inequality=" << (inequality == 0 ? "true" : "false") << " equality=" << (equality == 0 ? "true" : "false") << std::endl;
            }
          } // equality
    } // algo
  }
  catch (TestFailed & ex)
  {
    std::cerr << ex << std::endl;
    return ExitCode::Error;
  }

  return ExitCode::Success;
}
