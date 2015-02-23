#include "TestClasses.hpp"
#include "gtest/gtest.h"
#include <assert.h>

using namespace LWFTest;

typedef ::testing::Types<
    NonlinearTest,
    LinearTest
> TestClasses;

// The fixture for testing class UpdateModel
template<typename TestClass>
class UpdateModelTest : public ::testing::Test, public TestClass {
 public:
  UpdateModelTest() {
    this->init(this->testState_,this->testUpdateMeas_,this->testPredictionMeas_);
    this->testOutlierDetection_.setEnabledAll(true);
  }
  virtual ~UpdateModelTest() {
  }
  using typename TestClass::mtState;
  using typename TestClass::mtUpdateMeas;
  using typename TestClass::mtUpdateNoise;
  using typename TestClass::mtInnovation;
  using typename TestClass::mtPredictionNoise;
  using typename TestClass::mtPredictionMeas;
  using typename TestClass::mtUpdateExample;
  using typename TestClass::mtPredictionExample;
  using typename TestClass::mtPredictAndUpdateExample;
  using typename TestClass::mtOutlierDetectionExample;
  mtUpdateExample testUpdate_;
  mtPredictAndUpdateExample testPredictAndUpdate_;
  mtPredictionExample testPrediction_;
  mtState testState_;
  mtUpdateMeas testUpdateMeas_;
  mtPredictionMeas testPredictionMeas_;
  mtOutlierDetectionExample testOutlierDetection_;
  const double dt_ = 0.1;
};

TYPED_TEST_CASE(UpdateModelTest, TestClasses);

// Test constructors
TYPED_TEST(UpdateModelTest, constructors) {
  typename TestFixture::mtUpdateExample testUpdate;
  ASSERT_EQ(testUpdate.mode_,LWF::UpdateEKF);
  bool coupledToPrediction = testUpdate.coupledToPrediction_;
  ASSERT_EQ(coupledToPrediction,false);
  ASSERT_EQ((testUpdate.updnoiP_-TestFixture::mtUpdateExample::mtNoise::mtCovMat::Identity()*0.0001).norm(),0.0);
  typename TestFixture::mtUpdateExample::mtNoise::mtDifVec dif;
  typename TestFixture::mtUpdateExample::mtNoise noise;
  noise.setIdentity();
  testUpdate.stateSigmaPointsNoi_.getMean().boxMinus(noise,dif);
  ASSERT_NEAR(dif.norm(),0.0,1e-6);
  ASSERT_NEAR((testUpdate.updnoiP_-testUpdate.stateSigmaPointsNoi_.getCovarianceMatrix()).norm(),0.0,1e-8);
  typename TestFixture::mtPredictAndUpdateExample testPredictAndUpdate;
  ASSERT_EQ(testPredictAndUpdate.mode_,LWF::UpdateEKF);
  coupledToPrediction = testPredictAndUpdate.coupledToPrediction_;
  ASSERT_EQ(coupledToPrediction,true);
  ASSERT_EQ((testPredictAndUpdate.updnoiP_-TestFixture::mtPredictAndUpdateExample::mtNoise::mtCovMat::Identity()*0.0001).norm(),0.0);
}

// Test finite difference Jacobians
TYPED_TEST(UpdateModelTest, FDjacobians) {
  typename TestFixture::mtUpdateExample::mtJacInput F = this->testUpdate_.jacInputFD(this->testState_,this->testUpdateMeas_,this->dt_,0.0000001);
  typename TestFixture::mtUpdateExample::mtJacNoise Fn = this->testUpdate_.jacNoiseFD(this->testState_,this->testUpdateMeas_,this->dt_,0.0000001);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR((F-this->testUpdate_.jacInput(this->testState_,this->testUpdateMeas_,this->dt_)).norm(),0.0,1e-5);
      ASSERT_NEAR((Fn-this->testUpdate_.jacNoise(this->testState_,this->testUpdateMeas_,this->dt_)).norm(),0.0,1e-5);
      break;
    case 1:
      ASSERT_NEAR((F-this->testUpdate_.jacInput(this->testState_,this->testUpdateMeas_,this->dt_)).norm(),0.0,1e-8);
      ASSERT_NEAR((Fn-this->testUpdate_.jacNoise(this->testState_,this->testUpdateMeas_,this->dt_)).norm(),0.0,1e-8);
      break;
    default:
      ASSERT_NEAR((F-this->testUpdate_.jacInput(this->testState_,this->testUpdateMeas_,this->dt_)).norm(),0.0,1e-5);
      ASSERT_NEAR((Fn-this->testUpdate_.jacNoise(this->testState_,this->testUpdateMeas_,this->dt_)).norm(),0.0,1e-5);
  }
}

// Test performUpdateEKF
TYPED_TEST(UpdateModelTest, performUpdateEKF) {
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat updateCov;
  cov.setIdentity();
  typename TestFixture::mtUpdateExample::mtJacInput H = this->testUpdate_.jacInput(this->testState_,this->testUpdateMeas_,this->dt_);
  typename TestFixture::mtUpdateExample::mtJacNoise Hn = this->testUpdate_.jacNoise(this->testState_,this->testUpdateMeas_,this->dt_);

  typename TestFixture::mtUpdateExample::mtInnovation y;
  this->testUpdate_.eval(y,this->testState_,this->testUpdateMeas_);
  typename TestFixture::mtUpdateExample::mtInnovation yIdentity;
  yIdentity.setIdentity();
  typename TestFixture::mtUpdateExample::mtInnovation::mtDifVec innVector;

  typename TestFixture::mtUpdateExample::mtState state;
  typename TestFixture::mtUpdateExample::mtState stateUpdated;
  state = this->testState_;

  // Update
  typename TestFixture::mtUpdateExample::mtInnovation::mtCovMat Py = H*cov*H.transpose() + Hn*this->testUpdate_.updnoiP_*Hn.transpose();
  y.boxMinus(yIdentity,innVector);
  typename TestFixture::mtUpdateExample::mtInnovation::mtCovMat Pyinv = Py.inverse();

  // Kalman Update
  Eigen::Matrix<double,TestFixture::mtUpdateExample::mtState::D_,TestFixture::mtUpdateExample::mtInnovation::D_> K = cov*H.transpose()*Pyinv;
  updateCov = cov - K*Py*K.transpose();
  typename TestFixture::mtUpdateExample::mtState::mtDifVec updateVec;
  updateVec = -K*innVector;
  state.boxPlus(updateVec,stateUpdated);

  this->testUpdate_.performUpdateEKF(state,cov,this->testUpdateMeas_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state.boxMinus(stateUpdated,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-6);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-6);
  }
}

// Test updateEKFWithOutlier
TYPED_TEST(UpdateModelTest, updateEKFWithOutlier) {
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat updateCov;
  cov.setIdentity();
  typename TestFixture::mtUpdateExample::mtJacInput H = this->testUpdate_.jacInput(this->testState_,this->testUpdateMeas_,this->dt_);
  typename TestFixture::mtUpdateExample::mtJacNoise Hn = this->testUpdate_.jacNoise(this->testState_,this->testUpdateMeas_,this->dt_);

  typename TestFixture::mtUpdateExample::mtInnovation y;
  this->testUpdate_.eval(y,this->testState_,this->testUpdateMeas_);
  typename TestFixture::mtUpdateExample::mtInnovation yIdentity;
  yIdentity.setIdentity();
  typename TestFixture::mtUpdateExample::mtInnovation::mtDifVec innVector;

  typename TestFixture::mtUpdateExample::mtState state;
  typename TestFixture::mtUpdateExample::mtState stateUpdated;
  state = this->testState_;

  // Update
  typename TestFixture::mtUpdateExample::mtInnovation::mtCovMat Py = H*cov*H.transpose() + Hn*this->testUpdate_.updnoiP_*Hn.transpose();
  y.boxMinus(yIdentity,innVector);
  Py.block(0,0,TestFixture::mtUpdateExample::mtInnovation::D_,3).setZero();
  Py.block(0,0,3,TestFixture::mtUpdateExample::mtInnovation::D_).setZero();
  Py.block(0,0,3,3).setIdentity();
  H.block(0,0,3,TestFixture::mtUpdateExample::mtState::D_).setZero();
  typename TestFixture::mtUpdateExample::mtInnovation::mtCovMat Pyinv = Py.inverse();

  // Kalman Update
  Eigen::Matrix<double,TestFixture::mtUpdateExample::mtState::D_,TestFixture::mtUpdateExample::mtInnovation::D_> K = cov*H.transpose()*Pyinv;
  updateCov = cov - K*Py*K.transpose();
  typename TestFixture::mtUpdateExample::mtState::mtDifVec updateVec;
  updateVec = -K*innVector;
  state.boxPlus(updateVec,stateUpdated);

  this->testUpdate_.performUpdateEKF(state,cov,this->testUpdateMeas_,&this->testOutlierDetection_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state.boxMinus(stateUpdated,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-6);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-6);
  }
}

// Test compareUpdate
TYPED_TEST(UpdateModelTest, compareUpdate) {
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.000001;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov2 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.000001;
  typename TestFixture::mtUpdateExample::mtState state1 = this->testState_;
  typename TestFixture::mtUpdateExample::mtState state2 = this->testState_;
  this->testUpdate_.performUpdateEKF(state1,cov1,this->testUpdateMeas_);
  this->testUpdate_.performUpdateUKF(state2,cov2,this->testUpdateMeas_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-5);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-5);
  }

  // Test with outlier detection
  cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.000001;
  cov2 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.000001;
  state1 = this->testState_;
  state2 = this->testState_;
  this->testUpdate_.performUpdateEKF(state1,cov1,this->testUpdateMeas_,&this->testOutlierDetection_);
  this->testUpdate_.performUpdateUKF(state2,cov2,this->testUpdateMeas_,&this->testOutlierDetection_);
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-5);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-5);
  }
}

// Test performPredictionAndUpdateEKF
TYPED_TEST(UpdateModelTest, performPredictionAndUpdateEKF) {
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.000001;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov2 = cov1;
  typename TestFixture::mtUpdateExample::mtState state1 = this->testState_;
  typename TestFixture::mtUpdateExample::mtState state2 = this->testState_;
  this->testPrediction_.performPredictionEKF(state1,cov1,this->testPredictionMeas_,this->dt_);
  this->testUpdate_.performUpdateEKF(state1,cov1,this->testUpdateMeas_);
  this->testPredictAndUpdate_.performPredictionAndUpdateEKF(state2,cov2,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-8);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-8);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-8);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-8);
  }

  // With outlier
  cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.000001;
  cov2 = cov1;
  state1 = this->testState_;
  state2 = this->testState_;
  this->testPrediction_.performPredictionEKF(state1,cov1,this->testPredictionMeas_,this->dt_);
  this->testUpdate_.performUpdateEKF(state1,cov1,this->testUpdateMeas_,&this->testOutlierDetection_);
  this->testPredictAndUpdate_.performPredictionAndUpdateEKF(state2,cov2,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_,&this->testOutlierDetection_);
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-8);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-8);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-8);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-8);
  }
}

// Test performPredictionAndUpdateUKF
TYPED_TEST(UpdateModelTest, performPredictionAndUpdateUKF) {
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.000001;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov2 = cov1;
  typename TestFixture::mtUpdateExample::mtState state1 = this->testState_;
  typename TestFixture::mtUpdateExample::mtState state2 = this->testState_;
  this->testPrediction_.performPredictionUKF(state1,cov1,this->testPredictionMeas_,this->dt_);
  this->testUpdate_.performUpdateUKF(state1,cov1,this->testUpdateMeas_);
  this->testPredictAndUpdate_.performPredictionAndUpdateUKF(state2,cov2,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-4); // Increased difference comes because of different sigma points
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-6);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,2e-10);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-4);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-6);
  }

  // With outlier
  cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.000001;
  cov2 = cov1;
  state1 = this->testState_;
  state2 = this->testState_;
  this->testPrediction_.performPredictionUKF(state1,cov1,this->testPredictionMeas_,this->dt_);
  this->testUpdate_.performUpdateUKF(state1,cov1,this->testUpdateMeas_,&this->testOutlierDetection_);
  this->testPredictAndUpdate_.performPredictionAndUpdateUKF(state2,cov2,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_,&this->testOutlierDetection_);
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-4); // Increased difference comes because of different sigma points
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-6);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,2e-10);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-4);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-6);
  }
}

// Test comparePredictAndUpdate (including correlated noise)
TYPED_TEST(UpdateModelTest, comparePredictAndUpdate) {
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.0001;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov2 = cov1;
  typename TestFixture::mtUpdateExample::mtState state1 = this->testState_;
  typename TestFixture::mtUpdateExample::mtState state2 = this->testState_;
  this->testPredictAndUpdate_.performPredictionAndUpdateEKF(state1,cov1,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_);
  this->testPredictAndUpdate_.performPredictionAndUpdateUKF(state2,cov2,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,2e-2);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,8e-5);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-9);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-9);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,2e-2);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,8e-5);
  }

  // Negativ Control (Based on above)
  cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.0001;
  cov2 = cov1;
  state1 = this->testState_;
  state2 = this->testState_;
  this->testPredictAndUpdate_.performPredictionAndUpdateEKF(state1,cov1,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_);
  this->testPredictAndUpdate_.preupdnoiP_.block(0,0,3,3) = Eigen::Matrix3d::Identity()*0.00009;
  this->testPredictAndUpdate_.performPredictionAndUpdateUKF(state2,cov2,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_);
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_TRUE(dif.norm()>1e-1);
      ASSERT_TRUE((cov1-cov2).norm()>7e-5); // Bad discremination for nonlinear case
      break;
    case 1:
      ASSERT_TRUE(dif.norm()>1e-1);
      ASSERT_TRUE((cov1-cov2).norm()>1e-5);
      break;
    default:
      ASSERT_TRUE(dif.norm()>1e-1);
      ASSERT_TRUE((cov1-cov2).norm()>7e-5);
  }

  cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.0001;
  cov2 = cov1;
  state1 = this->testState_;
  state2 = this->testState_;
  this->testPredictAndUpdate_.performPredictionAndUpdateEKF(state1,cov1,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_);
  this->testPredictAndUpdate_.performPredictionAndUpdateUKF(state2,cov2,this->testUpdateMeas_,this->testPrediction_,this->testPredictionMeas_,this->dt_);
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,2e-2);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,7e-5);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-9);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-9);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,2e-2);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,7e-5);
  }
}

// Test performUpdateLEKF1 (linearization point = prediction)
TYPED_TEST(UpdateModelTest, performUpdateLEKF1) {
  // Linearization point
  typename TestFixture::mtUpdateExample::mtState linState;
  linState = this->testState_;

  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.0001;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov2 = cov1;
  typename TestFixture::mtUpdateExample::mtState state1 = this->testState_;
  typename TestFixture::mtUpdateExample::mtState state2 = this->testState_;
  linState.boxMinus(state1,state1.difVecLin_);
  this->testUpdate_.useSpecialLinearizationPoint_ = true;
  this->testUpdate_.performUpdateEKF(state1,cov1,this->testUpdateMeas_);
  this->testUpdate_.useSpecialLinearizationPoint_ = false;
  this->testUpdate_.performUpdateEKF(state2,cov2,this->testUpdateMeas_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,2e-2);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,8e-5);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-9);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-9);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,2e-2);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,8e-5);
  }
}

// Test performUpdateLEKF2 (linearization point <> prediction, but for linear case still the same result)
TYPED_TEST(UpdateModelTest, performUpdateLEKF2) {
  // Linearization point
  typename TestFixture::mtUpdateExample::mtState linState;
  linState = this->testState_;

  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.0001;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov2 = cov1;
  typename TestFixture::mtUpdateExample::mtState state1 = this->testState_;
  typename TestFixture::mtUpdateExample::mtState state2 = this->testState_;
  linState.boxMinus(state1,state1.difVecLin_);
  this->testUpdate_.useSpecialLinearizationPoint_ = true;
  this->testUpdate_.performUpdateEKF(state1,cov1,this->testUpdateMeas_);
  this->testUpdate_.useSpecialLinearizationPoint_ = false;
  this->testUpdate_.performUpdateEKF(state2,cov2,this->testUpdateMeas_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      // No ASSERT
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-10);
      break;
    default:
      // No ASSERT
      break;
  }
}

// Test performUpdateLEKF3 (linearization point <> prediction, general)
TYPED_TEST(UpdateModelTest, performUpdateLEKF3) {
  // Linearization point
  typename TestFixture::mtUpdateExample::mtState linState;
  unsigned int s = 2;
  linState.setRandom(s);

  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat updateCov;
  cov.setIdentity();
  typename TestFixture::mtUpdateExample::mtJacInput H = this->testUpdate_.jacInput(linState,this->testUpdateMeas_,this->dt_);
  typename TestFixture::mtUpdateExample::mtJacNoise Hn = this->testUpdate_.jacNoise(linState,this->testUpdateMeas_,this->dt_);

  typename TestFixture::mtUpdateExample::mtInnovation y;
  this->testUpdate_.eval(y,linState,this->testUpdateMeas_);
  typename TestFixture::mtUpdateExample::mtInnovation yIdentity;
  yIdentity.setIdentity();
  typename TestFixture::mtUpdateExample::mtInnovation::mtDifVec innVector;

  typename TestFixture::mtUpdateExample::mtState state;
  typename TestFixture::mtUpdateExample::mtState stateUpdated;
  state = this->testState_;

  // Update
  typename TestFixture::mtUpdateExample::mtInnovation::mtCovMat Py = H*cov*H.transpose() + Hn*this->testUpdate_.updnoiP_*Hn.transpose();
  y.boxMinus(yIdentity,innVector);
  typename TestFixture::mtUpdateExample::mtInnovation::mtCovMat Pyinv = Py.inverse();

  // Kalman Update
  Eigen::Matrix<double,TestFixture::mtUpdateExample::mtState::D_,TestFixture::mtUpdateExample::mtInnovation::D_> K = cov*H.transpose()*Pyinv;
  updateCov = cov - K*Py*K.transpose();
  typename TestFixture::mtUpdateExample::mtState::mtDifVec updateVec;
  typename TestFixture::mtUpdateExample::mtState::mtDifVec difVecLin;
  linState.boxMinus(state,difVecLin);
  updateVec = -K*(innVector-H*difVecLin);
  state.boxPlus(updateVec,stateUpdated);

  linState.boxMinus(state,state.difVecLin_);
  this->testUpdate_.useSpecialLinearizationPoint_ = true;
  this->testUpdate_.performUpdateEKF(state,cov,this->testUpdateMeas_);
  this->testUpdate_.useSpecialLinearizationPoint_ = false;
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state.boxMinus(stateUpdated,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-6);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-6);
      break;
  }
}

// Test performUpdateIEKF1 (for linear case still the same result)
TYPED_TEST(UpdateModelTest, performUpdateIEKF1) {
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov1 = TestFixture::mtUpdateExample::mtState::mtCovMat::Identity()*0.0001;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov2 = cov1;
  typename TestFixture::mtUpdateExample::mtState state1 = this->testState_;
  typename TestFixture::mtUpdateExample::mtState state2 = this->testState_;
  this->testUpdate_.performUpdateIEKF(state1,cov1,this->testUpdateMeas_);
  this->testUpdate_.performUpdateEKF(state2,cov2,this->testUpdateMeas_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state1.boxMinus(state2,dif);
  switch(TestFixture::id_){
    case 0:
      // No ASSERT
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov1-cov2).norm(),0.0,1e-10);
      break;
    default:
      // No ASSERT
      break;
  }
}

// Test performUpdateIEKF2 (general test)
TYPED_TEST(UpdateModelTest, performUpdateIEKF2) {
  // Linearization point
  typename TestFixture::mtUpdateExample::mtState linState;
  linState = this->testState_;

  typename TestFixture::mtUpdateExample::mtState::mtCovMat cov;
  typename TestFixture::mtUpdateExample::mtState::mtCovMat updateCov;
  cov.setIdentity();
  typename TestFixture::mtUpdateExample::mtJacInput H;
  typename TestFixture::mtUpdateExample::mtJacNoise Hn;

  typename TestFixture::mtUpdateExample::mtInnovation y;
  typename TestFixture::mtUpdateExample::mtInnovation yIdentity;
  yIdentity.setIdentity();
  typename TestFixture::mtUpdateExample::mtInnovation::mtDifVec innVector;

  typename TestFixture::mtUpdateExample::mtState state;
  typename TestFixture::mtUpdateExample::mtState stateUpdated;
  state = this->testState_;

  typename TestFixture::mtUpdateExample::mtInnovation::mtCovMat Py;
  typename TestFixture::mtUpdateExample::mtInnovation::mtCovMat Pyinv;
  Eigen::Matrix<double,TestFixture::mtUpdateExample::mtState::D_,TestFixture::mtUpdateExample::mtInnovation::D_> K;
  typename TestFixture::mtUpdateExample::mtState::mtDifVec updateVec;
  typename TestFixture::mtUpdateExample::mtState::mtDifVec difVecLin;

  double updateVecNorm = this->testUpdate_.updateVecNormTermination_;
  for(unsigned int i=0;i<this->testUpdate_.maxNumIteration_ & updateVecNorm>=this->testUpdate_.updateVecNormTermination_;i++){
    H = this->testUpdate_.jacInput(linState,this->testUpdateMeas_,this->dt_);
    Hn = this->testUpdate_.jacNoise(linState,this->testUpdateMeas_,this->dt_);

    this->testUpdate_.eval(y,linState,this->testUpdateMeas_);

    // Update
    Py = H*cov*H.transpose() + Hn*this->testUpdate_.updnoiP_*Hn.transpose();
    y.boxMinus(yIdentity,innVector);
    Pyinv = Py.inverse();

    // Kalman Update
    K = cov*H.transpose()*Pyinv;
    linState.boxMinus(state,difVecLin);
    updateVec = -K*(innVector-H*difVecLin);
    state.boxPlus(updateVec,linState);
    updateVecNorm = updateVec.norm();
  }
  stateUpdated = linState;
  updateCov = cov - K*Py*K.transpose();

  this->testUpdate_.performUpdateIEKF(state,cov,this->testUpdateMeas_);
  typename TestFixture::mtUpdateExample::mtState::mtDifVec dif;
  state.boxMinus(stateUpdated,dif);
  switch(TestFixture::id_){
    case 0:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-6);
      break;
    case 1:
      ASSERT_NEAR(dif.norm(),0.0,1e-10);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-10);
      break;
    default:
      ASSERT_NEAR(dif.norm(),0.0,1e-6);
      ASSERT_NEAR((cov-updateCov).norm(),0.0,1e-6);
      break;
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
