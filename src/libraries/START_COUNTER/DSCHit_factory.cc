// $Id$
//
//    File: DSCHit_factory.cc
// Created: Tue Aug  6 12:53:32 EDT 2013
// Creator: davidl (on Darwin harriet.jlab.org 11.4.2 i386)
//


#include <iostream>
#include <iomanip>
#include <cmath>
using namespace std;

#include <START_COUNTER/DSCDigiHit.h>
#include <START_COUNTER/DSCTDCDigiHit.h>
#include "DSCHit_factory.h"
using namespace jana;

//------------------
// init
//------------------
jerror_t DSCHit_factory::init(void)
{
	DELTA_T_ADC_TDC_MAX = 4.0; // ns
	gPARMS->SetDefaultParameter("SC:DELTA_T_ADC_TDC_MAX", DELTA_T_ADC_TDC_MAX, "Maximum difference in ns between a (calibrated) fADC time and F1TDC time for them to be matched in a single hit");

	/// set the base conversion scales
	a_scale    = 2.0E-2/5.2E-5; 
	t_scale    = 0.0625;   // 62.5 ps/count
	tdc_scale  = 0.060;    // 60 ps/count

	return NOERROR;
}

//------------------
// brun
//------------------
jerror_t DSCHit_factory::brun(jana::JEventLoop *eventLoop, int runnumber)
{

	/// Read in calibration constants
	jout << "In DSCHit_factory, loading constants..." << endl;

	// load scale factors
	map<string,double> scale_factors;
	if(eventLoop->GetCalib("/START_COUNTER/digi_scales", scale_factors))
	    jout << "Error loading /START_COUNTER/digi_scales !" << endl;
	if( scale_factors.find("SC_ADC_ASCALE") != scale_factors.end() ) {
	    a_scale = scale_factors["SC_ADC_ASCALE"];
	} else {
	    jerr << "Unable to get SC_ADC_ASCALE from /START_COUNTER/digi_scales !" << endl;
	}
	if( scale_factors.find("SC_ADC_TSCALE") != scale_factors.end() ) {
	    t_scale = scale_factors["SC_ADC_TSCALE"];
	} else {
	    jerr << "Unable to get SC_ADC_TSCALE from /START_COUNTER/digi_scales !" << endl;
	}
	if( scale_factors.find("SC_TDC_SCALE") != scale_factors.end() ) {
	    tdc_scale = scale_factors["SC_TDC_SCALE"];
	} else {
	    jerr << "Unable to get SC_TDC_SCALE from /START_COUNTER/digi_scales !" << endl;
	}
	
	if(eventLoop->GetCalib("/START_COUNTER/gains", a_gains))
	    jout << "Error loading /START_COUNTER/gains !" << endl;
	if(eventLoop->GetCalib("/START_COUNTER/pedestals", a_pedestals))
	    jout << "Error loading /START_COUNTER/pedestals !" << endl;
	if(eventLoop->GetCalib("/START_COUNTER/adc_timing_offsets", adc_time_offsets))
	    jout << "Error loading /START_COUNTER/adc_timing_offsets !" << endl;
	if(eventLoop->GetCalib("/START_COUNTER/tdc_timing_offsets", tdc_time_offsets))
	    jout << "Error loading /START_COUNTER/tdc_timing_offsets !" << endl;

	/* 
	   // load higher order corrections
	   map<string,double> in_prop_corr;
	   map<string,double> in_atten_corr;

	   if(!eventLoop->GetCalib("/START_COUNTER/propagation_speed",in_prop_corr ))
	      jout << "Error loading /START_COUNTER/propagation_speed !" << endl;
	   if(!eventLoop->GetCalib("/START_COUNTER/attenuation_factor", in_atten_corr))
	      jout << "Error loading /START_COUNTER/attenuation_factor !" << endl;
	  
	   // propogation correction:  A + Bx
	   propogation_corr_factors.push_back(in_prop_corr["A"]);
	   propogation_corr_factors.push_back(in_prop_corr["B"]);

	   // attenuation correction:  A + Bx + Cx^2 + Dx^3 + Ex^4 + Fx^5
	   attenuation_corr_factors.push_back(in_atten_corr["A"]);
	   attenuation_corr_factors.push_back(in_atten_corr["B"]);
	   attenuation_corr_factors.push_back(in_atten_corr["C"]);
	   attenuation_corr_factors.push_back(in_atten_corr["D"]);
	   attenuation_corr_factors.push_back(in_atten_corr["E"]);
	   attenuation_corr_factors.push_back(in_atten_corr["F"]);
	 */

	return NOERROR;
}

//------------------
// evnt
//------------------
jerror_t DSCHit_factory::evnt(JEventLoop *loop, int eventnumber)
{
	/// Generate DSCHit object for each DSCDigiHit object.
	/// This is where the first set of calibration constants
	/// is applied to convert from digitzed units into natural
	/// units.
	///
	/// Note that this code does NOT get called for simulated
	/// data in HDDM format. The HDDM event source will copy
	/// the precalibrated values directly into the _data vector.

	// First, make hits out of all fADC250 hits
	vector<const DSCDigiHit*> digihits;
	loop->Get(digihits);
	char str[256];

	for(unsigned int i=0; i<digihits.size(); i++){
		const DSCDigiHit *digihit = digihits[i];

		DSCHit *hit = new DSCHit;
		hit->sector = digihit->sector;
		
		// Make sure sector is in valid range
		if( (hit->sector <= 0) && (hit->sector > MAX_SECTORS)) {
			sprintf(str, "DSCDigiHit sector out of range! sector=%d (should be 1-%d)", 
				hit->sector, MAX_SECTORS);
			throw JException(str);
		}

		// Apply calibration constants here
		double A = (double)digihit->pulse_integral;
		double T = (double)digihit->pulse_time;
		// Sectors are numbered from 1-30
		hit->dE = a_scale * a_gains[hit->sector-1] * (A - a_pedestals[hit->sector-1]);
		hit->t = t_scale * (T - adc_time_offsets[hit->sector-1]);
		hit->sigma_t = 4.0;    // ns (what is the fADC time resolution?)
		hit->has_fADC = true;
		hit->has_TDC  = false; // will get set to true below if appropriate

		// add in higher order corrections?
		
		hit->AddAssociatedObject(digihit);
		
		_data.push_back(hit);
	}

	// Second, loop over TDC hits, matching them to the
	// existing fADC hits where possible and updating
	// their time information. If no match is found, then
	// create a new hit with just the TDC info.
	vector<const DSCTDCDigiHit*> tdcdigihits;
	loop->Get(tdcdigihits);
	for(unsigned int i=0; i<tdcdigihits.size(); i++){
		const DSCTDCDigiHit *digihit = tdcdigihits[i];

		// Make sure sector is in valid range
		if( (digihit->sector <= 0) && (digihit->sector > MAX_SECTORS)) {
			sprintf(str, "DSCDigiHit sector out of range! sector=%d (should be 1-%d)", 
				digihit->sector, MAX_SECTORS);
			throw JException(str);
		}

		// Apply calibration constants here
		double T = (double)digihit->time;
		T = tdc_scale * (T - tdc_time_offsets[digihit->sector-1]);

		// Look for existing hits to see if there is a match
		// or create new one if there is no match
		DSCHit *hit = FindMatch(digihit->sector, T);
		if(!hit){
			hit = new DSCHit;
			hit->sector = digihit->sector;
			hit->dE = 0.0;
			hit->has_fADC = false;

			_data.push_back(hit);
		}		
		
		hit->t = T;
		hit->sigma_t = 0.160;    // ns (what is the SC TDC time resolution?)
		hit->has_TDC = true;

		// add in higher order corrections?
		
		hit->AddAssociatedObject(digihit);
	}

	return NOERROR;
}

//------------------
// FindMatch
//------------------
DSCHit* DSCHit_factory::FindMatch(int sector, double T)
{
	// Loop over existing hits (from fADC) and look for a match
	// in both the sector and the time.
	for(unsigned int i=0; i<_data.size(); i++){
		DSCHit *hit = _data[i];
		
		if(!hit->has_fADC) continue; // only match to fADC hits, not bachelor TDC hits
		if(hit->sector != sector) continue;
		
		double delta_T = fabs(hit->t - T);
		if(delta_T > DELTA_T_ADC_TDC_MAX) continue;
		
		return hit;
	}
	
	return NULL;
}

//------------------
// erun
//------------------
jerror_t DSCHit_factory::erun(void)
{
	return NOERROR;
}

//------------------
// fini
//------------------
jerror_t DSCHit_factory::fini(void)
{
	return NOERROR;
}


//------------------------------------
// GetConstant
//   Allow a few different interfaces
//------------------------------------
const double DSCHit_factory::GetConstant(const vector<double> &the_table, const int in_sector) const {
	
	char str[256];
	
	if( (in_sector < 0) || (in_sector >= MAX_SECTORS)) {
		sprintf(str, "Bad sector # requested in DSCHit_factory::GetConstant()! requested=%d , should be %ud", in_sector, MAX_SECTORS);
		cerr << str << endl;
		throw JException(str);
	}

	return the_table[in_sector];
}

const double DSCHit_factory::GetConstant(const vector<double> &the_table,
				   const DSCDigiHit *in_digihit) const {

	char str[256];
	
	if( (in_digihit->sector < 0) || (in_digihit->sector >= MAX_SECTORS)) {
		sprintf(str, "Bad sector # requested in DSCHit_factory::GetConstant()! requested=%d , should be %ud", in_digihit->sector, MAX_SECTORS);
		cerr << str << endl;
		throw JException(str);
	}
	
	return the_table[in_digihit->sector];
}

const double DSCHit_factory::GetConstant(const vector<double> &the_table,
				   const DSCHit *in_hit) const {

	char str[256];
	
	if( (in_hit->sector < 0) || (in_hit->sector >= MAX_SECTORS)) {
		sprintf(str, "Bad sector # requested in DSCHit_factory::GetConstant()! requested=%d , should be %ud", in_hit->sector, MAX_SECTORS);
		cerr << str << endl;
		throw JException(str);
	}
	
	return the_table[in_hit->sector];
}
/*
const double DSCHit_factory::GetConstant(const vector<double> &the_table,
				   const DTranslationTable *ttab,
				   const int in_rocid, const int in_slot, const int in_channel) const {

	char str[256];
	
	DTranslationTable::csc_t daq_index = { in_rocid, in_slot, in_channel };
	DTranslationTable::DChannelInfo channel_info = ttab->GetDetectorIndex(daq_index);
	
	if( (channel_info.sc.sector <= 0) 
	    || (channel_info.sc.sector > static_cast<unsigned int>(MAX_SECTORS))) {
		sprintf(str, "Bad sector # requested in DSCHit_factory::GetConstant()! requested=%d , should be %ud", channel_info.sc.sector, MAX_SECTORS);
		cerr << str << endl;
		throw JException(str);
	}
	
	return the_table[channel_info.sc.sector];
}
*/
