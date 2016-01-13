//                                               -*- C++ -*-
/**
 *  @brief AbdoRackwitz is an actual implementation for
 *         OptimizationSolverImplementation using the AbdoRackwitz algorithm.
 *
 *  Copyright 2005-2015 Airbus-EDF-IMACS-Phimeca
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
 *  You should have received a copy of the GNU Lesser General Public
 *  along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <cmath>
#include "AbdoRackwitz.hxx"
#include "Log.hxx"
#include "PersistentObjectFactory.hxx"

BEGIN_NAMESPACE_OPENTURNS

CLASSNAMEINIT(AbdoRackwitz);

static const Factory<AbdoRackwitz> RegisteredFactory;

/* Default constructor */
AbdoRackwitz::AbdoRackwitz()
  : OptimizationSolverImplementation()
  , tau_(ResourceMap::GetAsNumericalScalar("AbdoRackwitz-DefaultTau"))
  , omega_(ResourceMap::GetAsNumericalScalar("AbdoRackwitz-DefaultOmega"))
  , smooth_(ResourceMap::GetAsNumericalScalar("AbdoRackwitz-DefaultSmooth"))
{
  initialize();
}

AbdoRackwitz::AbdoRackwitz (const OptimizationProblem & problem,
                            const NumericalScalar tau,
                            const NumericalScalar omega,
                            const NumericalScalar smooth)
  : OptimizationSolverImplementation(problem)
  , tau_(tau)
  , omega_(omega)
  , smooth_(smooth)
{
  initialize();
}

/*
 * @brief  Standard constructor: the problem is defined by a scalar valued function  (in fact, a 1-D vector valued fonction)
 *         and a level value
 */
AbdoRackwitz::AbdoRackwitz(const AbdoRackwitzSpecificParameters & specificParameters,
                           const OptimizationProblem & problem)
  : OptimizationSolverImplementation(problem)
  , currentPoint_(getStartingPoint().getDimension())
  , currentDirection_(getStartingPoint().getDimension())
  , currentGradient_(getStartingPoint().getDimension())
{
  initialize();
  setSpecificParameters(specificParameters);
}

AbdoRackwitz::AbdoRackwitz(const OptimizationProblem & problem)
  : OptimizationSolverImplementation(problem)
  , tau_(ResourceMap::GetAsNumericalScalar("AbdoRackwitz-DefaultTau"))
  , omega_(ResourceMap::GetAsNumericalScalar("AbdoRackwitz-DefaultOmega"))
  , smooth_(ResourceMap::GetAsNumericalScalar("AbdoRackwitz-DefaultSmooth"))
{
  initialize();
}

/* Virtual constructor */
AbdoRackwitz * AbdoRackwitz::clone() const
{
  return new AbdoRackwitz(*this);
}

void AbdoRackwitz::initialize()
{
  currentSigma_ = 0.0;
  currentLevelValue_ = 0.0;
  currentLambda_ = 0.0;
}

/** Check whether this problem can be solved by this solver.  Must be overloaded by the actual optimisation algorithm */
void AbdoRackwitz::checkProblem(const OptimizationProblem & problem) const
{
  if (!problem.hasLevelFunction())
    throw InvalidArgumentException(HERE) << "Error : " << this->getClassName() << " can only solve nearest-point optimization problems";
  if (problem.hasMultipleObjective())
    throw InvalidArgumentException(HERE) << "Error: " << this->getClassName() << " does not support multi-objective optimization ";
  if (problem.hasBounds())
    throw InvalidArgumentException(HERE) << "Error : " << this->getClassName() << " cannot solve bound-constrained optimization problems";
}

/* Line search for globalization of the algorithm */
NumericalScalar AbdoRackwitz::computeLineSearch()
{
  /* Logal copy of the level function and the level value */
  const NumericalMathFunction levelFunction(getLevelFunction());
  const NumericalScalar levelValue(getLevelValue());
  /* Actualize sigma */
  currentSigma_ = std::max(currentSigma_ + 1.0, smooth_ * currentPoint_.norm() / currentGradient_.norm());
  /* Compute penalized scalar objective function at current point */
  const NumericalScalar currentTheta(0.5 * currentPoint_.normSquare() + currentSigma_ * fabs(currentLevelValue_ - levelValue));
  /* Min bound for step */
  // const NumericalScalar minStep(getMaximumAbsoluteError() / currentDirection_.norm());
  const NumericalScalar minStep(std::pow(tau_, 9));
  /* Minimum decrease for the penalized objective function */
  const NumericalScalar levelIncrement(omega_ * dot(currentPoint_ + (currentSigma_ * ((currentLevelValue_ > levelValue) ? 1.0 : -1.0)) * currentGradient_, currentDirection_));
  /* Initialization of the line search */
  /* We start with step=1 */
  NumericalScalar step(1.0);
  NumericalPoint currentStepPoint(NumericalPoint(currentPoint_.getDimension()));
  NumericalScalar currentStepLevelValue;
  NumericalScalar currentStepTheta;
  NumericalScalar oldBeta(oldPoint_.norm());
  NumericalScalar currentBeta(currentPoint_.norm());
  NumericalScalar StepBeta;
  do
  {
    currentStepPoint = currentPoint_ + step * currentDirection_;
    currentStepLevelValue = levelFunction(currentStepPoint)[0];

    currentStepTheta = 0.5 * currentStepPoint.normSquare() + currentSigma_ * fabs(currentStepLevelValue - levelValue);
    if (getVerbose()) LOGINFO(OSS() << "line search step=" << step << " currentStepPoint=" << currentStepPoint << " currentStepLevelValue=" << currentStepLevelValue << " currentStepTheta=" << currentStepTheta);
    step *= tau_;
  }
  while ((step >= minStep) && ( currentStepTheta > currentTheta + step * levelIncrement));
  /* Check circuitous iterations */
  if (oldBeta != 0)
  {
    NumericalScalar compareA(dot(oldPoint_, currentStepPoint) / (oldPoint_.norm() * currentStepPoint.norm()));
    NumericalScalar compareB(dot(currentPoint_, currentStepPoint) / (currentPoint_.norm() * currentStepPoint.norm()));
    if (compareA > compareB)
    {
      StepBeta = (currentBeta * (oldLevelValue_ - levelValue) - oldBeta * (currentLevelValue_ - levelValue)) / (oldLevelValue_ - currentLevelValue_);
      currentStepPoint = StepBeta * (oldPoint_ + currentPoint_) / (oldPoint_ + currentPoint_).norm();
      currentStepLevelValue = levelFunction(currentStepPoint)[0];
    }
  }
  oldPoint_ = currentPoint_;
  oldLevelValue_ = currentLevelValue_;
  currentPoint_ = currentStepPoint;
  currentLevelValue_ = currentStepLevelValue;
  /* We went one step beyond */
  return step / tau_;
}


/* Performs the actual computation by using the AbdoRackwitz algorithm
 */
void AbdoRackwitz::run()
{
  initialize();


  /* Get a local copy of the level function */
  const NumericalMathFunction levelFunction(getLevelFunction());
  /* Get a local copy of the level value */
  const NumericalScalar levelValue(getLevelValue());
  /* Current point -> u */
  currentPoint_ = getStartingPoint();
  Bool convergence(false);
  UnsignedInteger iterationNumber = 0;
  NumericalScalar absoluteError(-1.0);
  NumericalScalar constraintError(-1.0);
  NumericalScalar relativeError(-1.0);
  NumericalScalar residualError(-1.0);

  /* Compute the level function at the current point -> G */
  currentLevelValue_ = levelFunction(currentPoint_)[0];

  /* initialize old values */
  NumericalPoint oldPoint_(currentPoint_);
  NumericalScalar oldLevelValue_(currentLevelValue_);

  // reset result
  setResult(OptimizationResult(currentPoint_, NumericalPoint(1, currentLevelValue_), 0, absoluteError, relativeError, residualError, constraintError));

  while ( (!convergence) && (iterationNumber <= getMaximumIterationsNumber()) )
  {
    /* Go to next iteration */
    ++ iterationNumber;

    /* Compute the level function gradient at the current point -> Grad(G) */
    currentGradient_ = levelFunction.gradient(currentPoint_) * NumericalPoint(1, 1.0);
    if (getVerbose()) LOGINFO(OSS() << "current point=" << currentPoint_ << " current level value=" << currentLevelValue_ << " current gradient=" << currentGradient_);
    /* Compute the current Lagrange multiplier */
    const NumericalScalar normGradientSquared(currentGradient_.normSquare());
    /* In case of a null gradient, throw an internal exception */
    if (normGradientSquared == 0)
    {
      result_.update(currentPoint_, iterationNumber);
      throw InternalException(HERE) << "Error in Abdo Rackwitz algorithm: the gradient of the level function is zero at point u=" << currentPoint_;
    }
    /* Lambda = (G - <Grad(G), u>) / ||Grad(G)||^2 */
    currentLambda_ = (currentLevelValue_ - levelValue - dot(currentGradient_, currentPoint_)) / normGradientSquared;
    /* Compute the current direction Du = -Lambda Grad(G) - u */
    /* Be careful! currentGradient_ is an n by 1 matrix, we must multiply it by a 1 by 1
     * vector in order to get an n dimensional equivalente vector
     */
    currentDirection_ = -currentLambda_ * currentGradient_ - currentPoint_;
    /* Perform a line search in the given direction */
    const NumericalScalar alpha(computeLineSearch());
    /* Check if convergence has been achieved */
    absoluteError = fabs(alpha) * currentDirection_.norm();
    constraintError = fabs(currentLevelValue_ - levelValue);
    const NumericalScalar pointNorm(currentPoint_.norm());
    if (pointNorm > 0.0)
    {
      relativeError = absoluteError / pointNorm;
    }
    else
    {
      relativeError = -1.0;
    }
    residualError = (currentPoint_ + currentLambda_ * currentGradient_).norm();
    convergence = ((absoluteError < getMaximumAbsoluteError()) && (relativeError < getMaximumRelativeError())) || ((residualError < getMaximumResidualError()) && (constraintError < getMaximumConstraintError()));

    // update result
    result_.update(currentPoint_, iterationNumber);
    result_.store(currentPoint_, NumericalPoint(1, currentLevelValue_), absoluteError, relativeError, residualError, constraintError);
    LOGINFO( getResult().__repr__() );
  }

  /* Check if we converged */
  if (!convergence)
  {
    LOGWARN(OSS() << "Warning! The AbdoRackwitz algorithm failed to converge after " << getMaximumIterationsNumber() << " iterations");
  }
} // run()


/* Tau accessor */
NumericalScalar AbdoRackwitz::getTau() const
{
  return tau_;
}

void AbdoRackwitz::setTau(const NumericalScalar tau)
{
  tau_ = tau;
}

/* Omega accessor */
NumericalScalar AbdoRackwitz::getOmega() const
{
  return omega_;
}

void AbdoRackwitz::setOmega(const NumericalScalar omega)
{
  omega_ = omega;
}

/* Smooth accessor */
NumericalScalar AbdoRackwitz::getSmooth() const
{
  return smooth_;
}

void AbdoRackwitz::setSmooth(const NumericalScalar smooth)
{
  smooth_ = smooth;
}

/* Specific parameters accessor */
AbdoRackwitzSpecificParameters AbdoRackwitz::getSpecificParameters() const
{
  Log::Info(OSS() << "AbdoRackwitz::getSpecificParameters is deprecated.");
  return AbdoRackwitzSpecificParameters(tau_, omega_, smooth_);
}

/* Specific parameters accessor */
void AbdoRackwitz::setSpecificParameters(const AbdoRackwitzSpecificParameters & specificParameters)
{
  Log::Info(OSS() << "AbdoRackwitz::setSpecificParameters is deprecated.");
  tau_ = specificParameters.getTau();
  omega_ = specificParameters.getOmega();
  smooth_ = specificParameters.getSmooth();
}

/* Level function accessor */
NumericalMathFunction AbdoRackwitz::getLevelFunction() const
{
  Log::Info(OSS() << "AbdoRackwitz::getLevelFunction is deprecated.");
  return getProblem().getLevelFunction();
}

/* Level function accessor */
void AbdoRackwitz::setLevelFunction(const NumericalMathFunction & levelFunction)
{
  Log::Info(OSS() << "AbdoRackwitz::setLevelFunction is deprecated.");
  getProblem().setLevelFunction(levelFunction);
}

/* Level value accessor */
NumericalScalar AbdoRackwitz::getLevelValue() const
{
  Log::Info(OSS() << "AbdoRackwitz::getLevelValue is deprecated.");
  return getProblem().getLevelValue();
}

/* Level value accessor */
void AbdoRackwitz::setLevelValue(const NumericalScalar levelValue)
{
  Log::Info(OSS() << "AbdoRackwitz::setLevelValue is deprecated.");
  getProblem().setLevelValue(levelValue);
}

/* String converter */
String AbdoRackwitz::__repr__() const
{
  OSS oss;
  oss << "class=" << AbdoRackwitz::GetClassName()
      << " " << OptimizationSolverImplementation::__repr__()
      << " tau=" << tau_
      << " omega=" << omega_
      << " smooth=" << smooth_;
  return oss;
}

/* Method save() stores the object through the StorageManager */
void AbdoRackwitz::save(Advocate & adv) const
{
  OptimizationSolverImplementation::save(adv);
  adv.saveAttribute("tau_", tau_);
  adv.saveAttribute("omega_", omega_);
  adv.saveAttribute("smooth_", smooth_);
  adv.saveAttribute("currentSigma_", currentSigma_);
  adv.saveAttribute("currentPoint_", currentPoint_);
  adv.saveAttribute("currentDirection_", currentDirection_);
  adv.saveAttribute("currentLevelValue_", currentLevelValue_);
  adv.saveAttribute("currentGradient_", currentGradient_);
  adv.saveAttribute("currentLambda_", currentLambda_);
}

/* Method load() reloads the object from the StorageManager */
void AbdoRackwitz::load(Advocate & adv)
{
  OptimizationSolverImplementation::load(adv);
  adv.loadAttribute("tau_", tau_);
  adv.loadAttribute("omega_", omega_);
  adv.loadAttribute("smooth_", smooth_);
  adv.loadAttribute("currentSigma_", currentSigma_);
  adv.loadAttribute("currentPoint_", currentPoint_);
  adv.loadAttribute("currentDirection_", currentDirection_);
  adv.loadAttribute("currentLevelValue_", currentLevelValue_);
  adv.loadAttribute("currentLambda_", currentLambda_);
}

END_NAMESPACE_OPENTURNS
