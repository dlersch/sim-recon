// $Id: MyProcessor.h 2319 2006-12-12 18:23:09Z davidl $
// Author: David Lawrence  June 25, 2004
//
//
// MyProcessor.h
//
/// hd_dump print event info to screen
///

#ifndef _MYPROCESSOR_
#define _MYPROCESSOR_


#include "JANA/JEventProcessor.h"
#include "JANA/JEventLoop.h"
#include "JANA/JFactory.h"
#include "TRACKING/DMagneticFieldMapGlueX.h"
#include "JANA/JParameterManager.h"


#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TH3F.h"
#include "TTree.h"

extern int PAUSE_BETWEEN_EVENTS;
extern int SKIP_BORING_EVENTS;
extern int PRINT_ALL;
extern float MAX_EVENTS;
extern int COUNT;
extern int VERBOSE;
extern int USE_TIMING;
extern char* OUTNAME;
extern float MASS;
extern float ERRMATRIXWEIGHT;
//extern TFile *fout;
//extern TH1F *hpi0[4];
//extern TTree *tree;

extern vector<string> toprint;

class MyProcessor:public JEventProcessor
{
	public:
		jerror_t init(void);				///< Called once at program start.
		jerror_t brun(JEventLoop *eventLoop, int runnumber);	///< Called everytime a new run number is detected.
		jerror_t evnt(JEventLoop *eventLoop, int eventnumber);						///< Called every event.
		jerror_t erun(void){return NOERROR;};				///< Called everytime run number changes, provided brun has been called.
		jerror_t fini(void);				///< Called after last event of last event source has been processed.

    //const DMagneticFieldMap *Bfield;
    DMagneticFieldMapGlueX *bfield;

		typedef struct{
			string dataClassName;
			string tag;
		}factory_info_t;
		vector<factory_info_t> fac_info;

  private:
    TFile* fout;
    TH1F* hcl[15][5]; // before-afterfit/fit hypothesis/clcuts
    TH3F* hdetectorhits[15]; // detector hits
    TH1F* hnumpass[15][5]; // before-afterfit/fit hypothesis/clcuts
    TH1F* hinvmass[15][5][16]; //before-afterfit/fit hypothesis /clcuts / mass combos
    TH2F* hpvtheta[15][5][16]; //before-afterfit/fit hypothesis /clcuts / mass combos
    TH2F* hpvcosCM[15][5][16]; //before-afterfit/fit hypothesis /clcuts / mass combos
    TH2F* hphivcosGJ[15][5][16]; //before-afterfit/fit hypothesis /clcuts / mass combos
};

#endif
