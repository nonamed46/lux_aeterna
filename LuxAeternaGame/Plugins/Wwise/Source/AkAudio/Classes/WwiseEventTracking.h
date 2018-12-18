// Copyright (c) 2017 Audiokinetic Inc. / All Rights Reserved

#pragma once

#include "AkAudioDevice.h"
#include "AkComponent.h"
#include "UObject/ObjectMacros.h"

/** This can be used to track a Wwise event as it is triggered and stopped.
 *  Maintains a collection of playing IDs and a collection of IDs that have scheduled stop calls.
 *  Also maintains a collection of Vector2Ds that indicate the history of start times and durations
 *  of event retriggers.
 */
struct FWwiseEventTracker
{
	static int GetScrubTimeMs() { return 100; }

	/** Callback receieved at various points during lifetime Wwise event. 
	 *  The FWwiseEventTracker is stored in the AkCallbackInfo as pCookie.
	 */
	static void PostEventCallbackHandler(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo)
	{
		if (in_pCallbackInfo != nullptr)
		{
			/* Event end */
			if (in_eType == AkCallbackType::AK_EndOfEvent)
			{
				if (in_pCallbackInfo->pCookie != nullptr)
				{
					auto       CBInfo   = (AkEventCallbackInfo*)in_pCallbackInfo;
					const auto IDToStop = CBInfo->playingID;
					auto       Tracker  = (FWwiseEventTracker*)in_pCallbackInfo->pCookie;
					Tracker->RemovePlayingID(IDToStop);
					Tracker->RemoveScheduledStop(IDToStop);
				}
			}/* Received close to the beginning of the event */
			else if (in_eType == AkCallbackType::AK_Duration)
			{
				if (in_pCallbackInfo->pCookie != nullptr)
				{
					const auto CBInfo  = (AkDurationCallbackInfo*)in_pCallbackInfo;
					auto       Tracker = (FWwiseEventTracker*)in_pCallbackInfo->pCookie;
					Tracker->CurrentDurationEstimation = (CBInfo->fEstimatedDuration * Tracker->CurrentDurationProportionRemaining) / 1000.0f;
					if (Tracker->RetriggeredEventDescriptors.Num() == 0)
					{
						const float PlayPositionOffset = CBInfo->fEstimatedDuration * (1.0f - Tracker->CurrentDurationProportionRemaining) / 1000.0f;
						Tracker->StoreRetriggeredEventDescriptor(PlayPositionOffset);
					}
					else
					{
						Tracker->StoreRetriggeredEventDescriptor();
					}
				}
			}
		}
	}

	void RemoveScheduledStop(AkPlayingID InID)
	{
		FScopeLock autoLock(&ScheduledStopsLock);

		for (auto PlayingID : ScheduledStops)
		{
			if (PlayingID == InID)
			{
				ScheduledStops.Remove(PlayingID);
				break;
			}
		}
	}

	void RemovePlayingID(AkPlayingID InID)
	{
		FScopeLock autoLock(&PlayingIDsLock);

		for (auto PlayingID : PlayingIDs)
		{
			if (PlayingID == InID)
			{
				PlayingIDs.Remove(PlayingID);
				break;
			}
		}
	}

	void TryAddPlayingID(const AkPlayingID& PlayingID)
	{
		if (PlayingID != AK_INVALID_PLAYING_ID)
		{
			FScopeLock autoLock(&PlayingIDsLock);
			PlayingIDs.Add(PlayingID);
		}
	}

	void EmptyPlayingIDs()
	{
		FScopeLock autoLock(&PlayingIDsLock);
		PlayingIDs.Empty();
	}

	void EmptyScheduledStops()
	{
		FScopeLock autoLock(&ScheduledStopsLock);
		ScheduledStops.Empty();
	}

	bool PlayingIDHasScheduledStop(AkPlayingID InID)
	{
		FScopeLock autoLock(&ScheduledStopsLock);

		for (auto PlayingID : ScheduledStops)
		{
			if (PlayingID == InID)
			{
				return true;
			}
		}

		return false;
	}

	void AddScheduledStop(AkPlayingID InID)
	{
		FScopeLock autoLock(&ScheduledStopsLock);
		ScheduledStops.Add(InID);
	}

	/** Use the PreviousEventStartTime and CurrentDurationEstimation to store 
	 *  a trigger time and duration pair. 
	 *
	 *  @param PlayPositionOffset - the offset to apply (if the event was triggered from some non-start position).
	 */
	void StoreRetriggeredEventDescriptor(float PlayPositionOffset = 0.0f)
	{
		RetriggeredEventDescriptors.Add(FVector2D(PreviousEventStartTime - PlayPositionOffset, CurrentDurationEstimation + PlayPositionOffset));
	}

	bool IsDirty = false;

	bool IsPlaying()        const { FScopeLock autoLock(&PlayingIDsLock); return PlayingIDs.Num()     > 0; }
	bool HasScheduledStop() const { FScopeLock autoLock(&ScheduledStopsLock); return ScheduledStops.Num() > 0; }
	float GetClipDuration() const { return ClipEndTime - ClipStartTime; }
	
	TArray<AkPlayingID> PlayingIDs;
	TArray<AkPlayingID> ScheduledStops;
	TArray<FVector2D>   RetriggeredEventDescriptors;
	FFloatRange         EventDuration;
	FString             EventName;
	mutable FCriticalSection    PlayingIDsLock;
	mutable FCriticalSection    ScheduledStopsLock;
	float               ClipStartTime                      = 0.0f;
	float               ClipEndTime                        = 0.0f;
	int                 ScrubTailLengthMs                  = GetScrubTimeMs();
	float               PreviousEventStartTime             = -1.0f;
	float               PreviousPlayingTime                = -1.0f;
	float               CurrentDurationEstimation          = -1.0f;
	float               CurrentDurationProportionRemaining = 1.0f;
	bool                bStopAtSectionEnd                  = true;
};

/** A collection of helper functions for triggering tracked Wwise events */
namespace WwiseEventTriggering
{
	static TArray<AkPlayingID, TInlineAllocator<16>> GetPlayingIds(FWwiseEventTracker& EventTracker)
	{
		FScopeLock autoLock(&EventTracker.PlayingIDsLock);
		return TArray<AkPlayingID, TInlineAllocator<16>> { EventTracker.PlayingIDs };
	}

	static void LogDirtyPlaybackWarning()
	{
		UE_LOG(LogAkAudio, Warning, TEXT("Playback occurred from sequencer section with new changes. You may need to re-generate your soundbanks."));
	}

	static void StopAllPlayingIDs(FAkAudioDevice* AudioDevice, FWwiseEventTracker& EventTracker)
	{
		ensure(AudioDevice != nullptr);
		if (AudioDevice)
		{
			for (auto PlayingID : GetPlayingIds(EventTracker))
			{
				AudioDevice->StopPlayingID(PlayingID);
			}
		}
	}

	static AkPlayingID PostEventOnDummyObject(FAkAudioDevice* AudioDevice, FWwiseEventTracker& EventTracker, float CurrentTime)
	{
		ensure(AudioDevice != nullptr);
		if (AudioDevice)
		{
			AActor* DummyActor = nullptr;
			AkPlayingID PlayingID = AudioDevice->PostEvent(EventTracker.EventName, DummyActor, AkCallbackType::AK_EndOfEvent | AkCallbackType::AK_Duration,
				&FWwiseEventTracker::PostEventCallbackHandler, &EventTracker);
			EventTracker.TryAddPlayingID(PlayingID);
			if (EventTracker.IsDirty)
				LogDirtyPlaybackWarning();
			return PlayingID;
		}
		return AK_INVALID_PLAYING_ID;
	}

	static AkPlayingID PostEvent(UObject* Object, FAkAudioDevice* AudioDevice, FWwiseEventTracker& EventTracker, float CurrentTime)
	{
		ensure(AudioDevice != nullptr);

		if (Object && AudioDevice)
		{
			auto AkComponent = Cast<UAkComponent>(Object);

			if (!IsValid(AkComponent))
			{
				auto Actor = CastChecked<AActor>(Object);
				if (IsValid(Actor))
				{
					AkComponent = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
				}
			}

			if (IsValid(AkComponent))
			{
				AkPlayingID PlayingID = AkComponent->PostAkEventByNameWithCallback(EventTracker.EventName,
					AkCallbackType::AK_EndOfEvent | AkCallbackType::AK_Duration,
					&FWwiseEventTracker::PostEventCallbackHandler,
					&EventTracker);
				EventTracker.TryAddPlayingID(PlayingID);
				if (EventTracker.IsDirty)
					LogDirtyPlaybackWarning();
				return PlayingID;
			}
		}
		return AK_INVALID_PLAYING_ID;
	}

	static void StopEvent(FAkAudioDevice* AudioDevice, AkPlayingID InPlayingID, FWwiseEventTracker* EventTracker)
	{
		ensure(AudioDevice != nullptr);
		if (AudioDevice)
			AudioDevice->StopPlayingID(InPlayingID);
	}

	static void TriggerStopEvent(FAkAudioDevice* AudioDevice, FWwiseEventTracker& EventTracker, AkPlayingID PlayingID)
	{
		AudioDevice->StopPlayingID(PlayingID, (float)EventTracker.ScrubTailLengthMs, AkCurveInterpolation::AkCurveInterpolation_Log1);
		EventTracker.AddScheduledStop(PlayingID);
	}

	static void ScheduleStopEventsForCurrentlyPlayingIDs(FAkAudioDevice* AudioDevice, FWwiseEventTracker& EventTracker)
	{
		ensure(AudioDevice != nullptr);
		if (AudioDevice)
		{
			for (auto PlayingID : GetPlayingIds(EventTracker))
			{
				if (!EventTracker.PlayingIDHasScheduledStop(PlayingID))
				{
					TriggerStopEvent(AudioDevice, EventTracker, PlayingID);
				}
			}
		}
	}

	/** Trigger and EventTracker's Wwise event and schedule an accompanying stop event. */
	static void TriggerScrubSnippetOnDummyObject(FAkAudioDevice* AudioDevice, FWwiseEventTracker& EventTracker)
	{
		ensure(AudioDevice != nullptr);
		if (AudioDevice)
		{
			AActor* DummyActor = nullptr;
			AkPlayingID PlayingID = AudioDevice->PostEvent(EventTracker.EventName, DummyActor, AkCallbackType::AK_EndOfEvent | AkCallbackType::AK_Duration,
				&FWwiseEventTracker::PostEventCallbackHandler, &EventTracker);
			EventTracker.TryAddPlayingID(PlayingID);
			if (EventTracker.IsDirty)
				LogDirtyPlaybackWarning();
			TriggerStopEvent(AudioDevice, EventTracker, PlayingID);
		}
	}

	/** Trigger and EventTracker's Wwise event and schedule an accompanying stop event. */
	static void TriggerScrubSnippet(UObject* Object, FAkAudioDevice* AudioDevice, FWwiseEventTracker& EventTracker)
	{
		ensure(AudioDevice != nullptr);

		if (Object && AudioDevice)
		{
			auto AkComponent = Cast<UAkComponent>(Object);

			if (!IsValid(AkComponent))
			{
				auto Actor = CastChecked<AActor>(Object);
				if (IsValid(Actor))
				{
					AkComponent = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
				}
			}

			if (IsValid(AkComponent))
			{
				AkPlayingID PlayingID = AkComponent->PostAkEventByNameWithCallback(EventTracker.EventName,
					AkCallbackType::AK_EndOfEvent | AkCallbackType::AK_Duration,
					&FWwiseEventTracker::PostEventCallbackHandler,
					&EventTracker);
				EventTracker.TryAddPlayingID(PlayingID);
				if (EventTracker.IsDirty)
					LogDirtyPlaybackWarning();
				TriggerStopEvent(AudioDevice, EventTracker, PlayingID);
			}
		}
	}

	static void SeekOnEvent(UObject* Object, FAkAudioDevice* AudioDevice, AkReal32 in_fPercent, FWwiseEventTracker& EventTracker, AkPlayingID InPlayingID)
	{
		ensure(AudioDevice != nullptr);

		if (Object && AudioDevice)
		{
			auto AkComponent = Cast<UAkComponent>(Object);
			if (!IsValid(AkComponent))
			{
				auto Actor = CastChecked<AActor>(Object);
				if (IsValid(Actor))
				{
					AkComponent = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
				}
			}

			if (IsValid(AkComponent))
			{
				AudioDevice->SeekOnEvent(EventTracker.EventName, AkComponent, in_fPercent, false, InPlayingID);
			}
		}
	}

	static void SeekOnEvent(UObject* Object, FAkAudioDevice* AudioDevice, AkReal32 in_fPercent, FWwiseEventTracker& EventTracker)
	{
		for (auto PlayingID : GetPlayingIds(EventTracker))
		{
			WwiseEventTriggering::SeekOnEvent(Object, AudioDevice, in_fPercent, EventTracker, PlayingID);
		}
	}

	static void SeekOnEventWithDummyObject(FAkAudioDevice* AudioDevice, AkReal32 ProportionalTime, FWwiseEventTracker& EventTracker, AkPlayingID InPlayingID)
	{
		ensure(AudioDevice != nullptr);
		if (AudioDevice)
		{
			if (ProportionalTime < 1.0f && ProportionalTime >= 0.0f)
			{
				AActor* DummyActor = nullptr;
				AudioDevice->SeekOnEvent(EventTracker.EventName, DummyActor, ProportionalTime, false, InPlayingID);
				// Update the duration proportion remaining property of the event tracker, rather than updating the current duration directly here.
				// This way, we ensure that the current duration is updated first by any PostEvent callback, 
				// before it is then multiplied by the remaining proportion.
				EventTracker.CurrentDurationProportionRemaining = 1.0f - ProportionalTime;
			}
		}
	}

	static void SeekOnEventWithDummyObject(FAkAudioDevice* AudioDevice, AkReal32 ProportionalTime, FWwiseEventTracker& EventTracker)
	{
		for (auto PlayingID : GetPlayingIds(EventTracker))
		{
			WwiseEventTriggering::SeekOnEventWithDummyObject(AudioDevice, ProportionalTime, EventTracker, PlayingID);
		}
	}
}
