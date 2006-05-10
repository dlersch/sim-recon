// $Id$
//
//    File: DApplication.cc
// Created: Wed Jun  8 12:00:20 EDT 2005
// Creator: davidl (on Darwin wire129.jlab.org 7.8.0 powerpc)
//

#include <iostream>
#include <iomanip>
using namespace std;

#ifdef __linux__
#include <execinfo.h>
#endif

#include <signal.h>
#include <dlfcn.h>
#include <dirent.h>

#include "DApplication.h"
#include "DEventProcessor.h"
#include "DEventSource.h"
#include "DEventLoop.h"
#include "DEvent.h"
#include "DGeometry.h"
#include "DParameterManager.h"
#include "DLog.h"

void* LaunchThread(void* arg);

DLog dlog;
DApplication *dapp = NULL;

int SIGINT_RECEIVED = 0;
int SIGUSR1_RECEIVED = 0;
int NTHREADS_COMMAND_LINE = 0;

//-----------------------------------------------------------------
// ctrlCHandle
//-----------------------------------------------------------------
void ctrlCHandle(int x)
{
	SIGINT_RECEIVED++;
	cerr<<endl<<"SIGINT received ("<<SIGINT_RECEIVED<<")....."<<endl;
	if(SIGINT_RECEIVED == 3){
		cerr<<endl<<"Three SIGINTS received! Attempting graceful exit ..."<<endl<<endl;
	}
}

//-----------------------------------------------------------------
// USR1_Handle
//-----------------------------------------------------------------
void USR1_Handle(int x)
{
	// This handles when the program receives a USR1(=30) signal.
	// This will dispatch the signal to all processing threads,
	// by sending a SIGUSR2 signal to them
	// causing a DException to be thrown which will be caught
	// and re-thrown while recording the factory stack info which is
	// eventually printed out.
	SIGUSR1_RECEIVED++;
	cerr<<endl<<"SIGUSR1 received ("<<SIGUSR1_RECEIVED<<").....(thread=0x"<<hex<<pthread_self()<<dec<<")"<<endl;
	if(dapp)dapp->SignalThreads(SIGUSR2);
}

//-----------------------------------------------------------------
// USR2_Handle
//-----------------------------------------------------------------
void USR2_Handle(int x)
{
	// This handles when the program receives a USR2(=31) signal.
	// This will dispatch the signal to all processing threads,
	// Causing a DException to be thrown which will be caught
	// and re-thrown while recording the factory stack info which is
	// eventually printed out.
	dapp->Lock();
	cerr<<endl<<"SIGUSR2 received .....(thread=0x"<<hex<<pthread_self()<<dec<<")"<<endl;

#ifdef __linux__
	void * array[50];
	int nSize = backtrace(array, 50);
	char ** symbols = backtrace_symbols(array, nSize);

	cout<<endl;
	cout<<"--- Stack trace for thread=0x"<<hex<<pthread_self()<<dec<<"): ---"<<endl;
	string cmd("c++filt ");
	for (int i = 0; i < nSize; i++){
		char *ptr = strstr(symbols[i], "(");
		if(!ptr)continue;
		char symbol[255];
		strcpy(symbol, ++ptr);
		ptr = strstr(symbol,"+");
		if(!ptr)continue;
		*ptr =0;		
		cmd += symbol;
		cmd += " ";
	}
	system(cmd.c_str());
	cout<<endl;
#else
	cerr<<"Stack trace only supported on Linux at this time"<<endl;
#endif
	dapp->Unlock();
	exit(0);
}

//---------------------------------
// DApplication    (Constructor)
//---------------------------------
DApplication::DApplication(int narg, char* argv[])
{
	// Set up to catch SIGINTs for graceful exits
	signal(SIGINT,ctrlCHandle);

	// Set up to catch SIGUSR1s for stack dumps
	struct sigaction sigaction_usr1;
	sigaction_usr1.sa_handler = USR1_Handle;
	sigemptyset(&sigaction_usr1.sa_mask);
	sigaction_usr1.sa_flags = 0;
	sigaction(SIGUSR1, &sigaction_usr1, NULL);

	// Set up to catch SIGUSR2s for stack dumps
	struct sigaction sigaction_usr2;
	sigaction_usr2.sa_handler = USR2_Handle;
	sigemptyset(&sigaction_usr2.sa_mask);
	sigaction_usr2.sa_flags = 0;
	sigaction(SIGUSR2, &sigaction_usr2, NULL);

	// Initialize application level mutexes
	pthread_mutex_init(&app_mutex, NULL);
	pthread_mutex_init(&current_source_mutex, NULL);
	pthread_mutex_init(&geometry_mutex, NULL);

	// Variables used for calculating the rate
	show_ticker = 1;
	NEvents = 0;
	last_NEvents = 0;
	avg_NEvents = 0;
	avg_time = 0.0;
	rate_instantaneous = 0.0;
	rate_average = 0.0;
	monitor_heartbeat= true;
	
	// Sources
	current_source = NULL;
	for(int i=1; i<narg; i++){
		const char *arg="--nthreads=";
		if(!strncmp(arg, argv[i],strlen(arg))){
			NTHREADS_COMMAND_LINE = atoi(&argv[i][strlen(arg)]);
			continue;
		}
		arg="--so=";
		if(!strncmp(arg, argv[i],strlen(arg))){
			const char* soname = &argv[i][strlen(arg)];
			RegisterSharedObject(soname);
			continue;
		}
		arg="--sodir=";
		if(!strncmp(arg, argv[i],strlen(arg))){
			const char* sodirname = &argv[i][strlen(arg)];
			RegisterSharedObjectDirectory(sodirname);
			continue;
		}
		arg="-P";
		if(!strncmp(arg, argv[i],strlen(arg))){
			char* pstr = strdup(&argv[i][strlen(arg)]);
			if(!strcmp(pstr, "print")){
				// Special option "-Pprint" flags even default parameters to be printed
				dparms.SetParameter("print", "all");
				continue;
			}
			char *ptr = strstr(pstr, "=");
			if(ptr){
				*ptr = 0;
				ptr++;
				dparms.SetParameter(pstr, ptr);
			}else{
				cerr<<__FILE__<<":"<<__LINE__<<" bad parameter argument ("<<argv[i]<<") should be of form -Pkey=value"<<endl;
			}
			free(pstr);
			continue;
		}
		if(argv[i][0] == '-')continue;
		source_names.push_back(argv[i]);
	}
	
	// Global variable
	dapp = this;
}

//---------------------------------
// ~DApplication    (Destructor)
//---------------------------------
DApplication::~DApplication()
{
	for(unsigned int i=0; i<geometries.size(); i++)delete geometries[i];
	geometries.clear();
	for(unsigned int i=0; i<heartbeats.size(); i++)delete heartbeats[i];
	heartbeats.clear();
}

//---------------------------------
// NextEvent
//---------------------------------
derror_t DApplication::NextEvent(DEvent &event)
{
	/// Call the current source's GetEvent() method. If no
	/// source is open, or the current source has no more events,
	/// then open the next source and recall ourself

	pthread_mutex_lock(&current_source_mutex);
	if(!current_source){
		derror_t err = OpenNext();
		pthread_mutex_unlock(&current_source_mutex);
		if(err != NOERROR)return err;
		return NextEvent(event);
	}
	
	// Read next event from source. If none, then simply set current_source
	// to NULL and recall ourself
	switch(current_source->GetEvent(event)){
		case NO_MORE_EVENTS_IN_SOURCE:
			current_source = NULL;
			pthread_mutex_unlock(&current_source_mutex);
			return NextEvent(event);
			break;
		default:
			break;
	}

	pthread_mutex_unlock(&current_source_mutex);

	// Event counter
	NEvents++;

	return NOERROR;
}

//---------------------------------
// AddProcessor
//---------------------------------
derror_t DApplication::AddProcessor(DEventProcessor *processor)
{
	processor->SetDApplication(this);
	processors.push_back(processor);

	return NOERROR;
}

//---------------------------------
// RemoveProcessor
//---------------------------------
derror_t DApplication::RemoveProcessor(DEventProcessor *processor)
{
	vector<DEventProcessor*>::iterator iter = processors.begin();
	for(; iter!=processors.end(); iter++){
		if((*iter) == processor){
			processors.erase(iter);
			break;
		}
	}

	return NOERROR;
}

//---------------------------------
// GetProcessors
//---------------------------------
derror_t DApplication::GetProcessors(vector<DEventProcessor*> &processors)
{
	processors = this->processors;

	return NOERROR;
}

//---------------------------------
// AddDEventLoop
//---------------------------------
derror_t DApplication::AddDEventLoop(DEventLoop *loop, double* &heartbeat)
{
	pthread_mutex_lock(&app_mutex);
	loops.push_back(loop);

	// Call InitFactories routines from shared objects.
	// Doing this here adds the factories from the
	// shared object to the front of the list before the
	// DEventLoop calls all of the subsystem Init_* routines
	for(unsigned int i=0; i<InitFactoriesProcs.size();i++){
		(*InitFactoriesProcs[i])(loop);
	}
	
	// For the heartbeat, we use a double that the thread should
	// set to zero each event. It will be added to periodically 
	// in Run() below by the main thread so as to keep track of
	// the amount of time in seconds the thread has been inactive.
	heartbeat = new double;
	*heartbeat = 0.0;
	heartbeats.push_back(heartbeat);
	
	pthread_mutex_unlock(&app_mutex);

	return NOERROR;
}

//---------------------------------
// RemoveDEventLoop
//---------------------------------
derror_t DApplication::RemoveDEventLoop(DEventLoop *loop)
{
	vector<DEventLoop*>::iterator iter = loops.begin();
	vector<double*>::iterator hbiter = heartbeats.begin();
	for(; iter!=loops.end(); iter++, hbiter++){
		if((*iter) == loop){
			loops.erase(iter);
			heartbeats.erase(hbiter);
			break;
		}
	}

	return NOERROR;
}

//---------------------------------
// GetDEventLoops
//---------------------------------
derror_t DApplication::GetDEventLoops(vector<DEventLoop*> &loops)
{
	loops = this->loops;

	return NOERROR;
}

//---------------------------------
// GetDGeometry
//---------------------------------
DGeometry* DApplication::GetDGeometry(unsigned int run_number)
{
	/// Return a pointer a DGeometry object that is valid for the
	/// given run number.
	///
	/// This first searches through the list of existing DGeometry
	/// objects created by this DApplication object to see if it
	/// already has the right one.If so, a pointer to it is returned.
	/// If not, a new DGeometry object is created and added to the
	/// internal list.
	/// Note that since we need to make sure the list is not modified 
	/// by one thread while being searched by another, a mutex is
	/// locked while searching the list. It is <b>NOT</b> efficient
	/// to get the DGeometry object pointer every event. Factories
	/// should get a copy in their brun() callback and keep a local
	/// copy of the pointer for use in the evnt() callback.

	// Lock mutex to keep list from being modified while we search it
	pthread_mutex_lock(&geometry_mutex);

	vector<DGeometry*>::iterator iter = geometries.begin();
	for(; iter!=geometries.end(); iter++){
		if((*iter)->IsInRange(run_number)){
			// Found it! Unlock mutex and return pointer
			DGeometry *g = *iter;
			pthread_mutex_unlock(&geometry_mutex);
			return g;
		}
	}
	
	// DGeometry object for this run_number doesn't exist in our list.
	// Create a new one and add it to the list.
	DGeometry *g = new DGeometry(run_number);
	if(g)geometries.push_back(g);

	// Unlock geometry mutex
	pthread_mutex_unlock(&geometry_mutex);

	return g;
}

//----------------
// LaunchThread
//----------------
void* LaunchThread(void* arg)
{
	/// This is a global function that is used to create
	/// a new DEventLoop object which runs in its own thread.

	// We don't want event processing threads handling
	// the SIGUSR1 signals. They should be handled by the main thread
	//sigset_t set;
	//sigemptyset(&set);
	//sigaddset(&set, SIGUSR2);
	//pthread_sigmask(SIG_BLOCK, &set, NULL);

	// Create DEventLoop object. He automatically registers himself
	// with the DApplication object. 
	DEventLoop *eventLoop = new DEventLoop((DApplication*)arg);

	// Loop over events until done. Catch any derror_t's thrown
	try{
		eventLoop->Loop();
	}catch(DException *exception){
		if(exception)delete exception;
		cerr<<__FILE__<<":"<<__LINE__<<" EXCEPTION caught for thread "<<pthread_self()<<endl;
	}

	// Delete DEventLoop object. He automatically de-registers himself
	// with the DEventLoop Object
	delete eventLoop;

	return arg;
}

//---------------------------------
// Init
//---------------------------------
derror_t DApplication::Init(void)
{
	// Call init Processors (note: factories don't exist yet)
	try{
		for(unsigned int i=0;i<processors.size();i++)processors[i]->init();
	}catch(derror_t err){
		cerr<<endl;
		cerr<<__FILE__<<":"<<__LINE__<<" Error thrown ("<<err<<") from DEventProcessor::init()"<<endl;
		exit(-1);
	}
	
	return NOERROR;
}

//---------------------------------
// Run
//---------------------------------
derror_t DApplication::Run(DEventProcessor *proc, int Nthreads)
{
	// If a DEventProcessor was passed, then add it to our list first
	if(proc){
		derror_t err = AddProcessor(proc);
		if(err)return err;
	}

	// Call init() routines
	Init();

	// Launch all threads
	if(Nthreads<1){
		// If Nthreads is less than 1 then automatically set to 1
		Nthreads = 1;
	}
	if(NTHREADS_COMMAND_LINE>0){
		Nthreads = NTHREADS_COMMAND_LINE;
	}
	cout<<"Launching threads "; cout.flush();
	for(int i=0; i<Nthreads; i++){
		pthread_t thr;
		pthread_create(&thr, NULL, LaunchThread, this);
		threads.push_back(thr);
		cout<<".";cout.flush();
	}
	cout<<endl;
	
	// Do a sleepy loop so the threads can do their work
	struct timespec req, rem;
	req.tv_nsec = (int)0.5E9; // set to 1/2 second
	req.tv_sec = 0;
	double sleep_time = (double)req.tv_sec + (1.0E-9)*(double)req.tv_nsec;
	do{
		// Sleep for a specific amount of time and calculate the rate
		// on each iteration through the loop
		rem.tv_sec = rem.tv_nsec = 0;
		nanosleep(&req, &rem);
		if(rem.tv_sec == 0 && rem.tv_nsec == 0){
			// If there was no time remaining, then we must have slept
			// the whole amount
			int delta_NEvents = NEvents - last_NEvents;
			avg_NEvents += delta_NEvents;
			avg_time += sleep_time;
			rate_instantaneous = (double)delta_NEvents/sleep_time;
			rate_average = (double)avg_NEvents/avg_time;
		}else{
			cout<<__FILE__<<":"<<__LINE__<<" didn't sleep full "<<sleep_time<<" seconds!"<<endl;
		}
		last_NEvents = NEvents;
		
		// If show_ticker is set, then update the screen with the rate(s)
		if(show_ticker && loops.size()>0)PrintRate();
		
		if(SIGINT_RECEIVED)Quit();
		
		// Add time slept to all heartbeats
		double rem_time = (double)rem.tv_sec + (1.0E-9)*(double)rem.tv_nsec;
		double slept_time = sleep_time - rem_time;
		for(unsigned int i=0;i<heartbeats.size();i++){
			double *hb = heartbeats[i];
			*hb += slept_time;
			if(monitor_heartbeat && (*hb > 2.0+sleep_time)){
				// Thread hasn't done anything for more than 2 seconds. 
				// Remove it from monitoring lists.
				cerr<<" Thread "<<i<<" hasn't responded in "<<*hb<<" seconds. Delisting it..."<<endl;
				DEventLoop *loop = *(loops.begin()+i);
				loops.erase(loops.begin()+i);
				heartbeats.erase(heartbeats.begin()+i);
				
				// Signal the non-responsive thread to commit suicide
				pthread_kill(loop->GetPThreadID(), SIGHUP);
			}
		}

		// When a DEventLoop runs out of events, it removes itself from
		// the list before returning from the thread.
	}while(loops.size() > 0);
	
	// Call erun() and fini() methods and delete event sources
	Fini();
	
	cout<<" "<<NEvents<<" events processed. Average rate: "
		<<Val2StringWithPrefix(rate_average)<<"Hz"<<endl;

	return NOERROR;
}

//---------------------------------
// Fini
//---------------------------------
derror_t DApplication::Fini(void)
{
	// Make sure erun is called
	for(unsigned int i=0;i<processors.size();i++){
		DEventProcessor *proc = processors[i];
		if(proc->brun_was_called() && !proc->erun_was_called()){
			try{
				proc->erun();
			}catch(derror_t err){
				cerr<<endl;
				cerr<<__FILE__<<":"<<__LINE__<<" Error thrown ("<<err<<") from DEventProcessor::erun()"<<endl;
			}
			proc->Set_erun_called();
		}
	}

	// Call fini Processors
	try{
		for(unsigned int i=0;i<processors.size();i++)processors[i]->fini();
	}catch(derror_t err){
		cerr<<endl;
		cerr<<__FILE__<<":"<<__LINE__<<" Error thrown ("<<err<<") from DEventProcessor::fini()"<<endl;
	}
	
	// Delete all sources allowing them to close cleanly
	for(unsigned int i=0;i<sources.size();i++)delete sources[i];
	sources.clear();

	return NOERROR;
}

//---------------------------------
// Pause
//---------------------------------
void DApplication::Pause(void)
{
	vector<DEventLoop*>::iterator iter = loops.begin();
	for(; iter!=loops.end(); iter++){
		(*iter)->Pause();
	}
}

//---------------------------------
// Resume
//---------------------------------
void DApplication::Resume(void)
{
	vector<DEventLoop*>::iterator iter = loops.begin();
	for(; iter!=loops.end(); iter++){
		(*iter)->Resume();
	}
}

//---------------------------------
// Quit
//---------------------------------
void DApplication::Quit(void)
{
	cout<<endl<<"Telling all threads to quit ..."<<endl;
	vector<DEventLoop*>::iterator iter = loops.begin();
	for(; iter!=loops.end(); iter++){
		(*iter)->Quit();
	}
}

//----------------
// Val2StringWithPrefix
//----------------
string DApplication::Val2StringWithPrefix(float val)
{
	char *units = "";
	if(val>1.5E9){
		val/=1.0E9;
		units = "G";
	}else 	if(val>1.5E6){
		val/=1.0E6;
		units = "M";
	}else if(val>1.5E3){
		val/=1.0E3;
		units = "k";
	}else if(val<1.0E-7){
		units = "";
	}else if(val<1.0E-4){
		val/=1.0E6;
		units = "u";
	}else if(val<1.0E-1){
		val/=1.0E3;
		units = "m";
	}
	
	char str[256];
	sprintf(str,"%3.1f%s", val, units);

	return string(str);
}
	
//----------------
// PrintRate
//----------------
void DApplication::PrintRate(void)
{
	string event_str = Val2StringWithPrefix(NEvents) + " events";
	string ir_str = Val2StringWithPrefix(rate_instantaneous) + "Hz";
	string ar_str = Val2StringWithPrefix(rate_average) + "Hz";
	cout<<"  "<<event_str<<"   "<<ir_str<<"  (average rate: "<<ar_str<<")   \r";
	cout.flush();
}

//---------------------------------
// OpenNext
//---------------------------------
derror_t DApplication::OpenNext(void)
{
	/// Open the next source in the list. If there are none,
	/// then return NO_MORE_EVENT_SOURCES
	
	if(sources.size() >= source_names.size())return NO_MORE_EVENT_SOURCES;
	
	// Get the source type for the next source name from the command line
	// Instantiate an object of the appropriate type.
	const char *sname = source_names[sources.size()];
	const string type = DEventSource::GuessSourceType(sname);
	
	current_source = NULL;
	
	// Look through shared objects first
	for(unsigned int i=0; i<EventSourceSharedObjects.size(); i++){
		EventSourceSharedObject_t &esso = EventSourceSharedObjects[i];
		if(type == esso.name){
			current_source = (*esso.MakeDEventSource)(sname);
			if(current_source)cout<<"Using "<<type<<" from "<<esso.soname<<endl;
			break;
		}
	}
	
	// If source not found in shared object, try built-in
	if(!current_source){
		current_source = DEventSource::OpenSource(sname);
	}

	if(!current_source){
		cerr<<"Unknown source type \""<<sname<<"\""<<endl;
	}
	
	// Add source to list (even if it's NULL!)
	sources.push_back(current_source);
	
	return NOERROR;
}

//---------------------------------
// RegisterSharedObject
//---------------------------------
derror_t DApplication::RegisterSharedObject(const char *soname)
{
	// Open shared object
	void* handle = dlopen(soname, RTLD_LAZY);
	if(!handle){
		cerr<<dlerror()<<endl;
		return NOERROR;
	}
	
	int things_found = 0;
	
	// Look for an event source
	GetDEventSourceType_t *GetDEventSourceType = (GetDEventSourceType_t*)dlsym(handle, "GetDEventSourceType");
	if(GetDEventSourceType){
		EventSourceSharedObject_t esso;
		esso.soname = soname;
		esso.name = (*GetDEventSourceType)();
		esso.MakeDEventSource = (MakeDEventSource_t*)dlsym(handle, "MakeDEventSource");
		if(!esso.MakeDEventSource){
			cerr<<dlerror()<<endl;
		}else{
			cout<<"Adding event source \""<<esso.name<<"\" from "<<soname<<endl;
			EventSourceSharedObjects.push_back(esso);
			things_found++;
		}
	}else{
		//cerr<<dlerror()<<endl;
	}
	
	// Look for InitFactories
	InitFactories_t *InitFactories = (InitFactories_t*)dlsym(handle, "InitFactories");
	if(InitFactories){
		cout<<"Adding InitFactories from "<<soname<<endl;
		InitFactoriesProcs.push_back(InitFactories);
		things_found++;
	}

	// Look for InitProcessors
	InitProcessors_t *InitProcessors = (InitProcessors_t*)dlsym(handle, "InitProcessors");
	if(InitProcessors){
		cout<<"Calling InitProcessors from "<<soname<<endl;
		(*InitProcessors)(this);
		things_found++;
	}
	
	if(!things_found)cout<<" --- Nothing useful found in "<<soname<<" ---"<<endl;

	return NOERROR;
}

//---------------------------------
// RegisterSharedObjectDirectory
//---------------------------------
derror_t DApplication::RegisterSharedObjectDirectory(const char *sodirname)
{
	DIR *dir = opendir(sodirname);

	struct dirent *d;
	char full_path[512];
	while((d=readdir(dir))){
		if(strncmp(d->d_name, "lib", 3))continue;
		if(strcmp(&d->d_name[strlen(d->d_name)-3], ".so"))continue;
		sprintf(full_path, "%s/%s",sodirname, d->d_name);

		RegisterSharedObject(full_path);
	}

	return NOERROR;
}

//---------------------------------
// SignalThreads
//---------------------------------
void DApplication::SignalThreads(int signo)
{
	// Send signal "signo" to all processing threads.
	for(unsigned int i=0; i<threads.size(); i++){
		pthread_kill(threads[i], signo);
	}
}
