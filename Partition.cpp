#include "Partition.h"

/*
  Constructor
*/
Partition::Partition(MasterProblem m, Subproblem s) {
  // How do we create partition?
  masterProb = m;
  subProb = s;
}

/*
  Destructor
*/
Partition::~Partition() {
  // Need to destroy partition?
  masterProb.~MasterProblem();
  subProb.~Subproblem();
}

//Took Out const vector<Component>& partition,
void Partition::setAggregatedBounds(const TSLP& prob, IloNumArray& secvarlb, IloNumArray& secvarub, IloNumArray& secconstrbd)
{
	for (int i = 0; i < partition.size(); ++i)
	{
		// set aggregated variable bound according to partition
		for (int j = 0; j < prob.nbSecVars; ++j)
		{
			bool lbinfflag = 0;
			bool ubinfflag = 0;
			double templb = 0.0;
			double tempub = 0.0;
			for (int k = 0; k < partition[i].indices.size(); ++k)
			{
				if (prob.secondvarlb[partition[i].indices[k] * prob.nbSecVars + j] == -IloInfinity)
				{
					lbinfflag = 1;
					break;
				}
				else
					templb += prob.secondvarlb[partition[i].indices[k] * prob.nbSecVars + j];
			}
			for (int k = 0; k < partition[i].indices.size(); ++k)
			{
				if (prob.secondvarub[partition[i].indices[k] * prob.nbSecVars + j] == IloInfinity)
				{
					ubinfflag = 1;
					break;
				}
				else
					tempub += prob.secondvarub[partition[i].indices[k] * prob.nbSecVars + j];
			}
			if (lbinfflag == 1)
				secvarlb[i * prob.nbSecVars + j] = -IloInfinity;
			else
				secvarlb[i * prob.nbSecVars + j] = templb;
			if (ubinfflag == 1)
				secvarub[i * prob.nbSecVars + j] = IloInfinity;
			else
				secvarub[i * prob.nbSecVars + j] = tempub;
		}
		// set second stage constraint bounds according to partition
		for (int d = 0; d < prob.nbSecRows; ++d)
		{
			double tempbd = 0.0;
			for (int k = 0; k < partition[i].indices.size(); ++k)
				tempbd += prob.secondconstrbd[partition[i].indices[k] * prob.nbSecRows + d];
			secconstrbd[i * prob.nbSecRows + d] = tempbd;
		}
	}
}

bool Partition::addToCollection(const VectorXf& dualvec, vector<DualInfo>& dualInfoCollection)
{
	// try to add dualvec into dualCollection: if it is not parallel to any other vector in the collection
	bool flag = 1;
	for (int j = 0; j < dualInfoCollection.size(); ++j)
	{
		double par = dualvec.dot(dualInfoCollection[j].dualvec) * 1.0 / (dualvec.norm() * dualInfoCollection[j].dualvec.norm());
		if (par > 1 - 1e-3)
		{
			flag = 0;
			break;
		}
	}
	return flag;
}

//Took Out const vector<Component>& partition,
void Partition::add_feas_cuts(IloEnv& env, TSLP& prob, IloModel& model, const IloNumVarArray& x, const IloNumArray& xvals, double subobjval, const VectorXf& dualvec, int i)
{
	// Add feasibility cuts
	vector<double> feas_cut_coef(prob.nbFirstVars, 0);
	double sum_xvals = 0.0;
	for (int ii = 0; ii < prob.nbSecRows; ++ii)
	{
		for (int j = 0; j < prob.nbPerRow[ii]; ++j)
		{
			if (prob.CoefInd[ii][j] < prob.nbFirstVars)
			{
				feas_cut_coef[prob.CoefInd[ii][j]] += prob.CoefMat[ii][j] * dualvec[ii] * partition[i].indices.size();
				sum_xvals += prob.CoefMat[ii][j] * dualvec[ii] * partition[i].indices.size() * xvals[prob.CoefInd[ii][j]];
			}
		}
	}
	IloExpr lhsfeas(env);
	for (int j = 0; j < prob.nbFirstVars; ++j)
	{
		if (fabs(feas_cut_coef[j]) > 1e-7)
			lhsfeas += feas_cut_coef[j] * x[j];
	}
	model.add(lhsfeas >= subobjval + sum_xvals);
	lhsfeas.end();
}

//Took Out const vector<Component>& partition,
double Partition::subprob_partition(IloNumArray& secvarlb, IloNumArray& secvarub, const TSLP& prob, const IloNumArray& xvals, IloNumArray& duals, int k, bool& feasflag)
{
	// Set constraint bounds
	for (int i = 0; i < prob.nbSecRows; ++i)
	{
		double bd = 0;
		for (int l = 0; l < partition[k].indices.size(); ++l)
		{
			double subbd = prob.secondconstrbd[partition[k].indices[l] * prob.nbSecRows + i];
			for (int j = 0; j < prob.nbPerRow[i]; ++j)
			{
				int ind = prob.CoefInd[i][j];
				if (ind < prob.nbFirstVars)
					subbd -= prob.CoefMat[i][j] * xvals[ind];
			}
			bd += subbd;
		}
		if (prob.secondconstrsense[i] == -1)
			subProb.suboptcon[i].setLB(bd);
		if (prob.secondconstrsense[i] == 0)
		{
			if (subProb.suboptcon[i].getUB() < bd)
			{
				subProb.suboptcon[i].setUB(bd);
				subProb.suboptcon[i].setLB(bd);
			}
			else
			{
				subProb.suboptcon[i].setLB(bd);
				subProb.suboptcon[i].setUB(bd);
			}
		}
		if (prob.secondconstrsense[i] == 1)
			subProb.suboptcon[i].setUB(bd);
	}
	// Set variable bounds
	for (int j = 0; j < prob.nbSecVars; ++j)
	{
		if (secvarlb[k * prob.nbSecVars + j] != -IloInfinity)
			subProb.suboptcon[prob.nbSecRows + j].setLB(secvarlb[k * prob.nbSecVars + j]);
		else
			subProb.suboptcon[prob.nbSecRows + j].setLB(-IloInfinity);
		if (secvarub[k * prob.nbSecVars + j] != IloInfinity)
			subProb.suboptcon[prob.nbSecRows + prob.nbSecVars + j].setLB(-secvarub[k * prob.nbSecVars + j]);
		else
			subProb.suboptcon[prob.nbSecRows + prob.nbSecVars + j].setLB(-IloInfinity);
	}
	subProb.suboptcplex.solve();
	double returnval;
	if (subProb.suboptcplex.getStatus() == IloAlgorithm::Optimal)
	{
		returnval = subProb.suboptcplex.getObjValue();
		subProb.suboptcplex.getDuals(duals, subProb.suboptcon);
		feasflag = 1;
	}
	else
	{
		feasflag = 0;
		// infeasible! Get extreme rays
		for (int i = 0; i < prob.nbSecRows; ++i)
		{
			double bd = 0;
			for (int l = 0; l < partition[k].indices.size(); ++l)
			{
				double subbd = prob.secondconstrbd[partition[k].indices[l] * prob.nbSecRows + i];
				for (int j = 0; j < prob.nbPerRow[i]; ++j)
				{
					int ind = prob.CoefInd[i][j];
					if (ind < prob.nbFirstVars)
						subbd -= prob.CoefMat[i][j] * xvals[ind];
				}
				bd += subbd;
			}
			if (prob.secondconstrsense[i] == -1)
				subProb.subfeascon[i].setLB(bd);
			if (prob.secondconstrsense[i] == 0)
			{
				if (subProb.subfeascon[i].getUB() < bd)
				{
					subProb.subfeascon[i].setUB(bd);
					subProb.subfeascon[i].setLB(bd);
				}
				else
				{
					subProb.subfeascon[i].setLB(bd);
					subProb.subfeascon[i].setUB(bd);
				}
			}
			if (prob.secondconstrsense[i] == 1)
				subProb.subfeascon[i].setUB(bd);
		}
		for (int j = 0; j < prob.nbSecVars; ++j)
		{
			if (secvarlb[k * prob.nbSecVars + j] != -IloInfinity)
				subProb.subfeascon[prob.nbSecRows + j].setLB(secvarlb[k * prob.nbSecVars + j]);
			else
				subProb.subfeascon[prob.nbSecRows + j].setLB(-IloInfinity);
			if (secvarub[k * prob.nbSecVars + j] != IloInfinity)
				subProb.subfeascon[prob.nbSecRows + prob.nbSecVars + j].setLB(-secvarub[k * prob.nbSecVars + j]);
			else
				subProb.subfeascon[prob.nbSecRows + prob.nbSecVars + j].setLB(-IloInfinity);
		}
		subProb.subfeascplex.solve();
		subProb.subfeascplex.getDuals(duals, subProb.subfeascon);
		returnval = subProb.subfeascplex.getObjValue();
	}
	return returnval;
}

void Partition::simple_refine(const Component& component, const TSLP& prob, const vector<IloNumArray>& extreme_points, const vector<int>& extreme_points_ind, const vector<IloNumArray>& extreme_rays, const vector<int>& extreme_rays_ind, vector<Component>& new_partition, vector< vector<int> >& extreme_ray_map)
{
	// Simple refinement strategy by putting vectors together according to a distance threshold
	vector<int> index_points_represent, index_rays_represent; // list of representing indices
	vector< vector<int> > index_points, index_rays; // index globally defined
	for (int i = 0; i < extreme_points.size(); ++i)
	{
		if (i == 0)
		{
			vector<int> newlist;
			newlist.push_back(extreme_points_ind[i]);
			index_points_represent.push_back(i);
			index_points.push_back(newlist);
		}
		else
		{
			bool distinctflag = 1;
			for (int j = 0; j < index_points_represent.size(); ++j)
			{
				bool tempflag = compare_arrays(prob, extreme_points[index_points_represent[j]], extreme_points[i]);
				if (tempflag == 1)
				{
					index_points[j].push_back(extreme_points_ind[i]);
					distinctflag = 0;
					break;
				}
			}
			if (distinctflag == 1)
			{
				vector<int> newlist;
				newlist.push_back(extreme_points_ind[i]);
				index_points_represent.push_back(i);
				index_points.push_back(newlist);
			}
		}
	}

	for (int i = 0; i < extreme_rays.size(); ++i)
	{
		if (i == 0)
		{
			// Since this is the first one, create a group starting with this one
			vector<int> newlist;
			newlist.push_back(extreme_rays_ind[i]);
			index_rays_represent.push_back(i);
			index_rays.push_back(newlist);
			vector<int> temp;
			temp.push_back(i);
			extreme_ray_map.push_back(temp);
		}
		else
		{
			bool distinctflag = 1;
			for (int j = 0; j < index_rays_represent.size(); ++j)
			{
				bool tempflag = compare_arrays(prob, extreme_rays[index_rays_represent[j]], extreme_rays[i]);
				if (tempflag == 1)
				{
					index_rays[j].push_back(extreme_rays_ind[i]);
					extreme_ray_map[j].push_back(i);
					distinctflag = 0;
					break;
				}
			}
			if (distinctflag == 1)
			{
				vector<int> newlist;
				newlist.push_back(extreme_rays_ind[i]);
				index_rays_represent.push_back(i);
				index_rays.push_back(newlist);
				vector<int> temp;
				temp.push_back(i);
				extreme_ray_map.push_back(temp);
			}
		}
	}
	for (int j = 0; j < index_rays.size(); ++j)
	{
		Component compo;
		compo.indices = index_rays[j];
		new_partition.push_back(compo);
	}
	for (int j = 0; j < index_points.size(); ++j)
	{
		Component compo;
		compo.indices = index_points[j];
		new_partition.push_back(compo);
	}
}

bool Partition::compare_arrays(const TSLP& prob, const IloNumArray& array1, const IloNumArray& array2)
{
	// check if they are "equal" to each other
	bool returnflag = 1;
	for (int j = 0; j < array1.getSize(); ++j)
	{
		if (fabs((array1[j] - array2[j]) * 1.0 / (array1[j] + 1e-5)) > prob.distinct_par)
		{
			returnflag = 0;
			break;
		}
	}
	return returnflag;
}

void Partition::gen_feasibility_cuts(IloEnv& env, const TSLP& prob, const IloNumArray& xvals, const vector<int>& extreme_ray_map, const vector<IloNumArray>& extreme_rays, const vector<int>& extreme_rays_ind, const double sum_of_infeas, IloModel& model, const IloNumVarArray& x)
{
	vector<double> feas_cut_coef(prob.nbFirstVars, 0);
	double sum_xvals = 0.0;
	for (int j = 0; j < extreme_ray_map.size(); ++j)
	{
		int ind = extreme_ray_map[j];
		for (int i = 0; i < prob.nbSecRows; ++i)
		{
			for (int j = 0; j < prob.nbPerRow[i]; ++j)
			{
				if (prob.CoefInd[i][j] < prob.nbFirstVars)
				{
					feas_cut_coef[prob.CoefInd[i][j]] += prob.CoefMat[i][j] * extreme_rays[ind][i];
					sum_xvals += prob.CoefMat[i][j] * extreme_rays[ind][i] * xvals[prob.CoefInd[i][j]];
				}
			}
		}
	}
	IloExpr lhs(env);
	for (int j = 0; j < prob.nbFirstVars; ++j)
	{
		if (fabs(feas_cut_coef[j]) > 1e-7)
			lhs += x[j] * feas_cut_coef[j];
	}
	model.add(lhs >= sum_of_infeas + sum_xvals);
	lhs.end();
}



// public






 /*

 */
double Partition::solve_warmstart(IloEnv& env, const TSLP& prob, const vector<int>& samples, const IloNumArray& stab_center, const vector<DualInfo>& dualInfoCollection, vector< vector<double> >& cutcoefs, vector<double>& cutrhs, vector<Component>& partition, const vector<VectorXf>& rhsvecs, IloNumArray& xvals, IloTimer& clock, STAT& stat) {
  // first stage constraints
  for (int i = 0; i < prob.firstconstrind.getSize(); ++i)
  {
    IloExpr lhs(env);
    for (int j = 0; j < prob.firstconstrind[i].getSize(); ++j)
      // Perform refinement
      lhs += masterProb.x[prob.firstconstrind[i][j]] * prob.firstconstrcoef[i][j];
    IloRange range(env, prob.firstconstrlb[i], lhs, prob.firstconstrub[i]);
    masterProb.model.add(range);
    lhs.end();
  }
  IloExpr obj(env);
  for (int i = 0; i < prob.nbFirstVars; ++i)
    obj += masterProb.x[i] * prob.objcoef[i];
  obj += masterProb.theta * 1.0 / (samples.size());
  masterProb.model.add(IloMinimize(env, obj));
  obj.end();

  masterProb.getCplex().setParam(IloCplex::TiLim, 3600);
  masterProb.getCplex().setParam(IloCplex::Threads, 1);
  masterProb.getCplex().setParam(IloCplex::BarDisplay, 0);
  masterProb.getCplex().setParam(IloCplex::SimDisplay, 0);
  masterProb.getCplex().setOut(env.getNullStream());
  // Assembly the matrix to do matrix-vector multiplication instead of componentwise inner products
  int nbRows = dualInfoCollection.size();
  MatrixXf dualvecMatrix(nbRows, prob.nbSecRows);
  MatrixXf coefvecMatrix(nbRows, prob.nbFirstVars);
  VectorXf rhsVector(nbRows);
  for (int l = 0; l < dualInfoCollection.size(); ++l)
  {
    dualvecMatrix.row(l) = dualInfoCollection[l].dualvec;
    coefvecMatrix.row(l) = dualInfoCollection[l].coefvec;
    rhsVector(l) = dualInfoCollection[l].rhs;
  }
  // Initial cut
  VectorXf xiterate(prob.nbFirstVars);
  for (int j = 0; j < prob.nbFirstVars; ++j)
    xiterate[j] = stab_center[j];
  VectorXf initialCutCoef(prob.nbFirstVars);
  initialCutCoef.setZero();
  double initialCutRhs = 0;
  double tempTime = clock.getTime();
  for (int k = 0; k < samples.size(); ++k)
  {
    // Go through all scenarios
    VectorXf vals = dualvecMatrix * rhsvecs[samples[k]] - coefvecMatrix * xiterate + rhsVector;
    int maxind;
    double maxval = vals.maxCoeff(&maxind);
    /*
    double maxval = -1e8;
    int maxind = -1;
    for (int l = 0; l < dualInfoCollection.size(); ++l)
    {
      double tempval = dualInfoCollection[l].dualvec.dot(rhsvecs[samples[k]]);
      tempval -= dualInfoCollection[l].coefvec.dot(xiterate);
      tempval += dualInfoCollection[l].rhs;
      if (tempval > maxval)
      {
        maxval = tempval;
        maxind = l;
      }
    }
    if (maxind == -1)
    {
      cout << "something is wrong in warmstart!" << endl;
      exit(0);
    }
    */
    initialCutCoef = initialCutCoef + dualInfoCollection[maxind].coefvec;
    initialCutRhs += dualInfoCollection[maxind].dualvec.dot(rhsvecs[samples[k]]) + dualInfoCollection[maxind].rhs;
  }
  stat.warmstartcuttime += clock.getTime() - tempTime;
  IloExpr initialCut(env);
  vector<double> tempcutcoef(prob.nbFirstVars, 0);
  for (int i = 0; i < prob.nbFirstVars; ++i)
  {
    if (fabs(initialCutCoef[i]) > 1e-7)
    {
      initialCut += x[i] * initialCutCoef[i];
      tempcutcoef[i] = initialCutCoef[i];
    }
  }
  cutcoefs.push_back(tempcutcoef);
  cutrhs.push_back(initialCutRhs);
  initialCut += theta;
  masterProb.model.add(initialCut >= initialCutRhs);
  initialCut.end();
  // Loop
  bool loopflag = 1;
  double returnval;
  int nCuts = 0;
  while (loopflag)
  {
    loopflag = 0;
    masterProb.getCplex().solve();
    returnval = cplex.getObjValue();
    masterProb.getCplex().getValues(xvals, x);
    for (int j = 0; j < prob.nbFirstVars; ++j)
      xiterate[j] = xvals[j];
    double thetaval = cplex.getValue(theta);
    VectorXf CutCoef(prob.nbFirstVars);
    CutCoef.setZero();
    double CutRhs = 0;
    tempTime = clock.getTime();
    for (int k = 0; k < samples.size(); ++k)
    {
      // Go through all scenarios
      /*
      double maxval = -1e8;
      int maxind = -1;
      for (int l = 0; l < dualInfoCollection.size(); ++l)
      {
        double tempval = dualInfoCollection[l].dualvec.dot(rhsvecs[samples[k]]);
        tempval -= dualInfoCollection[l].coefvec.dot(xiterate);
        tempval += dualInfoCollection[l].rhs;
        if (tempval > maxval)
        {
          maxval = tempval;
          maxind = l;
        }
      }
      if (maxind == -1)
      {
        cout << "something is wrong in warmstart!" << endl;
        exit(0);
      }
      */
      VectorXf vals = dualvecMatrix * rhsvecs[samples[k]] - coefvecMatrix * xiterate + rhsVector;
      int maxind;
      double maxval = vals.maxCoeff(&maxind);
      CutCoef = CutCoef + dualInfoCollection[maxind].coefvec;
      CutRhs += dualInfoCollection[maxind].dualvec.dot(rhsvecs[samples[k]]) + dualInfoCollection[maxind].rhs;
    }
    stat.warmstartcuttime += clock.getTime() - tempTime;
    double cutlhsval = 0;
    IloExpr Cut(env);
    for (int i = 0; i < prob.nbFirstVars; ++i)
    {
      if (fabs(CutCoef[i]) > 1e-7)
      {
        Cut += x[i] * CutCoef[i];
        cutlhsval += xvals[i] * CutCoef[i];
      }
    }
    Cut += masterProb.theta;
    cutlhsval += thetaval;
    if (cutlhsval < CutRhs - max(1e-2, abs(CutRhs) * 1e-5))
    {
      masterProb.model.add(Cut >= CutRhs);
      loopflag = 1;
      vector<double> tempcutcoef(prob.nbFirstVars, 0);
      for (int i = 0; i < prob.nbFirstVars; ++i)
      {
        if (fabs(CutCoef[i]) > 1e-7)
          tempcutcoef[i] = CutCoef[i];
      }
      cutcoefs.push_back(tempcutcoef);
      cutrhs.push_back(CutRhs);
      nCuts++;
    }
    Cut.end();
  }
  cout << "# of cuts added in the warmstart = " << nCuts << endl;
  masterProb.getCplex().end();
  masterProb.model.end();
  masterProb.x.end();
  masterProb.theta.end();
  return returnval;
}

/*

*/
void Partition::computeSamplingError(double& samplingError, const vector<double>& scenObjs) {
  samplingError = 0;
  double sampleMean = 0;
  for (int k = 0; k < scenObjs.size(); ++k)
    sampleMean += scenObjs[k];
  sampleMean = sampleMean * 1.0 / scenObjs.size();
  for (int k = 0; k < scenObjs.size(); ++k)
    samplingError += pow(scenObjs[k] - sampleMean, 2);
  samplingError = sqrt(samplingError) * 1.0 / scenObjs.size();
  // \delta = 1e-3
  if (samplingError < 1e-3 * 1.0 / sqrt(scenObjs.size()))
    samplingError = 1e-3 * 1.0 / sqrt(scenObjs.size());
}

/*
  Finds coarse cuts and applies them to the master problem.
*/
double coarse_oracle(IloEnv& env, TSLP& prob, Subprob& subp, vector<Component>& partition, IloNumArray& xvals, double& feasboundscen, VectorXf& cutcoefscen, IloCplex& cplex, IloModel& model, const IloNumVarArray& x, STAT& stat, IloRangeArray& center_cons, const IloNumArray& stab_center, IloRangeArray& cuts, const vector<double>& cutrhs, VectorXf& aggrCoarseCut, double& coarseCutRhs, vector<VectorXf>& partcoef, vector<double>& partrhs, double starttime, IloTimer& timer, vector<double>& scenObjs, const vector<int>& samples, vector<DualInfo>& dualInfoCollection, const vector<VectorXf>& rhsvecs, int option) {
  // coarse oracle
  // won't add any cuts in this subroutine, just collect information, and decide whether or not add the coarse cut depending on whether the descent target is achieved
  bool cutflag = 1;
  IloNumArray secvarlb(env, partition.size() * prob.nbSecVars);
  IloNumArray secvarub(env, partition.size() * prob.nbSecVars);
  IloNumArray secconstrbd(env, partition.size() * prob.nbSecRows);
  setAggregatedBounds(prob, partition, secvarlb, secvarub, secconstrbd);
  double inner_up = stat.feasobjval;
  double inner_low = stat.relaxobjval;
  double levelobj = prob.kappa * inner_low + (1 - prob.kappa) * inner_up;
  // Update all the oracles in the bundle for the QP model
  for (int l = 0; l < cuts.getSize(); ++l)
    cuts[l].setUB(levelobj - cutrhs[l]);
  // Update stablization center
  for (int j = 0; j < prob.nbFirstVars; ++j)
  {
    if (fabs(stab_center[j]) > 1e-7)
    {
      center_cons[j].setLB(-stab_center[j]);
      center_cons[j + prob.nbFirstVars].setLB(stab_center[j]);
    }
    else
    {
      center_cons[j].setLB(0);
      center_cons[j + prob.nbFirstVars].setLB(0);
    }
  }
  double totalobjval;
  while (cutflag == 1)
  {
    // Keep going if no opt cut has been added
    cutflag = 0;
    // First of all, solve the master problem
    masterProb.getCplex().solve();
    //cout << "master QP status = " << cplex.getCplexStatus() << endl;
    if (masterProb.getCplex().getStatus() == IloAlgorithm::Infeasible)
    {
      // Infeasible: update inner_low and level obj
      inner_low = levelobj;
      levelobj = prob.kappa * inner_low + (1 - prob.kappa) * inner_up;
      for (int l = 0; l < cuts.getSize(); ++l)
        cuts[l].setUB(levelobj - cutrhs[l]);
      stat.relaxobjval = levelobj;
      if (timer.getTime() - starttime > TIMELIMIT)
        break;
      if (fabs(stat.feasobjval - stat.relaxobjval) * 1.0 / (fabs(stat.feasobjval) + 1e-10) > 1e-6)
        cutflag = 1;
      else
      {
        // already optimal
        cutflag = 0;
      }
      continue;
    }
    aggrCoarseCut.setZero();
    masterProb.getCplex().getValues(xvals, x);
    VectorXf xiterateXf(prob.nbFirstVars);
    for (int j = 0; j < prob.nbFirstVars; ++j)
      xiterateXf(j) = xvals[j];
    // Start generating partition-based Benders cuts
    feasboundscen = 0;
    cutcoefscen.setZero();
    totalobjval = 0;
    for (int i = 0; i < partition.size(); ++i)
    {
      IloNumArray duals(env);
      bool scenfeasflag;
      double subobjval = subprob_partition(subp, secvarlb, secvarub, prob, xvals, duals, partition, i, scenfeasflag);
      VectorXf dualvec(prob.nbSecRows + prob.nbSecVars);
      for (int j = 0; j < prob.nbSecRows + prob.nbSecVars; ++j)
        dualvec(j) = duals[j];
      if (scenfeasflag == 1)
      {
        VectorXf opt_cut_coef = prob.CoefMatXf.transpose() * dualvec.segment(0, prob.nbSecRows);
        for (int j = 0; j < prob.nbFirstVars; ++j)
        {
          if (fabs(opt_cut_coef[j]) < 1e-7)
            opt_cut_coef[j] = 0;
        }
        aggrCoarseCut += opt_cut_coef * (partition[i].indices.size());
        totalobjval += subobjval;
        partcoef[i] = opt_cut_coef;
        partrhs[i] = subobjval;
        if (partition[i].indices.size() == 1)
        {
          feasboundscen += subobjval;
          cutcoefscen += opt_cut_coef;
          scenObjs.push_back(subobjval);
          if (option == 1)
          {
            if (addToCollection(dualvec, dualInfoCollection) == true)
            {
              DualInfo dual;
              dual.dualvec = dualvec;
              dual.coefvec = opt_cut_coef;
              dual.rhs = subobjval;
              for (int j = 0; j < prob.nbFirstVars; ++j)
                dual.rhs += opt_cut_coef[j] * xvals[j];
              dual.rhs -= rhsvecs[partition[i].indices[0]].dot(dualvec.segment(0, prob.nbSecRows));
              dualInfoCollection.push_back(dual);
            }
          }
        }
      }
      else
      {
        //cout << "feas cuts!" << endl;
        cutflag = 1;
        // Add feasibility cuts
        add_feas_cuts(env, prob, partition, model, x, xvals, subobjval, dualvec, i);
      }
      duals.end();
    }
    if (cutflag == 0)
    {
      // So it is feasible
      // Don't add immediately, save information
      double lhsval = 0;
      for (int j = 0; j < prob.nbFirstVars; ++j)
      {
        if (fabs(aggrCoarseCut[j]) > 1e-7)
          lhsval += xvals[j] * aggrCoarseCut[j];
      }
      coarseCutRhs = (totalobjval + lhsval) * 1.0 / samples.size();
      totalobjval = totalobjval * 1.0 / samples.size();
      for (int j = 0; j < prob.nbFirstVars; ++j)
        totalobjval += prob.objcoef[j] * xvals[j];
    }
  }
  secvarlb.end();
  secvarub.end();
  secconstrbd.end();
  return totalobjval;
}


/*
  Helper function for refinement.
  Uses earlier calculations to add cuts to master problem.
*/
void Partition::add_feasibility_cuts(vector< vector<int> > extreme_ray_map, vector<IloNumArray> extreme_points, vector<IloNumArray> extreme_rays, vector<int> extreme_points_ind, vector<int> extreme_rays_ind, double sum_of_infeas, IloEnv& env, const TSLP& prob, const IloNumArray& xvals, const IloNumVarArray& x, STAT& stat) {
  for (int j = 0; j < extreme_ray_map.size(); ++j)
  {
    // add feasibility cuts group by group
    gen_feasibility_cuts(env, prob, xvals, extreme_ray_map[j], extreme_rays, extreme_rays_ind, sum_of_infeas, masterProb.model, x);
    stat.num_feas_cuts++;
  }
  for (int j = 0; j < extreme_points.size(); ++j)
    extreme_points[j].end();
  for (int j = 0; j < extreme_rays.size(); ++j)
    extreme_rays[j].end();
}

/*
  Refines problem and creates a new partition.
*/
void Partition::perform_refinement(IloTimer& clock, const TSLP& prob, vector<IloNumArray> extreme_points, vector<int> extreme_points_ind, vector<IloNumArray> extreme_rays, vector<int> extreme_rays_ind, vector<Component>& new_partition, vector< vector<int> > extreme_ray_map, STAT& stat) {
  double refinestart = clock.getTime();
  simple_refine(partition[i], prob, extreme_points, extreme_points_ind, extreme_rays, extreme_rays_ind, new_partition, extreme_ray_map);
  stat.refinetime += clock.getTime() - refinestart;
}

/*
  Helper function for refinement functions.
  Finds the refinement for one Component of the partition.
  returns: the flag to be returned from the refinement functions.
*/
bool Partition::calculate_refinement(Component& comp, const TSLP& prob, Subprob& subp, vector<IloNumArray>& extreme_points, vector<IloNumArray>& extreme_rays, vector<int>& extreme_points_ind, vector<int>& extreme_rays_ind, double& feasboundscen, VectorXf& cutcoefscen, vector<double>& scenObjs, int option, vector<DualInfo>& dualInfoCollection, const IloNumArray& xvals, const vector<VectorXf>& rhsvecs, double& sum_of_infeas, /*Subproblem*/){
  bool returnflag = 1;
  for (int k = 0; k < comp.indices.size(); ++k)
  {
    IloNumArray duals(env2);
    bool feasflag;
    double subobjval = subProb(subp, prob, xvals, duals, comp.indices[k], feasflag);
    if (feasflag == 1)
    {
      // optimal, so return extreme point solution
      extreme_points.push_back(duals);
      extreme_points_ind.push_back(comp.indices[k]);
      feasboundscen += subobjval;
      VectorXf dualvec(prob.nbSecRows + prob.nbSecVars);
      for (int j = 0; j < prob.nbSecRows + prob.nbSecVars; ++j)
        dualvec(j) = duals[j];
      VectorXf opt_cut_coef = prob.CoefMatXf.transpose() * dualvec.segment(0, prob.nbSecRows);
      cutcoefscen += opt_cut_coef;
      scenObjs.push_back(subobjval);
      if (option == 1)
      {
        if (addToCollection(dualvec, dualInfoCollection) == true)
        {
          DualInfo dual;
          dual.dualvec = dualvec;
          dual.coefvec = opt_cut_coef;
          dual.rhs = subobjval;
          for (int j = 0; j < prob.nbFirstVars; ++j)
            dual.rhs += opt_cut_coef[j] * xvals[j];
          dual.rhs -= rhsvecs[partition[i].indices[k]].dot(dualvec.segment(0, prob.nbSecRows));
          dualInfoCollection.push_back(dual);
        }
      }
    }
    else
    {
      returnflag = 0;
      //cout << "infeasible scenario subproblem!" << endl;
      extreme_rays.push_back(duals);
      extreme_rays_ind.push_back(comp.indices[k]);
      *sum_of_infeas += subobjval;
    }
  }

  return returnflag;
}

/*
  Use scenarios in current partition to refine the master problem? and
  then form a better partition.
*/
bool Partition::refine_full(IloEnv& env, IloEnv& env2, const TSLP& prob, Subprob& subp, const vector<Component>& partition, const IloNumArray& xvals, double& feasboundscen, VectorXf& cutcoefscen, const IloNumVarArray& x, vector<Component>& new_partition, STAT& stat, IloTimer& clock, vector<double>& scenObjs, vector<DualInfo>& dualInfoCollection, const vector<VectorXf>& rhsvecs, int option)
{
	bool returnflag = 1;
	for (int i = 0; i < partition.size(); ++i)
	{
		double sum_of_infeas = 0;
		vector<IloNumArray> extreme_points, extreme_rays;
		vector<int> extreme_points_ind, extreme_rays_ind;
		vector< vector<int> > extreme_ray_map; // record groups of extreme rays that are stored in the list extreme_rays, extreme_rays_ind
		// solve subproblems for each partition
		if (partition[i].indices.size() > 1)
		{
      returnflag = calculate_refinement(partition[i], prob, subp, extreme_points, extreme_rays, extreme_points_ind, extreme_rays_ind, feasboundscen, cutcoefscen, scenObjs, option, dualInfoCollection, xvals, rhsvecs, &sum_of_infeas, /*Subproblem*/);
      void perform_refinement(clock, prob, extreme_points, extreme_points_ind, extreme_rays, extreme_rays_ind, new_partition, extreme_ray_map, stat);
		}
		else
		{
			// Don't need refine
			new_partition.push_back(partition[i]);
		}
		// Now add feasibility cuts
    add_feasibility_cuts(extreme_ray_map, extreme_points, extreme_rays, extreme_points_ind, extreme_rays_ind, sum_of_infeas, env, prob, xvals, x, stat);
	}
	return returnflag;
}

/*
  Use scenarios in current partition to refine the master problem? and
  then form a better partition, checking the precision of the new partition
  after each scenario.
*/
bool Partition::refine_part(IloEnv& env, IloEnv& env2, const TSLP& prob, Subprob& subp, const vector<Component>& partition, const IloNumArray& xvals, IloModel& model, const IloNumVarArray& x, vector<Component>& new_partition, STAT& stat, IloTimer& clock, vector<VectorXf>& partcoef, vector<double>& partrhs, double descent_target, bool& fullupdateflag, double& coarseLB, VectorXf& aggrCoarseCut, vector<double>& scenObjs, const vector<int>& samples, vector<DualInfo>& dualInfoCollection, const vector<VectorXf>& rhsvecs, int option)
{
	// Solve scenario subproblems component by component, stop when hopeless to achieve descent target
	bool returnflag = 1;
	// Currently: go through all the partition component by exploring the bigger component first
	vector<IndexVal> sizelist;
	for (int ii = 0; ii < partition.size(); ++ii)
	{
		IndexVal indexval;
		indexval.ind = ii;
		indexval.val = partition[ii].indices.size();
		sizelist.push_back(indexval);
	}
	sort(sizelist.begin(), sizelist.end(), less<IndexVal>());
	int ind = 0;
	while (ind < partition.size())
	{
		int i = sizelist[ind].ind;
		if (partition[i].indices.size() > 1)
		{
			// solve subproblems for each partition
			double sum_of_infeas = 0;
			vector<IloNumArray> extreme_points, extreme_rays;
			vector<int> extreme_points_ind, extreme_rays_ind;
			vector< vector<int> > extreme_ray_map; // record groups of extreme rays that are stored in the list extreme_rays, extreme_rays_ind
			coarseLB -= partrhs[i] * 1.0 / samples.size();
			aggrCoarseCut -= partcoef[i] * (partition[i].indices.size());

      returnflag = calculate_refinement(partition[i], prob, subp, extreme_points, extreme_rays, extreme_points_ind, extreme_rays_ind, feasboundscen, cutcoefscen, scenObjs, option, dualInfoCollection, xvals, rhsvecs, &sum_of_infeas, /*Subproblem*/);
      void perform_refinement(clock, prob, extreme_points, extreme_points_ind, extreme_rays, extreme_rays_ind, new_partition, extreme_ray_map, stat);
			// Now add feasibility cuts
      add_feasibility_cuts(extreme_ray_map, extreme_points, extreme_rays, extreme_points_ind, extreme_rays_ind, sum_of_infeas, env, prob, xvals, masterProb.model, x, stat);

			if (coarseLB > descent_target)
			{
				// If hopeless to achieve the descent target, break out of loop
				for (int j = ind + 1; j < partition.size(); ++j)
					new_partition.push_back(partition[sizelist[j].ind]);
				break;
			}
		}
		else
		{
			// Don't need refine
			new_partition.push_back(partition[i]);
		}
		ind++;
	}
	if (ind >= partition.size() - 1 || partition[sizelist[ind + 1].ind].indices.size() == 1)
	{
		// All scenario subproblems have been explored: either finish, or break at the last iteration, or after break, all partition components have size = 1
		fullupdateflag = 1;
	}
	return returnflag;
}