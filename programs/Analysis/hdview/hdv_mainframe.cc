
#include <iostream>
#include <iomanip>
using namespace std;

#include <pthread.h>

#include "hdv_mainframe.h"

#include <TPolyMarker3D.h>
#include <TLine.h>
#include <TMarker.h>
#include <TVector3.h>
#include <TGeoVolume.h>
#include <TGeoManager.h>

TGeoVolume *MOTHER = NULL;
TGeoCombiTrans *MotherRotTrans = NULL;

//-------------------
// Constructor
//-------------------
hdv_mainframe::hdv_mainframe(const TGWindow *p, UInt_t w, UInt_t h):TGMainFrame(p,w,h)
{
	fLayout = new TGLayoutHints(kLHintsCenterX | kLHintsCenterY);

	emcanvas = new TRootEmbeddedCanvas("Main Canvas",this,w, h-100, kSunkenFrame, GetWhitePixel());
	emcanvas->SetScrolling(TGCanvas::kCanvasNoScroll);
	AddFrame(emcanvas, fLayout);
	
	TGHorizontalFrame *buttonframe = new TGHorizontalFrame(this, w, 50);
	AddFrame(buttonframe, fLayout);

	next	= new TGTextButton(this,	"&Next", 2);
	quit	= new TGTextButton(this,	"&Quit", 1);
	quit->SetCommand(".q");
	
	buttonframe->AddFrame(next, fLayout);
	buttonframe->AddFrame(quit, fLayout);
	
	MapSubwindows();
	Layout();
	SetWindowName("Hall-D Event Viewer");
	SetIconName("HDView");
	MapWindow();

	maincanvas = emcanvas->GetCanvas();
	maincanvas->cd(0);
	maincanvas->Range(-2.1,-1.0,2.1, 1.0);	
}

//-------------------
// ProcessMessage
//-------------------
Bool_t hdv_mainframe::ProcessMessage(Long_t msg, Long_t parm1, Long_t parm2)
{
	derror_t err;

	switch(msg>>8){
		case kC_COMMAND:
			switch(msg&0xff){
				case kCM_BUTTON:
					switch(parm1){
						case 2: //Next
							err = hdv_getevent();
							break;
					}
					break;
			}
			break;
	}
}


