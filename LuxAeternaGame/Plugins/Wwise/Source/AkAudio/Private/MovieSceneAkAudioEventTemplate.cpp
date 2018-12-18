#include "MovieSceneAkAudioEventTemplate.h"
#include "AkAudioDevice.h"
#include "AkAudioClasses.h"

#include "MovieSceneAkAudioEventSection.h"

#include "MovieSceneExecutionToken.h"
#include "IMovieScenePlayer.h"

/** Defines the behaviour of an AKAdioEventSection within UE sequencer. */
struct FMovieSceneAkAudioEventSectionData
{
	FMovieSceneAkAudioEventSectionData(const UMovieSceneAkAudioEventSection& InSection)
		: EventTracker(InSection.EventTracker)
	{
        EventTracker->bStopAtSectionEnd = InSection.EventShouldStopAtSectionEnd();
        EventTracker->EventName         = InSection.GetEventName();
        EventTracker->ClipStartTime     = InSection.GetStartTime();
        EventTracker->ClipEndTime       = InSection.GetEndTime();
        EventTracker->EventDuration     = InSection.GetEventDuration();
        EventTracker->EmptyPlayingIDs();
        EventTracker->EmptyScheduledStops();
        RetriggerEvent = InSection.ShouldRetriggerEvent();
    }

	float inline GetTimeInSeconds(const FMovieSceneContext& Context)
	{
#if UE_4_20_OR_LATER
		return (float)Context.GetFrameRate().AsSeconds(Context.GetTime());
#else
		return (float)Context.GetTime();
#endif
	}

	void Update(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, IMovieScenePlayer& Player, FAkAudioDevice* AudioDevice)
	{
		ensure(AudioDevice != nullptr);

		switch (Player.GetPlaybackStatus())
		{
		    case EMovieScenePlayerStatus::Stopped:
            {
                ResetTracker(AudioDevice);
                break;
            }
		    case EMovieScenePlayerStatus::Playing:
            {
                /* We use a slight hack to support looping in the UE sequencer, by checking If our current time is <= the previous event start time.*/
				const float CurrentTime = GetTimeInSeconds(Context);
                bool bSequencerHasLooped = CurrentTime <= EventTracker->PreviousPlayingTime;
                /* If the section is played and no Wwise event has been triggered */
                if ((Context.GetDirection() == EPlayDirection::Forwards && !EventTracker->IsPlaying()) || bSequencerHasLooped)
                {
                    /* If the sequencer has looped, we want to kill any currently playing events */
                    if (EventTracker->IsPlaying() && bSequencerHasLooped)
                        WwiseEventTriggering::StopAllPlayingIDs(AudioDevice, *EventTracker);

                    /* If the section has a valid object binding */
                    if (Operand.ObjectBindingID.IsValid())
                    {
                        /* If the Wwise event hasn't been previously triggered */
                        if (EventTracker->PreviousEventStartTime == -1.0f || bSequencerHasLooped)
                            ObjectBindingPlay(AudioDevice, Player.FindBoundObjects(Operand), Context);
                        else if (RetriggerEvent)
                            ObjectBindingRetrigger(AudioDevice, Player.FindBoundObjects(Operand), Context);
                    }
                    else /* Otherwwise play or re-trigger the Wwise event on a dummy object. */
                    {
                        if (EventTracker->PreviousEventStartTime == -1.0f || bSequencerHasLooped)
                            MasterPlay(AudioDevice, Context);
                        else if (RetriggerEvent)
                            MasterRetrigger(AudioDevice, Context);
                    }
                }
                EventTracker->PreviousPlayingTime = CurrentTime;
                break;
            }
            case EMovieScenePlayerStatus::Scrubbing:
            case EMovieScenePlayerStatus::Stepping:
            case EMovieScenePlayerStatus::Jumping:
            {
                AkReal32 ProportionalTime = GetProportionalTime(Context);
                if (Operand.ObjectBindingID.IsValid())
                {
                    ObjectBindingScrub(AudioDevice, Player.FindBoundObjects(Operand), Context);
                }
                else
                {
                    MasterScrub(AudioDevice, Context);
                }
                break;
            }
		}
	}

	void SectionBeingDestroyed(FAkAudioDevice* AudioDevice)
	{
		AudioDevice->CancelEventCallbackCookie(EventTracker.Get());
		ResetTracker(AudioDevice);
	}

    void ResetTracker(FAkAudioDevice* AudioDevice) 
    {
        if (EventTracker->bStopAtSectionEnd)
            WwiseEventTriggering::StopAllPlayingIDs(AudioDevice, *EventTracker);
        EventTracker->PreviousEventStartTime = -1.0f;
        EventTracker->PreviousPlayingTime = -1.0f;
        EventTracker->RetriggeredEventDescriptors.Empty();
    }

    TSharedPtr<FWwiseEventTracker> GetEventTracker() { return EventTracker; }

private:
    /** Empty previous retriggered events, play the Wwise event, and seek to the current time. */
    void ObjectBindingPlay(FAkAudioDevice* AudioDevice, TArrayView<TWeakObjectPtr<>> BoundObjects, const FMovieSceneContext& Context)
    {
        if (EventTracker.IsValid() && EventShouldPlay(Context))
        {
            EventTracker->RetriggeredEventDescriptors.Empty();
			const float CurrentTime = GetTimeInSeconds(Context);
			for (auto ObjectPtr : BoundObjects)
            {
                auto Object = ObjectPtr.Get();
                AkPlayingID PlayingID = WwiseEventTriggering::PostEvent(Object, AudioDevice, *EventTracker, CurrentTime);
                WwiseEventTriggering::SeekOnEvent(Object, AudioDevice, GetProportionalTime(Context), *EventTracker, PlayingID);
                EventTracker->PreviousEventStartTime = CurrentTime;
            }
        }
    }

    /** Play the Wwise event, store the event start time in the event tracker, and jump to the current time. */
    void ObjectBindingRetrigger(FAkAudioDevice* AudioDevice, TArrayView<TWeakObjectPtr<>> BoundObjects, const FMovieSceneContext& Context)
    {
        if (EventTracker.IsValid() && EventShouldPlay(Context))
        {
			const float CurrentTime = GetTimeInSeconds(Context);
			for (auto ObjectPtr : BoundObjects)
            {
                auto Object = ObjectPtr.Get();
                AkPlayingID PlayingID = WwiseEventTriggering::PostEvent(Object, AudioDevice, *EventTracker, CurrentTime);
                EventTracker->PreviousEventStartTime = CurrentTime;
                WwiseEventTriggering::SeekOnEvent(Object, AudioDevice, GetProportionalTime(Context), *EventTracker, PlayingID);
            }
        }
    }

    /** Empty previous retriggered events, play the Wwise event, and seek to the current time. */
    void MasterPlay(FAkAudioDevice* AudioDevice, const FMovieSceneContext& Context)
    {
        if (EventTracker.IsValid() && EventShouldPlay(Context))
        {
            EventTracker->RetriggeredEventDescriptors.Empty();
            EventTracker->PreviousEventStartTime = EventTracker->ClipStartTime;
			const float CurrentTime = GetTimeInSeconds(Context);
			AkPlayingID PlayingID = WwiseEventTriggering::PostEventOnDummyObject(AudioDevice, *EventTracker, CurrentTime);
            WwiseEventTriggering::SeekOnEventWithDummyObject(AudioDevice, GetProportionalTime(Context), *EventTracker, PlayingID);
            EventTracker->PreviousEventStartTime = CurrentTime;
        }
    }

    /** Play the Wwise event, store the event start time in the event tracker, and jump to the current time. */
    void MasterRetrigger(FAkAudioDevice* AudioDevice, const FMovieSceneContext& Context)
    {
        if (EventTracker.IsValid() && EventShouldPlay(Context))
        {
			const float CurrentTime = GetTimeInSeconds(Context);
			AkPlayingID PlayingID = WwiseEventTriggering::PostEventOnDummyObject(AudioDevice, *EventTracker, CurrentTime);
            EventTracker->PreviousEventStartTime = CurrentTime;
            WwiseEventTriggering::SeekOnEventWithDummyObject(AudioDevice, GetProportionalTime(Context), *EventTracker, PlayingID);
        }
    }

    void ObjectBindingScrub(FAkAudioDevice* AudioDevice, TArrayView<TWeakObjectPtr<>> BoundObjects, const FMovieSceneContext& Context)
    {
        if (EventTracker.IsValid() && EventShouldPlay(Context))
        {
            EventTracker->RetriggeredEventDescriptors.Empty();
            AkReal32 ProportionalTime = GetProportionalTime(Context);
			const float CurrentTime = GetTimeInSeconds(Context);
			for (auto ObjectPtr : BoundObjects)
            {
                auto Object = ObjectPtr.Get();

                if (!EventTracker->IsPlaying())
                {
                    WwiseEventTriggering::TriggerScrubSnippet(Object, AudioDevice, *EventTracker);
                    EventTracker->PreviousEventStartTime = -1.0f;
                }
                else if (!EventTracker->HasScheduledStop())
                {
                    WwiseEventTriggering::ScheduleStopEventsForCurrentlyPlayingIDs(AudioDevice, *EventTracker);
                }
                WwiseEventTriggering::SeekOnEvent(Object, AudioDevice, ProportionalTime, *EventTracker);
            }
        }
    }

    void MasterScrub(FAkAudioDevice* AudioDevice, const FMovieSceneContext& Context)
    {
        if (EventTracker.IsValid() && EventShouldPlay(Context))
        {
            EventTracker->RetriggeredEventDescriptors.Empty();
            AkReal32 ProportionalTime = GetProportionalTime(Context);
			const float CurrentTime = GetTimeInSeconds(Context);
			if (!EventTracker->IsPlaying())
            {
                WwiseEventTriggering::TriggerScrubSnippetOnDummyObject(AudioDevice, *EventTracker);
            }
            else if (!EventTracker->HasScheduledStop())
            {
                WwiseEventTriggering::ScheduleStopEventsForCurrentlyPlayingIDs(AudioDevice, *EventTracker);
            }
            EventTracker->PreviousEventStartTime = -1.0f;
            WwiseEventTriggering::SeekOnEventWithDummyObject(AudioDevice, ProportionalTime, *EventTracker);
        }
    }

    /** Checks whether the current time is less than the maximum estimated duration OR if the event is set to retrigger. */
    bool EventShouldPlay(const FMovieSceneContext& Context)
    {
        const double PreviousStartTime = EventTracker->PreviousEventStartTime == -1.0f ? EventTracker->ClipStartTime
                                                                                       : EventTracker->PreviousEventStartTime;
        const double CurrentTime = GetTimeInSeconds(Context) - PreviousStartTime;
        return CurrentTime < EventTracker->EventDuration.GetUpperBoundValue() || RetriggerEvent;
    }

    /** Returns the current time as a proportion of the maximum duration (0 - 1) */
    AkReal32 GetProportionalTime(const FMovieSceneContext& Context)
    {
        if (EventTracker.IsValid())
        {
            auto DurationRange = EventTracker->EventDuration;
            // If max and min duration values from metadata are equal, we can assume a deterministic event.
            if (DurationRange.GetLowerBoundValue() == DurationRange.GetUpperBoundValue() && DurationRange.GetUpperBoundValue() != 0.0f)
            {
                const float MaxDuration = DurationRange.GetUpperBoundValue();
                const double PreviousStartTime = EventTracker->PreviousEventStartTime == -1.0f ? EventTracker->ClipStartTime
                                                                                               : EventTracker->PreviousEventStartTime;
                const double CurrentTime = GetTimeInSeconds(Context) - PreviousStartTime;
                // We may want to retrigger depending on the modulo time.
                if (MaxDuration > 0.0f)
                    return (float)fmod(CurrentTime, (double)MaxDuration) / MaxDuration;
            }
            else // Otherwise we need to use the estimated duration for the current event.
            {
                const double PreviousStartTime = EventTracker->PreviousEventStartTime == -1.0f ? EventTracker->ClipStartTime
                                                                                               : EventTracker->PreviousEventStartTime;
                const double CurrentTime = GetTimeInSeconds(Context) - PreviousStartTime;
                const float CurrentDuration = EventTracker->CurrentDurationEstimation;
                // If current duration is uninitialized, use the clip's duration.
                const float MaxDuration = CurrentDuration == -1.0f ? (float)EventTracker->GetClipDuration() : CurrentDuration;
                if (MaxDuration > 0.0f)
                    return (float)fmod(CurrentTime, (double)MaxDuration) / MaxDuration;
            }
        }
        return 0.0f;
    }

    TSharedPtr<FWwiseEventTracker> EventTracker;
    bool RetriggerEvent = false;
};

struct FAkAudioEventEvaluationData : IPersistentEvaluationData
{
    TSharedPtr<FMovieSceneAkAudioEventSectionData> SectionData;
};

struct FAkAudioEventExecutionToken : IMovieSceneExecutionToken
{
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, 
                         FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		auto AudioDevice = FAkAudioDevice::Get();
		if (!AudioDevice)
			return;
        auto persistentData = PersistentData.GetSectionData<FAkAudioEventEvaluationData>();
		TSharedPtr<FMovieSceneAkAudioEventSectionData> SectionData = persistentData.SectionData;
		if (SectionData.IsValid())
			SectionData->Update(Context, Operand, Player, AudioDevice);
	}
};


FMovieSceneAkAudioEventTemplate::FMovieSceneAkAudioEventTemplate(const UMovieSceneAkAudioEventSection* InSection)
	: Section(InSection)
{}

void FMovieSceneAkAudioEventTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, 
                                               const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	auto AudioDevice = FAkAudioDevice::Get();
	if (!AudioDevice)
		return;

	ExecutionTokens.Add(FAkAudioEventExecutionToken());
}

void FMovieSceneAkAudioEventTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	auto AudioDevice = FAkAudioDevice::Get();
	if (!AudioDevice)
		return;

	if (Section)
        PersistentData.AddSectionData<FAkAudioEventEvaluationData>().SectionData = MakeShareable(new FMovieSceneAkAudioEventSectionData(*Section));
}

void FMovieSceneAkAudioEventTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	auto AudioDevice = FAkAudioDevice::Get();
	if (!AudioDevice)
		return;
	TSharedPtr<FMovieSceneAkAudioEventSectionData> SectionData = PersistentData.GetSectionData<FAkAudioEventEvaluationData>().SectionData;
	if (SectionData.IsValid())
	{
		SectionData->SectionBeingDestroyed(AudioDevice);
	}
}
