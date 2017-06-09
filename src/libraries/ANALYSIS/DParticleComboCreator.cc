#include "ANALYSIS/DParticleComboCreator.h"
#include "ANALYSIS/DSourceComboer.h"

namespace DAnalysis
{

void DParticleComboCreator::DParticleComboCreator(JEventLoop* locEventLoop, const DSourceComboer* locSourceComboer, const DSourceComboTimeHandler* locSourceComboTimeHandler, const DSourceComboVertexer* dSourceComboVertexer) :
		dSourceComboer(locSourceComboer), dSourceComboTimeHandler(locSourceComboTimeHandler), dSourceComboVertexer(locSourceComboVertexer)
{
	vector<const DNeutralParticleHypothesis*> locNeutralParticleHypotheses;
	locEventLoop->Get(locNeutralParticleHypotheses); //make sure that brun() is called for the default factory!!!
	dNeutralParticleHypothesisFactory = static_cast<DNeutralParticleHypothesis_factory*>(locEventLoop->GetFactory("DNeutralParticleHypothesis"));

	vector<const DChargedTrackHypothesis*> locChargedTrackHypotheses;
	locEventLoop->Get(locChargedTrackHypotheses); //make sure that brun() is called for the default factory!!!
	dChargedTrackHypothesisFactory = static_cast<DChargedTrackHypothesis_factory*>(locEventLoop->GetFactory("DChargedTrackHypothesis"));

	vector<const DBeamPhoton*> locBeamPhotons;
	locEventLoop->Get(locBeamPhotons); //make sure that brun() is called for the default factory!!!
	dBeamPhotonfactory = static_cast<DBeamPhoton_factory*>(locEventLoop->GetFactory("DBeamPhoton"));

	locEventLoop->GetSingle(dParticleID);

	//error matrix //too lazy to compute properly right now ... need to hack DAnalysisUtilities::Calc_DOCA()
	dVertexCovMatrix.ResizeTo(4, 4);
	dVertexCovMatrix.Zero();
	dVertexCovMatrix(0, 0) = 0.65; //x variance //from monitoring plots of vertex
	dVertexCovMatrix(1, 1) = 0.65; //y variance //from monitoring plots of vertex
	dVertexCovMatrix(2, 2) = 1.5; //z variance //a guess, semi-guarding against the worst case scenario //ugh
	dVertexCovMatrix(3, 3) = 0.0; //t variance //not used
}

void DParticleComboCreator::Reset(void)
{
	for(const auto& locRFPair : dRFBunchMap)
		dResourcePool_EventRFBunch.Recycle(locRFPair.second);
	dResourcePool_EventRFBunch.clear();

	for(const auto& locStepPair : dComboStepMap)
		dResourcePool_ParticleComboStep.Recycle(locStepPair.second);
	dComboStepMap.clear();

	for(const auto& locComboPair : dComboMap)
		dResourcePool_ParticleCombo.Recycle(locComboPair.second);
	dComboMap.clear();

	for(const auto& locHypoPair : dChargedHypoMap)
		dChargedTrackHypothesisFactory->Recycle_Hypothesis(locHypoPair.second);
	dChargedHypoMap.clear();

	for(const auto& locHypoPair : dKinFitChargedHypoMap)
		dChargedTrackHypothesisFactory->Recycle_Hypothesis(locHypoPair.second);
	dKinFitChargedHypoMap.clear();

	for(const auto& locHypoPair : dNeutralHypoMap)
		dNeutralParticleHypothesisFactory->Recycle_Hypothesis(locHypoPair.second);
	dNeutralHypoMap.clear();

	for(const auto& locHypoPair : dKinFitNeutralHypoMap)
		dNeutralParticleHypothesisFactory->Recycle_Hypothesis(locHypoPair.second);
	dKinFitNeutralHypoMap.clear();

	for(const auto& locBeamPair : dKinFitBeamPhotonMap)
		dBeamPhotonfactory->Recycle_Beam(locBeamPair.second);
	dKinFitBeamPhotonMap.clear();
}

bool DParticleComboCreator::Get_CreateNeutralErrorMatrixFlag_Combo(const DReactionVertexInfo* locReactionVertexInfo, DKinFitType locKinFitType)
{
	//is there at least one dangling vertex that has neutrals?
	auto locDanglingIterator = dDanglingNeutralsFlagMap.find(locReactionVertexInfo);
	bool locDanglingNeutralsFlag = false;
	if(locDanglingIterator != dDanglingNeutralsFlagMap.end())
		locDanglingNeutralsFlag = locDanglingIterator->second;
	else
	{
		for(auto locStepVertexInfo : locReactionVertexInfo->Get_StepVertexInfos())
		{
			if(!locStepVertexInfo->Get_DanglingVertexFlag())
				continue;
			if(!locStepVertexInfo->Get_OnlyConstrainTimeParticles().empty())
			{
				locDanglingNeutralsFlag = true; //photons
				break;
			}
			if(!locStepVertexInfo->Get_NoConstrainParticles(d_FinalState, d_Neutral, false, false, false).empty())
			{
				locDanglingNeutralsFlag = true; //massive neutrals
				break;
			}
		}
		dDanglingNeutralsFlagMap.emplace(locReactionVertexInfo, locDanglingNeutralsFlag);
	}
	return ((locKinFitType != d_NoFit) && ((locKinFitType == d_P4Fit) || locDanglingNeutralsFlag));
}

const DParticleCombo* DParticleComboCreator::Build_ParticleCombo(const DReactionVertexInfo* locReactionVertexInfo, const DSourceCombo* locFullCombo, const DKinematicData* locBeamParticle, int locRFBunchShift, DKinFitType locKinFitType)
{
	auto locCreateNeutralErrorMatrixFlag_Combo = Get_CreateNeutralErrorMatrixFlag_Combo(locReactionVertexInfo, locKinFitType);
	auto locComboTuple = std::make_tuple(locReactionVertexInfo, locFullCombo, locBeamParticle, locRFBunchShift, locCreateNeutralErrorMatrixFlag_Combo);
	auto locComboIterator = dComboMap.find(locComboTuple);
	if(locComboIterator != dComboMap.end())
		return locComboIterator->second;

	auto locParticleCombo = dResourcePool_ParticleCombo.Get_Resource();
	locParticleCombo->Reset();
	dComboMap.emplace(locComboTuple, locParticleCombo);

	auto locReaction = locReactionVertexInfo->Get_Reaction();
	auto locIsPrimaryProductionVertex = locReactionVertexInfo->Get_StepVertexInfos().front()->Get_ProductionVertexFlag();
	auto locPrimaryVertexZ = dSourceComboVertexer->Get_PrimaryVertex(locReactionVertexInfo, locFullCombo, locBeamParticle).Z();

	//Get/Create RF Bunch
	const DEventRFBunch* locEventRFBunch = nullptr;
	auto locRFIterator = dRFBunchMap.find(locRFBunchShift);
	if(locRFIterator != dRFBunchMap.end())
		locEventRFBunch = locRFIterator->second;
	else
	{
		auto locInitialEventRFBunch = dSourceComboTimeHandler->Get_InitialEventRFBunch();
		auto locEventRFBunch = dResourcePool_EventRFBunch.Get_Resource();
		auto locRFTime = dSourceComboTimeHandler->Calc_RFTime(locRFBunchShift);
		locEventRFBunch->Set_Content(locInitialEventRFBunch->dTimeSource, locRFTime, locInitialEventRFBunch->dTimeVariance, 0);
	}
	locParticleCombo->Set_EventRFBunch(locEventRFBunch);

	auto locReactionSteps = locReaction->Get_ReactionSteps();
	auto locPreviousStepSourceCombo = locFullCombo;
	for(size_t loc_i = 0; loc_i < locReactionSteps.size(); ++loc_i)
	{
		auto locReactionStep = locReactionSteps[loc_i];
		auto locStepBeamParticle = (loc_i == 0) ? locBeamParticle : nullptr;
		auto locIsProductionVertex = (loc_i == 0) ? locIsPrimaryProductionVertex : false;
		auto locSourceCombo = (loc_i == 0) ? locFullCombo : dSourceComboer->Get_StepSourceCombo(locReaction, loc_i, locPreviousStepSourceCombo, loc_i - 1);

		auto locStepVertexInfo = locReactionVertexInfo->Get_StepVertexInfo(loc_i);
		auto locVertexPrimaryCombo = dSourceComboer->Get_VertexPrimaryCombo(locFullCombo, locStepVertexInfo);
		bool locCreateNeutralErrorMatrixFlag = (locKinFitType != d_NoFit) && ((locKinFitType == d_P4Fit) || locStepVertexInfo->Get_DanglingVertexFlag());
		if(locReactionStep->Get_FinalPIDs(false, d_Neutral, false).empty())
			locCreateNeutralErrorMatrixFlag = false; //no neutrals!

		//reuse step if already created
		auto locStepTuple = std::make_tuple(locSourceCombo, locCreateNeutralErrorMatrixFlag, locIsProductionVertex, locFullCombo, locBeamParticle);
		auto locStepIterator = dComboStepMap.find(locStepTuple);
		if(locStepIterator != dComboStepMap.end())
		{
			locParticleCombo->Add_ParticleComboStep(locStepIterator->second);
			locPreviousStepSourceCombo = locSourceCombo;
			continue;
		}

		//Create a new step
		auto locParticleComboStep = dResourcePool_ParticleComboStep.Get_Resource();
		locParticleComboStep->Reset();

		//build spacetime vertex
		auto locVertex = dSourceComboVertexer->Get_Vertex(locIsProductionVertex, locVertexPrimaryCombo, locBeamParticle);
		auto locTimeOffset = dSourceComboVertexer->Get_TimeOffset(locIsProductionVertex, locFullCombo, locVertexPrimaryCombo, locBeamParticle);
		auto locPropagatedRFTime = dSourceComboTimeHandler->Calc_PropagatedRFTime(locPrimaryVertexZ, locRFBunchShift, locTimeOffset);
		DLorentzVector locSpacetimeVertex(locVertex, locPropagatedRFTime);

		//Build final particles
		auto locFinalPIDs = locReactionStep->Get_FinalPIDs();
		unordered_map<Particle_t, size_t> locPIDCountMap;
		vector<const DKinematicData*> locFinalParticles;
		for(size_t loc_j = 0; loc_j < locFinalPIDs.size(); ++loc_j)
		{
			if(loc_j == locReactionStep->Get_MissingParticleIndex())
			{
				locFinalParticles.push_back(nullptr);
				continue;
			}

			//Get source objects, in order
			auto locPID = locFinalPIDs[loc_j];
			auto locPIDIterator = locPIDCountMap.find(locPID);
			if(locPIDIterator != locPIDCountMap.end())
				++(locPIDIterator->second);
			else
				locPIDCountMap.emplace(locPID, 1);

			size_t locPIDCountSoFar = 0;
			auto locSourceParticle = DAnalysis::Get_SourceParticle_ThisStep(locSourceCombo, locPID, locPIDCountMap[locPID], locPIDCountSoFar);

			//build hypo
			if(ParticleCharge(locPID) == 0) //neutral
			{
				auto locNeutralShower = static_cast<const DNeutralShower*>(locSourceParticle);
				auto locHypoTuple = std::make_tuple(locNeutralShower, locPID, locRFBunchShift, locCreateNeutralErrorMatrixFlag, locIsProductionVertex, locFullCombo, locVertexPrimaryCombo, locBeamParticle); //last 4 needed for spacetime vertex
				const DNeutralParticleHypothesis* locNewNeutralHypo = nullptr;

				auto locHypoIterator = dNeutralHypoMap.find(locHypoTuple);
				if(locHypoIterator != dNeutralHypoMap.end())
					locNewNeutralHypo = locHypoIterator->second;
				else
				{
					auto locVertexCovMatrix = locCreateNeutralErrorMatrixFlag ? *dVertexCovMatrix : nullptr;
					locNewNeutralHypo = dNeutralParticleHypothesisFactory->Create_DNeutralParticleHypothesis(locNeutralShower, locPID, locEventRFBunch, locSpacetimeVertex, locVertexCovMatrix);
					dNeutralHypoMap.emplace(locHypoTuple, locNewNeutralHypo);
				}

				locFinalParticles.push_back(static_cast<DKinematicData*>(locNewNeutralHypo));
			}
			else //charged
			{
				auto locChargedTrack = static_cast<const DChargedTrack*>(locSourceParticle);
				auto locHypoTuple = std::make_tuple(locChargedTrack, locPID, locRFBunchShift, locIsProductionVertex, locFullCombo, locVertexPrimaryCombo, locBeamParticle);
				const DChargedTrackHypothesis* locNewChargedHypo = nullptr;

				auto locHypoIterator = dChargedHypoMap.find(locHypoTuple);
				if(locHypoIterator != dChargedHypoMap.end())
					locNewChargedHypo = locHypoIterator->second;
				else
				{
					locNewChargedHypo = Create_ChargedHypo(locChargedTrack, locPID, locPropagatedRFTime, locIsProductionVertex, locVertexPrimaryCombo, locBeamParticle);
					dChargedHypoMap.emplace(locHypoTuple, locNewChargedHypo);
				}

				locFinalParticles.push_back(static_cast<DKinematicData*>(locNewChargedHypo));
			}
		}

		locParticleComboStep->Set_Contents(locStepBeamParticle, locFinalParticles, locSpacetimeVertex);
		if(loc_i == 0)
			locParticleComboStep->Set_InitialParticle(locBeamParticle);

		//save it
		locParticleCombo->Add_ParticleComboStep(locParticleComboStep);
		dComboStepMap.emplace(locStepTuple, locParticleComboStep);

		locPreviousStepSourceCombo = locSourceCombo;
	}

	return locParticleCombo;
}

const DChargedTrackHypothesis* DParticleComboCreator::Create_ChargedHypo(const DChargedTrack* locChargedTrack, Particle_t locPID, double locPropagatedRFTime, bool locIsProductionVertex, const DSourceCombo* locVertexPrimaryFullCombo, const DKinematicData* locBeamParticle)
{
	//see if DChargedTrackHypothesis with the desired PID was created by the default factory, AND it passed the PreSelect cuts
	auto locOrigHypo = locChargedTrack->Get_Hypothesis(locPID);
	auto locNewHypo = dChargedTrackHypothesisFactory->Get_Resource();
	locNewHypo->Share_FromInput(locOrigHypo, true, false, true); //share all but timing info

	auto locTrackPOCAX4 = dSourceComboTimeHandler->Get_ChargedParticlePOCAToVertexX4(locOrigHypo, locIsProductionVertex, locVertexPrimaryFullCombo, locBeamParticle);
	locNewHypo->Set_TimeAtPOCAToVertex(locTrackPOCAX4.T());

	locNewHypo->Set_T0(locPropagatedRFTime, locOrigHypo->t0_err(), locOrigHypo->t0_detector());
	dParticleID->Calc_ChargedPIDFOM(locNewHypo);

	return locNewHypo;
}

const DParticleCombo* DParticleComboCreator::Create_KinFitCombo_NewCombo(const DParticleCombo* locOrigCombo, const DReaction* locReaction, const DKinFitResults* locKinFitResults, const DKinFitChain* locKinFitChain)
{
	auto locNewCombo = dResourcePool_ParticleCombo.Get_Resource();
	locNewCombo->Reset();
	locNewCombo->Set_KinFitResults(locKinFitResultsVector[loc_i]);
	locNewCombo->Set_EventRFBunch(locOrigCombo->Get_EventRFBunch());
	set<DKinFitParticle*> locOutputKinFitParticles = locKinFitResults->Get_OutputKinFitParticles();

	auto locKinFitType = locKinFitResults->Get_KinFitType();
	for(size_t loc_j = 0; loc_j < locOrigCombo->Get_NumParticleComboSteps(); ++loc_j)
	{
		auto locComboStep = locOrigCombo->Get_ParticleComboStep(loc_j);
		auto locReactionStep = locReaction->Get_ReactionStep(loc_j);

		auto locNewComboStep = dResourcePool_ParticleComboStep.Get_Resource();
		locNewComboStep->Reset();
		locNewCombo->Add_ParticleComboStep(locNewComboStep);

		locNewComboStep->Set_MeasuredParticleComboStep(locComboStep);
		locNewComboStep->Set_SpacetimeVertex(locComboStep->Get_SpacetimeVertex()); //overridden if kinematic fit

		//INITIAL PARTICLE
		auto locInitialParticle_Measured = locComboStep->Get_InitialParticle_Measured();
		if(locInitialParticle_Measured != nullptr) //set beam photon
		{
			auto locKinFitParticle = locKinFitResults->Get_OutputKinFitParticle(locInitialParticle_Measured);
			if(locKinFitParticle == NULL) //not used in kinfit!!
				locNewParticleComboStep->Set_InitialParticle(locInitialParticle_Measured);
			else //create a new one
				locNewParticleComboStep->Set_InitialParticle(Create_BeamPhoton_KinFit(locInitialParticle_Measured, locKinFitParticle));
		}
		else //decaying particle! //set here for initial state, and in previous step for final state
			Set_DecayingParticles(locNewParticleCombo, locOrigCombo, loc_j, locNewParticleComboStep, locKinFitChain, locKinFitResultsVector[loc_i]);

		//FINAL PARTICLES
		for(size_t loc_k = 0; loc_k < locComboStep->Get_NumFinalParticles(); ++loc_k)
		{
			auto locKinematicData_Measured = locComboStep->Get_FinalParticle_Measured(loc_k);
			if(locReactionStep->Get_MissingParticleIndex() == loc_k) //missing!
			{
				set<DKinFitParticle*> locMissingParticles = locKinFitResults->Get_OutputKinFitParticles(d_MissingParticle);
				if(!locMissingParticles.empty())
				{
					DKinematicData* locNewKinematicData = Build_KinematicData(*locMissingParticles.begin(), locKinFitType, locComboStep->Get_SpacetimeVertex().Vect());
					locNewParticleComboStep->Add_FinalParticle(locNewKinematicData);
				}
				else //not used in kinfit: do not create: NULL
					locNewParticleComboStep->Add_FinalParticle(NULL);
			}
			else if(locKinematicData_Measured == nullptr) //decaying
				locNewParticleComboStep->Add_FinalParticle(NULL); //is set later, when it's in the initial state
			else if(locComboStep->Is_FinalParticleNeutral(loc_k)) //neutral
			{
				auto locNeutralHypo = static_cast<const DNeutralParticleHypothesis*>(locKinematicData_Measured);
				//might have used neutral shower OR neutral hypo. try hypo first
				auto locKinFitParticle = locKinFitResults->Get_OutputKinFitParticle(locNeutralHypo);
				if(locKinFitParticle == NULL)
					locKinFitParticle = locKinFitResults->Get_OutputKinFitParticle(locNeutralHypo->Get_NeutralShower());
				if(locKinFitParticle == NULL) //not used in kinfit!!
					locNewParticleComboStep->Add_FinalParticle(locKinematicData_Measured);
				else //create a new one
					locNewParticleComboStep->Add_FinalParticle(Create_NeutralHypo_KinFit(locKinematicData_Measured, locKinFitParticle, locKinFitType));
			}
			else //charged
			{
				auto locChargedHypo = static_cast<const DChargedTrackHypothesis*>(locKinematicData_Measured);
				auto locKinFitParticle = locKinFitResults->Get_OutputKinFitParticle(locChargedHypo);
				if(locKinFitParticle == NULL) //not used in kinfit!!
					locNewParticleComboStep->Add_FinalParticle(locKinematicData_Measured);
				else //create a new one
					locNewParticleComboStep->Add_FinalParticle(Create_ChargedHypo_KinFit(locChargedHypo, locKinFitParticle, locKinFitType));
			}
		}

		//SPACETIME VERTEX
		Set_SpacetimeVertex(locNewParticleCombo, locNewParticleComboStep, loc_j, locKinFitResultsVector[loc_i], locKinFitChain);
	}

	return locNewCombo;
}

void DParticleComboCreator::Set_DecayingParticles(const DParticleCombo* locNewParticleCombo, const DParticleCombo* locOldParticleCombo, size_t locStepIndex, DParticleComboStep* locNewParticleComboStep, const DKinFitChain* locKinFitChain, const DKinFitResults* locKinFitResults)
{
	DKinFitParticle* locKinFitParticle = Get_DecayingParticle(locOldParticleCombo, locStepIndex, locKinFitChain, locKinFitResults);
	if(locKinFitParticle == NULL) //not used in fit
	{
		locNewParticleComboStep->Set_InitialParticle(NULL);
		return; //no need to back-set NULL: was set to NULL by default
	}

	DKinFitType locKinFitType = locOldParticleCombo->Get_Reaction()->Get_KinFitType();
	DVector3 locEventVertex = locOldParticleCombo->Get_ParticleComboStep(0)->Get_SpacetimeVertex().Vect();
	Particle_t locPID = PDGtoPType(locKinFitParticle->Get_PID());
	int locFromStepIndex = locNewParticleComboStep->Get_InitialParticleDecayFromStepIndex();

	DKinematicData* locKinematicData_Common = Build_KinematicData(locKinFitParticle, locKinFitType, locEventVertex);
	bool locCreate2ndObjectFlag = (IsDetachedVertex(locPID) && (locStepIndex != 0) && (locFromStepIndex >= 0));
	DKinematicData* locKinematicData_Position = locCreate2ndObjectFlag ? Build_KinematicData(locKinFitParticle, locKinFitType, locEventVertex) : locKinematicData_Common;
	if(locKinFitParticle->Get_CommonVxParamIndex() >= 0)
		dKinFitUtils->Propagate_TrackInfoToCommonVertex(locKinematicData_Common, locKinFitParticle, &locKinFitResults->Get_VXi());

	bool locAtProdVertexFlag = locKinFitParticle->Get_VertexP4AtProductionVertex();
	DKinematicData* locKinematicData_InitState = locAtProdVertexFlag ? locKinematicData_Common : locKinematicData_Position;
	DKinematicData* locKinematicData_FinalState = locAtProdVertexFlag ? locKinematicData_Position : locKinematicData_Common;

	locNewParticleComboStep->Set_InitialParticle(locKinematicData_InitState);

	//now, back-set the particle at the other vertex
	if((locStepIndex == 0) || (locFromStepIndex < 0))
		return; //no other place to set it

	DParticleComboStep* locParticleComboStep = const_cast<DParticleComboStep*>(locNewParticleCombo->Get_ParticleComboStep(locFromStepIndex));
	//find where it is the decaying particle
	for(size_t loc_i = 0; loc_i < locParticleComboStep->Get_NumFinalParticles(); ++loc_i)
	{
		if(locParticleComboStep->Get_DecayStepIndex(loc_i) != int(locStepIndex))
			continue;
		locParticleComboStep->Set_FinalParticle(locKinematicData_FinalState, loc_i);
		break;
	}
}

DKinFitParticle* DParticleComboCreator::Get_DecayingParticle(const DParticleCombo* locOldParticleCombo, size_t locComboStepIndex, const DKinFitChain* locKinFitChain, const DKinFitResults* locKinFitResults)
{
	const DParticleComboStep* locParticleComboStep = locOldParticleCombo->Get_ParticleComboStep(locComboStepIndex);
	Particle_t locPID = locParticleComboStep->Get_InitialParticleID();
	if(!IsFixedMass(locPID))
		return NULL;

	//find which step in the DKinFitChain this combo step corresponds to
	for(size_t loc_i = 0; loc_i < locKinFitChain->Get_NumKinFitChainSteps(); ++loc_i)
	{
		const DKinFitChainStep* locKinFitChainStep = locKinFitChain->Get_KinFitChainStep(loc_i);

		//loop over init particles to get the decaying particle (if present)
		DKinFitParticle* locDecayingParticle = NULL;
		set<DKinFitParticle*> locInitialParticles = locKinFitChainStep->Get_InitialParticles();
		set<DKinFitParticle*>::iterator locParticleIterator = locInitialParticles.begin();
		for(; locParticleIterator != locInitialParticles.end(); ++locParticleIterator)
		{
			if((*locParticleIterator)->Get_KinFitParticleType() != d_DecayingParticle)
				continue; //not a decaying particle
			locDecayingParticle = *locParticleIterator;
			break;
		}
		if(locDecayingParticle == NULL)
			continue; //no decaying particles in this step

		if(PDGtoPType(locDecayingParticle->Get_PID()) != locPID)
			continue; //wrong PID

		//may still not be correct particle. compare decay products: if any of the combo step particles are in the kinfit step, this is it
			//if all step final particles are decaying, then dive down: through steps
			//if any step decay product at any step is located as any decay product at any step in the kinfit chain: then matches

		//get all measured products, then just pick the first one to search for
		deque<const DKinematicData*> locMeasuredParticles;
		locOldParticleCombo->Get_DecayChainParticles_Measured(locComboStepIndex, locMeasuredParticles);
		const DKinematicData* locMeasuredParticle = locMeasuredParticles[0];
		DKinFitParticle* locKinFitParticle = locKinFitResults->Get_OutputKinFitParticle(locMeasuredParticle);
		if(locKinFitParticle == NULL) //null: neutral shower. Use shower object
		{
			const DNeutralShower* locNeutralShower = NULL;
			locMeasuredParticle->GetSingle(locNeutralShower);
			locKinFitParticle = locKinFitResults->Get_OutputKinFitParticle(locNeutralShower);
		}

		if(!Search_ForParticleInDecay(locKinFitChain, loc_i, locKinFitParticle))
			continue;

		//Found!
		return locDecayingParticle;
	}

	return NULL;
}

bool DParticleComboCreator::Search_ForParticleInDecay(const DKinFitChain* locKinFitChain, size_t locStepToSearch, DKinFitParticle* locParticleToFind)
{
	const DKinFitChainStep* locKinFitChainStep = locKinFitChain->Get_KinFitChainStep(locStepToSearch);
	set<DKinFitParticle*> locFinalParticles = locKinFitChainStep->Get_FinalParticles();
	if(locFinalParticles.find(locParticleToFind) != locFinalParticles.end())
		return true; //found it

	//else loop over final state, diving through decays
	set<DKinFitParticle*>::iterator locParticleIterator = locFinalParticles.begin();
	for(; locParticleIterator != locFinalParticles.end(); ++locParticleIterator)
	{
		if((*locParticleIterator)->Get_KinFitParticleType() != d_DecayingParticle)
			continue; //not a decaying particle

		int locDecayStepIndex = locKinFitChain->Get_DecayStepIndex(*locParticleIterator);
		if(Search_ForParticleInDecay(locKinFitChain, locDecayStepIndex, locParticleToFind))
			return true; //found in in subsequent step
	}
	return false; //not found (yet)
}

void DParticleCombo_factory::Set_SpacetimeVertex(const DParticleCombo* locNewParticleCombo, DParticleComboStep* locNewParticleComboStep, size_t locStepIndex, const DKinFitResults* locKinFitResults, const DKinFitChain* locKinFitChain) const
{
	DKinFitType locKinFitType = locNewParticleCombo->Get_Reaction()->Get_KinFitType();
	if((locKinFitType == d_NoFit) || (locKinFitType == d_P4Fit))
		return; //neither vertex nor time was fit: no update to give

	//Position & Time
	const DKinematicData* locKinematicData = locNewParticleComboStep->Get_InitialParticle();
	if(locKinematicData != NULL)
	{
		DLorentzVector locSpacetimeVertex(locKinematicData->position(), locKinematicData->time());
		locNewParticleComboStep->Set_SpacetimeVertex(locSpacetimeVertex);
		return;
	}

	//mass not fixed: if not initial step, get spacetime vertex from step where this particle was produced
	if(locStepIndex != 0)
	{
		//get spacetime vertex from step where this particle was produced
		int locDecayFromStepIndex = locNewParticleComboStep->Get_InitialParticleDecayFromStepIndex();
		const DParticleComboStep* locPreviousParticleComboStep = locNewParticleCombo->Get_ParticleComboStep(locDecayFromStepIndex);
		locNewParticleComboStep->Set_SpacetimeVertex(locPreviousParticleComboStep->Get_SpacetimeVertex());
		return;
	}

	//instead, get from common vertex of final state particles
	DKinFitParticle* locFinalKinFitParticle = *(locKinFitChain->Get_KinFitChainStep(0)->Get_FinalParticles().begin());

	//need the spacetime vertex at the production vertex of the particle grabbed
	TLorentzVector locSpacetimeVertex;
	if(locFinalKinFitParticle->Get_VertexP4AtProductionVertex()) //"position" is at production vertex
		locSpacetimeVertex = locFinalKinFitParticle->Get_SpacetimeVertex();
	else //"position" is at decay vertex
		locSpacetimeVertex = locFinalKinFitParticle->Get_CommonSpacetimeVertex(); //get production
	locNewParticleComboStep->Set_SpacetimeVertex(locSpacetimeVertex);
}

const DBeamPhoton* DParticleComboCreator::Create_BeamPhoton_KinFit(const DBeamPhoton* locBeamPhoton, const DKinFitParticle* locKinFitParticle)
{
	auto locBeamIterator = dKinFitBeamPhotonMap.find(locKinFitParticle);
	if(locBeamIterator != dKinFitBeamPhotonMap.end())
		return locBeamIterator->second;

	DBeamPhoton* locNewBeamPhoton = dBeamPhotonfactory->Get_Resource();
	dKinFitBeamPhotonMap.emplace(locKinFitParticle, locNewBeamPhoton);

	locNewBeamPhoton->dCounter = locBeamPhoton->dCounter;
	locNewBeamPhoton->dSystem = locBeamPhoton->dSystem;
	locNewBeamPhoton->setMomentum(DVector3(locKinFitParticle->Get_Momentum().X(),locKinFitParticle->Get_Momentum().Y(),locKinFitParticle->Get_Momentum().Z()));
	locNewBeamPhoton->setPosition(DVector3(locKinFitParticle->Get_Position().X(),locKinFitParticle->Get_Position().Y(),locKinFitParticle->Get_Position().Z()));
	locNewBeamPhoton->setTime(locKinFitParticle->Get_Time());
	locNewBeamPhoton->setErrorMatrix(locKinFitParticle->Get_CovarianceMatrix());
	return locNewBeamPhoton;
}

const DChargedTrackHypothesis* DParticleComboCreator::Create_ChargedHypo_KinFit(const DChargedTrackHypothesis* locOrigHypo, const DKinFitParticle* locKinFitParticle, DKinFitType locKinFitType)
{
	auto locHypoIterator = dKinFitChargedHypoMap.find(locKinFitParticle);
	if(locHypoIterator != dKinFitChargedHypoMap.end())
		return locHypoIterator->second;

	//even if vertex is not fit, p4 is fit: different beta: update time info
	auto locNewHypo = dChargedTrackHypothesisFactory->Get_Resource();
	dKinFitChargedHypoMap.emplace(locKinFitParticle, locNewHypo);

	//p3 & v3
	TVector3 locFitMomentum = locKinFitParticle->Get_Momentum();
	TVector3 locFitVertex = locKinFitParticle->Get_Position();
	locNewHypo->setMomentum(DVector3(locFitMomentum.X(), locFitMomentum.Y(), locFitMomentum.Z()));
	locNewHypo->setPosition(DVector3(locFitVertex.X(), locFitVertex.Y(), locFitVertex.Z()));

	//t & error matrix
	locNewHypo->setTime(locKinFitParticle->Get_Time());
	locNewHypo->setErrorMatrix(locKinFitParticle->Get_CovarianceMatrix());

	//if timing was kinfit, there is no chisq to calculate: forced to correct time
	//therefore, just use measured timing info (pre-kinfit)
	if((locKinFitType == d_SpacetimeFit) || (locKinFitType == d_P4AndSpacetimeFit))
	{
		locNewHypo->Share_FromInput(locOrigHypo, true, true, false); //share all but kinematics
		return locNewHypo;
	}

	//only share tracking info (not timing or kinematics)
	locNewHypo->Share_FromInput(locOrigHypo, true, false, false);

	//update timing info
	if(locKinFitType != d_P4Fit) //a vertex was fit
	{
		//all kinematics propagated to vertex position
		auto locPropagatedRFTime = locOrigHypo->t0() + (locFitVertex.Z() - locOrigHypo->position().Z())/SPEED_OF_LIGHT;
		locNewHypo->Set_T0(locPropagatedRFTime, locOrigHypo->t0_err(), locOrigHypo->t0_detector());
		locNewHypo->Set_TimeAtPOCAToVertex(locNewHypo->time());
	}
	else //only momentum has changed (and thus time)
	{
		locNewHypo->Set_T0(locOrigHypo->t0(), locOrigHypo->t0_err(), locOrigHypo->t0_detector());
		locNewHypo->Set_TimeAtPOCAToVertex(locOrigHypo->Get_TimeAtPOCAToVertex() + locNewHypo->time() - locOrigHypo->time());
	}

	dParticleID->Calc_ChargedPIDFOM(locNewHypo);
	return locNewHypo;
}

const DNeutralParticleHypothesis* DParticleComboCreator::Create_NeutralHypo_KinFit(const DNeutralParticleHypothesis* locOrigHypo, DKinFitParticle* locKinFitParticle, DKinFitType locKinFitType)
{
	auto locHypoIterator = dKinFitNeutralHypoMap.find(locKinFitParticle);
	if(locHypoIterator != dKinFitNeutralHypoMap.end())
		return locHypoIterator->second;

	auto locNewHypo = dNeutralParticleHypothesisFactory->Get_Resource();
	dKinFitNeutralHypoMap.emplace(locKinFitParticle, locNewHypo);

	//p3 & v3
	TVector3 locFitMomentum = locKinFitParticle->Get_Momentum();
	TVector3 locFitVertex = locKinFitParticle->Get_Position();
	locNewHypo->setMomentum(DVector3(locFitMomentum.X(), locFitMomentum.Y(), locFitMomentum.Z()));
	locNewHypo->setPosition(DVector3(locFitVertex.X(), locFitVertex.Y(), locFitVertex.Z()));

	//t & error matrix
	locNewHypo->setTime(locKinFitParticle->Get_Time());
	locNewHypo->setErrorMatrix(locKinFitParticle->Get_CovarianceMatrix());

	//if timing was kinfit, there is no chisq to calculate: forced to correct time
	//also, if vertex was not kinfit, no chisq to calc either: photon beta is still 1, no chisq for massive neutrals
	//therefore, just use measured timing info (pre-kinfit)
	if((locKinFitType == d_P4Fit) || (locKinFitType == d_SpacetimeFit) || (locKinFitType == d_P4AndSpacetimeFit))
	{
		locNewHypo->Share_FromInput(locOrigHypo, true, false); //share timing but not kinematics
		return locNewHypo;
	}

	//update timing info
	auto locPropagatedRFTime = locOrigHypo->t0() + (locFitVertex.Z() - locOrigHypo->position().Z())/SPEED_OF_LIGHT;
	locNewHypo->Set_T0(locPropagatedRFTime, locOrigHypo->t0_err(), locOrigHypo->t0_detector());

	// Calculate DNeutralParticleHypothesis FOM
	unsigned int locNDF = 0;
	double locChiSq = 0.0;
	double locFOM = -1.0; //undefined for non-photons
	if(locNewHypo->PID() == Gamma)
	{
		double locTimePull = 0.0;
		//for this calc: if rf time part of timing constraint, don't use locKinFitParticle->Get_CommonTime() for chisq calc!!!
		locChiSq = dParticleID->Calc_TimingChiSq(locNewHypo, locNDF, locTimePull);
		locFOM = TMath::Prob(locChiSq, locNDF);
	}
	locNewHypo->dChiSq = locChiSq;
	locNewHypo->dNDF = locNDF;
	locNewHypo->dFOM = locFOM;

	return locNewHypo;
}

DKinematicData* DParticleComboCreator::Build_KinematicData(DKinFitParticle* locKinFitParticle, DKinFitType locKinFitType, DVector3 locPreKinFitVertex)
{
	DKinematicData* locKinematicData = dResourcePool_KinematicData.Get_Resource();
	locKinematicData->Reset();
	locKinematicData->setPID(PDGtoPType(locKinFitParticle->Get_PID()));
	locKinematicData->setMomentum(DVector3(locKinFitParticle->Get_Momentum().X(),locKinFitParticle->Get_Momentum().Y(),locKinFitParticle->Get_Momentum().Z()));
	if((locKinFitType == d_P4Fit) || (locKinFitType == d_NoFit))
		locKinematicData->setPosition(locPreKinFitVertex);
	else
		locKinematicData->setPosition(DVector3(locKinFitParticle->Get_Position().X(),locKinFitParticle->Get_Position().Y(),locKinFitParticle->Get_Position().Z()));
	locKinematicData->setTime(locKinFitParticle->Get_Time());
	if(locKinFitParticle->Get_CovarianceMatrix() != NULL)
		locKinematicData->setErrorMatrix(locKinFitParticle->Get_CovarianceMatrix());

	return locKinematicData;
}

}

#endif // DParticleComboCreator_h
