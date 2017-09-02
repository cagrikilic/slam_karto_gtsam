/*********************************************************************
*
*  Copyright (c) 2017, Saurav Agarwal
*  All rights reserved.
*
*********************************************************************/

/* Authors: Saurav Agarwal */

#include <limits>
#include <open_karto/Karto.h>
#include <ros/console.h>
#include "GTSAMSolver.h"

GTSAMSolver::GTSAMSolver()
{
  using namespace gtsam;

  // add the prior on the first node which is known
  noiseModel::Diagonal::shared_ptr priorNoise = noiseModel::Diagonal::Sigmas(Vector3(1e-6, 1e-6, 1e-8));
  
  graph_.emplace_shared<PriorFactor<Pose2> >(0, Pose2(0, 0, 0), priorNoise);

}

GTSAMSolver::~GTSAMSolver()
{
  
}

void GTSAMSolver::Clear()
{
  corrections_.clear();
}

const karto::ScanSolver::IdPoseVector& GTSAMSolver::GetCorrections() const
{
  return corrections_;
}

void GTSAMSolver::Compute()
{
  using namespace gtsam;

  corrections_.clear();

  GaussNewtonParams parameters;

  // Stop iterating once the change in error between steps is less than this value
  parameters.relativeErrorTol = 1e-5;
  
  // Do not perform more than N iteration steps
  parameters.maxIterations = 100;
  
  // Create the optimizer ...
  LevenbergMarquardtOptimizer optimizer(graph_, initialGuess_, parameters);
  
  // ... and optimize
  Values result = optimizer.optimize();

  // TODO: Put values in corrections
}

void GTSAMSolver::AddNode(karto::Vertex<karto::LocalizedRangeScan>* pVertex)
{
  
  using namespace gtsam;

  karto::Pose2 odom = pVertex->GetObject()->GetCorrectedPose();
  
  initialGuess_.insert(pVertex->GetObject()->GetUniqueId(), 
                        Pose2( odom.GetX(), odom.GetY(), odom.GetHeading() ));
  
  ROS_DEBUG("[gtsam] Adding node %d.", pVertex->GetObject()->GetUniqueId());

}

void GTSAMSolver::AddConstraint(karto::Edge<karto::LocalizedRangeScan>* pEdge)
{
  
  using namespace gtsam;

  // Set source and target
  int sourceID = pEdge->GetSource()->GetObject()->GetUniqueId();
  
  int targetID = pEdge->GetTarget()->GetObject()->GetUniqueId();
  
  // Set the measurement (poseGraphEdge distance between vertices)
  karto::LinkInfo* pLinkInfo = (karto::LinkInfo*)(pEdge->GetLabel());
  
  karto::Pose2 diff = pLinkInfo->GetPoseDifference();
  
  // Set the covariance of the measurement
  karto::Matrix3 precisionMatrix = pLinkInfo->GetCovariance()
  
  Eigen::Matrix<double,3,3> cov;
  
  cov(0,0) = precisionMatrix(0,0);
  
  cov(0,1) = cov(1,0) = precisionMatrix(0,1);
  
  cov(0,2) = cov(2,0) = precisionMatrix(0,2);
  
  cov(1,1) = precisionMatrix(1,1);
  
  cov(1,2) = cov(2,1) = precisionMatrix(1,2);
  
  cov(2,2) = precisionMatrix(2,2);
  
  noiseModel::Diagonal::shared_ptr model = noiseModel::Diagonal::Sigmas(Vector3(0.2, 0.2, 0.1));

  // Add odometry factors
  // Create odometry (Between) factors between consecutive poses
  graph_.emplace_shared<BetweenFactor<Pose2> >(sourceID, targetID, Pose2(diff.GetX(), diff.GetY(), diff.GetHeading()), model);
  
  // Add the constraint to the optimizer
  ROS_DEBUG("[gtsam] Adding Edge from node %d to node %d.", sourceID, targetID);
  
}

void GTSAMSolver::getGraph(std::vector<Eigen::Vector2d> &nodes, std::vector<std::pair<Eigen::Vector2d, Eigen::Vector2d> > &edges)
{
  using namespace gtsam;

  //HyperGraph::VertexIDMap vertexMap = optimizer_.vertices();

  //HyperGraph::EdgeSet edgeSet = optimizer_.edges();

  double *data = new double[3];

  for (SparseOptimizer::VertexIDMap::iterator it = optimizer_.vertices().begin(); it != optimizer_.vertices().end(); ++it) 
  {

    VertexSE2* v = dynamic_cast<VertexSE2*>(it->second);
    
    if(v) 
    {
      
      v->getEstimateData(data);

      Eigen::Vector2d pose(data[0], data[1]);

      nodes.push_back(pose);

    }
  }

  double *data1 = new double[3];

  double *data2 = new double[3];

  // for (SparseOptimizer::EdgeSet::iterator it = optimizer_.edges().begin(); it != optimizer_.edges().end(); ++it) 
  // {

  //   EdgeSE2* e = dynamic_cast<EdgeSE2*>(*it);
    
  //   if(e) 
  //   {
      
  //     VertexSE2* v1 = dynamic_cast<VertexSE2*>(e->vertices()[0]);

  //     v1->getEstimateData(data1);

  //     Eigen::Vector2d poseFrom(data1[0], data1[1]);

  //     VertexSE2* v2 = dynamic_cast<VertexSE2*>(e->vertices()[1]);

  //     v2->getEstimateData(data2);

  //     Eigen::Vector2d poseTo(data2[0], data2[1]);

  //     edges.push_back(std::make_pair(poseFrom, poseTo));

  //   }

  // }

  delete data;

  delete data1;

  delete data2;

}