
// Part of the Soundplane client software by Madrona Labs.
// Copyright (c) 2013 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

#ifndef __SOUNDPLANE_MODEL__
#define __SOUNDPLANE_MODEL__

#include <list>
#include <map>
#include <thread>
#include <stdint.h>

#include "cJSON.h"
#include "MLModel.h"
#include "MLOSCListener.h"
#include "MLNetServiceHub.h"
#include "MLSymbol.h"
#include "MLFileCollection.h"
#include "MLQueue.h"

#include "SoundplaneModelA.h"
#include "SoundplaneDriver.h"

#include "TouchTracker.h"
#include "SoundplaneMIDIOutput.h"
#include "SoundplaneOSCOutput.h"
#include "SoundplaneBinaryData.h"
#include "Zone.h"

using namespace ml;
using namespace std::chrono;

Matrix sensorFrameToSignal(const SensorFrame &f);

typedef enum
{
	xColumn = 0,
	yColumn = 1,
	zColumn = 2,
	dzColumn = 3,
	ageColumn = 4
} TouchSignalColumns;

const int kSensorFrameQueueSize = 16;

class SoundplaneModel :
public SoundplaneDriverListener,
public MLOSCListener,
public MLNetServiceHub,
public MLModel
{
public:
	
	SoundplaneModel();
	~SoundplaneModel();
	
	// SoundplaneDriverListener
	void onStartup() override;
	void onFrame(const SensorFrame& frame) override;
	void onError(int error, const char* errStr) override;
	void onClose() override;
	
	// MLModel
	void doPropertyChangeAction(ml::Symbol , const ml::Value & ) override;
	
	void setAllPropertiesToDefaults();
	
	// MLOSCListener
	void ProcessMessage(const osc::ReceivedMessage &m, const IpEndpointName& remoteEndpoint) override;
	void ProcessBundle(const osc::ReceivedBundle &b, const IpEndpointName& remoteEndpoint) override;
	
	// OSC services
	const std::vector<std::string>& getServicesList();
	
	MLFileCollection& getZonePresetsCollection() { return *mZonePresets; }
	
	float getSampleHistory(int x, int y);
	
	void getHistoryStats(float& mean, float& stdDev);
	int getWidth() { return mSurface.getWidth(); }
	int getHeight() { return mSurface.getHeight(); }
	
	void setDefaultCarriers();
	void setCarriers(const SoundplaneDriver::Carriers& c);
	int enableCarriers(unsigned long mask);
	int getNumCarriers() { return kSoundplaneNumCarriers; }
	void dumpCarriers(const SoundplaneDriver::Carriers& carriers);
	
	void enableOutput(bool b);
	
	int getStateIndex();
	const char* getHardwareStr();
	const char* getStatusStr();
	const char* getClientStr();
	
	int getSerialNumber() const {return mSerialNumber;}
	
	void clear();
	
	void setRaw(bool b);
	bool getRaw(){ return mRaw; }
	
	void beginCalibrate();
	bool isCalibrating() { return mCalibrating; }
	float getCalibrateProgress();
	void endCalibrate();
	
	void beginSelectCarriers();
	bool isSelectingCarriers() { return mSelectingCarriers; }
	float getSelectCarriersProgress();
	void nextSelectCarriersStep();
	void endSelectCarriers();
	
	void setFilter(bool b);
	
	void getMinMaxHistory(int n);
	
	const ml::Matrix& getTouchFrame() { return mTouchFrame; }
	const ml::Matrix& getTouchHistory() { return mTouchHistory; }
	const ml::Matrix getRawSignal() { std::lock_guard<std::mutex> lock(mRawSignalMutex); return mRawSignal; }
	const ml::Matrix getCalibratedSignal() { std::lock_guard<std::mutex> lock(mCalibratedSignalMutex); return sensorFrameToSignal(mCalibratedFrame); }
	
	const ml::Matrix getSmoothedSignal() { std::lock_guard<std::mutex> lock(mSmoothedSignalMutex); return mSmoothedSignal; }
	
	const TouchArray& getTouchArray() { return mTouchArray1; }
	
	bool isWithinTrackerCalibrateArea(int i, int j);
	const int getHistoryCtr() { return mHistoryCtr; }
	
	const std::vector< Zone >::const_iterator getZonesBegin(){ return mZones.begin(); }
	const std::vector< Zone >::const_iterator getZonesEnd(){ return mZones.end(); }
	
	void setStateFromJSON(cJSON* pNode, int depth);
	bool loadZonePresetByName(const std::string& name);
	
	int getDeviceState(void);
	int getClientState(void);
	
	SoundplaneMIDIOutput& getMIDIOutput() { return mMIDIOutput; }
	
private:
	TouchArray mTouchArray1{};
	TouchArray mZoneOutputTouches{};
	
	std::unique_ptr< SoundplaneDriver > mpDriver;
	std::unique_ptr< Queue< SensorFrame > > mSensorFrameQueue;
	
	// TODO order!
	void process(time_point<system_clock> now);
	void outputTouches(TouchArray touches, time_point<system_clock> now);
	void dumpOutputsByZone();
	
	TouchArray trackTouches(const SensorFrame& frame);
	TouchArray getTestTouchesFromTracker(time_point<system_clock> now);
	void saveTouchHistory(const TouchArray& t);

	void initialize();
	bool findNoteChanges(TouchArray t0, TouchArray t1);
	TouchArray scaleTouchPressureData(TouchArray in);
	
	void sendTouchesToZones(TouchArray touches);
	
	void sendFrameToOutputs(time_point<system_clock> now);
	void beginOutputFrame(time_point<system_clock> now);
	void sendTouchToOutputs(int i, int offset, const Touch& t);
	void sendControllerToOutputs(int zoneID, int offset, const ZoneMessage& m);
	void endOutputFrame();
	
	void clearZones();
	void sendParametersToZones();
	
	std::vector< Zone > mZones;
	ml::Matrix mZoneIndexMap;
	
	bool mOutputEnabled;
	
	static const int kMiscStringSize{256};
	void loadZonesFromString(const std::string& zoneStr);
	
	void doInfrequentTasks();
	uint64_t mLastInfrequentTaskTime;
	
	int mSerialNumber;
	
	SoundplaneMIDIOutput mMIDIOutput;
	SoundplaneOSCOutput mOSCOutput;
	
	SensorFrame mSensorFrame{};
	SensorFrame mCalibratedFrame{};
	
	ml::Matrix mSurface;
	
	int	mMaxTouches;
	
	ml::Matrix mTouchFrame;
	std::mutex mTouchFrameMutex;
	ml::Matrix mTouchHistory;
	
	bool mCalibrating;
	bool mTestTouchesOn;
	bool mTestTouchesWasOn;
	bool mRequireSendNextFrame{false};
	bool mSelectingCarriers;
	bool mRaw;
	bool mSendMatrixData;
	
	SoundplaneDriver::Carriers mCarriers;
	
	bool mHasCalibration;
	
	SensorFrameStats mStats;
	SensorFrame mCalibrateMeanInv{};
	
	ml::Matrix mRawSignal;
	std::mutex mRawSignalMutex;
	
	ml::Matrix mCalibratedSignal;
	std::mutex mCalibratedSignalMutex;
	
	SensorFrame mSmoothedFrame{};
	ml::Matrix mSmoothedSignal;
	std::mutex mSmoothedSignalMutex;
	
	int mCalibrateStep; // calibrate step from 0 - end
	int mTotalCalibrateSteps;
	int mSelectCarriersStep;
	
	float mSurfaceWidthInv;
	float mSurfaceHeightInv;
	
	// store current key for each touch to implement hysteresis.
	int mCurrentKeyX[kMaxTouches];
	int mCurrentKeyY[kMaxTouches];
	
	char mHardwareStr[kMiscStringSize];
	char mStatusStr[kMiscStringSize];
	char mClientStr[kMiscStringSize];
	
	TouchTracker mTracker;
	
	int mHistoryCtr;
	bool mCarrierMaskDirty;
	bool mNeedsCarriersSet;
	bool mNeedsCalibrate;
	unsigned long mCarriersMask;
	
	bool mDoOverrideCarriers;
	SoundplaneDriver::Carriers mOverrideCarriers;
	
	std::vector<float> mMaxNoiseByCarrierSet;
	std::vector<float> mMaxNoiseFreqByCarrierSet;
	
	bool mKymaMode;
	int mKymaIsConnected; // TODO more custom clients
	
	std::unique_ptr<MLFileCollection> mTouchPresets;
	std::unique_ptr<MLFileCollection> mZonePresets;
	
	bool mVerbose;
	
	bool mTerminating{false};
	int mProcessCounter{0};
	void processThread();
	std::thread mProcessThread;
	
	size_t mMaxRecentQueueSize{0};
	
	int mDataRate{100};
	time_point<system_clock> mPrevProcessTouchesTime{};
};

#endif // __SOUNDPLANE_MODEL__

