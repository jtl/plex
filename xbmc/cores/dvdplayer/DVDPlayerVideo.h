
#pragma once

#include "../../utils/thread.h"
#include "DVDMessageQueue.h"
#include "DVDDemuxers\DVDDemuxUtils.h"
#include "DVDCodecs\Video\DVDVideoCodec.h"
#include "DVDClock.h"
#include "DVDOverlayContainer.h"
#include "../VideoRenderers/RenderManager.h"

enum CodecID;
class CDemuxStreamVideo;

#define VIDEO_PICTURE_QUEUE_SIZE 1

class CDVDPlayerVideo : public CThread
{
public:
  CDVDPlayerVideo(CDVDClock* pClock, CDVDOverlayContainer* pOverlayContainer);
  virtual ~CDVDPlayerVideo();

  bool OpenStream(CDVDStreamInfo &hint);
  void CloseStream(bool bWaitForBuffers);

  void Pause();
  void Resume();
  void Flush();

  // waits untill all available data has been rendered
  // just waiting for packetqueue should be enough for video
  void WaitForBuffers()                             { m_messageQueue.WaitUntilEmpty(); }
  bool AcceptsData()                                { return !m_messageQueue.IsFull(); }
  void SendMessage(CDVDMsg* pMsg)                   { m_messageQueue.Put(pMsg); }
  
  void Update(bool bPauseDrawing)                   { g_renderManager.Update(bPauseDrawing); }
  void UpdateMenuPicture();
 
  void EnableSubtitle(bool bEnable)                 { m_bRenderSubs = bEnable; }
  void GetVideoRect(RECT& SrcRect, RECT& DestRect)  { g_renderManager.GetVideoRect(SrcRect, DestRect); }
  float GetAspectRatio()                            { return g_renderManager.GetAspectRatio(); }

  //Set a forced aspect ratio
  void SetAspectRatio(float fAspectRatio);

  __int64 GetDelay();
  void SetDelay(__int64 delay);

  bool IsStalled()                                  { return m_DetectedStill;  }
  int GetNrOfDroppedFrames()                        { return m_iDroppedFrames; }

  bool InitializedOutputDevice();
  
  __int64 GetCurrentPts()                           { return m_iCurrentPts; }

  void SetSpeed(int iSpeed);

  // classes
  CDVDMessageQueue m_messageQueue;
  CDVDOverlayContainer* m_pOverlayContainer;
  
  CDVDClock* m_pClock;

protected:  
  virtual void OnStartup();
  virtual void OnExit();
  virtual void Process();

  enum EOUTPUTSTATUS
  {
    EOS_OK=0,
    EOS_ABORT=1,
    EOS_DROPPED_VERYLATE=2,
    EOS_DROPPED=3,
  };

  EOUTPUTSTATUS OutputPicture(DVDVideoPicture* pPicture, __int64 pts);
  void ProcessOverlays(DVDVideoPicture* pSource, YV12Image* pDest, __int64 pts);

  __int64 m_iCurrentPts; // last pts displayed
  __int64 m_iVideoDelay; // not really needed to be an __int64  
  __int64 m_iFlipTimeStamp; // time stamp of last flippage. used to play at a forced framerate

  int m_iDroppedFrames;
  bool m_bInitializedOutputDevice;
  float m_fFrameRate;

  
  bool m_bRenderSubs;
  
  float m_fForcedAspectRatio;
  
  int m_iNrOfPicturesNotToSkip;
  int m_speed;

  bool m_DetectedStill;

  /* autosync decides on how much of clock we should use when deciding sleep time */
  /* the value is the same as 63% timeconstant, ie that the step response of */
  /* iSleepTime will be at 63% of iClockSleep after autosync frames */
  unsigned int m_autosync;
  
  // classes
  CDVDDemuxSPU* m_pDVDSpu;
  CDVDVideoCodec* m_pVideoCodec;
  
  DVDVideoPicture* m_pTempOverlayPicture;
  
  CRITICAL_SECTION m_critCodecSection;

public:
  class CPresentThread : public CThread
  {
  public:
    CPresentThread( CDVDClock *pClock )
    {           
      m_pClock = pClock;
      m_iTimestamp = 0i64;
      m_iDelay = 0;
      CThread::Create();
      CThread::SetPriority(THREAD_PRIORITY_TIME_CRITICAL);      
      CThread::SetName("CPresentThread");
    }

    virtual ~CPresentThread() { StopThread(); }

    virtual void StopThread()
    {
      CThread::m_bStop = true;
      m_eventFrame.Set();

      CThread::StopThread();
    }

    // aborts any pending displays.
    void AbortPresent() { m_iTimestamp = 0i64; } 

    // delay before we want to present this picture
    void Present(__int64 iTimeStamp, EFIELDSYNC m_OnField); 

    // delay between when we wanted frame to be presented and it acually was
    __int64 GetDelay() { return m_iDelay; }

  protected:

    virtual void Process();

  private:
    __int64 m_iTimestamp;
    __int64 m_iDelay;
    CCriticalSection m_critSection;
    CEvent m_eventFrame;
    CDVDClock *m_pClock;
  } m_PresentThread;

};
